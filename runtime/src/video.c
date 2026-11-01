/**
 *    Name:        video.c
 *    Description: the Cave 038 tile layers, 013 sprite generator and xGRB_555 palette,
 *                 rendered to a 320x240 RGB frame
 *    Version:     0.2.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    Reference: MAME src/mame/atlus/cave_v.cpp, sprite013.cpp and
 *    src/devices/video/tmap038.cpp (BSD-3-Clause, Luca Elia, Paul Priest, David Haywood).
 *    The pixel formats and priority rules are reimplemented here; no MAME code is linked.
 */
#include "cave/cave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SCREEN_W 320
#define SCREEN_H 240
#define TILEMAP_W 512
#define TILEMAP_H 512

static uint32_t *framebuffer;
static uint8_t *priority_map;
static uint16_t *sprite_bitmap;
static int rotation = 3;
static int video_mask = -1;

static int layers_enabled(void)
{
	if (video_mask < 0)
	{
		const char *value = getenv("ESPRADE_VIDEO_MASK");
		video_mask = value ? (int)strtol(value, NULL, 0) : 0xF;
	}
	return video_mask;
}

static const int layer_plane[8] = { 8, 9, 10, 11, 0, 1, 2, 3 };
static const int layer_x[8] = { 0, 4, 16, 20, 32, 36, 48, 52 };
static const int layer_y[8] = { 0, 64, 128, 192, 256, 320, 384, 448 };

static const int sprite_plane[8] = { 0, 1, 2, 3, 16, 17, 18, 19 };
static const int sprite_x[16] =
{
	12, 8, 4, 0, 44, 40, 36, 32, 76, 72, 68, 64, 108, 104, 100, 96
};

static uint16_t get16(const uint8_t *data, size_t offset)
{
	return (uint16_t)(((uint16_t)data[offset] << 8) | data[offset + 1]);
}

static uint32_t palette_color(uint16_t value)
{
	uint32_t green = (value >> 10) & 0x1F;
	uint32_t red = (value >> 5) & 0x1F;
	uint32_t blue = value & 0x1F;
	/* expand 5 bits to 8 with the usual replication */
	red = (red << 3) | (red >> 2);
	green = (green << 3) | (green >> 2);
	blue = (blue << 3) | (blue >> 2);
	return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

static int bit_at(const uint8_t *data, int bit)
{
	return (data[bit >> 3] >> (7 - (bit & 7))) & 1;
}

/* layout_8x8x8 (MAME cave.cpp): 8x8, 8bpp, 512 bits per character */
static void decode_layer_tiles(const uint8_t *rom, size_t size, uint8_t *pixels)
{
	size_t tiles = size / 64;
	for (size_t tile = 0; tile < tiles; tile++)
	{
		const uint8_t *source = rom + tile * 64;
		uint8_t *destination = pixels + tile * 64;
		for (int y = 0; y < 8; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				int pen = 0;
				for (int plane = 0; plane < 8; plane++)
					pen |= bit_at(source, layer_plane[plane] + layer_x[x] + layer_y[y]) << (7 - plane);
				destination[y * 8 + x] = (uint8_t)pen;
			}
		}
	}
}

/* layout_sprites_8bpp (MAME cave.cpp): 16x1, 8bpp, 128 bits per character */
static void decode_sprite_tiles(const uint8_t *rom, size_t size, uint8_t *pixels)
{
	size_t elements = size / 16;
	for (size_t element = 0; element < elements; element++)
	{
		const uint8_t *source = rom + element * 16;
		uint8_t *destination = pixels + element * 16;
		for (int x = 0; x < 16; x++)
		{
			int pen = 0;
			for (int plane = 0; plane < 8; plane++)
				pen |= bit_at(source, sprite_plane[plane] + sprite_x[x]) << (7 - plane);
			destination[x] = (uint8_t)pen;
		}
	}
}

