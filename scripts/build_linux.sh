#!/usr/bin/env bash
# Build the Linux x86_64 port.
#   ROM=/path/to/esprade.zip ./scripts/build_linux.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
ROM="${ROM:-$ROOT/rom/esprade.zip}"
BUILD="${BUILD:-$ROOT/build/cmake}"

if [[ ! -f "$ROM" ]]; then
    echo "ROM not found: $ROM (set ROM=/path/to/esprade.zip)" >&2
    exit 2
fi

python3 tools/verify_rom.py "$ROM"
python3 tools/seed_symbols.py "$ROM"
python3 tools/disassemble.py "$ROM"
python3 tools/recompile.py "$ROM"

cmake -S . -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD"

echo "Headless: $BUILD/esprade_headless"
echo "Windowed: $BUILD/esprade"
