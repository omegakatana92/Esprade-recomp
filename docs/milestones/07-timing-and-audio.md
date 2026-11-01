# Milestone 7 — Timing and audio-latency repair

Date: 2026-10-31

This milestone addresses the remaining “gameplay slower than arcade / audio delayed”
report after the earlier renderer change.

## Root causes found

1. **Frame scheduler used low-resolution wall time.**
   `SDL_GetTicks64()` + `SDL_Delay()` could oversleep and the deadline was derived from
   the end of the previous frame rather than an absolute timeline. During busy scenes the
   process fell behind and never caught up.
2. **YMZ280B ran at the wrong rate.**
   MAME's YMZ280B internal stream is `clock / 192` = **88.2 kHz**
   (`m_master_clock = clock / 384`, `INTERNAL_SAMPLE_RATE = m_master_clock * 2`).
   This port generated the chip at **44.1 kHz**, so every voice played at half rate and
   low pitch.
3. **SDL device format was ignored.**
   The device returned `AUDIO_F32LSB` (format `0x8120`) while the port queued
   `AUDIO_S16SYS` bytes. That is the main source of the noisy/distorted sound. A
   `SDL_AudioStream` now converts 44.1 kHz S16 stereo to the obtained device format.
4. **Audio samples were generated from wall-clock time.**
   A slow frame generated more samples and desynchronised audio from the game. Samples are
   now generated from the simulated frame clock with fractional accumulation:
   `44100 * 271.5 / 15625 = 766.2912` samples per game frame.
5. **Layer composition recomputed scroll/flip/row parameters per pixel.**
   The loop now hoists layer parameters per frame and per scanline, precomputes source-x
   maps, and uses a cached palette RGB table.

## Scheduler

- Absolute deadline in `SDL_GetPerformanceCounter()` units.
- Period = `271.5 / 15625` s = 17.376 ms = 57.55 Hz.
- Hybrid wait: sleep until ~1.5 ms before the deadline, then yield/spin.
- Missed deadlines counted; if more than two frames late, the deadline resynchronises
  instead of bursting to catch up.
- VSync explicitly disabled (`SDL_RenderSetVSync(renderer, 0)`); there is no second
  frame limiter.
- `SDL_HINT_TIMER_RESOLUTION=1` on Windows.

## Audio

- YMZ280B mixer runs at 88.2 kHz; `cave_audio_render()` averages pairs to 44.1 kHz.
- Fractional accumulator: 766 or 767 output samples per simulated frame, exactly
  44100 samples/second on average.
- Device spec is logged. If the device is not 44.1 kHz S16 stereo, an
  `SDL_AudioStream` converts to the obtained format.
- Queue is bounded at ~167 ms; overflow frames are dropped and counted rather than
  growing without bound.
- Diagnostics: queue latency in ms, underruns, overruns, audio mixing time.

## Diagnostics added

Every 300 frames with `--stats` (or `ESPRADE_DEBUG_TIMING=1`):

```
sim frame N sim_fps=.. presented_fps=.. p50=.. p95=.. p99=.. max=..
game=avg/max video=avg/max audio_mix=avg/max present=avg/max sleep=avg
audio_q=min..max latency=min..max/avgms under=.. over=.. missed=..
```

## Measurements

Fullscreen, 1600x900 desktop, Direct3D accelerated, audio enabled:

| Metric | Result |
| --- | --- |
| Simulated FPS | **57.55** |
| Presented FPS | **57.55** |
| Frame p50 / p95 / p99 | ~17.4 / ~20.0 / ~21.0 ms |
| Missed deadlines | 0 |
| Game logic + video | ~2.2 ms avg |
| Video composition | ~2.1 ms avg |
| SDL present | ~1.1 ms avg |
| Audio mix | ~0.02 ms avg |
| Audio queue latency | **~41–63 ms, ~52 ms avg** |
| Audio underruns / overruns | 0 / 0 |

Windowed mode is also stable while the window is focused. If the window is fully
occluded by another application, Windows/DWM can throttle `SDL_RenderPresent`; that
shows up as present stalls and is a host-compositor behaviour, not simulation work.

## Regression checks

- Headless 600 frames: `bc585f53474caa03`, `bad=0/0`.
- Headless 6,000 frames with coin/start: `e9844fb0596e34be`, `bad=0/0`.
- Headless 60,000 frames: `3fe0b6dc3322198a`, `bad=0/0`.
- WAV: 44,100 Hz stereo, duration 104.256 s for 6,000 frames (expected 104.257 s).

## MAME reference

A MAME 0.289 Windows reference run was recorded for `esprade` (Interleaved AVI at
240x320, 57.55 fps, plus WAV). It confirms the refresh rate and provides a boot/attract
reference. An input-matched Stage 1 comparison was not completed: MAME's Lua input
scripting reached the character-select screen but could not be driven reliably into
Stage 1 in this session. This remains open and is not claimed as verified.
