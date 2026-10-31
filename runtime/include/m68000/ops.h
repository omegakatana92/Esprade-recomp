/**
 *    Name:        ops.h
 *    Description: the 68000's ALU helpers and the board's bus, as the translated program's C calls them
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    The semantics follow tools/core/m68000/interpreter.py (BSD-3-Clause, Sebastian Toyos), which is the
 *    C's oracle: shifts, BCD arithmetic, division and the carry/overflow helpers set the same flags, and
 *    memory goes through the board's 16-bit bus with the 68000's byte lanes.
 */
#ifndef M68000_OPS_H
#define M68000_OPS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- the board (runtime/src/machine.c) ---- */
uint16_t cave_read16(uint32_t address);
void cave_write16(uint32_t address, uint16_t value, uint16_t mem_mask);
uint32_t cave_read32(uint32_t address);
void cave_write32(uint32_t address, uint32_t value, uint16_t mem_mask);

/* ---- the bus as the translated C sees it ---- */
uint8_t mem_read8(uint32_t address);
uint16_t mem_read16(uint32_t address);
uint32_t mem_read32(uint32_t address);
void mem_write8(uint32_t address, uint8_t value);
void mem_write16(uint32_t address, uint16_t value);
void mem_write32(uint32_t address, uint32_t value);

/* ---- shifts (the core's m68k_shift) ---- */
enum
{
	M68K_ASL, M68K_ASR, M68K_LSL, M68K_LSR, M68K_ROL, M68K_ROR, M68K_ROXL, M68K_ROXR
};

static inline uint32_t m68k_mask_bits(int bits)
{
	return bits >= 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u);
}

static inline uint32_t m68k_shift(int name, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t top = 1u << (bits - 1);
	uint32_t full = m68k_mask_bits(bits);
	for (uint32_t i = 0; i < count; i++)
	{
		uint32_t out, shifted;
		if (name == M68K_ASL || name == M68K_LSL || name == M68K_ROL || name == M68K_ROXL)
		{
			out = (value & top) ? 1u : 0u;
			shifted = (value << 1) & full;
			if (name == M68K_ROL)
				shifted |= out;
			else if (name == M68K_ROXL)
			{
				shifted |= extend & 1u;
				extend = out;
			}
		}
		else
		{
			out = value & 1u;
			shifted = (value & full) >> 1;
			if (name == M68K_ASR)
				shifted |= value & top;
			else if (name == M68K_ROR)
				shifted |= out << (bits - 1);
			else if (name == M68K_ROXR)
			{
				shifted |= (extend & 1u) << (bits - 1);
				extend = out;
			}
		}
		value = shifted;
	}
	return value & full;
}

static inline uint32_t m68k_shift_carry(int name, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	uint32_t top = 1u << (bits - 1);
	uint32_t full = m68k_mask_bits(bits);
	uint32_t carry = (name == M68K_ROXL || name == M68K_ROXR) ? (extend & 1u) : 0u;
	for (uint32_t i = 0; i < count; i++)
	{
		uint32_t out, shifted;
		if (name == M68K_ASL || name == M68K_LSL || name == M68K_ROL || name == M68K_ROXL)
		{
			out = (value & top) ? 1u : 0u;
			shifted = (value << 1) & full;
			if (name == M68K_ROL)
				shifted |= out;
			else if (name == M68K_ROXL)
			{
				shifted |= extend & 1u;
				extend = out;
			}
		}
		else
		{
			out = value & 1u;
			shifted = (value & full) >> 1;
			if (name == M68K_ASR)
				shifted |= value & top;
			else if (name == M68K_ROR)
				shifted |= out << (bits - 1);
			else if (name == M68K_ROXR)
			{
				shifted |= (extend & 1u) << (bits - 1);
				extend = out;
			}
		}
		value = shifted;
		carry = out;
	}
	return carry;
}

static inline uint32_t m68k_shift_overflow(int name, uint32_t value, uint32_t count, uint32_t extend, int bits)
{
	(void)extend;
	uint32_t top = 1u << (bits - 1);
	uint32_t full = m68k_mask_bits(bits);
	uint32_t overflow = 0;
	for (uint32_t i = 0; i < count; i++)
	{
		uint32_t shifted;
		if (name == M68K_ASL || name == M68K_LSL || name == M68K_ROL || name == M68K_ROXL)
		{
			uint32_t out = (value & top) ? 1u : 0u;
			shifted = (value << 1) & full;
			if (name == M68K_ROL)
				shifted |= out;
			else if (name == M68K_ROXL)
				shifted |= extend & 1u;
			if (name == M68K_ASL && ((shifted ^ value) & top))
				overflow = 1;
		}
		else
		{
			uint32_t out = value & 1u;
			shifted = (value & full) >> 1;
			if (name == M68K_ASR)
				shifted |= value & top;
			else if (name == M68K_ROR)
				shifted |= out << (bits - 1);
			else if (name == M68K_ROXR)
				shifted |= (extend & 1u) << (bits - 1);
		}
		value = shifted;
	}
	return overflow;
}