static void video_decode(cave_machine *machine)
{
	if (machine->video_ready)
		return;
	if (!machine->sprite_pixels)
		machine->sprite_pixels = (uint8_t *)malloc(machine->sprites_size ? machine->sprites_size : 1);
	for (int layer = 0; layer < DDP_LAYER_COUNT; layer++)
	{
		if (!machine->layer_pixels[layer])
			machine->layer_pixels[layer] = (uint8_t *)malloc(machine->layer_gfx_size[layer]);
		if (!machine->layer_pen[layer])
			machine->layer_pen[layer] = (uint8_t *)malloc(TILEMAP_W * TILEMAP_H);
		if (!machine->layer_color[layer])
			machine->layer_color[layer] = (uint8_t *)malloc(TILEMAP_W * TILEMAP_H);
		if (!machine->layer_cat[layer])
			machine->layer_cat[layer] = (uint8_t *)malloc(TILEMAP_W * TILEMAP_H);
	}
	if (!framebuffer)
		framebuffer = (uint32_t *)malloc(SCREEN_W * SCREEN_H * sizeof(uint32_t));
	if (!priority_map)
		priority_map = (uint8_t *)malloc(SCREEN_W * SCREEN_H);
	if (!sprite_bitmap)
		sprite_bitmap = (uint16_t *)malloc(SCREEN_W * SCREEN_H * sizeof(uint16_t));
	if (!machine->sprite_pixels || !framebuffer || !priority_map || !sprite_bitmap)
		return;
	for (int layer = 0; layer < DDP_LAYER_COUNT; layer++)
	{
		if (!machine->layer_pixels[layer] || !machine->layer_pen[layer]
			|| !machine->layer_color[layer] || !machine->layer_cat[layer])
			return;
	}
	decode_sprite_tiles(machine->sprites, machine->sprites_size, machine->sprite_pixels);
	for (int layer = 0; layer < DDP_LAYER_COUNT; layer++)
		decode_layer_tiles(machine->layer_gfx[layer], machine->layer_gfx_size[layer],
			machine->layer_pixels[layer]);
	machine->video_ready = 1;
}

/* Build one layer's 512x512 pen/colour/category map from its tile RAM (MAME tmap038). */
static void build_layer(cave_machine *machine, int layer)
{
	const uint16_t *registers = machine->layer_regs[layer];
	const uint8_t *ram = machine->layer_ram[layer];
	uint8_t *pen_map = machine->layer_pen[layer];
	uint8_t *color_map = machine->layer_color[layer];
	uint8_t *category_map = machine->layer_cat[layer];
	const uint8_t *pixels = machine->layer_pixels[layer];
	size_t tile_count = machine->layer_gfx_size[layer] / 64;
	int tiledim = (registers[1] >> 13) & 1;
	if (!machine->layer_dirty[layer] && machine->layer_tiledim[layer] == tiledim)
		return;
	int nonzero = 0;

	for (int cell = 0; cell < 4096; cell++)
	{
		uint32_t tile;
		uint32_t code;
		if (tiledim)
		{
			int tile_index = (cell % 64) / 2 + ((cell / 64) / 2) * (512 / 16);
			tile = ((uint32_t)get16(ram, tile_index * 4) << 16) | get16(ram, tile_index * 4 + 2);
			code = (tile & 0xFFFFu) * 4;
			code ^= (uint32_t)(cell & 1);
			code ^= (uint32_t)(((cell / 64) & 1) * 2);
		}
		else
		{
			tile = ((uint32_t)get16(ram, 0x4000 + cell * 4) << 16) | get16(ram, 0x4000 + cell * 4 + 2);
			code = tile & 0x3FFFFu;
		}
		uint8_t color = (uint8_t)((tile >> 24) & 0x3F);
		uint8_t category = (uint8_t)((tile >> 30) & 3);
		code &= (uint32_t)(tile_count - 1);

		int cell_x = (cell % 64) * 8;
		int cell_y = (cell / 64) * 8;
		const uint8_t *source = pixels + code * 64;
		for (int y = 0; y < 8; y++)
		{
			int offset = (cell_y + y) * TILEMAP_W + cell_x;
			for (int x = 0; x < 8; x++)
			{
				uint8_t pen = source[y * 8 + x];
				pen_map[offset + x] = pen;
				color_map[offset + x] = color;
				category_map[offset + x] = pen ? category : 0;
				nonzero += pen ? 1 : 0;
			}
		}
	}
	if (getenv("ESPRADE_DEBUG_VIDEO"))
		fprintf(stderr, "layer%d tiledim=%d nonzero=%d first=%08X\n", layer, tiledim, nonzero,
			(unsigned)(((uint32_t)get16(ram, 0) << 16) | get16(ram, 2)));
	machine->layer_dirty[layer] = 0;
	machine->layer_tiledim[layer] = tiledim;
}

