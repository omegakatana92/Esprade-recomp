/**
 *    Name:        m68000.c
 *    Description: the 68000 as the translated C needs it: registers, status, exceptions, interrupts,
 *                 the calls and jumps through computed addresses, and the return addresses the calls
 *                 wait for
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    Behavior follows MAME's 68000 core (src/devices/cpu/m68000, BSD-3-Clause, Olivier Galibert) as
 *    tools/core/m68000/interpreter.py models it.  The CPU executes no instructions here: the program
 *    is native C.
 */
#include "m68000/cpu.h"
#include "m68000/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cave/cave.h"
#include "m68000/ops.h"

m68k_cpu cpu;

#define M68K_CALL_STACK 4096
static uint32_t call_stack[M68K_CALL_STACK];
static int call_depth;
static uint32_t missing_targets;
static uint32_t last_missing;
static int stopped;

static const m68k_code_entry *find(uint32_t address)
{
	address &= 0xFFFFFFu;
	size_t low = 0, high = m68k_code_entry_count;
	while (low < high)
	{
		size_t middle = (low + high) / 2;
		if (m68k_code_entries[middle].address < address)
			low = middle + 1;
		else
			high = middle;
	}
	if (low < m68k_code_entry_count && m68k_code_entries[low].address == address)
		return &m68k_code_entries[low];
	return NULL;
}

/* A dispatch table holds BRA.W/BRA.S or JMP instructions: follow one before asking for C. */
static uint32_t follow_dispatch(uint32_t address)
{
	for (int depth = 0; depth < 8; depth++)
	{
		if (find(address))
			break;
		uint16_t opcode = mem_read16(address);
		int32_t displacement;
		if (opcode == 0x6000)
		{
			displacement = (int16_t)mem_read16(address + 2);
			address = (address + 2 + (uint32_t)displacement) & 0xFFFFFFu;
		}
		else if ((opcode & 0xFF00u) == 0x6000)
		{
			displacement = (int8_t)(opcode & 0xFFu);
			address = (address + 2 + (uint32_t)displacement) & 0xFFFFFFu;
		}
		else if (opcode == 0x4EF9)
		{
			address = mem_read32(address + 2) & 0xFFFFFFu;
		}
		else if (opcode == 0x4EFA)
		{
			displacement = (int16_t)mem_read16(address + 2);
			address = (address + 2 + (uint32_t)displacement) & 0xFFFFFFu;
		}
		else
			break;
	}
	return address;
}

const m68k_code_entry *m68k_find_entry(uint32_t address)
{
	return find(address);
}

static bool dispatch_or_return(uint32_t address, int is_call)
{
	address = follow_dispatch(address & 0xFFFFFFu);
	const m68k_code_entry *entry = find(address);
	if (entry)
	{
		entry->function();
		return true;
	}
	uint16_t opcode = mem_read16(address);
	if (opcode == 0x4E75)                 /* the table's empty handler is an RTS */
	{
		cpu.pc = mem_read32(cpu.r[15]) & 0xFFFFFFu;
		cpu.r[15] += 4;
		return true;
	}
	if (opcode == 0x4E71)                 /* NOP, then look again */
		return dispatch_or_return(address + 2, is_call);
	missing_targets++;
	last_missing = address & 0xFFFFFFu;
	fprintf(stderr, "m68k: no translated code at %06X\n", last_missing);
	if (getenv("ESPRADE_TRACE_MISSING"))
	{
		/* Discovery mode: behave as if the missing routine returned at once, so the
		   run reaches later handlers instead of unwinding to the top. */
		cpu.pc = mem_read32(cpu.r[15]) & 0xFFFFFFu;
		cpu.r[15] += 4;
		(void)is_call;
		return true;
	}
	return false;
}

uint32_t m68k_missing_targets(void) { return missing_targets; }
uint32_t m68k_last_missing_target(void) { return last_missing; }

bool m68k_run_function(uint32_t address)
{
	address = follow_dispatch(address & 0xFFFFFFu);
	const m68k_code_entry *entry = find(address);
	if (!entry)
	{
		missing_targets++;
		last_missing = address & 0xFFFFFFu;
		fprintf(stderr, "m68k: no translated code at %06X\n", last_missing);
		return false;
	}
	entry->function();
	return true;
}

void m68k_reset(void)
{
	memset(&cpu, 0, sizeof(cpu));
	cpu.r[15] = mem_read32(0);          /* initial SSP */
	cpu.r[M68K_OTHER_SP] = 0;
	cpu.r[M68K_SR_SYSTEM] = 0x2700;     /* supervisor, interrupts masked */
	cpu.pc = mem_read32(4);
	cpu.interrupts = 0;
	call_depth = 0;
	stopped = 0;
}

