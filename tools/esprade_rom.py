"""
Name:        esprade_rom.py
Description: MAME 'esprade' ROM set (ESP Ra.De., World, 1998 4/22 International Ver.) and its regions
Version:     0.1.0
Created:     31-10-2026
Modified:    31-10-2026

Files are identified by CRC32 + SHA-1, never by name or by the folder inside the archive.
Region layouts follow ROM_START( esprade ) in MAME src/mame/atlus/cave.cpp
(BSD-3-Clause, copyright holders Luca Elia, David Haywood and others).

This module describes data, not the game: no ROM bytes live here.
"""
import hashlib
import zipfile
import zlib
from pathlib import Path

SET_NAME = "esprade"
SET_TITLE = "ESP Ra.De. (World, 1998 4/22 International Ver.)"

# MAME name -> (crc32, sha1, size)
ROM_FILES = {
    "u42.int": ("3b510a73", "ab1666eb826cb4a71588d86831dd18a2ef1c2a33", 0x080000),
    "u41.int": ("97c1b649", "37a56b7b9662219a356aee3f4b5cbb774ac4950e", 0x080000),
    "esp_u63.u63": ("2f2fe92c", "9519e365248bcec8419786eabb16fe4aae299af5", 0x400000),
    "esp_u64.u64": ("491a3da4", "53549a2bd3edc7b5e73fb46e1421b156bb0c190f", 0x400000),
    "esp_u65.u65": ("06563efe", "94e72da1f542b4e0525b4b43994242816b43dbdc", 0x400000),
    "esp_u66.u66": ("7bbe4cfc", "e77d0ed7a11b5abca1df8a0eb20ac9360cf79e76", 0x400000),
    "esp_u54.u54": ("e7ca6936", "b7f5ab67071a1d9dd3d2c1cd2304d9cdad68850c", 0x400000),
    "esp_u55.u55": ("f53bd94f", "d0a74fb3d36fe522ef075e5ae44a9980da8abe2f", 0x400000),
    "esp_u52.u52": ("e7abe7b4", "e98da45497e1aaf0d6ab352ec3e43c7438ed792a", 0x400000),
    "esp_u53.u53": ("51a0f391", "8b7355cbad119f4e1add14e5cd5e343ec6706104", 0x400000),
    "esp_u51.u51": ("0b9b875c", "ef05447cd8565ae24bb71db42342724622ad1e3e", 0x400000),
    "esp_u19.u19": ("f54b1cab", "34d70bb5798de85d892c062001d9ac1d6604fd9f", 0x400000),
    "eeprom-esprade.bin": ("315fb546", "7f597107d1610fc286413e0e93c794c80c0c554f", 0x000080),
}

# The 68000 program image: 0x000000-0x0FFFFF, big endian (u42 = even bytes, u41 = odd bytes).
PROGRAM_AREAS = [(0x000000, 0x100000)]
PROGRAM_SIZE = 0x100000

# Where the board puts each region (MAME cave.cpp esprade_map).
WORK_RAM = (0x100000, 0x110000)
SPRITE_RAM = (0x400000, 0x410000)
LAYER_RAM = [(0x500000, 0x508000), (0x600000, 0x608000), (0x700000, 0x708000)]
PALETTE_RAM = (0xC00000, 0xC10000)


def read_files(source):
    """Return {base name: bytes} from a folder or a .zip.

    Archives that hold the set in a directory (``esprade/u41.int``) are accepted: only the base
    name is significant. Duplicate base names are an error; ROMs are matched by content anyway.
    """
    source = Path(source)
    contents = {}
    if source.is_file() and source.suffix.lower() == ".zip":
        with zipfile.ZipFile(source) as archive:
            for entry in archive.infolist():
                if entry.is_dir():
                    continue
                name = Path(entry.filename).name
                if name in contents:
                    raise SystemExit(f"{source}: two entries named {name}")
                contents[name] = archive.read(entry)
        return contents
    if source.is_dir():
        for path in sorted(source.iterdir()):
            if path.is_file() and path.suffix.lower() != ".zip":
                contents[path.name] = path.read_bytes()
        return contents
    raise SystemExit(f"{source} is not a ROM folder or .zip")


