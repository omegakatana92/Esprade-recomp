#!/usr/bin/env bash
# Package the Linux x86_64 build as a Steam Deck AppImage.
#
# Run on a Linux x86_64 host with the ROM available:
#   ROM=/path/to/esprade.zip ./scripts/package_appimage.sh
#
# The resulting AppImage contains only this project's binary, SDL2 and the licenses.
# It never contains the ROM.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$ROOT/build/cmake}"
APPDIR="$ROOT/build/AppDir"
VERSION="${VERSION:-0.2.0}"

cd "$ROOT"
"$ROOT/scripts/build_linux.sh"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/share/licenses/esprade-recomp"

cp "$BUILD/esprade" "$APPDIR/usr/bin/esprade"
cp "$BUILD/esprade_headless" "$APPDIR/usr/bin/esprade_headless"
strip "$APPDIR/usr/bin/esprade" || true
strip "$APPDIR/usr/bin/esprade_headless" || true

# Bundle the SDL2 shared library and its dependencies that are not guaranteed on SteamOS.
if command -v ldd >/dev/null 2>&1; then
    ldd "$APPDIR/usr/bin/esprade" | awk '/=> \// {print $3}' | while read -r lib; do
        case "$(basename "$lib")" in
            libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libgcc_s.so*|ld-linux*) ;;
            *) cp -n "$lib" "$APPDIR/usr/lib/" || true ;;
        esac
    done
fi

cat > "$APPDIR/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH:-}"
# The cabinet is vertical; start fullscreen. Pass --windowed to override.
exec "$HERE/usr/bin/esprade" --fullscreen "$@"
EOF
chmod +x "$APPDIR/AppRun"

cat > "$APPDIR/esprade.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=ESP Ra.De. Native
Comment=Native recompilation of ESP Ra.De. (requires your own ROM)
Exec=esprade
Icon=esprade
Categories=Game;ArcadeGame;
Terminal=false
EOF
cp "$APPDIR/esprade.desktop" "$APPDIR/usr/share/applications/"

# A plain 256x256 icon, generated without game art.
python3 - "$APPDIR/usr/share/icons/hicolor/256x256/apps/esprade.png" <<'PY'
import struct, sys, zlib
path = sys.argv[1]
w = h = 256
rows = bytearray()
for y in range(h):
    rows.append(0)
    for x in range(w):
        rows += bytes((24, 24, 32)) if (x // 8 + y // 8) % 2 else bytes((40, 40, 64))
def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
open(path, "wb").write(png)
PY
ln -sf usr/share/icons/hicolor/256x256/apps/esprade.png "$APPDIR/esprade.png"

cp "$ROOT/LICENSE" "$APPDIR/usr/share/licenses/esprade-recomp/LICENSE"
cp "$ROOT/THIRD_PARTY.md" "$APPDIR/usr/share/licenses/esprade-recomp/"
cp "$ROOT/LICENSES/"* "$APPDIR/usr/share/licenses/esprade-recomp/"

TOOL="$ROOT/build/appimagetool-x86_64.AppImage"
if [[ ! -x "$TOOL" ]]; then
    curl -L -o "$TOOL" "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
    chmod +x "$TOOL"
fi

OUT="$ROOT/build/ESPRaDe-SteamDeck-x86_64.AppImage"
ARCH=x86_64 "$TOOL" --appimage-extract-and-run "$APPDIR" "$OUT" 2>/dev/null || \
    ARCH=x86_64 "$TOOL" "$APPDIR" "$OUT"
chmod +x "$OUT"
echo "AppImage: $OUT"
echo "ROM is NOT bundled; place esprade.zip next to the AppImage or pass --rom."
