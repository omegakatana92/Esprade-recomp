# Milestone 6 — Frame pacing, presentation and audio timing fix

Date: 2026-10-31

## Symptom

The windowed port showed slowdown during play and the sound dragged / stuttered.

## Measurements

Headless core (Windows, clang):

- 10,000 frames: ~189 fps before the layer cache, ~234 fps after.
- 20,000 frames including gameplay: average ~4.3 ms/frame, worst frame ~52 ms,
  54 frames over the 17.37 ms budget.

SDL port at 720x960:

- software renderer: avg 17.5 ms, present max 8.0 ms — acceptable.
- accelerated renderer: avg 17.5 ms, present max 6.6 ms.

SDL port fullscreen (desktop 1600x900):

- **software**: avg 18.1–18.8 ms, present max **30.5 ms** — below 57.55 Hz.
- **accelerated**: avg 17.5 ms, present max 10.8–13.5 ms — on budget.

## Cause

1. The SDL port defaulted to the **software** renderer. An earlier D3D9 crash had been
   caused by the rotation-buffer overflow, not by D3D9 itself; after fixing the rotation
   mapping, accelerated D3D9/OpenGL rendering works.
2. Software fullscreen upscaling exceeded the 17.37 ms arcade frame time. The audio queue
   was filled with a fixed 766 samples per frame, so every late frame starved it and the
   sound slowed/stuttered with the picture.

## Fix

- Accelerated renderer is now the default; `--software` opts out, and a failed accelerated
  creation falls back to software.
- The renderer name and mode are printed at startup.
- Audio is queued from the **measured elapsed wall time** instead of a fixed 766 samples.
  Late frames queue more samples, early frames queue fewer, keeping 44.1 kHz audio on the
  wall clock.
- Layer 512x512 pen/colour/category maps are cached and rebuilt only when tile RAM or the
  8x8/16x16 mode changes. This raised the headless core from ~189 to ~234 fps and reduced
  bus-visible frame cost.

## Verified after the fix

- Headless 20,000 frames: same hash `f06c83ecc7f163b3`, `bad=0/0`.
- SDL fullscreen (accelerated): avg 17.5 ms, present max 13.5 ms.
- SDL gameplay run (coin/start): avg 17.56–17.68 ms through frame 3240, present max 9.6 ms.
- SDL gameplay with audio (accelerated): avg 17.5–17.7 ms, present max 11.8 ms,
  audio queue stayed between **4,948 and 10,376 bytes** (≈28–59 ms of audio) — no
  underrun.
- 60,000 headless frames after the cache: same hash `3fe0b6dc3322198a`, `bad=0/0`,
  60 frames over 17.4 ms out of 60,000 (0.1%).
- Startup diagnostics now report e.g. `renderer: direct3d (accelerated)`.

Remaining spikes are rare scene transitions (worst headless frame ~150 ms, measured while
running unpaced under a loaded desktop); they no longer change audio speed because of the
wall-clock audio queue. Further work can pre-decode large sprite runs or split the mixer into
smaller chunks.
