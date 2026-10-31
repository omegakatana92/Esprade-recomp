/**
 *    Name:        machine.c
 *    Description: the Cave 68000 board as ESP Ra.De. wires it: bus, RAM, video registers, inputs,
 *                 EEPROM, and the player's ROM set loaded and verified at run time
 *    Version:     0.1.0
 *    Created:     31-10-2026
 *    Modified:    31-10-2026
 *
 *    The memory map follows MAME's esprade_map (src/mame/atlus/cave.cpp, BSD-3-Clause).
 */
#include "cave/cave.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cave/sha1.h"
#include "miniz.h"
#include "m68000/cpu.h"
#include "m68000/ops.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

cave_machine *cave_current;

typedef struct
{
	const char *name;
	const char *crc;
	const char *sha1;
	size_t size;
} rom_expectation;

static const rom_expectation ROM_FILES[] =
{
	{ "u42.int", "3b510a73", "ab1666eb826cb4a71588d86831dd18a2ef1c2a33", 0x080000 },
	{ "u41.int", "97c1b649", "37a56b7b9662219a356aee3f4b5cbb774ac4950e", 0x080000 },
	{ "esp_u63.u63", "2f2fe92c", "9519e365248bcec8419786eabb16fe4aae299af5", 0x400000 },
	{ "esp_u64.u64", "491a3da4", "53549a2bd3edc7b5e73fb46e1421b156bb0c190f", 0x400000 },
	{ "esp_u65.u65", "06563efe", "94e72da1f542b4e0525b4b43994242816b43dbdc", 0x400000 },
	{ "esp_u66.u66", "7bbe4cfc", "e77d0ed7a11b5abca1df8a0eb20ac9360cf79e76", 0x400000 },
	{ "esp_u54.u54", "e7ca6936", "b7f5ab67071a1d9dd3d2c1cd2304d9cdad68850c", 0x400000 },
	{ "esp_u55.u55", "f53bd94f", "d0a74fb3d36fe522ef075e5ae44a9980da8abe2f", 0x400000 },
	{ "esp_u52.u52", "e7abe7b4", "e98da45497e1aaf0d6ab352ec3e43c7438ed792a", 0x400000 },
	{ "esp_u53.u53", "51a0f391", "8b7355cbad119f4e1add14e5cd5e343ec6706104", 0x400000 },
	{ "esp_u51.u51", "0b9b875c", "ef05447cd8565ae24bb71db42342724622ad1e3e", 0x400000 },
	{ "esp_u19.u19", "f54b1cab", "34d70bb5798de85d892c062001d9ac1d6604fd9f", 0x400000 },
	{ "eeprom-esprade.bin", "315fb546", "7f597107d1610fc286413e0e93c794c80c0c554f", 0x000080 },
};

#define ROM_FILE_COUNT ((int)(sizeof(ROM_FILES) / sizeof(ROM_FILES[0])))

static void set_message(char *message, size_t size, const char *format, ...)
{
	if (!message || size == 0)
		return;
	va_list arguments;
	va_start(arguments, format);
	vsnprintf(message, size, format, arguments);
	va_end(arguments);
}

static uint16_t get16(const uint8_t *data, size_t offset)
{
	return (uint16_t)(((uint16_t)data[offset] << 8) | data[offset + 1]);
}

static void put16(uint8_t *data, size_t offset, uint16_t value, uint16_t mask)
{
	if (mask & 0xFF00u)
		data[offset] = (uint8_t)(value >> 8);
	if (mask & 0x00FFu)
		data[offset + 1] = (uint8_t)value;
}

static uint16_t ram_read16(const uint8_t *data, size_t size, uint32_t offset)
{
	return get16(data, offset & (size - 1));
}

static void ram_write16(uint8_t *data, size_t size, uint32_t offset, uint16_t value, uint16_t mask)
{
	put16(data, offset & (size - 1), value, mask);
}