static int signed10(int value)
{
	value &= 0x3FF;
	return (value & 0x200) ? value - 0x400 : value;
}

static void put_sprite_pixel(int x, int y, uint8_t pen, uint8_t colpri)
{
	if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H || pen == 0)
		return;
	sprite_bitmap[y * SCREEN_W + x] = (uint16_t)(pen | ((uint16_t)colpri << 8));
}

static void draw_sprite_nozoom(const cave_machine *machine, uint32_t code, uint8_t colpri,
	int x, int y, int flipx, int flipy, int width, int height)
{
	(void)machine;
	const uint8_t *gfx = machine->sprite_pixels;
	for (int sy = 0; sy < height; sy++)
	{
		int drawy = y + sy;
		if (drawy < 0 || drawy >= SCREEN_H)
			continue;
		int source_y = flipy ? height - 1 - sy : sy;
		uint32_t row = code * 256u + (uint32_t)(source_y * width);
		for (int sx = 0; sx < width; sx++)
		{
			int drawx = x + sx;
			if (drawx < 0 || drawx >= SCREEN_W)
				continue;
			int source_x = flipx ? width - 1 - sx : sx;
			uint8_t pen = gfx[(row + (uint32_t)source_x) & 0xFFFFFFu];
			put_sprite_pixel(drawx, drawy, pen, colpri);
		}
	}
}

static void draw_sprite_zoom(const cave_machine *machine, uint32_t code, uint8_t colpri,
	int x, int y, int flipx, int flipy, int width, int height, int zoomx, int zoomy)
{
	const uint8_t *gfx = machine->sprite_pixels;
	for (int sy = 0; sy < height; sy++)
	{
		int drawy = ((y << 8) + sy * zoomy) >> 8;
		if (drawy < 0 || drawy >= SCREEN_H)
			continue;
		int source_y = flipy ? height - 1 - sy : sy;
		uint32_t row = code * 256u + (uint32_t)(source_y * width);
		for (int sx = 0; sx < width; sx++)
		{
			int drawx = ((x << 8) + sx * zoomx) >> 8;
			if (drawx < 0 || drawx >= SCREEN_W)
				continue;
			int source_x = flipx ? width - 1 - sx : sx;
			uint8_t pen = gfx[(row + (uint32_t)source_x) & 0xFFFFFFu];
			put_sprite_pixel(drawx, drawy, pen, colpri);
		}
	}
}

