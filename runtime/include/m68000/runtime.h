/**
 *    Name:        runtime.h
 *    Description: the entry points the board and the ports use to start and resume the translated program
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 */
#ifndef M68000_RUNTIME_H
#define M68000_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "m68000/cpu.h"
#include "m68000/code.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reset from the vector table: SSP at 000000, PC at 000004, supervisor mode, IRQs masked. */
void m68k_reset(void);

/* Run the translated C function that starts at 'address'. False if the table has none. */
bool m68k_run_function(uint32_t address);

/* Look a translated function up without running it. */
const m68k_code_entry *m68k_find_entry(uint32_t address);

/* Fatal bookkeeping: a computed call or jump went where the translation has no code. */
uint32_t m68k_missing_targets(void);
uint32_t m68k_last_missing_target(void);

#ifdef __cplusplus
}
#endif

#endif
