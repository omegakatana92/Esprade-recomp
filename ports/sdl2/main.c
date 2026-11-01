/**
 *    Name:        main.c (SDL2 front end)
 *    Description: window, keyboard/gamepad input, audio device and frame pacing for the native
 *                 ESP Ra.De. program
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cave/cave.h"
#include "m68000/cpu.h"
#include "m68000/runtime.h"

static jmp_buf escape;
static int running = 1;
static int stats_enabled;
static const double sim_period = 271.5 / 15625.0;          /* 17.376 ms, 57.55 Hz */
static const double audio_per_frame = 44100.0 * 271.5 / 15625.0;

typedef struct
{
	uint32_t *rotated;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	int width, height;
	int fullscreen;
	SDL_GameController *controller;
	SDL_GameController *controller2;
	SDL_AudioDeviceID audio;
	SDL_AudioStream *audio_stream;
	Uint32 audio_bytes_per_second;
} frontend;

static frontend app;
static int coin_pulse[2];

/* scheduler and diagnostics */
static uint64_t perf_frequency;
static uint64_t next_deadline;
static uint64_t missed_deadlines;
static uint64_t stats_window_start;
static uint64_t stats_window_frames;
static double frame_deltas[600];
static int frame_delta_count;
static int frame_delta_position;
static double present_max;
static double present_sum;
static double audio_accumulator;
static uint64_t audio_underruns;
static uint64_t audio_overruns;
static Uint32 queue_min = UINT32_MAX;
static Uint32 queue_max;
static double queue_latency_min = 1e9;
static double queue_latency_max;
static double queue_latency_sum;
static uint64_t queue_latency_count;
static double audio_mix_sum;
static double audio_mix_max;
static uint64_t audio_mix_count;
static double video_sum;
static double video_max;
static double game_sum;
static double game_max;
static uint64_t game_count;
static double sleep_sum;
static uint64_t sleep_count;
static uint64_t presented_frames;
static uint64_t last_frame_end;
static double perf_last;

static void rotate_270(const uint32_t *source, uint32_t *destination)
{
	/* 320x240 source -> 240x320 destination, 270 degrees clockwise */
	for (int y = 0; y < 320; y++)
		for (int x = 0; x < 240; x++)
			destination[y * 240 + x] = source[x * 320 + (319 - y)];
}

static void queue_audio(cave_machine *machine)
{
	static int16_t buffer[4096];
	static Uint8 converted[8192];

	/* Audio follows the simulated frame clock, never wall time. */
	audio_accumulator += audio_per_frame;
	int frames = (int)audio_accumulator;
	audio_accumulator -= frames;
	if (frames < 1) frames = 1;
	if (frames > 2048) frames = 2048;

	uint64_t mix_start = SDL_GetPerformanceCounter();
	cave_audio_render(machine, buffer, frames);
	double mix_ms = (double)(SDL_GetPerformanceCounter() - mix_start) * 1000.0
		/ (double)perf_frequency;
	audio_mix_sum += mix_ms;
	audio_mix_count++;
	if (mix_ms > audio_mix_max) audio_mix_max = mix_ms;

	if (!app.audio)
		return;

	Uint32 queued = SDL_GetQueuedAudioSize(app.audio);
	if (queued == 0 && machine->frame > 180)
		audio_underruns++;
	if (app.audio_bytes_per_second && queued > app.audio_bytes_per_second / 6)
	{
		/* More than ~167 ms queued: drop this frame instead of growing forever. */
		audio_overruns++;
		return;
	}

	if (app.audio_stream)
	{
		SDL_AudioStreamPut(app.audio_stream, buffer, frames * 2 * (int)sizeof(int16_t));
		int available;
		while ((available = SDL_AudioStreamAvailable(app.audio_stream)) > 0)
		{
			int take = available > (int)sizeof(converted) ? (int)sizeof(converted) : available;
			int got = SDL_AudioStreamGet(app.audio_stream, converted, take);
			if (got <= 0)
				break;
			SDL_QueueAudio(app.audio, converted, (Uint32)got);
		}
	}
	else
	{
		SDL_QueueAudio(app.audio, buffer, (Uint32)(frames * 2 * sizeof(int16_t)));
	}

	queued = SDL_GetQueuedAudioSize(app.audio);
	if (queued < queue_min) queue_min = queued;
	if (queued > queue_max) queue_max = queued;
	if (app.audio_bytes_per_second)
	{
		double latency_ms = (double)queued * 1000.0 / (double)app.audio_bytes_per_second;
		if (latency_ms < queue_latency_min) queue_latency_min = latency_ms;
		if (latency_ms > queue_latency_max) queue_latency_max = latency_ms;
		queue_latency_sum += latency_ms;
		queue_latency_count++;
	}
}

