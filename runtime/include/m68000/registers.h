/**
 *    Name:        registers.h
 *    Description: the 68000's registers by name, for the translated program's C
 *    Version:     0.1.0
 *    Created:     29-10-2026
 *    Modified:    29-10-2026
 *    Author:      Sebastian Toyos
 *
 *    license:BSD-3-Clause
 */
#ifndef M68000_REGISTERS_H
#define M68000_REGISTERS_H

#include "m68000/cpu.h"

#define D0 cpu.r[0]
#define D1 cpu.r[1]
#define D2 cpu.r[2]
#define D3 cpu.r[3]
#define D4 cpu.r[4]
#define D5 cpu.r[5]
#define D6 cpu.r[6]
#define D7 cpu.r[7]
#define A0 cpu.r[8]
#define A1 cpu.r[9]
#define A2 cpu.r[10]
#define A3 cpu.r[11]
#define A4 cpu.r[12]
#define A5 cpu.r[13]
#define A6 cpu.r[14]
#define A7 cpu.r[15]
#define OTHER_SP cpu.r[M68K_OTHER_SP]
#define SR_SYSTEM cpu.r[M68K_SR_SYSTEM]

#endif
