# Tests

All commands run from the repository root. The ROM is the developer's own file and is
never committed.

## 1. ROM integrity

```powershell
python tools\verify_rom.py C:\Users\omega\Downloads\esprade.zip
```

Expected: 13 lines starting `OK`, then `13/13 files of the 'esprade' set`, exit 0.

## 2. Static analysis

```powershell
python tools\seed_symbols.py C:\Users\omega\Downloads\esprade.zip
python tools\seed_prologues.py C:\Users\omega\Downloads\esprade.zip   # once, or after a new ROM
python tools\disassemble.py C:\Users\omega\Downloads\esprade.zip
```

Expected: reset, main loop and frame wait are present; `build/listing/report.txt` has no
invalid/overlap section. The current build reports roughly 48k instructions and 575
functions.

## 3. Native translation

```powershell
python tools\recompile.py C:\Users\omega\Downloads\esprade.zip
```

Expected: `not translated: 0`, generated files under `build/generated/`.

## 4. Headless boot (CI gate)

```powershell
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip --frames 600 --hashes build\boot.hashes
```

Expected: exit 0, `bad=0/0`, no `no translated code` line.

## 5. Title screen snapshot

```powershell
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip --frames 3000 --png build\title.png 3000
```

Expected: `build/title.png` is 320x240; rotated 270 degrees it shows the ESP Ra.De. logo,
"PLEASE INSERT COIN" and the Atlus/Cave copyright.

## 6. Coin/start and in-game scene

```powershell
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip --frames 7400 --coin 2400 --start 2600 --png build\game7300.png 7300
```

Expected: frame 7300 is an in-game street scene. The run may stop at the next unseeded
handler after frame 7394; that is the coverage work in `docs/status.md`.

## 7. Missing-handler discovery

```powershell
$env:ESPRADE_TRACE_MISSING="1"
build\cmake\esprade_headless.exe --rom C:\Users\omega\Downloads\esprade.zip --frames 30000 --coin 2400 --start 2600
Remove-Item Env:\ESPRADE_TRACE_MISSING
```

Every `m68k: no translated code at XXXXXX` is a handler that still needs a `func` line in
`symbols.txt`. This is how the computed dispatch tables are covered.

## 8. Windows front end

```powershell
build\cmake\esprade.exe --rom C:\Users\omega\Downloads\esprade.zip
```

Check: window appears, title screen renders, F11 toggles fullscreen, Esc quits, keyboard
and gamepad move/coin/start work. This is manual until an automated harness exists.

## 9. Linux / AppImage

On Linux:

```bash
ROM=/path/to/esprade.zip ./scripts/package_appimage.sh
./build/ESPRaDe-SteamDeck-x86_64.AppImage --appimage-extract-and-run --rom /path/to/esprade.zip
```

Check the AppImage launches from Desktop and Gaming Mode, audio device opens, and the
save/config directory is writable (`$XDG_DATA_HOME` / SDL preference path).