/* ---- BCD ---- */
static inline uint32_t m68k_abcd(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t low = ((destination & 0xF) + (source & 0xF) + extend) & 0xFF;
	uint32_t first = (destination + source + extend) & 0xFFFF;
	uint32_t result = (first + ((low > 9) ? 6 : 0)) & 0xFFFF;
	if (result > 0x9F)
		result = (result + 0x60) & 0xFFFF;
	return result & 0xFF;
}

static inline uint32_t m68k_abcd_cy(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t low = ((destination & 0xF) + (source & 0xF) + extend) & 0xFF;
	uint32_t first = (destination + source + extend) & 0xFFFF;
	uint32_t result = (first + ((low > 9) ? 6 : 0)) & 0xFFFF;
	if (result > 0x9F)
		result = (result + 0x60) & 0xFFFF;
	return (result & 0x300) ? 1u : 0u;
}

static inline uint32_t m68k_abcd_ov(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t low = ((destination & 0xF) + (source & 0xF) + extend) & 0xFF;
	uint32_t first = (destination + source + extend) & 0xFFFF;
	uint32_t result = (first + ((low > 9) ? 6 : 0)) & 0xFFFF;
	if (result > 0x9F)
		result = (result + 0x60) & 0xFFFF;
	return ((result & 0x80) && !(first & 0x80)) ? 1u : 0u;
}

static inline uint32_t m68k_sbcd(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t low = ((destination & 0xF) - (source & 0xF) - extend) & 0xFF;
	uint32_t first = (destination - source - extend) & 0xFFFF;
	uint32_t result = (first - ((low & 0x10) ? 6 : 0)) & 0xFFFF;
	if (first & 0x100)
		result = (result - 0x60) & 0xFFFF;
	return result & 0xFF;
}

static inline uint32_t m68k_sbcd_cy(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t low = ((destination & 0xF) - (source & 0xF) - extend) & 0xFF;
	uint32_t first = (destination - source - extend) & 0xFFFF;
	uint32_t result = (first - ((low & 0x10) ? 6 : 0)) & 0xFFFF;
	if (first & 0x100)
		result = (result - 0x60) & 0xFFFF;
	return (result & 0x300) ? 1u : 0u;
}

static inline uint32_t m68k_sbcd_ov(uint32_t destination, uint32_t source, uint32_t extend)
{
	uint32_t low = ((destination & 0xF) - (source & 0xF) - extend) & 0xFF;
	uint32_t first = (destination - source - extend) & 0xFFFF;
	uint32_t result = (first - ((low & 0x10) ? 6 : 0)) & 0xFFFF;
	if (first & 0x100)
		result = (result - 0x60) & 0xFFFF;
	return (!(result & 0x80) && (first & 0x80)) ? 1u : 0u;
}

/* ---- division ---- */
static inline uint32_t m68k_divu_result(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return dividend;
	uint32_t quotient = dividend / divisor;
	uint32_t remainder = dividend % divisor;
	return (quotient > 0xFFFF) ? dividend : ((remainder << 16) | quotient);
}

static inline uint32_t m68k_divu(uint32_t dividend, uint32_t divisor) { return m68k_divu_result(dividend, divisor); }

static inline uint32_t m68k_divu_s(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return (dividend >> 31) & 1u;
	uint32_t quotient = dividend / divisor;
	return (quotient > 0xFFFF) ? 1u : ((quotient >> 15) & 1u);
}

static inline uint32_t m68k_divu_z(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return (dividend >> 16) == 0;
	uint32_t quotient = dividend / divisor;
	return (quotient > 0xFFFF) ? 0u : ((quotient & 0xFFFF) == 0);
}

static inline uint32_t m68k_divu_ov(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return 0;
	return (dividend / divisor) > 0xFFFF;
}

