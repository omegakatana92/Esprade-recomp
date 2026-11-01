# Milestone 4 — Linux build and Steam Deck AppImage

Date: 2026-10-31

## Verified

- Ubuntu 22.04 x86_64 (WSL2): CMake/Ninja/gcc 11.4 build succeeds.
- Linux headless:
  - `--frames 600` → `hashes=bc585f53474caa03`, `bad=0/0`, exit 0;
  - `--frames 60000 --coin 2400 --start 2600` → `hashes=3fe0b6dc3322198a`,
    `bad=0/0`, same hash as the Windows build (cross-platform determinism).
- Linux YMZ280B WAV: 44.1 kHz stereo, 60,000 frames, non-silent.
- `build/ESPRaDe-SteamDeck-x86_64.AppImage` produced by
  `scripts/package_appimage.sh` (6.0 MiB compressed, 14.5 MiB AppDir).
- AppImage contents: `usr/bin/esprade`, `usr/bin/esprade_headless`, SDL2 and non-base
  libraries, AppRun, desktop file, generated icon, GPL/BSD/MIT notices.
- AppImage headless extracted and run: 600 frames, `bad=0/0`, exit 0.
- AppImage windowed binary launched under `xvfb-run`: reached reset and kept running until
  the 8-second timeout; the X server was then torn down (`XIO` is the harness, not the
  port).

## Not verified

- Physical Steam Deck (Desktop and Gaming Mode), Steam Input mapping, and audio device
  selection on SteamOS.
- The AppImage's bundled library set on a SteamOS image newer than Ubuntu 22.04; the
  build uses glibc 2.35 as the floor.

## Commands

```bash
ROM=/path/to/esprade.zip ./scripts/build_linux.sh
ROM=/path/to/esprade.zip VERSION=0.3.0 ./scripts/package_appimage.sh
./build/ESPRaDe-SteamDeck-x86_64.AppImage --appimage-extract-and-run --rom /path/to/esprade.zip
```

Set `ESPRADE_ROM=/path/to/esprade.zip` or place the ZIP in
`~/.local/share/ESPRaDe/rom/esprade.zip` for double-click launch. The ROM is never bundled.
