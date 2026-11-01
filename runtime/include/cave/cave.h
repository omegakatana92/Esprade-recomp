/**
 *    Name:        cave.h
 *    Description: the Cave 68000 board as ESP Ra.De. wires it: memory map, video devices, inputs,
 *                 EEPROM and YMZ280B
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    Reference: MAME src/mame/atlus/cave.cpp (esprade_map, esprade machine config) and
 *    src/devices/video/{tmap038,sprite013}.cpp, all BSD-3-Clause.  No MAME code is linked into this
 *    runtime; the behaviour is reimplemented here.
 */
#ifndef CAVE_H
#define CAVE_H

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DDP_PROGRAM_SIZE  0x100000u
#define DDP_WORK_RAM_SIZE 0x10000u
#define DDP_SPRITE_SIZE   0x10000u
#define DDP_LAYER_SIZE    0x8000u
#define DDP_LAYER_COUNT   3
#define DDP_PALETTE_COUNT 0x8000u
#define DDP_VIDEO_REGS    0x40u
#define DDP_EEPROM_SIZE   0x80u

typedef struct cave_machine cave_machine;

typedef void (*cave_frame_hook)(cave_machine *machine, void *user);

struct cave_machine
{
	uint8_t *program;                 /* 1 MiB, the player's ROM */
	uint8_t work_ram[DDP_WORK_RAM_SIZE];
	uint8_t sprite_ram[DDP_SPRITE_SIZE];
	uint8_t layer_ram[DDP_LAYER_COUNT][DDP_LAYER_SIZE];
	uint16_t palette[DDP_PALETTE_COUNT];
	uint16_t video_regs[DDP_VIDEO_REGS];
	uint16_t layer_regs[DDP_LAYER_COUNT][3];

	uint8_t *sprites;                 /* 16 MiB, decoded to 8bpp by video.c */
	size_t sprites_size;
	uint8_t *layer_gfx[DDP_LAYER_COUNT];
	size_t layer_gfx_size[DDP_LAYER_COUNT];
	uint8_t *sprite_pixels;
	uint8_t *layer_pixels[DDP_LAYER_COUNT];
	uint8_t *layer_pen[DDP_LAYER_COUNT];
	uint8_t *layer_color[DDP_LAYER_COUNT];
	uint8_t *layer_cat[DDP_LAYER_COUNT];
	int video_ready;
	int layer_dirty[DDP_LAYER_COUNT];
	int layer_tiledim[DDP_LAYER_COUNT];
	double video_ms;
	double video_ms_max;
	uint8_t *ymz_rom;
	size_t ymz_size;
	uint8_t eeprom[DDP_EEPROM_SIZE];
	int ee_cs, ee_clk, ee_di;
	int ee_state, ee_bits, ee_locked, ee_do;
	uint32_t ee_accum;
	uint16_t ee_shift;

	int vblank_pending;
	int sound_pending;
	int irq_level;
	uint64_t frame;

	uint16_t input0;                  /* active low, as the ports are */
	uint16_t input1;
	int coin[2];                      /* frames left of the coin impulse */

	cave_frame_hook frame_hook;
	void *frame_user;
	uint64_t frame_limit;
	jmp_buf *escape;
	int escape_code;

	/* address diagnostics */
	uint64_t bad_reads;
	uint64_t bad_writes;
	uint32_t last_bad_address;
};

/* the machine the translated program's bus helpers use */
extern cave_machine *cave_current;

/* inputs: bit set = pressed (the ports are active low) */
void cave_set_inputs(cave_machine *machine, uint16_t player1, uint16_t player2);
void cave_insert_coin(cave_machine *machine, int player);
int cave_interrupt_level(void);
void cave_interrupt_acknowledge(int level);

/* lifecycle */
cave_machine *cave_machine_create(void);
void cave_machine_destroy(cave_machine *machine);
int cave_machine_load_rom(cave_machine *machine, const char *path, char *message, size_t message_size);
int cave_machine_load_folder(cave_machine *machine, const char *path, char *message, size_t message_size);

/* the bus (runtime/src/machine.c) */
uint16_t cave_read16(uint32_t address);
void cave_write16(uint32_t address, uint16_t value, uint16_t mem_mask);
uint32_t cave_read32(uint32_t address);
void cave_write32(uint32_t address, uint32_t value, uint16_t mem_mask);

/* the frame the translated program waits for (runtime/src/program.c) */
void program_frame_wait(void);

/* video (runtime/src/video.c) */
void cave_video_refresh(cave_machine *machine);
const uint32_t *cave_video_frame(const cave_machine *machine);
void cave_video_set_rotation(int quarter_turns);   /* 0, 1, 2, 3: the cabinet is 270 degrees */

/* audio (runtime/src/ymz280b.c) */
void ymz280b_reset(cave_machine *machine);
void ymz280b_write(cave_machine *machine, uint8_t offset, uint8_t value);
uint8_t ymz280b_read(cave_machine *machine, uint8_t offset);
void ymz280b_render(cave_machine *machine, int16_t *samples, int frames);
void ymz280b_set_irq(cave_machine *machine);

/* EEPROM (runtime/src/eeprom.c) */
void eeprom_reset(cave_machine *machine);
void eeprom_write(cave_machine *machine, uint8_t value);
int eeprom_do_read(cave_machine *machine);
void eeprom_save(const cave_machine *machine, const char *path);
void eeprom_load(cave_machine *machine, const char *path);

/* diagnostics */
void cave_machine_report(const cave_machine *machine);

#ifdef __cplusplus
}
#endif

#endif
