# Milestone 0 — Repository, ROM identification and static analysis

Date: 2026-10-31

## Repository

- Official remote: `https://github.com/omegakatana92/Esprade-recomp`.
- Existing `LICENSE` is GPL-2.0 and is preserved.
- `tools/core` is a pinned submodule of `cheviman/arcade-decomp` at commit
  `ed4e157b4bb3d34b6555766262c70d6f010ba619` (BSD-3-Clause).

## ROM identification

The local archive is `C:\Users\omega\Downloads\esprade.zip`. Matching is by CRC32 and
SHA-1, never by filename alone:

- 13/13 files match MAME's `esprade` World set.
- Program: `u42.int` (even bytes) + `u41.int` (odd bytes) = 1 MiB.
- Sprites: `esp_u63`–`esp_u66`, 16 MiB after `ROM_LOAD32_WORD_SWAP`.
- Layers: `esp_u54`+`esp_u55`, `esp_u52`+`esp_u53`, `esp_u51`.
- Samples: `esp_u19`, 4 MiB.
- EEPROM: `eeprom-esprade.bin`, 128 bytes.

## Program layout

- Reset SSP `0x110000`; reset PC `0x00595A`.
- Level-1 autovector handler `0x00061A`; sets `$100F04` at `0x000646`.
- Frame wait `0x04F35E`; idle instruction `0x04F382` (`tst.w $100F04`).
- Main loop near `0x0008E4`.

## Analysis

Initial analysis found 34,945 instructions / 386 functions from the vectors and the
`symbols.txt` jump seeds. Coverage grew to **50,629 instructions / 734 functions**
(196,946 bytes) after seeding the inline dispatch tables and runtime-reported handlers.
`build/listing/report.txt` has no unresolved, invalid or overlapping path section.

## Notes

- The static analysis does not follow PC-indexed dispatch tables; `tools/seed_symbols.py`
  and the runtime trace loop supply their handlers.
- Generated C stays under `build/generated/` and is git-ignored.
