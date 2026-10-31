/**
 *    Name:        sha1.h
 *    Description: SHA-1 for the runtime's ROM verification (public-domain implementation style)
 */
#ifndef ESPRADE_SHA1_H
#define ESPRADE_SHA1_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint32_t state[5];
	uint64_t bits;
	uint8_t buffer[64];
	size_t used;
} sha1_context;

void sha1_init(sha1_context *context);
void sha1_update(sha1_context *context, const void *data, size_t size);
void sha1_final(sha1_context *context, uint8_t digest[20]);
void sha1_hex(const void *data, size_t size, char out[41]);

#ifdef __cplusplus
}
#endif

#endif
