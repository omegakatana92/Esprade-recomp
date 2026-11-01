/**
 *    Name:        ymz280b.c
 *    Description: Yamaha YMZ280B PCMD8 (8-channel 4-bit ADPCM / 8-bit / 16-bit PCM)
 *    Version:     0.2.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    Register interface and playback model follow MAME's ymz280b_device
 *    (src/devices/sound/ymz280b.cpp, BSD-3-Clause, Aaron Giles).  The chip runs from the
 *    player's 4 MiB sample ROM at 16.9344 MHz / 384 = 44100 Hz.
 */
#include "cave/cave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FRAC_BITS 9
#define FRAC_ONE (1 << FRAC_BITS)
#define MAX_SAMPLE_CHUNK 1024

static const int index_scale[8] = { 0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 };
static int diff_lookup[16];

typedef struct
{
	uint16_t fnum;
	uint8_t level;
	uint8_t pan;
	uint8_t mode;
	uint8_t looping;
	uint8_t playing;
	uint8_t ended;
	uint8_t keyon;
	uint32_t start;
	uint32_t stop;
	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t position;
	int32_t signal;
	int32_t step;
	int32_t loop_signal;
	int32_t loop_step;
	uint32_t loop_count;
	int32_t output_left;
	int32_t output_right;
	uint32_t output_pos;
	uint32_t output_step;
	int16_t last_sample;
	int16_t curr_sample;
} ymz_voice;

typedef struct
{
	cave_machine *machine;
	uint8_t current_register;
	uint8_t registers[0x100];
	uint8_t status;
	uint8_t irq_mask;
	uint8_t irq_enable;
	uint8_t keyon_enable;
	uint8_t ext_enable;
	uint32_t ext_address;
	uint8_t ext_latch;
	ymz_voice voice[8];
} ymz_state;

static ymz_state ymz;
static int16_t scratch[MAX_SAMPLE_CHUNK];

static uint8_t read_byte(uint32_t address)
{
	if (!ymz.machine || !ymz.machine->ymz_rom || !ymz.machine->ymz_size)
		return 0xFF;
	address &= (uint32_t)(ymz.machine->ymz_size - 1);
	return ymz.machine->ymz_rom[address];
}

static void compute_tables(void)
{
	for (int nib = 0; nib < 16; nib++)
	{
		int value = (nib & 0x07) * 2 + 1;
		diff_lookup[nib] = (nib & 0x08) ? -value : value;
	}
}

void ymz280b_reset(cave_machine *machine)
{
	memset(&ymz, 0, sizeof(ymz));
	ymz.machine = machine;
	compute_tables();
	for (int index = 0; index < 8; index++)
	{
		ymz.voice[index].output_pos = FRAC_ONE;
		ymz.voice[index].output_step = 1;
		ymz.voice[index].step = 0x7f;
		ymz.voice[index].loop_step = 0x7f;
	}
}

static void update_step(ymz_voice *voice)
{
	int frequency = (voice->mode == 1) ? (voice->fnum & 0x0FF) : (voice->fnum & 0x1FF);
	voice->output_step = (uint32_t)(frequency + 1);
}

static void update_volumes(ymz_voice *voice)
{
	if (voice->pan == 8)
	{
		voice->output_left = voice->level;
		voice->output_right = voice->level;
	}
	else if (voice->pan < 8)
	{
		voice->output_left = voice->level;
		voice->output_right = (voice->pan == 0) ? 0 : voice->level * (voice->pan - 1) / 7;
	}
	else
	{
		voice->output_left = voice->level * (15 - voice->pan) / 7;
		voice->output_right = voice->level;
	}
}

static void update_irq_state(void)
{
	int bits = ymz.status & ymz.irq_mask;
	int active = ymz.irq_enable && bits;
	if (ymz.machine)
	{
		ymz.machine->sound_pending = active ? 1 : 0;
		if (active)
			ymz.machine->irq_level = 1;
	}
}

