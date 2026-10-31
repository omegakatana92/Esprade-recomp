# Milestone 3 — Audio, long run and Windows window

Date: 2026-10-31

## Verified

- **60,000 headless frames** (about 17 minutes at 57.55 Hz) with coin at frame 2400
  and start at frame 2600:
  - exit 0
  - `bad=0/0` (no invalid bus reads or writes)
  - no `no translated code` messages
- **YMZ280B audio**:
  - `--wav build/audio.wav` produces 44,100 Hz stereo PCM;
  - 20,000-frame run: 15,320,000 audio frames, RMS ≈ 3800, peak 32768;
  - voices use the player's 4 MiB sample ROM with address mirroring.
- **Windows SDL2 front end**:
  - `esprade.exe --rom ...` opens and stays alive;
  - the warning/title screen renders correctly rotated;
  - SDL2 audio device opens and queued audio is driven from the same thread as the game;
  - default renderer is software (the D3D9 accelerated path crashed in this environment);
    `--accelerated` opts in.
- **Handler coverage loop**: `tools/traced_targets.txt` now holds 100+ computed targets
  discovered with `ESPRADE_TRACE_MISSING=1`; regenerated with `tools/seed_traced.py`.

## Commands

```powershell
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip `
  --frames 60000 --coin 2400 --start 2600 --wav build\audio20.wav --png build\game30000.png 30000
build\cmake\esprade.exe --rom C:\Users\omega\Downloads\esprade.zip
```

## Notes

- The 68000's exception frame layout is SR, PC high, PC low from the new SP; the first
  implementation had the words in the wrong order and corrupted A7 on `RTE`. Fixed.
- Computed dispatch tables are followed by a small runtime trampoline for `BRA/JMP/RTS`
  entries; the branch destinations are the only code that needs static translation.
- Tile decoding uses the plane order from MAME's `gfx_element::decode` (plane 0 is the
  highest pen bit); the first implementation had it reversed.
- The SDL rotation buffer overflowed with the original mapping; the 320x240→240x320
  rotation is now bounds-checked by construction.
