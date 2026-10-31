"""
Name:        game_profile.py
Description: what the shared arcade-decomp core (tools/core) needs to know of ESP Ra.De.
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

The core is the same for every game; this module gives it ESP Ra.De.'s ROM set, the names the
generated files carry, and the settings of Cave's 68000 board as wired for this game.  Nothing
here is game data: the ROM is the player's.
"""
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS / "core"))

import esprade_rom as rom  # noqa: E402,F401

ROOT = TOOLS.parent
GAME = "ESP Ra.De. (arcade, Cave/Atlus 1998)"
ROM_SET_TITLE = "ESP Ra.De. (esprade, World, 1998 4/22 International Ver.)"
CPU = "68000"
BINARY = "esprade"
GUARD = "ESPRADEI"
REFERENCE = "cave_ref"          # the MAME-based reference tool (tools/reference)
ANALYSIS = "m68000.analysis"
DISASSEMBLER = "m68000.disassembler"
DECOMPILER = "m68000.decompile"
TRANSLATOR = "m68000.translate"

CODE_START = 0x400             # exception vectors and their padding are not code
CODE_END = rom.PROGRAM_SIZE
WORK_RAM_AREA = rom.WORK_RAM
PROGRAM_AREA = (0x000000, rom.PROGRAM_SIZE)

# The board's devices, reached through the bus. The decompiler names pointers that land here by
# the area's name: the palette, the three tile maps, the sprite RAM and its registers, the YMZ280B
# and the inputs/EEPROM.
HARDWARE = [
    (0x300000, 0x300004, "YMZ280B"),
    (0x400000, 0x410000, "SPRITE_RAM"),
    (0x500000, 0x508000, "LAYER0_RAM"),
    (0x600000, 0x608000, "LAYER1_RAM"),
    (0x700000, 0x708000, "LAYER2_RAM"),
    (0x800000, 0x800080, "VIDEO_REGS"),
    (0x900000, 0x900006, "LAYER0_REGS"),
    (0xA00000, 0xA00006, "LAYER1_REGS"),
    (0xB00000, 0xB00006, "LAYER2_REGS"),
    (0xC00000, 0xC10000, "PALETTE_RAM"),
    (0xD00000, 0xD00004, "INPUTS"),
    (0xE00000, 0xE00001, "EEPROM"),
]
DEVICE_AREAS = [(start, end) for start, end, _ in HARDWARE]

# The main loop's idle point: the frame-wait routine at 04F35E clears 100F04 and waits here;
# the VBLANK handler (00061A -> 000646) sets it. The translator runs the board's next frame
# (and the interrupt it raises) after this instruction.
FRAME_WAIT = 0x4F382

# How many values a register may hold before the analysis stops following it.
ANALYSIS_MAX_VALUES = 128

NOTES = {
    "variables": ["/* the game's variables in work RAM (100000-10FFFF) */"],
    "scoped": ["/* work RAM as one family of functions uses it */"],
    "tables": ["/* the program's data the code reads by name, loaded from the player's assets */"],
    "hardware": ["/* Cave's board registers (read and written through the bus) */"],
}