static void write_to_register(int data)
{
	if (ymz.current_register < 0x80)
	{
		ymz_voice *voice = &ymz.voice[(ymz.current_register >> 2) & 7];
		switch (ymz.current_register & 0xE3)
		{
		case 0x00:
			voice->fnum = (uint16_t)((voice->fnum & 0x100) | (data & 0xFF));
			update_step(voice);
			break;
		case 0x01:
			voice->fnum = (uint16_t)((voice->fnum & 0xFF) | ((data & 0x01) << 8));
			voice->looping = (uint8_t)((data & 0x10) >> 4);
			if ((data & 0x60) == 0)
				data &= 0x7F;
			else
				voice->mode = (uint8_t)((data & 0x60) >> 5);
			if (!voice->keyon && (data & 0x80) && ymz.keyon_enable)
			{
				voice->playing = 1;
				voice->position = voice->start;
				voice->signal = voice->loop_signal = 0;
				voice->step = voice->loop_step = 0x7F;
				voice->loop_count = 0;
				voice->ended = 0;
			}
			else if (voice->keyon && !(data & 0x80))
			{
				voice->playing = 0;
			}
			voice->keyon = (uint8_t)((data & 0x80) >> 7);
			update_step(voice);
			break;
		case 0x02:
			voice->level = (uint8_t)data;
			update_volumes(voice);
			break;
		case 0x03:
			voice->pan = (uint8_t)(data & 0x0F);
			update_volumes(voice);
			break;
		case 0x20:
			voice->start = (voice->start & (0x00FFFFu << 1)) | ((uint32_t)data << 17);
			break;
		case 0x21:
			voice->loop_start = (voice->loop_start & (0x00FFFFu << 1)) | ((uint32_t)data << 17);
			break;
		case 0x22:
			voice->loop_end = (voice->loop_end & (0x00FFFFu << 1)) | ((uint32_t)data << 17);
			break;
		case 0x23:
			voice->stop = (voice->stop & (0x00FFFFu << 1)) | ((uint32_t)data << 17);
			break;
		case 0x40:
			voice->start = (voice->start & (0xFF00FFu << 1)) | ((uint32_t)data << 9);
			break;
		case 0x41:
			voice->loop_start = (voice->loop_start & (0xFF00FFu << 1)) | ((uint32_t)data << 9);
			break;
		case 0x42:
			voice->loop_end = (voice->loop_end & (0xFF00FFu << 1)) | ((uint32_t)data << 9);
			break;
		case 0x43:
			voice->stop = (voice->stop & (0xFF00FFu << 1)) | ((uint32_t)data << 9);
			break;
		case 0x60:
			voice->start = (voice->start & (0xFFFF00u << 1)) | ((uint32_t)data << 1);
			break;
		case 0x61:
			voice->loop_start = (voice->loop_start & (0xFFFF00u << 1)) | ((uint32_t)data << 1);
			break;
		case 0x62:
			voice->loop_end = (voice->loop_end & (0xFFFF00u << 1)) | ((uint32_t)data << 1);
			break;
		case 0x63:
			voice->stop = (voice->stop & (0xFFFF00u << 1)) | ((uint32_t)data << 1);
			break;
		default:
			break;
		}
	}
	else
	{
		switch (ymz.current_register)
		{
		case 0x84:
			ymz.ext_address = (ymz.ext_address & 0x00FFFFu) | ((uint32_t)data << 16);
			break;
		case 0x85:
			ymz.ext_address = (ymz.ext_address & 0xFF00FFu) | ((uint32_t)data << 8);
			break;
		case 0x86:
			ymz.ext_address = (ymz.ext_address & 0xFFFF00u) | data;
			ymz.ext_latch = read_byte(ymz.ext_address);
			break;
		case 0x87:
			ymz.ext_address = (ymz.ext_address + 1) & 0xFFFFFF;
			break;
		case 0xFE:
			ymz.irq_mask = (uint8_t)data;
			update_irq_state();
			break;
		case 0xFF:
			ymz.ext_enable = (uint8_t)((data & 0x40) >> 6);
			ymz.irq_enable = (uint8_t)((data & 0x10) >> 4);
			update_irq_state();
			if (ymz.keyon_enable && !(data & 0x80))
			{
				for (int index = 0; index < 8; index++)
					ymz.voice[index].playing = 0;
			}
			else if (!ymz.keyon_enable && (data & 0x80))
			{
				for (int index = 0; index < 8; index++)
					if (ymz.voice[index].keyon && ymz.voice[index].looping)
						ymz.voice[index].playing = 1;
			}
			ymz.keyon_enable = (uint8_t)((data & 0x80) >> 7);
			break;
		default:
			break;
		}
	}
}

