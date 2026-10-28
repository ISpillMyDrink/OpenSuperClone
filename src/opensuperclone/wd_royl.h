// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef WD_ROYL_H
#define WD_ROYL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WD_ROYL_SECTOR_SIZE 512
#define WD_ROYL_ROM_BLOCK_SIZE 16384
#define WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET 164
#define WD_ROYL_MAX_FILE_SIZE (16ULL * 1024ULL * 1024ULL)

#define WD_ROYL_RAM_BASE 0x08000000ULL
#define WD_ROYL_RAM_MIN_SIZE 0x00040000ULL
#define WD_ROYL_RAM_MAX_SIZE 0x04000000ULL
#define WD_ROYL_RAM_BLOCK_SIZE 0x00010000ULL

// The drive data table (VSC action 0x0D) reports the DRAM size in megabytes as
// a little endian 16 bit value at this offset. Using it avoids probing RAM
// addresses that may not be mapped, which can hang the drive on some families.
#define WD_ROYL_DATA_TABLE_DRAM_SIZE_OFFSET 182

#define WD_ROYL_MODULE_01 0x0001
#define WD_ROYL_MODULE_02 0x0002
#define WD_ROYL_MODULE_0A 0x000a
#define WD_ROYL_MODULE_32 0x0032
#define WD_ROYL_MODULE_0A_SA 0x0102

#define WD_ROYL_MODULE_MASK_02 0x0001
#define WD_ROYL_MODULE_MASK_32 0x0002

// Head map module 0x0A. The ROM copy and the SA shadow 0x0102 use the same
// payload offsets, but their module headers carry different module IDs.
#define WD_ROYL_HEAD_MAP_PHYSICAL_COUNT_OFFSET 0x21
#define WD_ROYL_HEAD_MAP_IN_USE_COUNT_OFFSET 0x22
// The 16-bit field at 0x3C is opaque vendor data. It does not match a standard
// checksum over the module on known dumps, so it is preserved byte for byte
// and reported rather than validated.
#define WD_ROYL_HEAD_MAP_VENDOR_FIELD_OFFSET 0x3c
#define WD_ROYL_HEAD_MAP_ACTIVE_MAP_OFFSET 0x3e
#define WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET 0x40
#define WD_ROYL_HEAD_MAP_MAX_HEADS 16

#define WD_ROYL_DIR_NONE 0
#define WD_ROYL_DIR_OUT 1
#define WD_ROYL_DIR_IN 2

#define WD_ROYL_ERROR_SIZE 512

// One module entry from the service area directory in module 0x0001. The
// directory lists the module ID, its size, the number of copies, and where
// those copies are stored in the service area.
typedef struct
{
  uint16_t module_id;
  uint16_t sectors;
  uint8_t copies;
  uint32_t location1;
  uint32_t location2;
} wd_royl_module_entry_ccc;

// Where a parsed head map came from. AUTO tries the direct ROM module read
// first, then a full ROM scan, then the service area shadow.
typedef enum
{
  WD_ROYL_HEAD_MAP_SOURCE_AUTO = 0,
  WD_ROYL_HEAD_MAP_SOURCE_ROM,
  WD_ROYL_HEAD_MAP_SOURCE_SA,
  WD_ROYL_HEAD_MAP_SOURCE_RAM
} wd_royl_head_map_source_ccc;

// Decoded module 0x0A head map. The module checksum is reported separately so
// a RAM copy that firmware has modified can still be displayed and repaired.
// vendor_field_3c is informational only and is never changed by build.
typedef struct
{
  bool valid;
  bool header_valid;
  bool module_checksum_valid;
  bool counts_match_maps;
  uint8_t physical_count;
  uint8_t in_use_count;
  uint16_t active_map;
  uint16_t physical_map;
  uint16_t vendor_field_3c;
} wd_royl_head_map_ccc;

// Called from the vendor operations so the GUI can update its progress
// display. done and total are in bytes (or another operation specific unit).
typedef void (*wd_royl_progress_callback_ccc)(const char *stage, unsigned long long done, unsigned long long total);

extern char wd_royl_error_ccc[WD_ROYL_ERROR_SIZE];

// Human readable description of the last failed operation.
const char *wd_royl_last_error_ccc(void);

// Low level transport. The register array uses the ATA28 order:
// features, count, lba low, lba mid, lba high, device, command.
// data_size is the size of the PIO data transfer (0 for no data).
// Returns 0 on success and -1 on failure.
int wd_royl_send_ata28_ccc(const uint8_t regs[7], void *data, size_t data_size, int direction);

// Vendor specific command mode.
int wd_royl_enable_vsc_ccc(void);
int wd_royl_disable_vsc_ccc(void);

// ROM access.
int wd_royl_read_data_table_ccc(uint8_t table[WD_ROYL_SECTOR_SIZE]);
int wd_royl_get_rom_size_ccc(size_t *rom_size);
int wd_royl_read_rom_ccc(uint8_t **rom, size_t *rom_size, wd_royl_progress_callback_ccc progress);
int wd_royl_write_rom_ccc(const uint8_t *rom, size_t rom_size, bool slow_and_stupid, wd_royl_progress_callback_ccc progress);
int wd_royl_verify_rom_ccc(const uint8_t *rom, size_t rom_size, wd_royl_progress_callback_ccc progress);
int wd_royl_validate_rom_ccc(const uint8_t *rom, size_t rom_size);

