# AGENTS.md — ESP Ra.De. Native Recompilation

## Mission

Build a genuine native port of **ESP Ra.De. (Cave/Atlus, 1998, World set `esprade`)** for
Windows 11 x64 and Steam Deck (Linux x86_64). The 68000 program is translated ahead of time to
C by the shared `arcade-decomp` toolchain and compiled to native code. The runtime implements
the Cave board directly; no emulator, no 68000 instruction interpreter in the shipped game.

The user supplies their own `esprade.zip`; no copyrighted ROM, graphics, sample, or translated
ROM-derived C is committed or shipped.

## Repository

- Official remote: `https://github.com/omegakatana92/Esprade-recomp` (branch `main`).
- Existing `LICENSE` is **GPL-2.0** and must be preserved. Do not replace it.
- `tools/core` is a pinned submodule of `cheviman/arcade-decomp` (BSD-3-Clause).
- Local development ROM: `C:\Users\omega\Downloads\esprade.zip` (never commit).

## Non-negotiables

1. Never commit ROM data, extracted graphics/audio, or translated ROM-derived C.
2. Never claim success without running the check, and record the exact command and result.
3. Keep Windows and Linux on the same `runtime/`, `ports/`, and CMake targets.
4. Do not edit `tools/core` unless fixing the upstream toolchain; prefer a newer upstream pin.
5. Preserve GPL-2.0 for project code and all upstream BSD/MIT notices.

## Layout

```
AGENTS.md                  this file
README.md                  player/build overview
CMakeLists.txt             shared Windows/Linux build
symbols.txt                68000 symbols for the static analysis
tools/                     game profile, ROM set, analysis/translation wrappers
tools/core/                arcade-decomp submodule (pinned)
runtime/include/           68000 contract, Cave board, SHA-1
runtime/src/               board bus, CPU glue, video, audio, EEPROM
runtime/third_party/miniz  ZIP reader (MIT)
ports/headless/            deterministic frame runner / CI gate
ports/sdl2/                window, input, audio
tests/                     scripts and control scenarios
scripts/                   Windows/Linux build and AppImage packaging
docs/                      architecture, status, milestones, packaging
```

## Hardware target

Cave first-generation 68000 board as wired for `esprade` in MAME `src/mame/atlus/cave.cpp`
(`esprade_map`). Reference only.

- CPU: 68000 at 16 MHz; 1 MiB program ROM (`u42.int` even, `u41.int` odd).
- Work RAM: 64 KiB at `0x100000`.
- YMZ280B at `0x300000`, byte-wide register/data, 16.9344 MHz, 4 MiB samples (`esp_u19`).
- Sprite RAM 64 KiB at `0x400000`; 013 generator, 8bpp sprites, 16 MiB (`u63`–`u66`).
- Three 038 tilemaps at `0x500000`, `0x600000`, `0x700000`; 8bpp tiles (`u51`–`u55`).
- Video registers `0x800000`; IRQ cause read `0x800000-0x800007`.
- Layer control `0x900000`, `0xA00000`, `0xB00000`.
- Palette `0xC00000-0xC0FFFF` (xGRB_555, 0x8000 entries).
- Inputs `0xD00000` (IN0) and `0xD00002` (IN1 + EEPROM DO), active low.
- EEPROM 93C46 16-bit at `0xE00000`; coin counter bits in the same byte.
- Video: 320x240 visible, cabinet ROT270; 57.55 Hz (15.625 kHz / 271.5 lines).
- Interrupt: VBLANK on level 1 autovector (vector 25).

## Key addresses

- Reset: SSP `0x110000`, PC `0x00595A`.
- Main loop / frame wait: routine `0x04F35E`; idle instruction `0x04F382` (`tst.w $100F04`).
- VBLANK/level-1 handler: `0x00061A`; sets `$100F04` at `0x000646`.

## Toolchain workflow

```powershell
python tools\verify_rom.py C:\Users\omega\Downloads\esprade.zip
python tools\seed_symbols.py C:\Users\omega\Downloads\esprade.zip
python tools\disassemble.py C:\Users\omega\Downloads\esprade.zip
python tools\recompile.py C:\Users\omega\Downloads\esprade.zip
cmake -S . -B build\cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang `
  -DCMAKE_PREFIX_PATH="$PWD\build\downloads\SDL2-2.32.10"
cmake --build build\cmake
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip --frames 600 --hashes build\boot.hashes
```

## Testing / milestones

- ROM verification must stay 13/13.
- Analysis should not lose reset, main loop, frame wait, or the vector handlers.
- Translation must report `not translated: 0`.
- Headless: at least 60 frames in CI, 600 locally, no bad bus access, no missing target.
- Video changes need a saved PNG and the expected frame number.
- Audio changes need a WAV and sample-rate/channel metadata.
- Log each milestone under `docs/milestones/`.

## Licensing

Project code: GPL-2.0 (existing `LICENSE`). `tools/core`: BSD-3-Clause, Sebastian Toyos.
MAME: BSD-3-Clause, reference only, never linked into `esprade` or ports. miniz: MIT.
Keep notices in `LICENSES/` and release archives. Do not relicense third-party code.