void ymz280b_write(cave_machine *machine, uint8_t offset, uint8_t value)
{
	ymz.machine = machine;
	if ((offset & 1u) == 0)
	{
		ymz.current_register = value;
		return;
	}
	ymz.registers[ymz.current_register] = value;
	write_to_register(value);
}

uint8_t ymz280b_read(cave_machine *machine, uint8_t offset)
{
	ymz.machine = machine;
	if ((offset & 1u) == 0)
	{
		if (!ymz.ext_enable)
			return 0xFF;
		uint8_t result = ymz.ext_latch;
		ymz.ext_latch = read_byte(ymz.ext_address);
		ymz.ext_address = (ymz.ext_address + 1) & 0xFFFFFF;
		return result;
	}
	uint8_t status = ymz.status;
	ymz.status = 0;
	update_irq_state();
	return status;
}

void ymz280b_set_irq(cave_machine *machine)
{
	ymz.machine = machine;
	update_irq_state();
}

static int generate_adpcm(ymz_voice *voice, int16_t *buffer, int samples)
{
	int position = (int)voice->position;
	int signal = voice->signal;
	int step = voice->step;
	while (samples)
	{
		int val = read_byte((uint32_t)(position / 2)) >> ((~position & 1) << 2);
		signal += (step * diff_lookup[val & 15]) / 8;
		if (signal > 32767) signal = 32767;
		else if (signal < -32768) signal = -32768;
		step = (step * index_scale[val & 7]) >> 8;
		if (step > 0x6000) step = 0x6000;
		else if (step < 0x7F) step = 0x7F;
		*buffer++ = (int16_t)signal;
		samples--;
		position++;
		if (voice->looping)
		{
			if (position == (int)voice->loop_start && voice->loop_count == 0)
			{
				voice->loop_signal = signal;
				voice->loop_step = step;
			}
			if (position >= (int)voice->loop_end && voice->keyon)
			{
				position = (int)voice->loop_start;
				signal = voice->loop_signal;
				step = voice->loop_step;
				voice->loop_count++;
			}
		}
		if (position >= (int)voice->stop)
		{
			voice->ended = 1;
			break;
		}
	}
	voice->position = (uint32_t)position;
	voice->signal = signal;
	voice->step = step;
	return samples;
}

static int generate_pcm8(ymz_voice *voice, int16_t *buffer, int samples)
{
	int position = (int)voice->position;
	while (samples)
	{
		int val = read_byte((uint32_t)(position / 2));
		*buffer++ = (int16_t)((int8_t)val * 256);
		samples--;
		position += 2;
		if (voice->looping && position >= (int)voice->loop_end && voice->keyon)
			position = (int)voice->loop_start;
		if (position >= (int)voice->stop)
		{
			voice->ended = 1;
			break;
		}
	}
	voice->position = (uint32_t)position;
	return samples;
}

