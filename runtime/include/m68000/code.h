/**
 *    Name:        code.h
 *    Description: the translated program's functions by the address they start at (build/generated/entries.c),
 *                 for the calls and jumps through computed addresses, the vectors and the interrupts
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 */
#ifndef M68000_CODE_H
#define M68000_CODE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint32_t address;       /* 24 bits */
	void (*function)(void);
} m68k_code_entry;

/* Sorted by address */
extern const m68k_code_entry m68k_code_entries[];
extern const size_t m68k_code_entry_count;

#ifdef __cplusplus
}
#endif

#endif
