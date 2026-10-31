/**
 *    Name:        png.c
 *    Description: minimal PNG writer for frame snapshots, deflate from miniz
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 */
#include "cave/png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "miniz.h"

static void put32(uint8_t *data, uint32_t value)
{
	data[0] = (uint8_t)(value >> 24);
	data[1] = (uint8_t)(value >> 16);
	data[2] = (uint8_t)(value >> 8);
	data[3] = (uint8_t)value;
}

static void chunk(FILE *file, const char *type, const uint8_t *data, uint32_t size)
{
	uint8_t header[8];
	put32(header, size);
	memcpy(header + 4, type, 4);
	fwrite(header, 1, 8, file);
	if (size)
		fwrite(data, 1, size, file);
	uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, (const uint8_t *)type, 4);
	if (size)
		crc = (uint32_t)mz_crc32(crc, data, size);
	uint8_t trailer[4];
	put32(trailer, crc);
	fwrite(trailer, 1, 4, file);
}

int cave_png_write(const char *path, const uint32_t *pixels, int width, int height)
{
	size_t row = (size_t)width * 3 + 1;
	size_t raw_size = row * (size_t)height;
	uint8_t *raw = (uint8_t *)malloc(raw_size);
	if (!raw)
		return 0;
	for (int y = 0; y < height; y++)
	{
		uint8_t *line = raw + (size_t)y * row;
		line[0] = 0;
		for (int x = 0; x < width; x++)
		{
			uint32_t value = pixels[(size_t)y * width + x];
			line[1 + x * 3 + 0] = (uint8_t)(value >> 16);
			line[1 + x * 3 + 1] = (uint8_t)(value >> 8);
			line[1 + x * 3 + 2] = (uint8_t)value;
		}
	}
	mz_ulong compressed_size = mz_compressBound((mz_ulong)raw_size);
	uint8_t *compressed = (uint8_t *)malloc(compressed_size);
	if (!compressed)
	{
		free(raw);
		return 0;
	}
	if (mz_compress(compressed, &compressed_size, raw, (mz_ulong)raw_size) != MZ_OK)
	{
		free(compressed);
		free(raw);
		return 0;
	}
	FILE *file = fopen(path, "wb");
	if (!file)
	{
		free(compressed);
		free(raw);
		return 0;
	}
	static const uint8_t signature[8] = { 137, 'P', 'N', 'G', 13, 10, 26, 10 };
	fwrite(signature, 1, 8, file);
	uint8_t ihdr[13];
	put32(ihdr, (uint32_t)width);
	put32(ihdr + 4, (uint32_t)height);
	ihdr[8] = 8;      /* bit depth */
	ihdr[9] = 2;      /* truecolour */
	ihdr[10] = 0;
	ihdr[11] = 0;
	ihdr[12] = 0;
	chunk(file, "IHDR", ihdr, 13);
	chunk(file, "IDAT", compressed, (uint32_t)compressed_size);
	chunk(file, "IEND", NULL, 0);
	fclose(file);
	free(compressed);
	free(raw);
	return 1;
}