static inline int32_t m68k_sext32(uint32_t value, int bits)
{
	uint32_t mask = m68k_mask_bits(bits);
	value &= mask;
	return (value & (1u << (bits - 1))) ? (int32_t)(value - (1u << bits)) : (int32_t)value;
}

static inline uint32_t m68k_divs_result(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return dividend;
	int32_t a = (int32_t)dividend;
	int32_t b = m68k_sext32(divisor, 16);
	if (b == 0)
		return dividend;
	int32_t quotient = a / b;
	int32_t remainder = a % b;
	if (quotient < -0x8000 || quotient > 0x7FFF)
		return dividend;
	return ((uint32_t)(remainder & 0xFFFF) << 16) | (uint32_t)(quotient & 0xFFFF);
}

static inline uint32_t m68k_divs(uint32_t dividend, uint32_t divisor) { return m68k_divs_result(dividend, divisor); }

static inline uint32_t m68k_divs_s(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return 0;
	int32_t a = (int32_t)dividend;
	int32_t b = m68k_sext32(divisor, 16);
	if (b == 0)
		return 0;
	int32_t quotient = a / b;
	if (quotient < -0x8000 || quotient > 0x7FFF)
		return 1;
	return ((uint32_t)quotient >> 15) & 1u;
}

static inline uint32_t m68k_divs_z(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return 1;
	int32_t a = (int32_t)dividend;
	int32_t b = m68k_sext32(divisor, 16);
	if (b == 0)
		return 1;
	int32_t quotient = a / b;
	if (quotient < -0x8000 || quotient > 0x7FFF)
		return 0;
	return quotient == 0;
}

static inline uint32_t m68k_divs_ov(uint32_t dividend, uint32_t divisor)
{
	if (divisor == 0)
		return 0;
	int32_t a = (int32_t)dividend;
	int32_t b = m68k_sext32(divisor, 16);
	if (b == 0)
		return 0;
	int32_t quotient = a / b;
	return quotient < -0x8000 || quotient > 0x7FFF;
}

/* ---- carry and overflow ---- */
#define M68K_CARRY_ADD(name, type) \
	static inline uint32_t m68k_##name(type a, type b, type carry) \
	{ \
		uint64_t total = (uint64_t)a + (uint64_t)b + (uint64_t)carry; \
		return total > (uint64_t)((type)~(type)0); \
	}
#define M68K_OVERFLOW_ADD(name, type) \
	static inline uint32_t m68k_##name(type a, type b, type carry) \
	{ \
		type result = (type)(a + b + carry); \
		type top = (type)((type)1 << (sizeof(type) * 8 - 1)); \
		return (uint32_t)((((a ^ result) & (b ^ result) & top) != 0)); \
	}
#define M68K_CARRY_SUB(name, type) \
	static inline uint32_t m68k_##name(type a, type b, type carry) \
	{ \
		return (uint64_t)a < ((uint64_t)b + (uint64_t)carry); \
	}
#define M68K_OVERFLOW_SUB(name, type) \
	static inline uint32_t m68k_##name(type a, type b, type carry) \
	{ \
		type result = (type)(a - b - carry); \
		type top = (type)((type)1 << (sizeof(type) * 8 - 1)); \
		return (uint32_t)((((a ^ b) & (a ^ result) & top) != 0)); \
	}

M68K_CARRY_ADD(carry_add8, uint8_t)
M68K_CARRY_ADD(carry_add16, uint16_t)
M68K_CARRY_ADD(carry_add32, uint32_t)
M68K_OVERFLOW_ADD(overflow_add8, uint8_t)
M68K_OVERFLOW_ADD(overflow_add16, uint16_t)
M68K_OVERFLOW_ADD(overflow_add32, uint32_t)
M68K_CARRY_SUB(carry_sub8, uint8_t)
M68K_CARRY_SUB(carry_sub16, uint16_t)
M68K_CARRY_SUB(carry_sub32, uint32_t)
M68K_OVERFLOW_SUB(overflow_sub8, uint8_t)
M68K_OVERFLOW_SUB(overflow_sub16, uint16_t)
M68K_OVERFLOW_SUB(overflow_sub32, uint32_t)

/* ---- scalars over the IR ---- */
static inline uint32_t shl32(uint32_t value, int count) { return value << (count & 31); }
static inline uint32_t shr32(uint32_t value, int count) { return value >> (count & 31); }
static inline uint32_t sar32(uint32_t value, int count) { return (uint32_t)((int32_t)value >> (count & 31)); }

#ifdef __cplusplus
}
#endif

#endif
