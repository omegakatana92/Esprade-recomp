# Milestone 8 — VBLANK/render ordering and display-list alignment

Date: 2026-10-31

## Problem

The renderer ran inside `program_frame_wait()`, *before* the level-1 VBLANK handler.
The handler is what clears and rebuilds the object/sprite display list. Rendering before
it could capture zero/partial sprite entries and also did the frame's video work at the
wrong point in the interrupt sequence.

The object animation timer at `0x00A890` (`subq.b #1,($28,A6)`) is reached from the
object update routine `0x00A5E2`; the generated C for this instruction is correct
(`m68k_carry_sub8` + `bcc`). The divergence was therefore the render ordering, not the
timer instruction.

## Change

- `program_frame_wait()` now only advances time, raises VBLANK and marks the frame pending.
- The VBLANK handler runs through `m68k_take_interrupts()` as before.
- `program_frame_end()` then renders, mixes audio, presents and paces, after the handler
  has finished the display list for that frame.

This also removed the previous pre-handler/post-handler double-frame ordering.

## Verification

- **Warning screen**: native frame 300 vs MAME 0.289 frame 287 (aligned by screen state):
  mean absolute pixel difference **0.0**, 0 differing pixels.
- **Stage 1 intro**: native frame 7300 (coin/start) vs MAME frame 1400 (input-matched):
  mean difference ~2.5/255; 5% pixels differ (animation/fade timing).
- **Zero sprite entries** at the Stage 1 intro:
  - MAME frame 1400: **0**
  - native frame 7300: **0**

## Headless regression after ordering

- 60,000 frames with coin/start: hash `a61a8f035dcbc528`, `bad=0/0`,
  `irq=60000`, `vmiss=0`, only 5 frames over 17.4 ms (max 34.2 ms).
  Before the ordering change the same run had hundreds of over-budget frames and
  max ~150 ms.
- 1,000/20,000-frame runs: no bad accesses, no missing translated code.

## Interactive SDL run

Deterministic inputs (`--coin 2400 --start 2600 --autofire --frames 12000`) over 12,000
frames (~208 s), fullscreen, audio enabled:

| Metric | Result |
| --- | --- |
| Simulated FPS | **57.55** |
| Presented FPS | **57.55** |
| p50 / p95 / p99 / max | 17.6 / 18.7 / 19.4 / 20.0 ms |
| Missed deadlines | 0 |
| Audio queue latency | 21–45 ms, average ~34 ms |
| Underruns / overruns | 0 / 0 |
| Game/video/present/audio mix | ~0.9 / 0.9 / 0.8 / 0.01 ms |

The 167 ms queue ceiling is never approached; actual queued audio is ~7–16 KB in the
float32 device format. A 256-frame device buffer reduced latency but caused startup
underruns, so 512 frames is retained.

## Remaining

- A clean interactive Stage 1 confirmation with a human-controlled player, plus exact
  audiovisual offset measurement, is still required. The automated run reached the
  character select and Stage 1 intro, but the input script does not prove full
  player-controlled Stage 1 survival.
- MAME reference AVI/WAV is local-only (no ROM or game assets committed).
