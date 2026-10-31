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
static uint64_t last_tick;
static const double frame_seconds = 1.0 / (15625.0 / 271.5);

typedef struct
{
	uint32_t *rotated;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	int width, height;
	int fullscreen;
	SDL_GameController *controller;
	SDL_AudioDeviceID audio;
} frontend;

static frontend app;
static int coin_pulse[2];

static void rotate_270(const uint32_t *source, uint32_t *destination)
{
	/* source 320x240 (rows 240, columns 320) -> destination 240x320 */
	for (int y = 0; y < 240; y++)
		for (int x = 0; x < 320; x++)
			destination[(239 - x) * 320 + y] = source[y * 320 + x];
}

static void audio_callback(void *user, Uint8 *stream, int length)
{
	(void)user;
	ymz280b_render(cave_current, (int16_t *)stream, length / 4);
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
	rotate_270(cave_video_frame(cave_current), app.rotated);
	SDL_UpdateTexture(app.texture, NULL, app.rotated, 240 * sizeof(uint32_t));
	SDL_RenderClear(app.renderer);
	SDL_RenderCopy(app.renderer, app.texture, NULL, NULL);
	SDL_RenderPresent(app.renderer);
}

static void frame_ready(cave_machine *machine, void *user)
{
	(void)user;
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
	if (!running)
		longjmp(escape, 1);
	present();

	uint64_t now = SDL_GetTicks64();
	double target = last_tick + frame_seconds * 1000.0;
	if (now < (uint64_t)target)
		SDL_Delay((uint32_t)(target - now));
	last_tick = SDL_GetTicks64();
}

static const char *default_rom(void)
{
	static char path[1024];
	const char *base = SDL_GetBasePath();
	if (base)
	{
		snprintf(path, sizeof(path), "%srom/esprade.zip", base);
		if (SDL_RWFromFile(path, "rb"))
			return path;
		snprintf(path, sizeof(path), "%sesprade.zip", base);
		if (SDL_RWFromFile(path, "rb"))
			return path;
	}
	return "rom/esprade.zip";
}

int main(int argc, char **argv)
{
	const char *rom = NULL;
	int fullscreen = 0;
	for (int index = 1; index < argc; index++)
	{
		if (strcmp(argv[index], "--rom") == 0 && index + 1 < argc)
			rom = argv[++index];
		else if (strcmp(argv[index], "--fullscreen") == 0)
			fullscreen = 1;
		else if (strcmp(argv[index], "--windowed") == 0)
			fullscreen = 0;
	}

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
	app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!app.renderer)
		app.renderer = SDL_CreateRenderer(app.window, -1, 0);
	SDL_RenderSetLogicalSize(app.renderer, 240, 320);
	app.texture = SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING, 240, 320);
	app.rotated = (uint32_t *)malloc(240 * 320 * sizeof(uint32_t));
	if (!app.window || !app.renderer || !app.texture || !app.rotated)
	{
		fprintf(stderr, "SDL: cannot create the window: %s\n", SDL_GetError());
		return 2;
	}
	for (int index = 0; index < SDL_NumJoysticks(); index++)
	{
		if (SDL_IsGameController(index))
		{
			app.controller = SDL_GameControllerOpen(index);
			if (app.controller)
				break;
		}
	}

	SDL_AudioSpec want, have;
	memset(&want, 0, sizeof(want));
	want.freq = 44100;
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	want.callback = audio_callback;
	app.audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (app.audio)
		SDL_PauseAudioDevice(app.audio, 0);

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
	m68k_reset();
	fprintf(stderr, "ESPRaDe native: reset PC=%06X\n", cpu.pc & 0xFFFFFFu);
	last_tick = SDL_GetTicks64();
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
	if (app.audio)
		SDL_CloseAudioDevice(app.audio);
	if (app.controller)
		SDL_GameControllerClose(app.controller);
	SDL_DestroyTexture(app.texture);
	SDL_DestroyRenderer(app.renderer);
	SDL_DestroyWindow(app.window);
	free(app.rotated);
	SDL_Quit();
	return 0;
}
