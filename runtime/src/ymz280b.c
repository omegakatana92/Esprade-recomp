/**
 *    Name:        ymz280b.c
 *    Description: Yamaha YMZ280B PCMD8 (8-channel 4-bit ADPCM / 8-bit / 16-bit PCM)
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    Register interface and playback model follow MAME's ymz280b_device
 *    (src/devices/sound/ymz280b.cpp, BSD-3-Clause, Aaron Giles).  The chip runs from the
 *    player's 4 MiB sample ROM at 16.9344 MHz / 384 = 44100 Hz.
 *
 *    For the boot milestone only the register file and the status register are modelled; the
 *    mixer is added by the audio milestone (see docs/milestones).
 */
#include "cave/cave.h"

#include <string.h>

typedef struct
{
	uint16_t fnum;
	uint8_t level;
	uint8_t pan;
	uint8_t mode;
	uint8_t looping;
	uint8_t playing;
	uint32_t start;
	uint32_t stop;
	uint32_t position;
	int32_t signal;
	int32_t step;
} ymz_voice;

typedef struct
{
	uint8_t current_register;
	uint8_t registers[0x100];
	uint8_t status;
	uint8_t irq_mask;
	uint8_t irq_enable;
	ymz_voice voice[8];
	uint32_t ext_address;
	uint8_t ext_latch;
	uint8_t ext_enable;
} ymz_state;

static ymz_state ymz;

void ymz280b_reset(cave_machine *machine)
{
	(void)machine;
	memset(&ymz, 0, sizeof(ymz));
}

static uint8_t read_byte(cave_machine *machine, uint32_t address)
{
	if (!machine->ymz_rom || address >= machine->ymz_size)
		return 0xFF;
	return machine->ymz_rom[address];
}

void ymz280b_write(cave_machine *machine, uint8_t offset, uint8_t value)
{
	(void)machine;
	if ((offset & 1u) == 0)
	{
		ymz.current_register = value;
		return;
	}
	ymz.registers[ymz.current_register] = value;
	/* the mixer milestone interprets the voice registers here */
}

uint8_t ymz280b_read(cave_machine *machine)
{
	if (ymz.ext_enable)
	{
		uint8_t result = ymz.ext_latch;
		ymz.ext_latch = read_byte(machine, ymz.ext_address);
		ymz.ext_address = (ymz.ext_address + 1) & 0xFFFFFF;
		return result;
	}
	uint8_t status = ymz.status;
	ymz.status = 0;
	return status;
}

void ymz280b_set_irq(cave_machine *machine)
{
	(void)machine;
}

void ymz280b_render(cave_machine *machine, int16_t *samples, int frames)
{
	(void)machine;
	memset(samples, 0, (size_t)frames * 2 * sizeof(int16_t));
}
