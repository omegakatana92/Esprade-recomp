import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as r

img = r.build_program(r.load_set(sys.argv[1]))
target = int(sys.argv[2], 0)
for address in range(0x400, len(img) - 8, 2):
    opcode = struct.unpack_from(">H", img, address)[0]
    if opcode not in (0x4EFB, 0x4EBB):
        continue
    ext = struct.unpack_from(">H", img, address + 2)[0]
    disp = ext & 0xFF
    if disp & 0x80:
        disp -= 0x100
    base = (address + 2 + disp) & 0xFFFFFF
    for index in range(64):
        entry = base + index * 4
        if (entry & 0xFFFFFF) == (target & 0xFFFFFF):
            print(f"table at {address:06X} ext={ext:04X} base={base:06X} index={index}")