cave_machine *cave_machine_create(void)
{
	cave_machine *machine = (cave_machine *)calloc(1, sizeof(cave_machine));
	if (!machine)
		return NULL;
	machine->frame_limit = 0;
	machine->escape_code = 0;
	machine->input0 = 0;
	machine->input1 = 0;
	eeprom_reset(machine);
	ymz280b_reset(machine);
	cave_current = machine;
	return machine;
}

void cave_machine_destroy(cave_machine *machine)
{
	if (!machine)
		return;
	free(machine->program);
	free(machine->sprites);
	free(machine->sprite_pixels);
	for (int layer = 0; layer < DDP_LAYER_COUNT; layer++)
	{
		free(machine->layer_gfx[layer]);
		free(machine->layer_pixels[layer]);
		free(machine->layer_pen[layer]);
		free(machine->layer_color[layer]);
		free(machine->layer_cat[layer]);
	}
	free(machine->ymz_rom);
	free(machine);
	if (cave_current == machine)
		cave_current = NULL;
}

void cave_set_inputs(cave_machine *machine, uint16_t player1, uint16_t player2)
{
	machine->input0 = player1;
	machine->input1 = player2;
}

void cave_insert_coin(cave_machine *machine, int player)
{
	(void)player;
	machine->coin[0] = 6;
}

int cave_interrupt_level(void) { return cave_current ? cave_current->irq_level : 0; }

void cave_interrupt_acknowledge(int level)
{
	(void)level;
	if (cave_current)
		cave_current->irq_level = 0;
}

/* ---- the bus ---- */

uint16_t cave_read16(uint32_t address)
{
	cave_machine *m = cave_current;
	address &= 0xFFFFFFu;
	if (!m)
		return 0;

	if (address < DDP_PROGRAM_SIZE)
		return get16(m->program, address & ~1u);

	if (address >= 0x100000u && address < 0x110000u)
		return ram_read16(m->work_ram, DDP_WORK_RAM_SIZE, address - 0x100000u);

	if (address >= 0x300000u && address < 0x300004u)
		return (uint16_t)ymz280b_read(m);

	if (address >= 0x400000u && address < 0x410000u)
		return ram_read16(m->sprite_ram, DDP_SPRITE_SIZE, address - 0x400000u);

	if (address >= 0x500000u && address < 0x508000u)
		return ram_read16(m->layer_ram[0], DDP_LAYER_SIZE, address - 0x500000u);
	if (address >= 0x600000u && address < 0x608000u)
		return ram_read16(m->layer_ram[1], DDP_LAYER_SIZE, address - 0x600000u);
	if (address >= 0x700000u && address < 0x708000u)
		return ram_read16(m->layer_ram[2], DDP_LAYER_SIZE, address - 0x700000u);

	if (address >= 0x800000u && address < 0x800008u)
	{
		uint32_t offset = (address - 0x800000u) >> 1;
		uint16_t result = 3;
		if (m->vblank_pending)
			result ^= 1;
		if (m->sound_pending)
			result ^= 2;
		if (offset == 2)
			m->vblank_pending = 0;
		if (offset == 0)
		{
			result &= (uint16_t)~4u;
			if (!m->vblank_pending)
				result |= 4;
		}
		return result;
	}
	if (address >= 0x800000u && address < 0x801000u)
		return 0;                         /* the video registers are write-only / open bus */

	if (address >= 0x900000u && address < 0x900006u)
		return m->layer_regs[0][(address - 0x900000u) >> 1];
	if (address >= 0xA00000u && address < 0xA00006u)
		return m->layer_regs[1][(address - 0xA00000u) >> 1];
	if (address >= 0xB00000u && address < 0xB00006u)
		return m->layer_regs[2][(address - 0xB00000u) >> 1];

	if (address >= 0xC00000u && address < 0xC10000u)
		return m->palette[(address - 0xC00000u) >> 1];

	if (address >= 0xD00000u && address < 0xD00002u)
	{
		uint16_t value = (uint16_t)~m->input0;
		return value;
	}
	if (address >= 0xD00002u && address < 0xD00004u)
	{
		uint16_t value = (uint16_t)~m->input1;
		value = (uint16_t)((value & ~0x0800u) | (eeprom_do_read(m) ? 0x0800u : 0));
		return value;
	}

	m->bad_reads++;
	m->last_bad_address = address;
	if (getenv("ESPRADE_DEBUG_BUS"))
		fprintf(stderr, "bad read  %06X pc=%06X sp=%08X\n", address, cpu.pc & 0xFFFFFFu, cpu.r[15]);
	return 0;
}