static void build_sprites(cave_machine *machine)
{
	memset(sprite_bitmap, 0, SCREEN_W * SCREEN_H * sizeof(uint16_t));
	const uint16_t *registers = machine->video_regs;
	uint32_t bank = registers[4] & 3;
	const uint8_t *source = machine->sprite_ram + bank * 0x4000;
	const int global_flipx = (registers[0] & 0x8000) != 0;
	const int global_flipy = (registers[1] & 0x8000) != 0;
	const int positional = (registers[5] & 0x3000) != 0;
	const uint32_t max_clock = (SCREEN_W > 360 ? 512u : 448u) * 272u * 2u;
	uint32_t clock = 0;

	for (int index = 0; index < 1024; index++)
	{
		const uint8_t *sprite = source + index * 16;
		int x, y;
		if (!positional)
		{
			x = (get16(sprite, 0) & 0x3FF) << 8;
			y = (get16(sprite, 2) & 0x3FF) << 8;
		}
		else
		{
			x = get16(sprite, 0) << 2;
			y = get16(sprite, 2) << 2;
		}
		uint16_t attr = get16(sprite, 4);
		uint32_t code = get16(sprite, 6) + ((attr & 3u) << 16);
		int zoomx = get16(sprite, 8);
		int zoomy = get16(sprite, 10);
		uint16_t size = get16(sprite, 12);
		int width = ((size >> 8) & 0x1F) * 16;
		int height = (size & 0x1F) * 16;
		clock += 32;
		if (clock > max_clock)
			break;
		if (!width || !height)
			continue;
		int total_width = (width * zoomx) / 0x100;
		int total_height = (height * zoomy) / 0x100;
		clock += (uint32_t)(width * height);
		if (clock > max_clock)
			break;
		int flipx = (attr >> 3) & 1;
		int flipy = (attr >> 2) & 1;
		uint8_t colpri = (uint8_t)((((attr & 0x30u) << 10) | (attr & 0x3F00u)) >> 8);

		if (!positional)
		{
			x >>= 8;
			y >>= 8;
			if (flipx && zoomx != 0x100) x += width - total_width;
			if (flipy && zoomy != 0x100) y += height - total_height;
		}
		else
		{
			if (flipx && zoomx != 0x100) x += (width << 8) - width * zoomx - 0x80;
			if (flipy && zoomy != 0x100) y += (height << 8) - height * zoomy - 0x80;
			x >>= 8;
			y >>= 8;
		}
		x = signed10(x);
		y = signed10(y);
		if (x + total_width <= 0 || x >= SCREEN_W || y + total_height <= 0 || y >= SCREEN_H)
			continue;
		if (global_flipx)
		{
			x = 320 - x - total_width;
			flipx = !flipx;
		}
		if (global_flipy)
		{
			y = 240 - y - total_height;
			flipy = !flipy;
		}
		if (zoomx == 0x100 && zoomy == 0x100)
			draw_sprite_nozoom(machine, code, colpri, x, y, flipx, flipy, width, height);
		else
			draw_sprite_zoom(machine, code, colpri, x, y, flipx, flipy, width, height, zoomx, zoomy);
	}
}

void cave_video_set_rotation(int quarter_turns)
{
	rotation = quarter_turns & 3;
}

const uint32_t *cave_video_frame(const cave_machine *machine)
{
	(void)machine;
	return framebuffer ? framebuffer : (const uint32_t *)0;
}

