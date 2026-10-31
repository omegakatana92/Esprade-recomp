import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as r

img = r.build_program(r.load_set(sys.argv[1]))
start = int(sys.argv[2], 0)
end = int(sys.argv[3], 0)
for a in range(start, end, 2):
    print(f"{a:06X} {struct.unpack_from('>H', img, a)[0]:04X}")