void cave_write16(uint32_t address, uint16_t value, uint16_t mem_mask)
{
	cave_machine *m = cave_current;
	address &= 0xFFFFFFu;
	if (!m)
		return;

	if (address < DDP_PROGRAM_SIZE)
		return;                           /* ROM */

	if (address >= 0x100000u && address < 0x110000u)
	{
		ram_write16(m->work_ram, DDP_WORK_RAM_SIZE, address - 0x100000u, value, mem_mask);
		return;
	}

	if (address >= 0x300000u && address < 0x300004u)
	{
		ymz280b_write(m, (uint8_t)(((address - 0x300000u) >> 1) & 1), (uint8_t)(value & 0xFFu));
		return;
	}

	if (address >= 0x400000u && address < 0x410000u)
	{
		ram_write16(m->sprite_ram, DDP_SPRITE_SIZE, address - 0x400000u, value, mem_mask);
		return;
	}

	if (address >= 0x500000u && address < 0x508000u)
	{
		ram_write16(m->layer_ram[0], DDP_LAYER_SIZE, address - 0x500000u, value, mem_mask);
		return;
	}
	if (address >= 0x600000u && address < 0x608000u)
	{
		ram_write16(m->layer_ram[1], DDP_LAYER_SIZE, address - 0x600000u, value, mem_mask);
		return;
	}
	if (address >= 0x700000u && address < 0x708000u)
	{
		ram_write16(m->layer_ram[2], DDP_LAYER_SIZE, address - 0x700000u, value, mem_mask);
		return;
	}

	if (address >= 0x800000u && address < 0x800080u)
	{
		uint32_t offset = (address - 0x800000u) >> 1;
		uint16_t old = m->video_regs[offset];
		if (mem_mask & 0xFF00u) old = (uint16_t)((old & 0x00FFu) | (value & 0xFF00u));
		if (mem_mask & 0x00FFu) old = (uint16_t)((old & 0xFF00u) | (value & 0x00FFu));
		m->video_regs[offset] = old;
		return;
	}
	if (address >= 0x800080u && address < 0x801000u)
		return;                           /* unused video register space */

	if (address >= 0x900000u && address < 0x900006u)
	{
		uint32_t offset = (address - 0x900000u) >> 1;
		uint16_t old = m->layer_regs[0][offset];
		if (mem_mask & 0xFF00u) old = (uint16_t)((old & 0x00FFu) | (value & 0xFF00u));
		if (mem_mask & 0x00FFu) old = (uint16_t)((old & 0xFF00u) | (value & 0x00FFu));
		m->layer_regs[0][offset] = old;
		return;
	}
	if (address >= 0xA00000u && address < 0xA00006u)
	{
		uint32_t offset = (address - 0xA00000u) >> 1;
		uint16_t old = m->layer_regs[1][offset];
		if (mem_mask & 0xFF00u) old = (uint16_t)((old & 0x00FFu) | (value & 0xFF00u));
		if (mem_mask & 0x00FFu) old = (uint16_t)((old & 0xFF00u) | (value & 0x00FFu));
		m->layer_regs[1][offset] = old;
		return;
	}
	if (address >= 0xB00000u && address < 0xB00006u)
	{
		uint32_t offset = (address - 0xB00000u) >> 1;
		uint16_t old = m->layer_regs[2][offset];
		if (mem_mask & 0xFF00u) old = (uint16_t)((old & 0x00FFu) | (value & 0xFF00u));
		if (mem_mask & 0x00FFu) old = (uint16_t)((old & 0xFF00u) | (value & 0x00FFu));
		m->layer_regs[2][offset] = old;
		return;
	}

	if (address >= 0xC00000u && address < 0xC10000u)
	{
		uint32_t offset = (address - 0xC00000u) >> 1;
		uint16_t old = m->palette[offset];
		if (mem_mask & 0xFF00u) old = (uint16_t)((old & 0x00FFu) | (value & 0xFF00u));
		if (mem_mask & 0x00FFu) old = (uint16_t)((old & 0xFF00u) | (value & 0x00FFu));
		m->palette[offset] = old;
		return;
	}

	if (address == 0xE00000u)
	{
		eeprom_write(m, (uint8_t)((mem_mask & 0xFF00u) ? (value >> 8) : (value & 0xFFu)));
		return;
	}

	m->bad_writes++;
	m->last_bad_address = address;
	if (getenv("ESPRADE_DEBUG_BUS"))
		fprintf(stderr, "bad write %06X=%04X pc=%06X sp=%08X\n", address, value, cpu.pc & 0xFFFFFFu, cpu.r[15]);
}

