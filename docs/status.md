# Status and known limitations

Updated: 2026-10-31

## Verified

| Check | Command | Result |
| --- | --- | --- |
| ROM integrity | `python tools/verify_rom.py C:\Users\omega\Downloads\esprade.zip` | 13/13 files, CRC32 + SHA-1 |
| Static analysis | `python tools/disassemble.py ...` | ~48,300 instructions, 575 functions, no invalid paths |
| Static translation | `python tools/recompile.py ...` | ~49,050 instructions, 0 unsupported |
| Headless boot | `build\cmake\esprade_headless.exe --rom ... --frames 600` | exit 0, 0 bad reads/writes |
| Video | `--png build\game7300.png 7300` | correct title screen; in-game street scene at frame 7300 |
| Coin/start | `--coin 2400 --start 2600` | how-to-play screen, then in-game scene |
| Windows build | `cmake --build build\cmake` | `esprade_headless.exe`, `esprade.exe` |
| SDL2 window | not yet run interactively | — |
| Audio | not implemented | — |
| AppImage | not yet built | — |

## Works

- The exact World `esprade` set is identified by content, not names.
- Program region reconstructed from `u42.int` (even) and `u41.int` (odd).
- 68000 exceptions, VBLANK interrupts, the return-address stack and the 24-bit bus.
- 038 tilemaps (8x8 and 16x16 modes), 013 8bpp sprites, xGRB_555 palette and priority.
- Coin/start input and the 93C46 EEPROM.
- Portable C11 runtime built on Windows with clang; Linux build scripts included.

## Not yet working

- **Coverage tail**: some computed handler-table entries are still outside the static
  analysis. The run currently stops around frame 7394 and the ports resume at the address
  left in `cpu.pc`; the next missing target is reported. `ESPRADE_TRACE_MISSING=1` is the
  discovery mode used to seed them.
- **Audio**: the YMZ280B register interface exists, but the ADPCM/PCM mixer is silent.
- **Interactive validation**: the SDL2 window has not been run on this machine yet.
- **Steam Deck**: no AppImage has been produced or tested.
- **MAME frame comparison**: no reference trace has been recorded yet.

## Next

1. Finish the handler-table coverage loop until 12,000+ frames run without a missing
   target.
2. Implement the YMZ280B mixer and record a WAV.
3. Run the SDL2 port interactively and validate keyboard/gamepad and fullscreen.
4. Build and audit the Linux AppImage on a Linux x86_64 host.
5. Record a MAME reference trace for frame-by-frame comparison.