static void apply_buttons(cave_machine *machine)
{
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	uint16_t p1 = 0, p2 = 0;
	if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) p1 |= 0x0001;
	if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) p1 |= 0x0002;
	if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) p1 |= 0x0004;
	if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) p1 |= 0x0008;
	if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_J]) p1 |= 0x0010;
	if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_K]) p1 |= 0x0020;
	if (keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_L]) p1 |= 0x0040;
	if (keys[SDL_SCANCODE_1] || keys[SDL_SCANCODE_RETURN]) p1 |= 0x0080;
	if (keys[SDL_SCANCODE_5]) coin_pulse[0] = 1;
	if (keys[SDL_SCANCODE_6]) coin_pulse[1] = 1;

	/* player 2 mirrors the second half of a split keyboard */
	if (keys[SDL_SCANCODE_I]) p2 |= 0x0001;
	if (keys[SDL_SCANCODE_KP_5]) p2 |= 0x0002;
	if (keys[SDL_SCANCODE_J]) p2 |= 0x0004;
	if (keys[SDL_SCANCODE_L]) p2 |= 0x0008;
	if (keys[SDL_SCANCODE_KP_1]) p2 |= 0x0010;
	if (keys[SDL_SCANCODE_KP_2]) p2 |= 0x0020;
	if (keys[SDL_SCANCODE_KP_3]) p2 |= 0x0040;
	if (keys[SDL_SCANCODE_2]) p2 |= 0x0080;

	if (app.controller)
	{
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) p1 |= 0x0001;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) p1 |= 0x0002;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) p1 |= 0x0004;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) p1 |= 0x0008;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_A)) p1 |= 0x0010;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_B)) p1 |= 0x0020;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_X)) p1 |= 0x0040;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_START)) p1 |= 0x0080;
		if (SDL_GameControllerGetButton(app.controller, SDL_CONTROLLER_BUTTON_BACK)) coin_pulse[0] = 1;
	}
	if (app.controller2)
	{
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_DPAD_UP)) p2 |= 0x0001;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) p2 |= 0x0002;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) p2 |= 0x0004;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) p2 |= 0x0008;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_A)) p2 |= 0x0010;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_B)) p2 |= 0x0020;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_X)) p2 |= 0x0040;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_START)) p2 |= 0x0080;
		if (SDL_GameControllerGetButton(app.controller2, SDL_CONTROLLER_BUTTON_BACK)) coin_pulse[1] = 1;
	}
	if (coin_pulse[0] > 0)
	{
		p1 |= 0x0100;
		coin_pulse[0]--;
	}
	if (coin_pulse[1] > 0)
	{
		p2 |= 0x0100;
		coin_pulse[1]--;
	}
	cave_set_inputs(machine, p1, p2);
}

static void present(void)
{
	if (!app.renderer || !app.texture)
		return;
	if (getenv("ESPRADE_DEBUG_SDL")) fprintf(stderr, "present: rotate\n");
	uint64_t start = SDL_GetPerformanceCounter();
	rotate_270(cave_video_frame(cave_current), app.rotated);
	if (getenv("ESPRADE_DEBUG_SDL")) fprintf(stderr, "present: update\n");
	SDL_UpdateTexture(app.texture, NULL, app.rotated, 240 * sizeof(uint32_t));
	SDL_RenderClear(app.renderer);
	SDL_RenderCopy(app.renderer, app.texture, NULL, NULL);
	SDL_RenderPresent(app.renderer);
	double elapsed = (double)(SDL_GetPerformanceCounter() - start) * 1000.0
		/ (double)SDL_GetPerformanceFrequency();
	if (elapsed > present_max) present_max = elapsed;
	present_sum += elapsed;
	presented_frames++;
	if (getenv("ESPRADE_DEBUG_SDL")) fprintf(stderr, "present: done %.2fms\n", elapsed);
}

