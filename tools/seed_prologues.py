import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as rom

source = sys.argv[1]
image = rom.build_program(rom.load_set(source))
pattern = bytes.fromhex("48e7fffe")
addresses = []
index = 0
while True:
    found = image.find(pattern, index)
    if found < 0:
        break
    if found % 2 == 0:
        addresses.append(found)
    index = found + 1

path = Path(__file__).resolve().parent.parent / "symbols.txt"
text = path.read_text(encoding="utf-8")
marker = "# --- prologue-seeded functions ---"
if marker in text:
    text = text.split(marker)[0].rstrip() + "\n"
lines = [marker]
lines += [f"{address:06X}  func  auto_{address:06X}" for address in addresses]
lines += ["# --- end prologue-seeded functions ---", ""]
path.write_text(text + "\n".join(lines), encoding="utf-8")
print(f"{len(addresses)} prologue functions seeded")
