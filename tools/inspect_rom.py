import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as r

source = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\omega\Downloads\esprade.zip"
found = r.load_set(source)
img = r.build_program(found)
print("program", len(img), "sha1", __import__("hashlib").sha1(img).hexdigest())
print("SSP", hex(struct.unpack_from(">I", img, 0)[0]))
for vector in range(1, 48):
    print(f"vector {vector:3} {struct.unpack_from('>I', img, 4 * vector)[0]:08X}")
print("sprites", len(r.build_sprites(found)))
print("layers", [len(r.build_layer(found, i)) for i in range(3)])
print("ymz", len(r.build_ymz(found)))
print("eeprom", len(r.build_eeprom(found)))