uint32_t cave_read32(uint32_t address)
{
	return ((uint32_t)cave_read16(address) << 16) | cave_read16(address + 2);
}

void cave_write32(uint32_t address, uint32_t value, uint16_t mem_mask)
{
	cave_write16(address, (uint16_t)(value >> 16), mem_mask);
	cave_write16(address + 2, (uint16_t)value, mem_mask);
}

/* ---- the C the translator writes ---- */

uint8_t mem_read8(uint32_t address)
{
	uint16_t word = cave_read16(address & ~1u);
	return (address & 1u) ? (uint8_t)word : (uint8_t)(word >> 8);
}

uint16_t mem_read16(uint32_t address)
{
	return cave_read16(address);
}

uint32_t mem_read32(uint32_t address)
{
	return cave_read32(address);
}

void mem_write8(uint32_t address, uint8_t value)
{
	if (address & 1u)
		cave_write16(address & ~1u, value, 0x00FFu);
	else
		cave_write16(address & ~1u, (uint16_t)((uint16_t)value << 8), 0xFF00u);
}

void mem_write16(uint32_t address, uint16_t value)
{
	cave_write16(address, value, 0xFFFFu);
}

void mem_write32(uint32_t address, uint32_t value)
{
	cave_write16(address, (uint16_t)(value >> 16), 0xFFFFu);
	cave_write16(address + 2, (uint16_t)value, 0xFFFFu);
}

/* ---- ROM loading ---- */

typedef struct
{
	const rom_expectation *expectation;
	uint8_t *data;
} rom_loaded;

static int identify(rom_loaded *loaded, const char *name, uint8_t *data, size_t size,
                    const char *source_name, char *message, size_t message_size)
{
	char crc[9];
	char sha1[41];
	snprintf(crc, sizeof(crc), "%08lx", (unsigned long)(mz_crc32(MZ_CRC32_INIT, data, size) & 0xFFFFFFFFu));
	sha1_hex(data, size, sha1);
	for (int index = 0; index < ROM_FILE_COUNT; index++)
	{
		const rom_expectation *expectation = &ROM_FILES[index];
		if (loaded[index].data)
			continue;
		if (expectation->size == size && strcmp(expectation->crc, crc) == 0
			&& strcmp(expectation->sha1, sha1) == 0)
		{
			loaded[index].expectation = expectation;
			loaded[index].data = data;
			return 1;
		}
	}
	set_message(message, message_size, "%s: not part of the esprade set (crc %s)", source_name, crc);
	return 0;
}

static const char *base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	const char *backslash = strrchr(path, '\\');
	const char *result = slash ? slash + 1 : path;
	if (backslash && backslash + 1 > result)
		result = backslash + 1;
	return result;
}

