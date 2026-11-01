# Milestone 5 — Validation summary

Date: 2026-10-31

This file records what has actually been executed. It does not claim tests that were not
run.

## Passed

| Test | Environment | Result |
| --- | --- | --- |
| ROM CRC32/SHA-1 | Windows Python 3.14 | 13/13 files |
| Static analysis | Windows Python 3.14 | 50,629 instructions, 734 functions, no invalid paths |
| Translation | Windows Python 3.14 | 0 unsupported |
| Headless 600 frames | Windows clang, Linux gcc | exit 0, `bad=0/0` |
| Headless 60,000 frames + coin/start | Windows clang | exit 0, `bad=0/0`, hash `3fe0b6dc3322198a` |
| Headless 60,000 frames + coin/start | Ubuntu 22.04 gcc | exit 0, same hash `3fe0b6dc3322198a` |
| Title screen PNG | Windows headless | logo, coin prompt, copyright |
| In-game PNG | Windows headless | frame 7300 street scene |
| Ranking PNG | Windows headless | frame 30000 |
| Audio WAV | Windows headless | 44.1 kHz stereo, RMS ~3178, peak 32129 |
| Windows SDL launch | Windows 11 | window alive after 8 s, warning/title screen captured |
| Windows SDL coin/start | Windows 11 | sent 5 then 1 to the focused window; gameplay scene reached (1P started, "PUSH 2P START") |
| AppImage build | Ubuntu 22.04 WSL2 | 7.6 MB AppImage produced |
| AppImage headless | Ubuntu 22.04 WSL2 | 600 frames, `bad=0/0` |
| AppImage SDL launch | Xvfb | reached reset, alive until timeout |

## Not run

- Physical Steam Deck / SteamOS.
- Interactive keyboard/gamepad gameplay and fullscreen toggle on a real display.
- Frame/sample comparison against MAME.
- Service menu, two-player mode, endings.

## Known limitations

- Analysis covers 196,946 bytes of the 1 MiB program (18.8%). Anything not exercised by the
  trace loop may still be missing.
- The YMZ280B mixer is functional but not bit-exact against MAME/ymfm.
- Windows defaults to the SDL software renderer because the D3D9 accelerated path crashed
  in this environment; `--accelerated` is available for testing.
