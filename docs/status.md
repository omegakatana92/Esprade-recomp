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
  (Windows window and headless). Player 2 uses the second SDL controller. Movement,
  shot/bomb and fullscreen are implemented but not yet exercised against gameplay.
- **Performance**: the scheduler now uses an absolute high-resolution deadline at
  57.55 Hz; fullscreen measured 57.55 simulated/presented FPS with p99 ≈21 ms and no
  missed deadlines. Windowed mode is stable while focused; an occluded window can be
  throttled by the desktop compositor.
- **Audio**: YMZ280B runs at its 88.2 kHz internal rate and is decimated to 44.1 kHz;
  device-format conversion, fractional sample accumulation and a bounded queue are in
  place. Measured latency ≈41–63 ms, no underruns in the fullscreen run.
- **Steam Deck**: the AppImage is built and launches under Xvfb, but it has not been run on
  a physical SteamOS device; Steam Input, Gaming Mode and audio device selection remain
  unverified.
- **MAME gameplay comparison**: a MAME 0.289 AVI/WAV reference was recorded and confirms
  the 57.55 Hz/240x320 presentation, but an input-matched Stage 1 A/B comparison is still
  open.
- **Analysis coverage**: ~200 KiB of the 1 MiB program is classified as code. The trace
  loop covers the attract/gameplay paths exercised so far; other modes (service menu,
  endings, both players) may still reveal unseeded handlers.
- **Audio fidelity**: the mixer is functional but not yet compared sample-by-sample with
  MAME/ymfm.

## Next

1. Exercise the SDL port interactively: coin, start, shot/bomb, fullscreen, gamepad.
2. Build the AppImage on Linux x86_64 and launch it on Steam Deck Desktop/Gaming Mode.
3. Record a MAME reference trace and compare deterministic frames/audio.
4. Continue the handler-table trace loop for service mode and both-player paths.
