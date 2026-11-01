# Status and known limitations

Updated: 2026-10-31

## Verified

| Check | Command | Result |
| --- | --- | --- |
| ROM integrity | `python tools/verify_rom.py C:\Users\omega\Downloads\esprade.zip` | 13/13 files, CRC32 + SHA-1 |
| Static analysis | `python tools/disassemble.py ...` | 50,629 instructions, 734 functions, no invalid paths |
| Static translation | `python tools/recompile.py ...` | 0 unsupported instructions |
| Headless boot | `--frames 600` | exit 0, 0 bad reads/writes |
| Attract/gameplay | `--frames 60000 --coin 2400 --start 2600` | 60,000 frames, 0 bad reads/writes, no missing targets |
| Crash regression | `--confirm 3800 --autofire --automove --autobomb --frames 20000` | 20,000 frames, `bad=0/0`, no missing targets; ASan 12,000 frames clean |
| Title screen | `--png title.png 3000` | logo, "PLEASE INSERT COIN", Atlus/Cave copyright |
| In-game scene | `--png game7300.png 7300` | street scene with characters/explosion |
| Score ranking | `--png game30000.png 30000` | fully rendered ranking screen |
| Audio | `--wav build/audio.wav` | 44.1 kHz stereo, RMS 3800, peak 32768 |
| Windows window | `esprade.exe --rom ...` | SDL window opens, warning/title screen renders, stays alive |
| Windows build | `cmake --build build\cmake` | `esprade.exe`, `esprade_headless.exe` |
| Linux build | `scripts/build_linux.sh` in Ubuntu 22.04 | `esprade`, `esprade_headless` |
| AppImage | `scripts/package_appimage.sh` | built; headless 600 frames; launched under Xvfb |

## Works

- Exact World `esprade` set identified by content, not names.
- Program region reconstructed from `u42.int` (even) and `u41.int` (odd).
- 68000 exceptions, VBLANK and sound interrupts, the call/return model and the 24-bit bus.
- 038 tilemaps (8x8/16x16, row scroll/select), 013 8bpp sprites, xGRB_555 palette and
  priority rules.
- YMZ280B: register interface, ADPCM/8-bit/16-bit PCM voices, 44.1 kHz stereo mixer.
- Coin/start input; 93C46 EEPROM persisted by the SDL port.
- Portable C11 runtime; Windows clang build and Linux CMake scripts.

## Not yet working / not yet verified

- **Interactive controls**: coin/start for both players are wired and were exercised
  (Windows window and headless). Player 2 uses the second SDL controller. Deterministic
  shot, character confirm, movement and bomb paths are exercised by
  `--confirm/--autofire/--automove/--autobomb`; a human play test is still required.
- **Performance**: absolute 57.55 Hz scheduler; fullscreen measured 57.55
  simulated/presented FPS, p99 20.1 ms, 0 missed deadlines in a 20,000-frame
  coin/start/confirm/move/bomb run. Rendering happens after the VBLANK handler.
- **Audio**: YMZ280B at 88.2 kHz decimated to 44.1 kHz, device-format conversion,
  fractional sample accumulation, bounded queue. Measured queue latency 33–52 ms
  (avg ~43 ms), 0 underruns/overruns in the 20,000-frame run.
- **Display list**: Stage 1 intro has 0 zero sprite entries in both native and MAME;
  warning screen pixel-identical to MAME. The green text-heavy transition is the
  expected Stage 1 intro, not corruption.
- **Steam Deck**: the AppImage is built and launches under Xvfb, but it has not been run on
  a physical SteamOS device; Steam Input, Gaming Mode and audio device selection remain
  unverified.
- **MAME gameplay comparison**: a MAME 0.289 AVI/WAV reference was recorded. The warning
  screen is pixel-identical and the Stage 1 intro has the same zero-sprite count, but a
  full 90 s player-controlled Stage 1 A/B and measured audiovisual offset are still open.
- **Analysis coverage**: ~200 KiB of the 1 MiB program is classified as code. The trace
  loop covers the attract/gameplay paths exercised so far, including handler `051B62`
  reached on the player-confirm/move path; other modes (service menu, endings, both
  players) may still reveal unseeded handlers.
- **Reported crash**: no `c0000005`/heap-corruption dump was produced by the current
  source in 20,000 SDL frames or 12,000 ASan frames. The historical dumps predate
  `48f8c3d` and are consistent with the already-fixed rotation/heap bug. The exact
  63-second report is still an open reproducer; see
  `docs/milestones/09-crash-investigation.md`.
- **Audio fidelity**: the mixer is functional but not yet compared sample-by-sample with
  MAME/ymfm.

## Next

1. Run a human-controlled Stage 1 session with `--stats` and measure audiovisual offset
   against the MAME reference.
2. Launch the AppImage on a physical Steam Deck (Desktop and Gaming Mode).
3. Compare the audio mixer sample-by-sample with MAME/ymfm.
4. Continue the handler-table trace loop for service mode and both-player paths.