uint32_t m68k_read_sr(void)
{
	uint32_t value = cpu.r[M68K_SR_SYSTEM] & M68K_SR_MASK;
	value |= (uint32_t)cpu.x << 4;
	value |= (uint32_t)cpu.s << 3;
	value |= (uint32_t)cpu.z << 2;
	value |= (uint32_t)cpu.ov << 1;
	value |= (uint32_t)cpu.cy;
	return value;
}

void m68k_write_sr(uint32_t value)
{
	uint32_t old = cpu.r[M68K_SR_SYSTEM];
	uint32_t new = value & M68K_SR_MASK;
	if ((old ^ new) & M68K_SR_S)
	{
		uint32_t swap = cpu.r[15];
		cpu.r[15] = cpu.r[M68K_OTHER_SP];
		cpu.r[M68K_OTHER_SP] = swap;
	}
	cpu.r[M68K_SR_SYSTEM] = new;
	cpu.x = (value >> 4) & 1;
	cpu.s = (value >> 3) & 1;
	cpu.z = (value >> 2) & 1;
	cpu.ov = (value >> 1) & 1;
	cpu.cy = value & 1;
}

static void push16(uint32_t value)
{
	cpu.r[15] -= 2;
	mem_write16(cpu.r[15], (uint16_t)value);
}

static void exception_frame(uint32_t vector, uint32_t pc, int level)
{
	uint32_t old_sr = m68k_read_sr();
	uint32_t new_sr = (old_sr & ~0x8000u) | M68K_SR_S;
	if (level > 0)
		new_sr = (new_sr & ~0x0700u) | ((uint32_t)level << 8);

	/* The frame from the lowest address: SR, PC high, PC low (MAME's 3-word frame) */
	push16(pc & 0xFFFFu);
	push16((pc >> 16) & 0xFFFFu);
	push16(old_sr);
	m68k_write_sr(new_sr);

	uint32_t handler = mem_read32(vector * 4);
	if (!m68k_run_function(handler))
	{
		fprintf(stderr, "m68k: exception %u has no handler at %06X\n", vector, handler & 0xFFFFFFu);
	}
}

bool m68k_exception(uint32_t vector, uint32_t pc, uint32_t from)
{
	(void)from;
	exception_frame(vector, pc & 0xFFFFFFu, 0);
	return cpu.pc == (pc & 0xFFFFFFu);
}

void m68k_take_interrupts(uint32_t resume)
{
	uint32_t level = (uint32_t)cave_interrupt_level();
	uint32_t mask = (m68k_read_sr() >> 8) & 7;
	if (level == 0 || (level != 7 && level <= mask))
		return;
	cave_interrupt_acknowledge((int)level);
	cpu.interrupts++;
	exception_frame(24 + level, resume & 0xFFFFFFu, (int)level);
	/* The frame's VBLANK handler has finished updating the display list. */
	program_frame_end();
}

void m68k_stop(uint32_t value)
{
	m68k_write_sr(value);
	stopped = 1;
}

void m68k_reset_devices(void)
{
	/* the board connects no device to the RESET line */
}

void m68k_calls_push(uint32_t address)
{
	if (call_depth < M68K_CALL_STACK)
		call_stack[call_depth++] = address & 0xFFFFFFu;
	else
		fprintf(stderr, "m68k: call stack overflow\n");
}

bool m68k_returned_to(uint32_t expected, uint32_t from)
{
	(void)from;
	expected &= 0xFFFFFFu;
	if (cpu.pc != expected)
		return false;
	while (call_depth > 0)
	{
		uint32_t address = call_stack[--call_depth];
		if (address == expected)
			break;
	}
	return true;
}

void m68k_call(uint32_t address, uint32_t from)
{
	(void)from;
	if (!dispatch_or_return(address, 1))
	{
		/* let the caller's m68k_returned_to see that control did not come back */
		cpu.pc = 0xFFFFFFFFu;
	}
}

void m68k_jump(uint32_t address, uint32_t from)
{
	(void)from;
	address &= 0xFFFFFFu;
	for (int index = call_depth - 1; index >= 0; index--)
	{
		if (call_stack[index] == address)
		{
			cpu.pc = address;
			return;
		}
	}
	if (!dispatch_or_return(address, 0))
		cpu.pc = 0xFFFFFFFFu;
}
