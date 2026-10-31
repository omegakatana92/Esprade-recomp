/**
 *    Name:        sha1.c
 *    Description: SHA-1 (FIPS 180-1) for the runtime's ROM verification
 */
#include "cave/sha1.h"

#include <string.h>

#define ROL(value, count) (((value) << (count)) | ((value) >> (32 - (count))))

static void transform(sha1_context *context, const uint8_t block[64])
{
	uint32_t w[80];
	for (int i = 0; i < 16; i++)
		w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16)
			| ((uint32_t)block[i * 4 + 2] << 8) | block[i * 4 + 3];
	for (int i = 16; i < 80; i++)
		w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	uint32_t a = context->state[0], b = context->state[1], c = context->state[2];
	uint32_t d = context->state[3], e = context->state[4];
	for (int i = 0; i < 80; i++)
	{
		uint32_t f, k;
		if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
		else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
		else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
		else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
		uint32_t temp = ROL(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = ROL(b, 30);
		b = a;
		a = temp;
	}
	context->state[0] += a;
	context->state[1] += b;
	context->state[2] += c;
	context->state[3] += d;
	context->state[4] += e;
}

void sha1_init(sha1_context *context)
{
	context->state[0] = 0x67452301u;
	context->state[1] = 0xEFCDAB89u;
	context->state[2] = 0x98BADCFEu;
	context->state[3] = 0x10325476u;
	context->state[4] = 0xC3D2E1F0u;
	context->bits = 0;
	context->used = 0;
}

void sha1_update(sha1_context *context, const void *data, size_t size)
{
	const uint8_t *bytes = (const uint8_t *)data;
	context->bits += (uint64_t)size * 8;
	while (size > 0)
	{
		size_t take = 64 - context->used;
		if (take > size)
			take = size;
		memcpy(context->buffer + context->used, bytes, take);
		context->used += take;
		bytes += take;
		size -= take;
		if (context->used == 64)
		{
			transform(context, context->buffer);
			context->used = 0;
		}
	}
}

void sha1_final(sha1_context *context, uint8_t digest[20])
{
	uint64_t bits = context->bits;
	uint8_t pad = 0x80;
	sha1_update(context, &pad, 1);
	uint8_t zero = 0;
	while (context->used != 56)
		sha1_update(context, &zero, 1);
	uint8_t length[8];
	for (int i = 0; i < 8; i++)
		length[i] = (uint8_t)(bits >> (56 - i * 8));
	sha1_update(context, length, 8);
	for (int i = 0; i < 5; i++)
	{
		digest[i * 4 + 0] = (uint8_t)(context->state[i] >> 24);
		digest[i * 4 + 1] = (uint8_t)(context->state[i] >> 16);
		digest[i * 4 + 2] = (uint8_t)(context->state[i] >> 8);
		digest[i * 4 + 3] = (uint8_t)(context->state[i]);
	}
}

void sha1_hex(const void *data, size_t size, char out[41])
{
	static const char digits[] = "0123456789abcdef";
	sha1_context context;
	uint8_t digest[20];
	sha1_init(&context);
	sha1_update(&context, data, size);
	sha1_final(&context, digest);
	for (int i = 0; i < 20; i++)
	{
		out[i * 2] = digits[digest[i] >> 4];
		out[i * 2 + 1] = digits[digest[i] & 15];
	}
	out[40] = 0;
}
