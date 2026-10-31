/**
 *    Name:        headless.c
 *    Description: deterministic frame runner for the native ESP Ra.De. program: no window, no
 *                 audio device, frame hashes for the CI gate
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cave/cave.h"
#include "cave/png.h"
#include "m68000/cpu.h"
#include "m68000/runtime.h"

static jmp_buf escape;
static FILE *hash_file;
static uint64_t hashes;
static int verbose;
static const char *png_path;
static uint64_t png_frame;
static uint64_t coin_frame = UINT64_MAX;
static uint64_t start_frame = UINT64_MAX;

static uint64_t fnv1a(const void *data, size_t size, uint64_t seed)
{
	const uint8_t *bytes = (const uint8_t *)data;
	uint64_t value = seed ? seed : 1469598103934665603ull;
	for (size_t index = 0; index < size; index++)
	{
		value ^= bytes[index];
		value *= 1099511628211ull;
	}
	return value;
}

static void frame_ready(cave_machine *machine, void *user)
{
	(void)user;
	uint16_t pressed = 0;
	if (machine->frame >= coin_frame && machine->frame < coin_frame + 6)
		pressed |= 0x0100;                 /* coin 1, active low on the port */
	if (machine->frame >= start_frame && machine->frame < start_frame + 6)
		pressed |= 0x0080;                 /* start 1 */
	cave_set_inputs(machine, pressed, 0);
	uint64_t video = fnv1a(cave_video_frame(machine), 320 * 240 * 4, 0);
	uint64_t ram = fnv1a(machine->work_ram, sizeof(machine->work_ram), 0);
	uint64_t palette = fnv1a(machine->palette, sizeof(machine->palette), 0);
	uint64_t combined = fnv1a(&video, sizeof(video), ram ^ palette);
	if (hash_file)
		fprintf(hash_file, "%llu %016llx %016llx %016llx %016llx\n",
			(unsigned long long)machine->frame, (unsigned long long)video,
			(unsigned long long)ram, (unsigned long long)palette, (unsigned long long)combined);
	hashes = combined;
	if (verbose && (machine->frame % 60 == 0 || machine->frame == 1))
		fprintf(stderr, "frame %llu video=%016llx ram=%016llx\n",
			(unsigned long long)machine->frame, (unsigned long long)video, (unsigned long long)ram);
	if (png_path && machine->frame == png_frame)
	{
		if (cave_png_write(png_path, cave_video_frame(machine), 320, 240))
			fprintf(stderr, "snapshot frame %llu -> %s\n",
				(unsigned long long)machine->frame, png_path);
		else
			fprintf(stderr, "snapshot: cannot write %s\n", png_path);
	}
}

int main(int argc, char **argv)
{
	const char *rom = NULL;
	const char *hash_path = NULL;
	const char *eeprom_path = NULL;
	uint64_t frames = 600;

	for (int index = 1; index < argc; index++)
	{
		if (strcmp(argv[index], "--rom") == 0 && index + 1 < argc)
			rom = argv[++index];
		else if (strcmp(argv[index], "--frames") == 0 && index + 1 < argc)
			frames = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--hashes") == 0 && index + 1 < argc)
			hash_path = argv[++index];
		else if (strcmp(argv[index], "--eeprom") == 0 && index + 1 < argc)
			eeprom_path = argv[++index];
		else if (strcmp(argv[index], "--png") == 0 && index + 2 < argc)
		{
			png_path = argv[++index];
			png_frame = strtoull(argv[++index], NULL, 10);
		}
		else if (strcmp(argv[index], "--coin") == 0 && index + 1 < argc)
			coin_frame = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--start") == 0 && index + 1 < argc)
			start_frame = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--verbose") == 0)
			verbose = 1;
		else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0)
		{
			printf("usage: esprade_headless --rom rom\\esprade.zip [--frames N] [--hashes file] [--eeprom file]\n");
			return 0;
		}
		else
		{
			fprintf(stderr, "unknown argument: %s\n", argv[index]);
			return 2;
		}
	}
	if (!rom)
	{
		fprintf(stderr, "esprade_headless: --rom <esprade.zip or folder> is required\n");
		return 2;
	}

	cave_machine *machine = cave_machine_create();
	if (!machine)
	{
		fprintf(stderr, "esprade_headless: out of memory\n");
		return 2;
	}
	char message[1024] = {0};
	if (!cave_machine_load_rom(machine, rom, message, sizeof(message)))
	{
		fprintf(stderr, "esprade_headless: %s\n", message);
		cave_machine_destroy(machine);
		return 3;
	}
	if (eeprom_path)
		eeprom_load(machine, eeprom_path);

	if (hash_path)
	{
		hash_file = fopen(hash_path, "w");
		if (!hash_file)
		{
			fprintf(stderr, "esprade_headless: cannot write %s\n", hash_path);
			cave_machine_destroy(machine);
			return 4;
		}
	}

	machine->frame_hook = frame_ready;
	machine->frame_limit = frames;
	machine->escape = &escape;

	printf("ROM: %s\n", rom);
	m68k_reset();
	fprintf(stderr, "reset: SSP=%08X PC=%08X frames=%llu\n", cpu.r[15], cpu.pc,
		(unsigned long long)frames);

	int result = 0;
	if (setjmp(escape) == 0)
	{
		uint32_t pc = cpu.pc & 0xFFFFFFu;
		for (int round = 0; round < 100000 && machine->frame < frames; round++)
		{
			if (!m68k_run_function(pc))
			{
				result = 5;
				break;
			}
			pc = cpu.pc & 0xFFFFFFu;
			if (pc == 0 || pc == 0xFFFFFFu)
				break;
		}
	}
	if (result == 0 && machine->frame < frames)
	{
		fprintf(stderr, "esprade_headless: program returned after %llu frames\n",
			(unsigned long long)machine->frame);
		result = 6;
	}
	if (machine->bad_reads || machine->bad_writes)
	{
		fprintf(stderr, "esprade_headless: %llu bad reads, %llu bad writes (last %06X)\n",
			(unsigned long long)machine->bad_reads, (unsigned long long)machine->bad_writes,
			machine->last_bad_address);
		result = 7;
	}
	if (m68k_missing_targets() && !getenv("ESPRADE_TRACE_MISSING"))
	{
		fprintf(stderr, "esprade_headless: %u computed targets had no translated code (last %06X)\n",
			m68k_missing_targets(), m68k_last_missing_target());
		result = 8;
	}
	if (eeprom_path)
		eeprom_save(machine, eeprom_path);
	if (getenv("ESPRADE_DUMP_VIDEO"))
	{
		for (int layer = 0; layer < 3; layer++)
			fprintf(stderr, "layer%d regs %04X %04X %04X\n", layer,
				machine->layer_regs[layer][0], machine->layer_regs[layer][1],
				machine->layer_regs[layer][2]);
		fprintf(stderr, "video regs %04X %04X %04X %04X %04X %04X\n",
			machine->video_regs[0], machine->video_regs[1], machine->video_regs[2],
			machine->video_regs[3], machine->video_regs[4], machine->video_regs[5]);
	}

	fprintf(stderr, "frames=%llu hashes=%016llx bad=%llu/%llu pc=%06X\n",
		(unsigned long long)machine->frame, (unsigned long long)hashes,
		(unsigned long long)machine->bad_reads, (unsigned long long)machine->bad_writes,
		cpu.pc & 0xFFFFFFu);
	if (hash_file)
		fclose(hash_file);
	cave_machine_destroy(machine);
	return result;
}
