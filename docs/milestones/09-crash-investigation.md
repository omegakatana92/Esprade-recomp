# Milestone 9 — Windows gameplay crash investigation

Date: 2026-10-31

## Report

The Windows x64 build was reported to close during Stage 1 at roughly 63 seconds,
while combat, the scene transition and a large enemy wave were on screen. The report did
not include a Windows exception code or a dump.

## What the crash records show

The local Windows Error Reporting history contains six `esprade.exe` crash dumps from
18:42:37–18:45:14 on 2026-10-31:

- `c0000005` access violation at `esprade.exe+0x1a55`, writing to `0x18ac4b3fb80`;
- several `c0000374` heap-corruption reports.

Those dumps **predate commit `48f8c3d`** (2026-10-31 22:04 -0500) and the current v0.6.0
build. The local debug PDB from the same build window resolves `0x1a55` to
`frame_ready` in `ports/sdl2/main.c`; the dumped module size does not match that debug
image exactly, so this mapping is indicative rather than exact. The `c0000374` records
show that heap corruption happened before the later access violation. That is consistent
with the rotation-buffer overflow that was fixed before the v0.6.0 build, not with a new
regression in `48f8c3d`.

No crash dump was produced by the current v0.6.0 binary. The published v0.6.0 binary also
predates the new scripting flags, so it could not exercise the player-input path below.

## Reproducible failure on the current source

Running the current source with a deterministic player path:

```powershell
build\cmake\esprade.exe --rom C:\Users\omega\Downloads\esprade.zip --fullscreen --stats `
  --coin 2400 --start 2600 --confirm 3800 --autofire --automove --autobomb --frames 9000
```

stopped at frame **5276** with:

```
m68k: no translated code at 051B62
```

The SDL loop then exited. This is a controlled termination, not an access violation, but
from the player's side the window closes exactly like a crash. `051B62` was a runtime
handler reachable only after confirming the character and moving/bombing through Stage 1.

## Change

- Added `051B62` to `tools/traced_targets.txt` and `symbols.txt`; regenerated the static
  translation.
- Added deterministic SDL test inputs used to reproduce the path:
  `--confirm FRAME`, `--automove`, `--autobomb` (in addition to `--coin`, `--start`,
  `--autofire`, `--frames`).

## Retest after the coverage fix

| Test | Result |
| --- | --- |
| Deterministic player path, 9,000 frames | clean, no missing targets |
| Deterministic player path, 20,000 frames (~348 s), fullscreen, audio | `bad=0/0`, no missing targets, 0 underruns/overruns/missed deadlines |
| F11 fullscreen toggles during 6,000-frame run | clean |
| ASan headless, 2,000 frames | clean |
| ASan SDL, 5,000 frames with confirm/move/bomb | clean |
| ASan SDL, 12,000 frames with confirm/move/bomb | clean |

20,000-frame timing (fullscreen, audio enabled):

| Metric | Result |
| --- | --- |
| Simulated / presented FPS | 57.55 / 57.55 |
| p50 / p95 / p99 / max | 17.68 / 19.69 / 20.12 / 20.44 ms |
| Game / video / present | ~1.40 / ~1.34 / ~0.87 ms avg |
| Audio queue latency | 33.4–51.9 ms, average 43.2 ms |
| Underruns / overruns / missed deadlines | 0 / 0 / 0 |

## Display-list and transition checks

- Warning screen: native frame 300 vs MAME 0.289 frame 287, mean absolute pixel
  difference **0.0**.
- Stage 1 intro zero-sprite entries: native frame 7300 = **0**, MAME frame 1400 = **0**.
- The green, text-heavy transition is the game's Stage 1 intro, not graphics corruption;
  it matches the MAME capture.
- `0x00A890` (`subq.b #1,($28,A6)`) is translated correctly; the earlier divergence was
  the render ordering fixed in milestone 8.

## Limitations

- The exact reported `0xc0000005` / 63-second event was **not** reproduced on the current
  source. The historical dumps are from older builds and are consistent with the
  already-fixed heap/rotation bug.
- The missing-handler exit is the only current-source failure that was reproduced. It is
  fixed for the exercised path, but it is a coverage fix, not proof that every possible
  report is resolved.
- A human-controlled 90 s Stage 1 A/B with measured audiovisual offset and a physical
  Steam Deck run are still open.
