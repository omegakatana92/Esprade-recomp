/**
 *    Name:        eeprom.c
 *    Description: the 93C46 serial EEPROM (64 x 16 bits) the board keeps its settings in
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    The serial protocol follows MAME's eepromser_93cxx_device (BSD-3-Clause, Aaron Giles):
 *    CS rising starts a command, the start bit and then 8 bits (2 opcode + 6 address) come in
 *    on DI at CLK rising edges, read data leaves on DO, MSB first.
 */
#include "cave/cave.h"

#include <stdio.h>
#include <string.h>

enum
{
	EE_IN_RESET,
	EE_WAIT_START,
	EE_WAIT_COMMAND,
	EE_READING,
	EE_WAIT_DATA,
	EE_WAIT_COMPLETION
};

static uint16_t ee_read_word(const cave_machine *machine, uint32_t address)
{
	address &= 0x3F;
	return (uint16_t)(((uint16_t)machine->eeprom[address * 2] << 8) | machine->eeprom[address * 2 + 1]);
}

static void ee_write_word(cave_machine *machine, uint32_t address, uint16_t value)
{
	address &= 0x3F;
	machine->eeprom[address * 2] = (uint8_t)(value >> 8);
	machine->eeprom[address * 2 + 1] = (uint8_t)value;
}

void eeprom_reset(cave_machine *machine)
{
	machine->ee_cs = 0;
	machine->ee_clk = 0;
	machine->ee_di = 0;
	machine->ee_state = EE_IN_RESET;
	machine->ee_bits = 0;
	machine->ee_accum = 0;
	machine->ee_shift = 0;
	machine->ee_locked = 1;
	machine->ee_do = 1;
}

int eeprom_do_read(cave_machine *machine)
{
	if (machine->ee_state == EE_WAIT_START)
		return 1;
	if (machine->ee_state == EE_READING)
		return (machine->ee_shift & 0x8000u) ? 1 : 0;
	return 1;
}

static void execute_command(cave_machine *machine)
{
	uint32_t opcode = (machine->ee_accum >> 6) & 3;
	uint32_t address = machine->ee_accum & 0x3F;
	machine->ee_bits = 0;
	machine->ee_shift = 0;
	switch (opcode)
	{
	case 1:                                  /* WRITE */
		machine->ee_state = EE_WAIT_DATA;
		break;
	case 2:                                  /* READ */
		machine->ee_state = EE_READING;
		machine->ee_do = 0;
		break;
	case 3:                                  /* ERASE */
		if (!machine->ee_locked)
			ee_write_word(machine, address, 0xFFFF);
		machine->ee_state = EE_WAIT_COMPLETION;
		break;
	default:                                 /* LOCK / WRITE ALL / ERASE ALL / UNLOCK */
		switch (address >> 4)
		{
		case 0: machine->ee_locked = 1; break;
		case 1:
			if (!machine->ee_locked)
				for (uint32_t index = 0; index < 64; index++)
					ee_write_word(machine, index,
						(uint16_t)(ee_read_word(machine, index) & machine->ee_shift));
			break;
		case 2:
			if (!machine->ee_locked)
				for (uint32_t index = 0; index < 64; index++)
					ee_write_word(machine, index, 0xFFFF);
			break;
		case 3: machine->ee_locked = 0; break;
		}
		if (opcode == 0 && (address >> 4) != 0)
			machine->ee_state = EE_WAIT_COMPLETION;
		else
			machine->ee_state = EE_IN_RESET;
		break;
	}
}

static void clock_rising(cave_machine *machine)
{
	switch (machine->ee_state)
	{
	case EE_WAIT_START:
		if (machine->ee_di)
		{
			machine->ee_accum = 0;
			machine->ee_bits = 0;
			machine->ee_state = EE_WAIT_COMMAND;
		}
		break;
	case EE_WAIT_COMMAND:
		machine->ee_accum = (machine->ee_accum << 1) | (uint32_t)machine->ee_di;
		if (++machine->ee_bits == 8)
			execute_command(machine);
		break;
	case EE_READING:
		if (machine->ee_bits == 0)
			machine->ee_shift = (uint16_t)(ee_read_word(machine, machine->ee_accum & 0x3F) << 0);
		else
			machine->ee_shift = (uint16_t)((machine->ee_shift << 1) | 1);
		machine->ee_do = (machine->ee_shift & 0x8000u) ? 1 : 0;
		machine->ee_bits++;
		break;
	case EE_WAIT_DATA:
		machine->ee_shift = (uint16_t)((machine->ee_shift << 1) | (machine->ee_di & 1));
		if (++machine->ee_bits == 16)
		{
			if (!machine->ee_locked)
				ee_write_word(machine, machine->ee_accum & 0x3F, machine->ee_shift);
			machine->ee_state = EE_WAIT_COMPLETION;
		}
		break;
	default:
		break;
	}
}

void eeprom_write(cave_machine *machine, uint8_t value)
{
	int cs = (value >> 1) & 1;
	int clk = (value >> 2) & 1;
	int di = (value >> 3) & 1;

	if (cs && !machine->ee_cs)
	{
		machine->ee_state = EE_WAIT_START;
		machine->ee_bits = 0;
		machine->ee_accum = 0;
		machine->ee_do = 1;
	}
	else if (!cs && machine->ee_cs)
	{
		machine->ee_state = EE_IN_RESET;
		machine->ee_do = 1;
	}
	machine->ee_cs = cs;
	machine->ee_di = di;
	if (clk && !machine->ee_clk && cs)
		clock_rising(machine);
	machine->ee_clk = clk;
}

void eeprom_save(const cave_machine *machine, const char *path)
{
	FILE *file = fopen(path, "wb");
	if (!file)
		return;
	fwrite(machine->eeprom, 1, DDP_EEPROM_SIZE, file);
	fclose(file);
}

void eeprom_load(cave_machine *machine, const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file)
		return;
	uint8_t data[DDP_EEPROM_SIZE];
	if (fread(data, 1, DDP_EEPROM_SIZE, file) == DDP_EEPROM_SIZE)
		memcpy(machine->eeprom, data, DDP_EEPROM_SIZE);
	fclose(file);
}
