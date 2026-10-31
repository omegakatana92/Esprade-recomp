import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import esprade_rom as rom
import seed_symbols as seed
from m68000 import disassembler as dis

image = rom.build_program(rom.load_set(sys.argv[1]))
mem = dis.FlatMemory(image)
wanted = int(sys.argv[2], 0)
for address in range(0x400, len(image) - 8, 2):
    opcode = mem.r16(address)
    if opcode not in (0x4EFB, 0x4EBB):
        continue
    extension = mem.r16(address + 2)
    displacement = extension & 0xFF
    if displacement & 0x80:
        displacement -= 0x100
    base = (address + 2 + displacement) & 0xFFFFFF
    for stride, allow in ((4, False), (2, True)):
        for index in range(16 if allow else 32):
            entry = (base + index * stride) & 0xFFFFFF
            if not 0x400 <= entry < len(image) - 4:
                break
            instruction = mem.r16(entry)
            target = None
            if (instruction & 0xFF00) == 0x6000:
                if instruction == 0x6000:
                    target = (entry + 2 + seed.signed16(mem.r16(entry + 2))) & 0xFFFFFF
                else:
                    target = (entry + 2 + seed.signed16(instruction & 0xFF)) & 0xFFFFFF
            elif instruction == 0x4EF9:
                target = mem.r32(entry + 2) & 0xFFFFFF
            elif instruction == 0x4EFA:
                target = (entry + 2 + seed.signed16(mem.r16(entry + 2))) & 0xFFFFFF
            if target == wanted:
                print(f"table {address:06X} ext={extension:04X} base={base:06X} stride={stride} index={index} entry={entry:06X} ({instruction:04X})")
