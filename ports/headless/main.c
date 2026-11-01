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
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
static uint64_t coin2_frame = UINT64_MAX;
static uint64_t start2_frame = UINT64_MAX;
static FILE *wav_file;
static uint32_t wav_data_bytes;
static double timing_last;
static double timing_max;
static double timing_cpu_last;
static double timing_cpu_max;
static uint64_t timing_overruns;

static double now_ms(void)
{
#ifdef _WIN32
	static LARGE_INTEGER frequency;
	LARGE_INTEGER counter;
	if (!frequency.QuadPart)
		QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&counter);
	return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
	struct timespec value;
	timespec_get(&value, TIME_UTC);
	return (double)value.tv_sec * 1000.0 + (double)value.tv_nsec / 1000000.0;
#endif
}

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
	double timing_now = now_ms();
	double cpu_now = (double)clock() * 1000.0 / CLOCKS_PER_SEC;
	if (timing_cpu_last > 0)
	{
		double cpu_delta = cpu_now - timing_cpu_last;
		if (cpu_delta > timing_cpu_max) timing_cpu_max = cpu_delta;
	}
	timing_cpu_last = cpu_now;
	if (timing_last > 0)
	{
		double delta = timing_now - timing_last;
		if (delta > timing_max)
			timing_max = delta;
		if (delta > 17.4)
			timing_overruns++;
	}
	timing_last = timing_now;
	uint16_t pressed = 0;
	uint16_t pressed2 = 0;
	if (machine->frame >= coin_frame && machine->frame < coin_frame + 6)
		pressed |= 0x0100;                 /* coin 1, active low on the port */
	if (machine->frame >= start_frame && machine->frame < start_frame + 6)
		pressed |= 0x0080;                 /* start 1 */
	if (machine->frame >= coin2_frame && machine->frame < coin2_frame + 6)
		pressed2 |= 0x0100;                /* coin 2 */
	if (machine->frame >= start2_frame && machine->frame < start2_frame + 6)
		pressed2 |= 0x0080;                /* start 2 */
	cave_set_inputs(machine, pressed, pressed2);
	if (wav_file)
	{
		static int16_t audio[2048];
		static double audio_accum;
		audio_accum += 44100.0 * 271.5 / 15625.0;
		int frames = (int)audio_accum;
		audio_accum -= frames;
		if (frames > 1024)
			frames = 1024;
		cave_audio_render(machine, audio, frames);
		fwrite(audio, sizeof(int16_t) * 2, (size_t)frames, wav_file);
		wav_data_bytes += (uint32_t)frames * 4;
	}
	uint64_t video = fnv1a(cave_video_frame(machine), 320 * 240 * 4, 0);
	uint64_t ram = fnv1a(machine->work_ram, sizeof(machine->work_ram), 0);
	uint64_t palette = fnv1a(machine->palette, sizeof(machine->palette), 0);
	uint64_t combined = fnv1a(&video, sizeof(video), ram ^ palette);
	if (hash_file)
		fprintf(hash_file, "%llu %016llx %016llx %016llx %016llx\n",
			(unsigned long long)machine->frame, (unsigned long long)video,
			(unsigned long long)ram, (unsigned long long)palette, (unsigned long long)combined);
	hashes = combined;
	if (verbose && (machine->frame % 10 == 0 || machine->frame == 1))
		fprintf(stderr, "frame %llu video=%016llx ram=%016llx max=%.2fms over=%llu\n",
			(unsigned long long)machine->frame, (unsigned long long)video, (unsigned long long)ram,
			timing_max, (unsigned long long)timing_overruns);
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
	const char *wav_path = NULL;
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
		else if (strcmp(argv[index], "--coin2") == 0 && index + 1 < argc)
			coin2_frame = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--start2") == 0 && index + 1 < argc)
			start2_frame = strtoull(argv[++index], NULL, 10);
		else if (strcmp(argv[index], "--wav") == 0 && index + 1 < argc)
			wav_path = argv[++index];
		else if (strcmp(argv[index], "--verbose") == 0)
			verbose = 1;
		else if (strcmp(argv[index], "--help") == 0 || strcmp(argv[index], "-h") == 0)
		{
			printf("usage: esprade_headless --rom rom\\esprade.zip [--frames N] [--hashes file]\n"
				"       [--eeprom file] [--png file frame] [--coin frame] [--start frame]\n"
				"       [--coin2 frame] [--start2 frame] [--wav file]\n");
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
	if (wav_path)
	{
		wav_file = fopen(wav_path, "wb");
		if (!wav_file)
		{
			fprintf(stderr, "esprade_headless: cannot write %s\n", wav_path);
			cave_machine_destroy(machine);
			return 4;
		}
		uint8_t zero[44] = {0};
		fwrite(zero, 1, sizeof(zero), wav_file);
	}

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

	fprintf(stderr, "frames=%llu hashes=%016llx bad=%llu/%llu pc=%06X frame_max=%.2fms cpu_max=%.2fms video_max=%.2fms over17.4=%llu\n",
		(unsigned long long)machine->frame, (unsigned long long)hashes,
		(unsigned long long)machine->bad_reads, (unsigned long long)machine->bad_writes,
		cpu.pc & 0xFFFFFFu, timing_max, timing_cpu_max, machine->video_ms_max,
		(unsigned long long)timing_overruns);
	if (hash_file)
		fclose(hash_file);
	if (wav_file)
	{
		uint8_t header[44];
		uint32_t riff = 36 + wav_data_bytes;
		memcpy(header + 0, "RIFF", 4);
		header[4] = (uint8_t)(riff); header[5] = (uint8_t)(riff >> 8);
		header[6] = (uint8_t)(riff >> 16); header[7] = (uint8_t)(riff >> 24);
		memcpy(header + 8, "WAVEfmt ", 8);
		header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
		header[20] = 1; header[21] = 0;             /* PCM */
		header[22] = 2; header[23] = 0;             /* stereo */
		uint32_t rate = 44100;
		header[24] = (uint8_t)(rate); header[25] = (uint8_t)(rate >> 8);
		header[26] = (uint8_t)(rate >> 16); header[27] = (uint8_t)(rate >> 24);
		uint32_t byte_rate = rate * 4;
		header[28] = (uint8_t)(byte_rate); header[29] = (uint8_t)(byte_rate >> 8);
		header[30] = (uint8_t)(byte_rate >> 16); header[31] = (uint8_t)(byte_rate >> 24);
		header[32] = 4; header[33] = 0;
		header[34] = 16; header[35] = 0;
		memcpy(header + 36, "data", 4);
		header[40] = (uint8_t)(wav_data_bytes); header[41] = (uint8_t)(wav_data_bytes >> 8);
		header[42] = (uint8_t)(wav_data_bytes >> 16); header[43] = (uint8_t)(wav_data_bytes >> 24);
		fseek(wav_file, 0, SEEK_SET);
		fwrite(header, 1, sizeof(header), wav_file);
		fclose(wav_file);
		fprintf(stderr, "audio: %s (%u bytes, 44100 Hz stereo)\n", wav_path, wav_data_bytes);
	}
	cave_machine_destroy(machine);
	return result;
}
