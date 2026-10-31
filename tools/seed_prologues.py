"""Seed function starts that the static analysis reaches through computed tables.

Two shapes found in ESP Ra.De.:

* ``48e7 fffe`` (movem.l D0-D7/A0-A6,-(A7)) full-register prologue;
* a zero padding word followed by ``224e`` (movea.l A6,A1), the start of an
  object/entity routine that the game reaches through a computed table.

Only addresses and names are written, never game data.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as rom

source = sys.argv[1]
image = rom.build_program(rom.load_set(source))
addresses = set()

pattern = bytes.fromhex("48e7fffe")
index = 0
while True:
    found = image.find(pattern, index)
    if found < 0:
        break
    if found % 2 == 0:
        addresses.add(found)
    index = found + 1

for address in range(0x402, len(image) - 2, 2):
    if image[address:address + 2] == b"\x22\x4e" and image[address - 2:address] == b"\x00\x00":
        addresses.add(address)

path = Path(__file__).resolve().parent.parent / "symbols.txt"
text = path.read_text(encoding="utf-8")
start = "# --- prologue-seeded functions ---"
end = "# --- end prologue-seeded functions ---"
kept, inside = [], False
for line in text.splitlines():
    if line.strip() == start:
        inside = True
        continue
    if line.strip() == end:
        inside = False
        continue
    if not inside:
        kept.append(line)
while kept and not kept[-1].strip():
    kept.pop()
lines = [start]
lines += [f"{address:06X}  func  auto_{address:06X}" for address in sorted(addresses)]
lines += [end, ""]
path.write_text("\n".join(kept + lines), encoding="utf-8")
print(f"{len(addresses)} function starts seeded")
