# Packaging

## Windows x64

`scripts/build_windows.ps1` builds `esprade.exe` and `esprade_headless.exe` with SDL2.
The SDL2 development ZIP is downloaded under `build/downloads/` and `SDL2.dll` is copied
next to `esprade.exe`.

Release archive contents:

```
ESPRaDe-Windows-x64.exe     (renamed esprade.exe)
esprade_headless.exe
SDL2.dll
LICENSE
THIRD_PARTY.md
LICENSES/
README.md
```

No ROM is included. Place `esprade.zip` in `rom\` next to the executable or pass
`--rom <path>`.

## Linux x86_64 / Steam Deck AppImage

`scripts/package_appimage.sh`:

1. verifies the ROM and regenerates the translated C;
2. builds `esprade` and `esprade_headless`;
3. copies the binary, SDL2 and non-base shared libraries into an AppDir;
4. writes `AppRun`, a desktop file and a generated (non-game-art) icon;
5. runs `appimagetool` to produce `build/ESPRaDe-SteamDeck-x86_64.AppImage`.

The AppImage does not embed the ROM. On Steam Deck:

1. copy `esprade.zip` next to the AppImage, or keep it under
   `~/.local/share/ESPRaDe/rom/`;
2. `chmod +x ESPRaDe-SteamDeck-x86_64.AppImage`;
3. double-click in Desktop Mode, or add it as a non-Steam game in Gaming Mode.

The window is portrait (240x320 logical, scaled) and requests fullscreen with F11. SDL2
game controller mappings are used for the Deck controls.

## glibc / dependency audit

Build on Ubuntu 22.04 (glibc 2.35) or older for broad SteamOS compatibility. The AppImage
bundles SDL2 and any non-base libraries found by `ldd`; libc, libm, libdl and libpthread
are left to the host. Run:

```bash
ldd build/cmake/esprade
file build/ESPRaDe-SteamDeck-x86_64.AppImage
```

and record the output in the release notes.
