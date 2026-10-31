# Milestone 2 — Video and first gameplay scene

Date: 2026-10-31

## Verified

- 038 tile layers decode and render: the title screen shows the ESP Ra.De. logo,
  "A.D. 2018 TOKYO", high-score table, "PLEASE INSERT COIN", "CREDIT 0" and
  "© ATLUS/CAVE 1998".
- 013 sprites decode and render with the correct priority rules.
- Palette conversion is xGRB_555.
- Coin and start inputs work: coin at frame 2400 and start at frame 2600.
- At frame 7300 the game is in an in-game scene (street, characters, explosion,
  "PLEASE WAIT").
- Snapshot: `build/game7300.png` (320x240 native, rotated 270 for viewing).

## Commands

```powershell
python tools\seed_symbols.py C:\Users\omega\Downloads\esprade.zip
python tools\disassemble.py C:\Users\omega\Downloads\esprade.zip
python tools\recompile.py C:\Users\omega\Downloads\esprade.zip
cmake --build build\cmake
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip `
  --frames 12000 --coin 2400 --start 2600 --png build\game7300.png 7300
```

## Coverage blocker

At frame 7394 the translated top-level routine returns; the ports resume at the address
left in `cpu.pc` (the 68000 would do the same). Some computed handler-table entries are
still outside the static analysis, so the run currently stops at the next one. The
`ESPRADE_TRACE_MISSING=1` mode records those targets and the coverage loop seeds them into
`symbols.txt`.

Known remaining work:

- Finish seeding the entity/handler tables used after the how-to-play screen.
- Verify a full attract loop and several minutes of gameplay.
- YMZ280B mixer/audio.
- Steam Deck AppImage build and launch test.

## Rendering notes

- Layer layouts: `layout_8x8x8` (MAME `cave.cpp`), 512 bits per 8x8 character, planes
  ordered from the highest pen bit.
- Sprite layout: `layout_sprites_8bpp`, 16x1, 128 bits per character.
- Background pen `0x7F00`, layer colour base `0x4000`, sprite colour base `0`.
- Priority: tile category 0..3 then layer priority 0..3; sprites are drawn last and lose
  to tiles of their own priority (MAME `cave_v.cpp` `colpri_cb`).