// Service area module access.
int wd_royl_read_module_ccc(uint16_t module, uint8_t **data, size_t *size, wd_royl_progress_callback_ccc progress);
int wd_royl_write_module_ccc(uint16_t module, const uint8_t *data, size_t size, wd_royl_progress_callback_ccc progress);
int wd_royl_verify_module_ccc(uint16_t module, const uint8_t *data, size_t size, wd_royl_progress_callback_ccc progress);

// ROYL header helpers.
uint16_t wd_royl_get_module_id_ccc(const uint8_t *module);
uint16_t wd_royl_get_module_sector_count_ccc(const uint8_t *module);
// The header word at 0x30 is module specific: module 0x0001 stores a record
// count there, while module 0x0032 stores the offset of its relo table. The
// accessors are not interchangeable even though both read the same word.
uint16_t wd_royl_get_module_record_count_ccc(const uint8_t *module);
uint16_t wd_royl_get_module_table_offset_ccc(const uint8_t *module);
uint32_t wd_royl_get_module_checksum_ccc(const uint8_t *module);
uint32_t wd_royl_calculate_checksum_ccc(const uint8_t *data, size_t size);
void wd_royl_update_checksum_ccc(uint8_t *data, size_t size);
int wd_royl_validate_module_header_ccc(const uint8_t *data, size_t size, uint16_t expected_module);
int wd_royl_validate_module_ccc(const uint8_t *data, size_t size, uint16_t expected_module);
int wd_royl_get_record_ccc(const uint8_t *data, size_t size, unsigned int record_number, uint16_t *record_offset, uint16_t *record_size);
int wd_royl_parse_module_directory_ccc(const uint8_t *data, size_t size, wd_royl_module_entry_ccc **entries, size_t *entry_count);

// Slow fix patch algorithms. The checksum is updated in place.
int wd_royl_patch_module_02_ccc(uint8_t *data, size_t size);
int wd_royl_patch_module_32_ccc(uint8_t *data, size_t size);

// Head map access. The parse and build helpers are pure and do not touch the
// drive. parse succeeds when the header and bitmaps are sane; checksum
// validity is reported in the struct and is not fatal. build updates 0x22 to
// the number of set bits in the active map, leaves the vendor field at 0x3C
// unchanged, and recomputes the module checksum.
int wd_royl_head_map_parse_ccc(const uint8_t *module, size_t size, uint16_t expected_module, wd_royl_head_map_ccc *map);
int wd_royl_head_map_build_ccc(uint8_t *module, size_t size, uint16_t expected_module, const wd_royl_head_map_ccc *map);
int wd_royl_find_module_in_buffer_ccc(const uint8_t *buffer, size_t size, uint16_t module_id, size_t *module_offset, size_t *module_size);
int wd_royl_find_rom_head_map_ccc(const uint8_t *rom, size_t rom_size, size_t *module_offset, size_t *module_size);

// Read a head map module. The returned buffer is allocated and must be freed
// by the caller. address is the ROM byte offset, the RAM address, or 0 for
// the service area shadow. actual_source may be NULL.
int wd_royl_read_head_map_ccc(wd_royl_head_map_source_ccc source, uint8_t **module, size_t *module_size, uint64_t *address,
                              wd_royl_head_map_source_ccc *actual_source, wd_royl_progress_callback_ccc progress);

// Apply a head map to every matching module 0x0A copy in drive RAM. Only the
// heads whose state differs between base_map and map are changed; every other
// head keeps the state already present in that live copy. This prevents a map
// read from ROM or the service area from re-enabling a head that was masked in
// RAM earlier. The change is temporary and is lost when the drive is power
// cycled. copy_count receives the number of copies patched. applied_active may
// be NULL; when given it receives the resulting active map of the preferred
// RAM copy after a successful patch.
int wd_royl_apply_head_map_ram_ccc(const wd_royl_head_map_ccc *base_map, const wd_royl_head_map_ccc *map,
                                   unsigned int *copy_count, uint16_t *applied_active,
                                   wd_royl_progress_callback_ccc progress);

// RAM access.
int wd_royl_find_ram_size_ccc(uint64_t *ram_size, wd_royl_progress_callback_ccc progress);
int wd_royl_read_ram_ccc(uint64_t address, void *data, size_t size, wd_royl_progress_callback_ccc progress);
int wd_royl_write_ram_ccc(uint64_t address, const void *data, size_t size, wd_royl_progress_callback_ccc progress);

// Apply the selected slow fix patches to every matching module in drive RAM.
// The patch is temporary and is lost when the drive is power cycled.
// `scanned_end` and `first_gap` may be NULL; when given they report how much of
// the RAM window was actually readable.
int wd_royl_patch_ram_modules_ccc(unsigned int module_mask, unsigned int *module_02_count,
                                  unsigned int *module_32_count, uint64_t *scanned_end, uint64_t *first_gap,
                                  wd_royl_progress_callback_ccc progress);

// Small file helpers used by the GUI and useful for future module editors.
int wd_royl_read_file_ccc(const char *filename, uint8_t **data, size_t *size);
int wd_royl_write_file_ccc(const char *filename, const uint8_t *data, size_t size);

#endif