static void pace_frame(void)
{
	uint64_t period = (uint64_t)((double)perf_frequency * sim_period + 0.5);
	uint64_t now = SDL_GetPerformanceCounter();
	next_deadline += period;

	/* If we are more than two frames late, resync instead of bursting to catch up. */
	if (now > next_deadline + period * 2)
	{
		next_deadline = now + period;
		missed_deadlines++;
	}

	uint64_t start_wait = SDL_GetPerformanceCounter();
	while (now < next_deadline)
	{
		double remaining_ms = (double)(next_deadline - now) * 1000.0 / (double)perf_frequency;
		if (remaining_ms > 1.5)
			SDL_Delay((Uint32)(remaining_ms - 1.0));
		else
			SDL_Delay(0);
		now = SDL_GetPerformanceCounter();
	}
	uint64_t end_wait = now < start_wait ? start_wait : now;
	double slept = (double)(end_wait - start_wait) * 1000.0 / (double)perf_frequency;
	sleep_sum += slept;
	sleep_count++;
	if (now > next_deadline + period / 2)
		missed_deadlines++;
}

static void record_frame_delta(double milliseconds)
{
	frame_deltas[frame_delta_position] = milliseconds;
	frame_delta_position = (frame_delta_position + 1) % (int)(sizeof(frame_deltas) / sizeof(frame_deltas[0]));
	if (frame_delta_count < (int)(sizeof(frame_deltas) / sizeof(frame_deltas[0])))
		frame_delta_count++;
}

static int compare_double(const void *left, const void *right)
{
	double a = *(const double *)left;
	double b = *(const double *)right;
	return (a > b) - (a < b);
}

static double percentile(double *values, int count, double fraction)
{
	if (count <= 0)
		return 0.0;
	int index = (int)(fraction * count);
	if (index >= count)
		index = count - 1;
	return values[index];
}

static void report_stats(cave_machine *machine)
{
	uint64_t now = SDL_GetPerformanceCounter();
	double elapsed = (double)(now - stats_window_start) / (double)perf_frequency;
	if (elapsed <= 0.0)
		return;
	double fps = (double)stats_window_frames / elapsed;
	double presented_fps = (double)presented_frames / elapsed;
	double sorted[600];
	int count = frame_delta_count;
	memcpy(sorted, frame_deltas, (size_t)count * sizeof(double));
	qsort(sorted, (size_t)count, sizeof(double), compare_double);
	double game_avg = game_count ? game_sum / (double)game_count : 0.0;
	double audio_mix_avg = audio_mix_count ? audio_mix_sum / (double)audio_mix_count : 0.0;
	double sleep_avg = sleep_count ? sleep_sum / (double)sleep_count : 0.0;
	double latency_avg = queue_latency_count ? queue_latency_sum / (double)queue_latency_count : 0.0;
	fprintf(stderr,
		"sim frame %llu sim_fps=%.2f presented_fps=%.2f p50=%.2fms p95=%.2fms p99=%.2fms "
		"max=%.2fms game=%.2f/%.2fms video=%.2f/%.2fms audio_mix=%.2f/%.2fms "
		"present=%.2f/%.2fms sleep=%.2fms audio_q=%u..%u latency=%.1f..%.1f/%.1fms "
		"under=%llu over=%llu missed=%llu\n",
		(unsigned long long)machine->frame, fps, presented_fps,
		percentile(sorted, count, 0.50), percentile(sorted, count, 0.95),
		percentile(sorted, count, 0.99), count ? sorted[count - 1] : 0.0,
		game_avg, game_max, video_sum / (double)(stats_window_frames ? stats_window_frames : 1),
		video_max, audio_mix_avg, audio_mix_max,
		presented_frames ? present_sum / (double)presented_frames : 0.0, present_max,
		sleep_avg, queue_min, queue_max,
		queue_latency_min > 1e8 ? 0.0 : queue_latency_min, queue_latency_max, latency_avg,
		(unsigned long long)audio_underruns, (unsigned long long)audio_overruns,
		(unsigned long long)missed_deadlines);
	stats_window_start = now;
	stats_window_frames = 0;
	frame_delta_count = 0;
	frame_delta_position = 0;
	queue_min = UINT32_MAX;
	queue_max = 0;
	queue_latency_min = 1e9;
	queue_latency_max = 0.0;
	queue_latency_sum = 0.0;
	queue_latency_count = 0;
	audio_mix_sum = 0.0;
	audio_mix_max = 0.0;
	audio_mix_count = 0;
	video_sum = 0.0;
	video_max = 0.0;
	game_sum = 0.0;
	game_max = 0.0;
	game_count = 0;
	sleep_sum = 0.0;
	sleep_count = 0;
	presented_frames = 0;
	present_sum = 0.0;
	audio_underruns = 0;
	audio_overruns = 0;
	present_max = 0.0;
	missed_deadlines = 0;
}