static int load_zip(const char *path, rom_loaded *loaded, char *message, size_t message_size)
{
	mz_zip_archive archive;
	memset(&archive, 0, sizeof(archive));
	if (!mz_zip_reader_init_file(&archive, path, 0))
	{
		set_message(message, message_size, "%s: not a readable zip archive", path);
		return 0;
	}
	mz_uint count = mz_zip_reader_get_num_files(&archive);
	for (mz_uint index = 0; index < count; index++)
	{
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&archive, index, &stat))
			continue;
		if (stat.m_is_directory)
			continue;
		size_t size = (size_t)stat.m_uncomp_size;
		uint8_t *data = (uint8_t *)malloc(size ? size : 1);
		if (!data || !mz_zip_reader_extract_to_mem(&archive, index, data, size, 0))
		{
			free(data);
			mz_zip_reader_end(&archive);
			set_message(message, message_size, "%s: cannot extract %s", path, stat.m_filename);
			return 0;
		}
		char scratch[256];
		if (!identify(loaded, stat.m_filename, data, size, base_name(stat.m_filename), scratch, sizeof(scratch)))
			free(data);
	}
	mz_zip_reader_end(&archive);
	return 1;
}

static int load_file(const char *path, rom_loaded *loaded, char *message, size_t message_size)
{
	FILE *file = fopen(path, "rb");
	if (!file)
	{
		set_message(message, message_size, "%s: cannot open", path);
		return 0;
	}
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t *data = (uint8_t *)malloc((size_t)(size > 0 ? size : 1));
	if (!data)
	{
		fclose(file);
		set_message(message, message_size, "%s: out of memory", path);
		return 0;
	}
	if (size > 0 && fread(data, 1, (size_t)size, file) != (size_t)size)
	{
		free(data);
		fclose(file);
		set_message(message, message_size, "%s: cannot read", path);
		return 0;
	}
	fclose(file);
	char scratch[256];
	if (!identify(loaded, path, data, (size_t)size, base_name(path), scratch, sizeof(scratch)))
		free(data);
	return 1;
}

static int load_folder(const char *path, rom_loaded *loaded, char *message, size_t message_size)
{
	int any = 0;
#ifdef _WIN32
	char pattern[1024];
	snprintf(pattern, sizeof(pattern), "%s\\*", path);
	WIN32_FIND_DATAA entry;
	HANDLE handle = FindFirstFileA(pattern, &entry);
	if (handle == INVALID_HANDLE_VALUE)
	{
		set_message(message, message_size, "%s: cannot list folder", path);
		return 0;
	}
	do
	{
		if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		char file[2048];
		snprintf(file, sizeof(file), "%s\\%s", path, entry.cFileName);
		if (!load_file(file, loaded, message, message_size))
			continue;
		any = 1;
	} while (FindNextFileA(handle, &entry));
	FindClose(handle);
#else
	DIR *directory = opendir(path);
	if (!directory)
	{
		set_message(message, message_size, "%s: cannot list folder", path);
		return 0;
	}
	struct dirent *entry;
	while ((entry = readdir(directory)) != NULL)
	{
		if (entry->d_name[0] == '.')
			continue;
		char file[2048];
		snprintf(file, sizeof(file), "%s/%s", path, entry->d_name);
		struct stat status;
		if (stat(file, &status) != 0 || !S_ISREG(status.st_mode))
			continue;
		if (!load_file(file, loaded, message, message_size))
			continue;
		any = 1;
	}
	closedir(directory);
#endif
	if (!any)
		set_message(message, message_size, "%s: no files of the esprade set", path);
	return any;
}

static void interleave_program(uint8_t *destination, const uint8_t *even, const uint8_t *odd, size_t size)
{
	for (size_t index = 0; index < size; index++)
	{
		destination[index * 2] = even[index];
		destination[index * 2 + 1] = odd[index];
	}
}

static void word_swap_region(uint8_t *destination, const uint8_t *first, const uint8_t *second, size_t size)
{
	for (size_t index = 0; index < size; index += 2)
	{
		size_t word = index * 2;
		destination[word + 0] = first[index + 1];
		destination[word + 1] = first[index + 0];
		destination[word + 2] = second[index + 1];
		destination[word + 3] = second[index + 0];
	}
}

