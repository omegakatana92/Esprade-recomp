# Third-party notices

This repository is distributed under the existing **GPL-2.0** `LICENSE`, which covers the
project's own code. The following components keep their own licenses; their notices must remain
in source and binary distributions.

## arcade-decomp (`tools/core/`)

- Copyright Sebastian Toyos.
- License: **BSD-3-Clause**, see `LICENSES/BSD-3-Clause-arcade-decomp.txt`.
- Pinned as a Git submodule. `tools/core` is not modified by this project.
- The 68000 decoder, analysis, lift, translator and decompiler used to generate the native C.

## MAME (`src/mame/atlus/cave.cpp`, `cave_v.cpp`, `sprite013.cpp`, `src/devices/video/tmap038.cpp`)

- Copyright holders include Luca Elia, David Haywood, Paul Priest and others.
- MAME source files are **BSD-3-Clause** unless stated otherwise; the MAME tree as a whole is
  GPL-2.0 (see `LICENSES/MAME-COPYING.txt`).
- Used **only as a behavioural reference** while reimplementing the Cave board. No MAME code is
  compiled into `esprade` or `esprade_headless`, and no MAME source is shipped in release
  archives beyond this notice.
- Board behaviour citations appear in the runtime source comments.

## miniz (`runtime/third_party/miniz/`)

- Copyright 2013-2014 RAD Game Tools and Valve Software; 2010-2014 Rich Geldreich and
  Tenacious Software LLC; 2016 Martin Raiber.
- License: **MIT**, see `LICENSES/MIT-miniz.txt`.
- Used only to read the player's ZIP archive at run time.

## SDL2

- Copyright Sam Lantinga and contributors.
- License: **zlib**, see the SDL2 distribution's `LICENSE.txt`.
- Linked dynamically by the windowed port; not bundled in the source tree.

## Game data

ESP Ra.De. is © Cave/Atlus and its respective rights holders. No ROM, graphics, audio sample,
EEPROM image, or ROM-derived translated C is included in this repository or its releases.