static void frame_ready(cave_machine *machine, void *user)
{
	(void)user;
	uint64_t now_counter = SDL_GetPerformanceCounter();
	double frequency = (double)perf_frequency;
	if (perf_last > 0.0)
		record_frame_delta((now_counter * 1000.0 / frequency) - perf_last);
	perf_last = now_counter * 1000.0 / frequency;
	if (last_frame_end)
	{
		double game_ms = (double)(now_counter - last_frame_end) * 1000.0 / frequency;
		game_sum += game_ms;
		game_count++;
		if (game_ms > game_max) game_max = game_ms;
	}
	video_sum += machine->video_ms;
	if (machine->video_ms > video_max) video_max = machine->video_ms;
	stats_window_frames++;
	if ((stats_enabled || getenv("ESPRADE_DEBUG_TIMING")) && machine->frame % 300 == 0)
		report_stats(machine);
	if (getenv("ESPRADE_DEBUG_SDL")) fprintf(stderr, "frame %llu start\n", (unsigned long long)machine->frame);
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
			running = 0;
		else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
			running = 0;
		else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11)
		{
			app.fullscreen = !app.fullscreen;
			SDL_SetWindowFullscreen(app.window, app.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		}
	}
	apply_buttons(machine);
	queue_audio(machine);
	if (!running)
		longjmp(escape, 1);
	present();
	pace_frame();
	last_frame_end = SDL_GetPerformanceCounter();
}

static const char *default_rom(void)
{
	static char path[2048];
	const char *from_env = SDL_getenv("ESPRADE_ROM");
	if (from_env && *from_env)
		return from_env;

	char *pref = SDL_GetPrefPath("omegakatana92", "ESPRaDe");
	if (pref)
	{
		snprintf(path, sizeof(path), "%srom/esprade.zip", pref);
		SDL_RWops *file = SDL_RWFromFile(path, "rb");
		if (file) { SDL_RWclose(file); SDL_free(pref); return path; }
		snprintf(path, sizeof(path), "%sesprade.zip", pref);
		file = SDL_RWFromFile(path, "rb");
		if (file) { SDL_RWclose(file); SDL_free(pref); return path; }
		SDL_free(pref);
	}
	const char *base = SDL_GetBasePath();
	if (base)
	{
		snprintf(path, sizeof(path), "%srom/esprade.zip", base);
		SDL_RWops *file = SDL_RWFromFile(path, "rb");
		if (file) { SDL_RWclose(file); return path; }
		snprintf(path, sizeof(path), "%sesprade.zip", base);
		file = SDL_RWFromFile(path, "rb");
		if (file) { SDL_RWclose(file); return path; }
	}
	return "rom/esprade.zip";
}