static int generate_pcm16(ymz_voice *voice, int16_t *buffer, int samples)
{
	int position = (int)voice->position;
	while (samples)
	{
		int val = (int16_t)((read_byte((uint32_t)(position / 2 + 1)) << 8)
			+ read_byte((uint32_t)(position / 2)));
		*buffer++ = (int16_t)val;
		samples--;
		position += 4;
		if (voice->looping && position >= (int)voice->loop_end && voice->keyon)
			position = (int)voice->loop_start;
		if (position >= (int)voice->stop)
		{
			voice->ended = 1;
			break;
		}
	}
	voice->position = (uint32_t)position;
	return samples;
}

void ymz280b_render(cave_machine *machine, int16_t *samples, int frames)
{
	ymz.machine = machine;
	memset(samples, 0, (size_t)frames * 2 * sizeof(int16_t));
	for (int index = 0; index < 8; index++)
	{
		ymz_voice *voice = &ymz.voice[index];
		int left = voice->output_left;
		int right = voice->output_right;
		if (voice->output_step == 0)
			voice->output_step = 1;
		if (!voice->playing && voice->last_sample == 0 && voice->curr_sample == 0)
		{
			voice->output_pos = FRAC_ONE;
			continue;
		}
		if (getenv("ESPRADE_DEBUG_AUDIO"))
			fprintf(stderr, "voice %d playing=%d mode=%d pos=%u stop=%u step=%u loop=%d level=%d pan=%d lvol=%d rvol=%d sig=%d curr=%d\n",
				index, voice->playing, voice->mode, voice->position, voice->stop,
				voice->output_step, voice->looping, voice->level, voice->pan,
				voice->output_left, voice->output_right, voice->signal, voice->curr_sample);
		for (int frame = 0; frame < frames; frame++)
		{
			while (voice->output_pos >= FRAC_ONE)
			{
				voice->last_sample = voice->curr_sample;
				int wanted = 1;
				int remaining;
				switch ((voice->playing << 7) | voice->mode)
				{
				case 0x81: remaining = generate_adpcm(voice, scratch, wanted); break;
				case 0x82: remaining = generate_pcm8(voice, scratch, wanted); break;
				case 0x83: remaining = generate_pcm16(voice, scratch, wanted); break;
				default: scratch[0] = 0; remaining = 0; break;
				}
				voice->curr_sample = (remaining == 0) ? scratch[0] : 0;
				voice->output_pos -= FRAC_ONE;
				if (voice->ended)
				{
					voice->ended = 0;
					voice->playing = 0;
					ymz.status |= (uint8_t)(1u << index);
					update_irq_state();
					voice->curr_sample = 0;
				}
			}
			int32_t interpolated = ((int32_t)voice->last_sample * (FRAC_ONE - (int32_t)voice->output_pos)
				+ (int32_t)voice->curr_sample * (int32_t)voice->output_pos) >> FRAC_BITS;
			int32_t l = samples[frame * 2 + 0] + interpolated * left / 256;
			int32_t r = samples[frame * 2 + 1] + interpolated * right / 256;
			if (l > 32767) l = 32767;
			if (l < -32768) l = -32768;
			if (r > 32767) r = 32767;
			if (r < -32768) r = -32768;
			samples[frame * 2 + 0] = (int16_t)l;
			samples[frame * 2 + 1] = (int16_t)r;
			voice->output_pos += voice->output_step;
		}
	}
}

void cave_audio_render(cave_machine *machine, int16_t *samples, int frames)
{
	static int16_t chip[8192];
	if (frames <= 0)
		return;
	if (frames > 2048)
		frames = 2048;
	ymz280b_render(machine, chip, frames * 2);
	for (int index = 0; index < frames; index++)
	{
		/* 88.2 kHz -> 44.1 kHz: average each pair (simple 2:1 decimation). */
		samples[index * 2 + 0] = (int16_t)(((int32_t)chip[index * 4 + 0]
			+ (int32_t)chip[index * 4 + 2]) / 2);
		samples[index * 2 + 1] = (int16_t)(((int32_t)chip[index * 4 + 1]
			+ (int32_t)chip[index * 4 + 3]) / 2);
	}
}
