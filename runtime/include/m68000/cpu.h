/**
 *    Name:        cpu.h
 *    Description: the 68000 as the translated program sees it: its registers and flags, and what the
 *                 runtime does for that C (calls and jumps through computed addresses, exceptions,
 *                 interrupts, the frame)
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    31-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 *    The registers are numbered as tools/core/m68000/lift.py numbers them: D0-D7, A0-A7 (A7 the stack pointer
 *    of the mode the CPU is in), the stack pointer of the other mode, and the system byte of the status
 *    register in its place (SR & 0xA700: trace, supervisor, the interrupt mask). The flags are X N Z V C as
 *    cpu.x, cpu.s, cpu.z, cpu.ov and cpu.cy. Behavior follows MAME's 68000 core (src/devices/cpu/m68000,
 *    Olivier Galibert), which tools/core/konami/reference/compare_lift_with_mame.py checks the lift against.
 */
#ifndef M68000_CPU_H
#define M68000_CPU_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define M68K_OTHER_SP   16
#define M68K_SR_SYSTEM  17
#define M68K_SR_MASK    0xA700u
#define M68K_SR_S       0x2000u

typedef struct
{
	uint32_t r[18];
	uint8_t x, s, z, ov, cy;
	uint32_t pc;                /* where the last return, or jump through the runtime, went */
	uint32_t interrupts;        /* interrupts taken */
} m68k_cpu;

extern m68k_cpu cpu;

/* The status register: read whole, or written whole (the mode's change swaps the stack pointers) */
uint32_t m68k_read_sr(void);
void m68k_write_sr(uint32_t value);

/* The exception 'vector', taken by the instruction at 'from' with 'pc' to come back to: the frame pushed,
   the handler run. True when it came back to 'pc' */
bool m68k_exception(uint32_t vector, uint32_t pc, uint32_t from);

/* The interrupts the board raises that the mask lets through, taken with 'resume' to come back to, as MAME's
   core takes them between two instructions */
void m68k_take_interrupts(uint32_t resume);

/* STOP: the status register, and the CPU waits for an interrupt */
void m68k_stop(uint32_t value);

/* RESET: the reset line pulsed for the devices (the board connects none) */
void m68k_reset_devices(void);

/* A call through a computed address, from the instruction at 'from' */
void m68k_call(uint32_t address, uint32_t from);

/* A jump through a computed address: to the return address a call waits for, a return; to the start of a
   function, a tail call */
void m68k_jump(uint32_t address, uint32_t from);

/* The return addresses the calls wait for: pushed before a call, and taken off when it comes back; true when
   it came back to 'expected' (cpu.pc), else the caller returns too */
void m68k_calls_push(uint32_t address);
bool m68k_returned_to(uint32_t expected, uint32_t from);

/* Where the main loop idles: the board moves on to its next frame when the program has nothing to do */
void program_frame_wait(void);

#ifdef __cplusplus
}
#endif

#endif