def identify_set(source):
    """Return ({mame_name: bytes}, [report lines]).

    Every file is matched by CRC32 and SHA-1, so renamed files and archives with a folder
    prefix still verify. A folder that holds no loose files is searched for a .zip that does.
    """
    found, report = _identify(source)
    if not found and Path(source).is_dir():
        for archive in sorted(Path(source).glob("*.zip")):
            found, report = _identify(archive)
            if found:
                break
    return found, report


def _identify(source):
    by_crc = {crc: name for name, (crc, _, _) in ROM_FILES.items()}
    found = {}
    report = []
    for original_name, data in read_files(source).items():
        crc = format(zlib.crc32(data) & 0xFFFFFFFF, "08x")
        mame_name = by_crc.get(crc)
        if mame_name and ROM_FILES[mame_name][1] == hashlib.sha1(data).hexdigest():
            found[mame_name] = data
            report.append(f"OK      {original_name:24} = {mame_name}")
        else:
            report.append(f"--      {original_name:24} not part of the set (crc {crc})")
    for mame_name in sorted(set(ROM_FILES) - set(found)):
        report.append(f"MISSING {mame_name}")
    return found, report


def load_set(source):
    """Like identify_set, but stops when a file is missing or a hash does not match."""
    found, report = identify_set(source)
    missing = sorted(set(ROM_FILES) - set(found))
    if missing:
        raise SystemExit("The 'esprade' ROM set is incomplete. Missing: " + ", ".join(missing)
                         + "\n" + "\n".join(report))
    for name, data in found.items():
        if len(data) != ROM_FILES[name][2]:
            raise SystemExit(f"{name}: {len(data)} bytes, the set has {ROM_FILES[name][2]}")
    return found


def interleave16(even, odd):
    """ROM_LOAD16_BYTE: even bytes from one chip, odd bytes from the other."""
    result = bytearray(len(even) * 2)
    result[0::2] = even
    result[1::2] = odd
    return bytes(result)


def load32_word_swap(first, second):
    """Two ROM_LOAD32_WORD_SWAP chips filling one 32-bit region.

    MAME's ROM_LOAD32_WORD_SWAP is ROM_GROUPWORD | ROM_REVERSE | ROM_SKIP(2): each 16-bit
    group of the file is byte-swapped and written four bytes apart. The chip at offset 0
    fills the first half of each 32-bit word, the chip at offset 2 the second half.
    """
    result = bytearray(len(first) * 2)
    for index in range(0, len(first), 2):
        word = index * 2
        result[word + 0] = first[index + 1]
        result[word + 1] = first[index + 0]
        result[word + 2] = second[index + 1]
        result[word + 3] = second[index + 0]
    return bytes(result)


def build_program(rom):
    """maincpu region (1 MB, big endian) as the 68000 sees it at 0x000000-0x0FFFFF."""
    return interleave16(rom["u42.int"], rom["u41.int"])


def build_sprites(rom):
    """sprites0 region (16 MB): the 013 sprite generator's 8bpp graphics."""
    return (load32_word_swap(rom["esp_u63.u63"], rom["esp_u64.u64"])
            + load32_word_swap(rom["esp_u65.u65"], rom["esp_u66.u66"]))


def build_layer(rom, index):
    """A tile layer's graphics (8 MB) in file byte order."""
    if index == 0:
        return rom["esp_u54.u54"] + rom["esp_u55.u55"]
    if index == 1:
        return rom["esp_u52.u52"] + rom["esp_u53.u53"]
    return rom["esp_u51.u51"]


def build_ymz(rom):
    """YMZ280B sample ROM (4 MB)."""
    return rom["esp_u19.u19"]


def build_eeprom(rom):
    """93C46 serial EEPROM image (128 bytes)."""
    return rom["eeprom-esprade.bin"]


def canonical_address(address):
    """Map a 68000 address to its offset in the program image, or None if it is not ROM."""
    address &= 0xFFFFFF
    return address if address < PROGRAM_SIZE else None


class ProgramMemory:
    """68000 view of the program ROM for the analysis: big endian, anything outside reads 0xFF."""

    def __init__(self, image):
        self.image = image

    def r8(self, address):
        offset = canonical_address(address)
        return self.image[offset] if offset is not None else 0xFF

    def r16(self, address):
        return (self.r8(address) << 8) | self.r8(address + 1)

    def r32(self, address):
        return (self.r16(address) << 16) | self.r16(address + 2)
