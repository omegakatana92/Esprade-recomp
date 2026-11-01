# Milestone 1 — Native translation and headless boot

Date: 2026-10-31

## Verified

- `python tools/recompile.py` translates the whole analysed program to C:
  - 0 unsupported instructions;
  - generated `program.h`, `program_NN.c`, `entries.c` under `build/generated/`.
- The C runtime implements:
  - the 68000 registers, SR/flags and supervisor/user stack swap;
  - exceptions and the three-word frame (SR, PC high, PC low);
  - VBLANK and sound interrupts;
  - calls/jumps through computed addresses and the return-address stack;
  - the 24-bit bus and byte lanes.
- Board model: work RAM, sprite RAM, three layer RAMs, palette, video registers, layer
  control, inputs and EEPROM.
- ROM ZIP is verified by CRC32 + SHA-1 at run time; no ROM is embedded.

## Command and result

```powershell
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip --frames 600 --hashes build\boot.hashes
```

Result: exit 0, `bad=0/0`, no missing translated code, 600 frames.

## Bug found and fixed

The first exception-frame implementation pushed PC low, SR, PC high, but the 68000's
memory order from the new SP is **SR, PC high, PC low**. The wrong order corrupted A7 on
`RTE` and caused writes below the stack. Corrected before the 600-frame run.