int main(int argc, char **argv)
{
	const char *rom = NULL;
	int fullscreen = 0;
	int audio_enabled = 1;
	int accelerated = 1;
	for (int index = 1; index < argc; index++)
	{
		if (strcmp(argv[index], "--rom") == 0 && index + 1 < argc)
			rom = argv[++index];
		else if (strcmp(argv[index], "--fullscreen") == 0)
			fullscreen = 1;
		else if (strcmp(argv[index], "--windowed") == 0)
			fullscreen = 0;
		else if (strcmp(argv[index], "--no-audio") == 0)
			audio_enabled = 0;
		else if (strcmp(argv[index], "--accelerated") == 0)
			accelerated = 1;
		else if (strcmp(argv[index], "--software") == 0)
			accelerated = 0;
		else if (strcmp(argv[index], "--stats") == 0)
			stats_enabled = 1;
	}

	SDL_SetHint(SDL_HINT_TIMER_RESOLUTION, "1");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0)
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 2;
	}
	app.width = 240 * 3;
	app.height = 320 * 3;
	app.window = SDL_CreateWindow("ESP Ra.De. Native",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, app.width, app.height,
		SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
	app.renderer = SDL_CreateRenderer(app.window, -1,
		accelerated ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
	if (!app.renderer)
		app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_SOFTWARE);
	if (!app.renderer)
		app.renderer = SDL_CreateRenderer(app.window, -1, 0);
	SDL_RenderSetLogicalSize(app.renderer, 240, 320);
#if SDL_VERSION_ATLEAST(2, 0, 18)
	SDL_RenderSetVSync(app.renderer, 0);
#endif
	app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, 240, 320);
	app.rotated = (uint32_t *)malloc(240 * 320 * sizeof(uint32_t));
	if (!app.window || !app.renderer || !app.texture || !app.rotated)
	{
		fprintf(stderr, "SDL: cannot create the window: %s\n", SDL_GetError());
		return 2;
	}
	{
		SDL_RendererInfo info;
		if (SDL_GetRendererInfo(app.renderer, &info) == 0)
			fprintf(stderr, "renderer: %s%s\n", info.name,
				(info.flags & SDL_RENDERER_ACCELERATED) ? " (accelerated)" : " (software)");
	}
	int opened = 0;
	for (int index = 0; index < SDL_NumJoysticks() && opened < 2; index++)
	{
		if (SDL_IsGameController(index))
		{
			SDL_GameController *controller = SDL_GameControllerOpen(index);
			if (controller)
			{
				if (opened == 0)
					app.controller = controller;
				else
					app.controller2 = controller;
				opened++;
			}
		}
	}

	SDL_AudioSpec want, have;
	memset(&want, 0, sizeof(want));
	memset(&have, 0, sizeof(have));
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 512;
	want.callback = NULL;
	app.audio = audio_enabled ? SDL_OpenAudioDevice(NULL, 0, &want, &have,
		SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE
		| SDL_AUDIO_ALLOW_CHANNELS_CHANGE) : 0;
	if (app.audio)
	{
		int bytes = (SDL_AUDIO_BITSIZE(have.format) / 8) * have.channels;
		app.audio_bytes_per_second = (Uint32)have.freq * (Uint32)bytes;
		fprintf(stderr,
			"audio: requested 44100 Hz S16 stereo, got %d Hz format=0x%04X channels=%d samples=%d\n",
			have.freq, (unsigned)have.format, have.channels, have.samples);
		if (have.freq != 44100 || have.format != AUDIO_S16SYS || have.channels != 2)
		{
			app.audio_stream = SDL_NewAudioStream(AUDIO_S16SYS, 2, 44100,
				have.format, have.channels, have.freq);
			if (!app.audio_stream)
				fprintf(stderr, "audio: cannot create converter: %s\n", SDL_GetError());
			else
				fprintf(stderr, "audio: converting 44100 Hz S16 stereo to the device format\n");
		}
		SDL_PauseAudioDevice(app.audio, 0);
	}
	else if (audio_enabled)
	{
		fprintf(stderr, "audio: cannot open device: %s\n", SDL_GetError());
	}

	cave_machine *machine = cave_machine_create();
	if (!machine)
		return 2;
	char message[1024] = {0};
	if (!cave_machine_load_rom(machine, rom ? rom : default_rom(), message, sizeof(message)))
	{
		fprintf(stderr, "ESPRaDe: %s\n", message);
		return 3;
	}
	char *pref = SDL_GetPrefPath("omegakatana92", "ESPRaDe");
	char eeprom_path[2048] = {0};
	if (pref)
	{
		snprintf(eeprom_path, sizeof(eeprom_path), "%sesprade-eeprom.bin", pref);
		eeprom_load(machine, eeprom_path);
	}

	machine->frame_hook = frame_ready;
	machine->escape = &escape;
	perf_frequency = SDL_GetPerformanceFrequency();
	next_deadline = SDL_GetPerformanceCounter();
	stats_window_start = next_deadline;
	m68k_reset();
	fprintf(stderr, "ESPRaDe native: reset PC=%06X target=%.2f Hz\n",
		cpu.pc & 0xFFFFFFu, 15625.0 / 271.5);
	if (setjmp(escape) == 0)
	{
		uint32_t pc = cpu.pc & 0xFFFFFFu;
		for (int round = 0; round < 100000 && running; round++)
		{
			if (!m68k_run_function(pc))
			{
				fprintf(stderr, "ESPRaDe: no translated code at %06X\n", pc);
				break;
			}
			pc = cpu.pc & 0xFFFFFFu;
			if (pc == 0 || pc == 0xFFFFFFu)
				break;
		}
	}
	if (pref)
	{
		eeprom_save(machine, eeprom_path);
		SDL_free(pref);
	}
	cave_machine_report(machine);
	cave_machine_destroy(machine);
	if (app.audio_stream)
		SDL_FreeAudioStream(app.audio_stream);
	if (app.audio)
		SDL_CloseAudioDevice(app.audio);
	if (app.controller)
		SDL_GameControllerClose(app.controller);
	if (app.controller2)
		SDL_GameControllerClose(app.controller2);
	SDL_DestroyTexture(app.texture);
	SDL_DestroyRenderer(app.renderer);
	SDL_DestroyWindow(app.window);
	free(app.rotated);
	SDL_Quit();
	return 0;
}