static int build_regions(cave_machine *machine, rom_loaded *loaded, char *message, size_t message_size)
{
	for (int index = 0; index < ROM_FILE_COUNT; index++)
	{
		if (!loaded[index].data)
		{
			set_message(message, message_size, "ROM set is incomplete: %s is missing", ROM_FILES[index].name);
			return 0;
		}
	}
	const uint8_t *u42 = loaded[0].data, *u41 = loaded[1].data;
	const uint8_t *u63 = loaded[2].data, *u64 = loaded[3].data;
	const uint8_t *u65 = loaded[4].data, *u66 = loaded[5].data;
	const uint8_t *u54 = loaded[6].data, *u55 = loaded[7].data;
	const uint8_t *u52 = loaded[8].data, *u53 = loaded[9].data;
	const uint8_t *u51 = loaded[10].data, *u19 = loaded[11].data;
	const uint8_t *eeprom = loaded[12].data;

	machine->program = (uint8_t *)calloc(1, DDP_PROGRAM_SIZE);
	machine->sprites = (uint8_t *)calloc(1, 0x1000000);
	machine->layer_gfx[0] = (uint8_t *)calloc(1, 0x800000);
	machine->layer_gfx[1] = (uint8_t *)calloc(1, 0x800000);
	machine->layer_gfx[2] = (uint8_t *)calloc(1, 0x400000);
	machine->ymz_rom = (uint8_t *)calloc(1, 0x400000);
	if (!machine->program || !machine->sprites || !machine->layer_gfx[0] || !machine->layer_gfx[1]
		|| !machine->layer_gfx[2] || !machine->ymz_rom)
	{
		set_message(message, message_size, "out of memory for the ROM regions");
		return 0;
	}

	interleave_program(machine->program, u42, u41, 0x80000);
	word_swap_region(machine->sprites, u63, u64, 0x400000);
	word_swap_region(machine->sprites + 0x800000, u65, u66, 0x400000);
	memcpy(machine->layer_gfx[0], u54, 0x400000);
	memcpy(machine->layer_gfx[0] + 0x400000, u55, 0x400000);
	memcpy(machine->layer_gfx[1], u52, 0x400000);
	memcpy(machine->layer_gfx[1] + 0x400000, u53, 0x400000);
	memcpy(machine->layer_gfx[2], u51, 0x400000);
	memcpy(machine->ymz_rom, u19, 0x400000);
	memcpy(machine->eeprom, eeprom, DDP_EEPROM_SIZE);
	machine->layer_gfx_size[0] = machine->layer_gfx_size[1] = 0x800000;
	machine->layer_gfx_size[2] = 0x400000;
	machine->sprites_size = 0x1000000;
	machine->ymz_size = 0x400000;

	return 1;
}

static int load_from(cave_machine *machine, const char *path, int zip, char *message, size_t message_size)
{
	rom_loaded loaded[ROM_FILE_COUNT];
	memset(loaded, 0, sizeof(loaded));
	char scratch[512];
	int ok = zip ? load_zip(path, loaded, scratch, sizeof(scratch))
		: load_folder(path, loaded, scratch, sizeof(scratch));
	int result = ok && build_regions(machine, loaded, message, message_size);
	for (int index = 0; index < ROM_FILE_COUNT; index++)
		free(loaded[index].data);
	if (!result && (!message || !message[0]))
		set_message(message, message_size, "%s", scratch);
	if (result)
		cave_video_refresh(machine);
	return result;
}

int cave_machine_load_rom(cave_machine *machine, const char *path, char *message, size_t message_size)
{
	message[0] = 0;
	size_t length = strlen(path);
	if (length > 4 && (strcmp(path + length - 4, ".zip") == 0 || strcmp(path + length - 4, ".ZIP") == 0))
		return load_from(machine, path, 1, message, message_size);
	return load_from(machine, path, 0, message, message_size);
}

int cave_machine_load_folder(cave_machine *machine, const char *path, char *message, size_t message_size)
{
	return load_from(machine, path, 0, message, message_size);
}

void cave_machine_report(const cave_machine *machine)
{
	fprintf(stderr, "frames=%llu bad_reads=%llu bad_writes=%llu last_bad=%06X\n",
		(unsigned long long)machine->frame, (unsigned long long)machine->bad_reads,
		(unsigned long long)machine->bad_writes, machine->last_bad_address);
}
