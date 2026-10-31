# ESP Ra.De. Native Recompilation

A native, ROM-dependent port of **ESP Ra.De.** (Cave/Atlus, 1998, World set `esprade`) for
Windows 11 x64 and Linux x86_64 / Steam Deck.

The game's 68000 program is analysed statically and translated to C with
[cheviman/arcade-decomp](https://github.com/cheviman/arcade-decomp), pinned as the `tools/core`
submodule. The translated C is compiled into a native executable. The port reimplements the Cave
board's bus, tilemaps, sprite generator, palette, inputs, EEPROM and YMZ280B audio; no emulator
core and no instruction interpreter is shipped.

> **You must own the arcade board or a legally obtained ROM.** This repository contains no ROM
> data, no extracted graphics or audio, and no translated ROM-derived C. The tools read
> `rom/esprade.zip` from your machine and write generated output under the ignored `build/`
> directory only.

## Status

| Area | State |
| --- | --- |
| ROM verification | **Working**: 13/13 files by CRC32 + SHA-1 (World set) |
| Static analysis | **Working**: 34,945 instructions, 386 functions, no unresolved paths |
| Native translation | **Working**: 1,561 C functions, 0 unsupported instructions |
| Headless boot | **Working**: 600 frames, 0 bad bus accesses, 0 missing code |
| Video | **Working**: title, warning, in-game and ranking screens verified |
| Audio | **Working**: YMZ280B ADPCM/PCM mixer, 44.1 kHz stereo WAV verified |
| Inputs | **Working headless**: coin/start; SDL mapping implemented |
| Gameplay | **Verified 60,000 frames** with no missing code or bad bus access |
| Windows executable | **Builds and launches**: SDL warning/title screen verified |
| Steam Deck AppImage | **Not built yet** |

A successful compilation is not a playable build. Verified results and known gaps live in
[`docs/status.md`](docs/status.md) and [`docs/milestones/`](docs/milestones/).

## Requirements

- Python 3.11+
- CMake 3.20+ and a C11 compiler (clang, MSVC 2022, or gcc)
- Ninja (recommended)
- SDL2 development files for the windowed port
  - Windows: the SDL2 VC development package under `build/downloads/SDL2-2.32.10/`
  - Linux: `libsdl2-dev`
- The exact MAME `esprade` ROM set (13 files, CRC32/SHA-1 checked)

## Quick start (Windows)

```powershell
python tools\verify_rom.py C:\Users\you\Downloads\esprade.zip
python tools\seed_symbols.py C:\Users\you\Downloads\esprade.zip
python tools\disassemble.py C:\Users\you\Downloads\esprade.zip
python tools\recompile.py C:\Users\you\Downloads\esprade.zip

cmake -S . -B build\cmake -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=clang `
  -DCMAKE_PREFIX_PATH="$PWD\build\downloads\SDL2-2.32.10"
cmake --build build\cmake

build\cmake\esprade_headless.exe --rom C:\Users\you\Downloads\esprade.zip --frames 600 --hashes build\boot.hashes
build\cmake\esprade.exe --rom C:\Users\you\Downloads\esprade.zip
```

`tools\build_windows.ps1` wraps the same steps.

## Quick start (Linux / Steam Deck)

```bash
python3 tools/verify_rom.py /path/to/esprade.zip
python3 tools/seed_symbols.py /path/to/esprade.zip
python3 tools/disassemble.py /path/to/esprade.zip
python3 tools/recompile.py /path/to/esprade.zip

cmake -S . -B build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake

./build/cmake/esprade_headless --rom /path/to/esprade.zip --frames 600 --hashes build/boot.hashes
./build/cmake/esprade --rom /path/to/esprade.zip
```

Packaging for Steam Deck is described in [`docs/packaging.md`](docs/packaging.md).

## Controls

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move | Arrows / WASD | D-pad / left stick |
| Shot | Z / J | A |
| Bomb | X / K | B |
| Special | C / L | X |
| Start | 1 / Enter | Start |
| Coin | 5 | Back |
| Fullscreen | F11 | — |
| Quit | Esc | — |

Player 2 mappings are documented in `docs/controls.md`.

## Legal

The repository's existing **GPL-2.0** license (see `LICENSE`) covers this project's own
contributions. The `tools/core` submodule is BSD-3-Clause, copyright Sebastian Toyos; MAME is
BSD-3-Clause and is used only as a behavioural reference; miniz is MIT. Their notices are kept
in [`LICENSES/`](LICENSES/) and [`THIRD_PARTY.md`](THIRD_PARTY.md).

ESP Ra.De. is a trademark of its respective owners. No ROM or ROM-derived asset is distributed.
