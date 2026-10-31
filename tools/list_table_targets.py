import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as rom
import seed_symbols as seed
from m68000 import disassembler as dis

image = rom.build_program(rom.load_set(sys.argv[1]))
mem = dis.FlatMemory(image)
for target in sorted(seed.table_targets(mem)):
    if int(sys.argv[2], 0) <= target <= int(sys.argv[3], 0):
        print(f"target {target:06X}")