void cave_video_refresh(cave_machine *machine)
{
	video_decode(machine);
	if (!framebuffer || !priority_map || !sprite_bitmap)
		return;

	clock_t t0 = clock();
	for (int layer = 0; layer < DDP_LAYER_COUNT; layer++)
		build_layer(machine, layer);
	clock_t t1 = clock();
	build_sprites(machine);
	clock_t t2 = clock();
	int mask = layers_enabled();

	uint32_t background = palette_color(0);
	if (machine->palette[0x7F00])
		background = palette_color(machine->palette[0x7F00]);
	memset(framebuffer, 0, SCREEN_W * SCREEN_H * sizeof(uint32_t));
	for (int index = 0; index < SCREEN_W * SCREEN_H; index++)
		framebuffer[index] = background;
	memset(priority_map, 0, SCREEN_W * SCREEN_H);

	for (int y = 0; y < SCREEN_H; y++)
	{
		for (int x = 0; x < SCREEN_W; x++)
		{
			int best_category = -1;
			int best_pri2 = -1;
			int best_layer = -1;
			uint8_t best_pen = 0;
			uint8_t best_color = 0;

			for (int layer = 0; layer < DDP_LAYER_COUNT; layer++)
			{
				if (!(mask & (1 << layer)))
					continue;
				const uint16_t *registers = machine->layer_regs[layer];
				if (registers[2] & 0x10)          /* BIT(~vregs[2], 4): disabled */
					continue;
				int pri2 = registers[2] & 0x0F;
				if (pri2 > 3)
					continue;
				int tiledim = (registers[1] >> 13) & 1;
				int flipx = !(registers[0] & 0x8000);
				int flipy = !(registers[1] & 0x8000);
				int offs_x = 0x13 - (layer + 1 + (tiledim ? 0 : 8));
				int offs_y = -0x12;
				int sx = (registers[0] & 0x1FF) - machine->video_regs[0]
					+ (flipx ? offs_x + 2 : -offs_x);
				int sy = (registers[1] & 0x1FF) - machine->video_regs[1]
					+ (flipy ? offs_y + 2 : -offs_y);
				int rowscroll_en = (registers[0] & 0x4000) != 0;
				int rowselect_en = (registers[1] & 0x4000) != 0;
				int offs_row = flipy ? 1 : -1;
				if (rowscroll_en)
					sx += get16(machine->layer_ram[layer], 0x1000 + (((sy + offs_row + y) & 0x1FF) * 4));
				int xpos = flipx ? (sx - 192) : (-sx);
				int rawx = (x - xpos) & 511;
				int source_x = flipx ? 511 - rawx : rawx;
				int source_y;
				if (rowselect_en)
					source_y = get16(machine->layer_ram[layer],
						0x1000 + (((sy + offs_row + y) & 0x1FF) * 4) + 2) & 511;
				else
				{
					int ypos = flipy ? (sy - 272) : (-sy);
					int rawy = (y - ypos) & 511;
					source_y = flipy ? 511 - rawy : rawy;
				}
				int offset = source_y * TILEMAP_W + source_x;
				uint8_t pen = machine->layer_pen[layer][offset];
				if (!pen)
					continue;
				int category = machine->layer_cat[layer][offset];
				if (category > best_category
					|| (category == best_category && pri2 >= best_pri2))
				{
					best_category = category;
					best_pri2 = pri2;
					best_layer = layer;
					best_pen = pen;
					best_color = machine->layer_color[layer][offset];
				}
			}
			(void)best_layer;
			if (best_category >= 0)
			{
				uint32_t index = 0x4000u + best_pen + (uint32_t)best_color * 256u;
				framebuffer[y * SCREEN_W + x] = palette_color(machine->palette[index & 0x7FFF]);
				priority_map[y * SCREEN_W + x] = (uint8_t)(best_category + 1);
			}
		}
	}

	clock_t t3 = clock();
	/* sprites last, below tiles of their own priority (MAME cave_v colpri_cb) */
	if (!(mask & 8))
		return;
	for (int y = 0; y < SCREEN_H; y++)
		for (int x = 0; x < SCREEN_W; x++)
		{
			uint16_t pixel = sprite_bitmap[y * SCREEN_W + x];
			if (!pixel)
				continue;
			uint8_t pen = (uint8_t)(pixel & 0xFF);
			uint8_t colpri = (uint8_t)(pixel >> 8);
			int priority = (colpri >> 6) & 3;
			int dstpri = priority_map[y * SCREEN_W + x];
			if (dstpri <= priority)
			{
				uint32_t index = pen + (uint32_t)(colpri & 0x3F) * 256u;
				framebuffer[y * SCREEN_W + x] = palette_color(machine->palette[index & 0x7FFF]);
				priority_map[y * SCREEN_W + x] = 0xFF;
			}
		}
	clock_t t4 = clock();
	{
		static int reports;
		double layers = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;
		double sprites = (double)(t2 - t1) * 1000.0 / CLOCKS_PER_SEC;
		double tiles = (double)(t3 - t2) * 1000.0 / CLOCKS_PER_SEC;
		double compose = (double)(t4 - t3) * 1000.0 / CLOCKS_PER_SEC;
		if (getenv("ESPRADE_DEBUG_VIDEO_TIME") && reports < 20
			&& layers + sprites + tiles + compose > 15.0)
		{
			fprintf(stderr, "video frame %llu layers=%.1f sprites=%.1f tiles=%.1f compose=%.1f ms\n",
				(unsigned long long)machine->frame, layers, sprites, tiles, compose);
			reports++;
		}
	}
	(void)rotation;
}
