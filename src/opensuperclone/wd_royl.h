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
#define WD_ROYL_RAM_MAX_SIZE 0x02000000ULL
#define WD_ROYL_RAM_BLOCK_SIZE 0x00010000ULL

#define WD_ROYL_MODULE_01 0x0001
#define WD_ROYL_MODULE_02 0x0002
#define WD_ROYL_MODULE_32 0x0032

#define WD_ROYL_MODULE_MASK_02 0x0001
#define WD_ROYL_MODULE_MASK_32 0x0002

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

// Called from the vendor operations so the GUI can update its progress
// display. done and total are in bytes (or another operation specific unit).
typedef void (*wd_royl_progress_callback_ccc)(const char *stage,
                                              unsigned long long done,
                                              unsigned long long total);

extern char wd_royl_error_ccc[WD_ROYL_ERROR_SIZE];

// Human readable description of the last failed operation.
const char *wd_royl_last_error_ccc(void);

// Low level transport. The register array uses the ATA28 order:
// features, count, lba low, lba mid, lba high, device, command.
// data_size is the size of the PIO data transfer (0 for no data).
// Returns 0 on success and -1 on failure.
int wd_royl_send_ata28_ccc(const uint8_t regs[7], void *data,
                           size_t data_size, int direction);

// Vendor specific command mode.
int wd_royl_enable_vsc_ccc(void);
int wd_royl_disable_vsc_ccc(void);

// ROM access.
int wd_royl_read_data_table_ccc(uint8_t table[WD_ROYL_SECTOR_SIZE]);
int wd_royl_get_rom_size_ccc(size_t *rom_size);
int wd_royl_read_rom_ccc(uint8_t **rom, size_t *rom_size,
                         wd_royl_progress_callback_ccc progress);
int wd_royl_write_rom_ccc(const uint8_t *rom, size_t rom_size,
                          bool slow_and_stupid,
                          wd_royl_progress_callback_ccc progress);
int wd_royl_verify_rom_ccc(const uint8_t *rom, size_t rom_size,
                           wd_royl_progress_callback_ccc progress);
int wd_royl_validate_rom_ccc(const uint8_t *rom, size_t rom_size);

// Service area module access.
int wd_royl_read_module_ccc(uint16_t module, uint8_t **data, size_t *size,
                            wd_royl_progress_callback_ccc progress);
int wd_royl_write_module_ccc(uint16_t module, const uint8_t *data, size_t size,
                             wd_royl_progress_callback_ccc progress);
int wd_royl_verify_module_ccc(uint16_t module, const uint8_t *data, size_t size,
                              wd_royl_progress_callback_ccc progress);

// ROYL header helpers.
uint16_t wd_royl_get_module_id_ccc(const uint8_t *module);
uint16_t wd_royl_get_module_sector_count_ccc(const uint8_t *module);
uint16_t wd_royl_get_module_record_count_ccc(const uint8_t *module);
uint16_t wd_royl_get_module_table_offset_ccc(const uint8_t *module);
uint32_t wd_royl_get_module_checksum_ccc(const uint8_t *module);
uint32_t wd_royl_calculate_checksum_ccc(const uint8_t *data, size_t size);
void wd_royl_update_checksum_ccc(uint8_t *data, size_t size);
int wd_royl_validate_module_header_ccc(const uint8_t *data, size_t size,
                                       uint16_t expected_module);
int wd_royl_validate_module_ccc(const uint8_t *data, size_t size,
                                uint16_t expected_module);
int wd_royl_get_record_ccc(const uint8_t *data, size_t size,
                           unsigned int record_number, uint16_t *record_offset,
                           uint16_t *record_size);
int wd_royl_parse_module_directory_ccc(
    const uint8_t *data, size_t size, wd_royl_module_entry_ccc **entries,
    size_t *entry_count);

// Slow fix patch algorithms. The checksum is updated in place.
int wd_royl_patch_module_02_ccc(uint8_t *data, size_t size);
int wd_royl_patch_module_32_ccc(uint8_t *data, size_t size);

// RAM access.
int wd_royl_find_ram_size_ccc(uint64_t *ram_size,
                              wd_royl_progress_callback_ccc progress);
int wd_royl_read_ram_ccc(uint64_t address, void *data, size_t size,
                         wd_royl_progress_callback_ccc progress);
int wd_royl_write_ram_ccc(uint64_t address, const void *data, size_t size,
                          wd_royl_progress_callback_ccc progress);

// Apply the selected slow fix patches to every matching module in drive RAM.
// The patch is temporary and is lost when the drive is power cycled.
int wd_royl_patch_ram_modules_ccc(unsigned int module_mask,
                                  unsigned int *module_02_count,
                                  unsigned int *module_32_count,
                                  wd_royl_progress_callback_ccc progress);

// Small file helpers used by the GUI and useful for future module editors.
int wd_royl_read_file_ccc(const char *filename, uint8_t **data, size_t *size);
int wd_royl_write_file_ccc(const char *filename, const uint8_t *data,
                           size_t size);

#endif
