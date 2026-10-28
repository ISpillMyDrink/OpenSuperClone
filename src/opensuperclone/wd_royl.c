// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "config.h"
#include "common.h"
#include "io.h"
#include "wd_royl.h"

#include <stdarg.h>

char wd_royl_error_ccc[WD_ROYL_ERROR_SIZE] = "";

#define WD_ROYL_ATA_CMD_D5 0xd5
#define WD_ROYL_ATA_CMD_D6 0xd6

// DRAM is mapped at different base addresses on different ROYL families. The
// base is detected from the drive and cached per disk handle.
static uint64_t wd_royl_ram_base_ccc = WD_ROYL_RAM_BASE;
static bool wd_royl_ram_base_known_ccc = false;
static int wd_royl_ram_base_fd_ccc = -1;

static void wd_royl_set_error_ccc(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vsnprintf(wd_royl_error_ccc, sizeof(wd_royl_error_ccc), format, args);
  va_end(args);
}

const char *wd_royl_last_error_ccc(void)
{
  return wd_royl_error_ccc;
}

static uint16_t wd_royl_le16_ccc(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t wd_royl_le32_ccc(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void wd_royl_put_le32_ccc(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
  data[2] = (uint8_t)(value >> 16);
  data[3] = (uint8_t)(value >> 24);
}

static void wd_royl_progress_ccc(wd_royl_progress_callback_ccc progress, const char *stage, unsigned long long done, unsigned long long total)
{
  if (progress != NULL)
  {
    progress(stage, done, total);
  }
}

static int wd_royl_get_disk_fd_ccc(void)
{
  if (current_disk_ccc > 0)
  {
    return current_disk_ccc;
  }
  if (disk1_fd_ccc > 0)
  {
    return disk1_fd_ccc;
  }
  return -1;
}

int wd_royl_send_ata28_ccc(const uint8_t regs[7], void *data, size_t data_size, int direction)
{
  int i;
  int result = -1;
  int ret = 0;
  int raw_ret = 0;
  unsigned char csw_status = 0;
  int disk_fd = -1;
  unsigned long long saved_buffer_size = ccc_main_buffer_size_ccc;

  if (regs == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no ATA registers supplied.\n");
    return -1;
  }
  if (direction != WD_ROYL_DIR_NONE && direction != WD_ROYL_DIR_IN && direction != WD_ROYL_DIR_OUT)
  {
    wd_royl_set_error_ccc("Internal error: invalid data direction %d.\n", direction);
    return -1;
  }
  if (direction == WD_ROYL_DIR_NONE && data_size != 0)
  {
    wd_royl_set_error_ccc("Internal error: no-data command with %zu bytes.\n", data_size);
    return -1;
  }
  if (direction != WD_ROYL_DIR_NONE && data == NULL)
  {
    wd_royl_set_error_ccc("Internal error: data command without a buffer.\n");
    return -1;
  }
  if (data_size > MAX_USB_BUFFER_SIZE)
  {
    wd_royl_set_error_ccc("Transfer size %zu is larger than the maximum.\n", data_size);
    return -1;
  }

  if (usb_mode_ccc)
  {
    if (!usb_allow_ata_ccc)
    {
      wd_royl_set_error_ccc("This operation requires ATA pass-through, not USB mass storage "
                            "mode.\nSelect USB ATA mode and choose the source drive again.\n");
      return -1;
    }
    if (usbd1_dev_handle_ccc == NULL)
    {
      wd_royl_set_error_ccc("No USB ATA device is open.\n");
      return -1;
    }
  }
  else
  {
    disk_fd = wd_royl_get_disk_fd_ccc();
    // Direct mode, including AHCI, talks to the drive through the driver or
    // port registers and does not use a disk file descriptor. A descriptor is
    // only required for ATA pass-through.
    if (direct_mode_ccc)
    {
      // The source drive is opened while it is being selected, but the port is
      // released again afterwards. Make sure the controller is actually
      // connected before touching AHCI registers or ISA I/O ports. Without
      // this check an unconnected source would dereference a stale or NULL
      // AHCI port pointer and crash the program.
      if (ahci_mode_ccc && port_virt_addr_ccc == NULL)
      {
        wd_royl_set_error_ccc("The source drive is not connected. Choose and connect the source "
                              "drive before using vendor specific commands.\n");
        return -1;
      }
      if (!ahci_mode_ccc && !io_permissions_ccc)
      {
        wd_royl_set_error_ccc("The source drive is not connected. Choose and connect the source "
                              "drive before using vendor specific commands.\n");
        return -1;
      }
    }
    else if (disk_fd < 0)
    {
      wd_royl_set_error_ccc("No source drive is open.\n");
      return -1;
    }
  }

  ccc_main_buffer_size_ccc = data_size;
  if (set_main_buffer_ccc() != 0)
  {
    wd_royl_set_error_ccc("Unable to set the data buffer size to %zu.\n", data_size);
    goto restore_buffer;
  }

  if (direction == WD_ROYL_DIR_OUT)
  {
    memcpy(ccc_buffer_ccc, data, data_size);
  }
  else if (direction == WD_ROYL_DIR_IN)
  {
    memset(ccc_buffer_ccc, 0, data_size);
  }

  passthrough_ccc.multiple_count = 0;
  passthrough_ccc.off_line = 0;
  passthrough_ccc.transfer_length = 2;
  passthrough_ccc.byte_block = 1;
  passthrough_ccc.check_condition = 1;

  for (i = 0; i < 7; i++)
  {
    passthrough_ccc.ata_cmd[i] = regs[i];
  }
  for (i = 7; i < 12; i++)
  {
    passthrough_ccc.ata_cmd[i] = 0;
  }

  if (direction == WD_ROYL_DIR_IN)
  {
    passthrough_ccc.protocol = 4;
    passthrough_ccc.direction = 2;
    passthrough_ccc.transfer_direction = 1;
    passthrough_ccc.sg_xfer_direction = SG_DXFER_FROM_DEV;
  }
  else if (direction == WD_ROYL_DIR_OUT)
  {
    passthrough_ccc.protocol = 5;
    passthrough_ccc.direction = 1;
    passthrough_ccc.transfer_direction = 0;
    passthrough_ccc.sg_xfer_direction = SG_DXFER_TO_DEV;
  }
  else
  {
    passthrough_ccc.protocol = 3;
    passthrough_ccc.direction = 0;
    passthrough_ccc.transfer_direction = 1;
    passthrough_ccc.sg_xfer_direction = SG_DXFER_NONE;
  }

  passthrough_ccc.scsi_cmd[0] = 0xa1;
  passthrough_ccc.scsi_cmd[1] = (unsigned char)((passthrough_ccc.multiple_count << 5) + (passthrough_ccc.protocol << 1));
  passthrough_ccc.scsi_cmd[2] =
    (unsigned char)((passthrough_ccc.off_line << 6) + (passthrough_ccc.check_condition << 5) + (passthrough_ccc.transfer_direction << 3) +
                    (passthrough_ccc.byte_block << 2) + passthrough_ccc.transfer_length);
  for (i = 0; i < 7; i++)
  {
    passthrough_ccc.scsi_cmd[3 + i] = regs[i];
  }
  passthrough_ccc.scsi_cmd[10] = 0;
  passthrough_ccc.scsi_cmd[11] = 0;
  command_length_ccc = 12;

  if (usb_mode_ccc)
  {
    if (direction == WD_ROYL_DIR_OUT)
    {
      usb_transfer_direction_ccc = USB_DIRECTION_OUT;
    }
    else
    {
      usb_transfer_direction_ccc = USB_DIRECTION_IN;
    }

    raw_ret = do_usb_cmd_ccc();
    csw_status = usb_csw_status_ccc;

    // Only ask for sense data when something went wrong. Some bridges do not
    // answer Request Sense after an otherwise successful ATA command, and the
    // extra round trip makes large module operations much slower.
    if (raw_ret < 0 || (direction == WD_ROYL_DIR_IN && raw_ret != (int)data_size) ||
        (direction == WD_ROYL_DIR_OUT && raw_ret != 0 && raw_ret != (int)data_size) || !usb_csw_valid_ccc || csw_status != 0)
    {
      (void)usb_get_sense_ccc();
    }

    if (raw_ret < 0)
    {
      wd_royl_set_error_ccc("USB ATA command %02x failed with return %d (%s), sense "
                            "%02x/%02x/%02x.\n",
                            regs[6], raw_ret, strerror(abs(raw_ret)), sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
    if (direction == WD_ROYL_DIR_IN && raw_ret != (int)data_size)
    {
      wd_royl_set_error_ccc("USB ATA command %02x returned %d bytes instead of %zu, sense "
                            "%02x/%02x/%02x.\n",
                            regs[6], raw_ret, data_size, sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
    if (direction == WD_ROYL_DIR_OUT && raw_ret != 0 && raw_ret != (int)data_size)
    {
      wd_royl_set_error_ccc("USB ATA command %02x transferred %d bytes instead of %zu, sense "
                            "%02x/%02x/%02x.\n",
                            regs[6], raw_ret, data_size, sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
  }
  else
  {
    ret = do_ata28_cmd_ccc(disk_fd);
    if (ret != 0)
    {
      wd_royl_set_error_ccc("ATA command %02x failed with return %d, status %02x, sense "
                            "%02x/%02x/%02x.\n",
                            regs[6], ret, ata_status_ccc, sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
  }

  if (ata_status_ccc & 0x01)
  {
    wd_royl_set_error_ccc("ATA command %02x reported an error, status %02x, sense "
                          "%02x/%02x/%02x.\n",
                          regs[6], ata_status_ccc, sense_key_ccc, asc_ccc, ascq_ccc);
    goto restore_buffer;
  }

  if (sense_key_ccc > 1)
  {
    // Some USB bridges report this sense combination for commands that
    // actually completed. The WD scripts treat it as success.
    if (!(sense_key_ccc == 4 && asc_ccc == 0 && ascq_ccc == 0))
    {
      wd_royl_set_error_ccc("ATA command %02x failed with sense %02x/%02x/%02x.\n", regs[6], sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
  }

  if (usb_mode_ccc && !usb_csw_valid_ccc)
  {
    wd_royl_set_error_ccc("USB ATA command %02x did not return a valid command status "
                          "wrapper.\n",
                          regs[6]);
    goto restore_buffer;
  }

  if (usb_mode_ccc && csw_status != 0)
  {
    if (!(sense_key_ccc == 4 && asc_ccc == 0 && ascq_ccc == 0))
    {
      wd_royl_set_error_ccc("USB ATA command %02x failed with CSW status %u and sense "
                            "%02x/%02x/%02x.\n",
                            regs[6], csw_status, sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
  }

  if (direction == WD_ROYL_DIR_IN)
  {
    memcpy(data, ccc_buffer_ccc, data_size);
  }
  result = 0;

restore_buffer:
  ccc_main_buffer_size_ccc = saved_buffer_size;
  if (set_main_buffer_ccc() != 0)
  {
    // Do not hide the original error if there was one.
    if (result == 0)
    {
      wd_royl_set_error_ccc("Unable to restore the original buffer size.\n");
      result = -1;
    }
  }
  return result;
}

static int wd_royl_prepare_ccc(const uint8_t request[16], bool send_twice)
{
  uint8_t buffer[WD_ROYL_SECTOR_SIZE];
  const uint8_t regs[7] = {0xd6, 0x01, 0xbe, 0x4f, 0xc2, 0xa0, 0xb0};

  memset(buffer, 0, sizeof(buffer));
  memcpy(buffer, request, 16);

  if (wd_royl_send_ata28_ccc(regs, buffer, sizeof(buffer), WD_ROYL_DIR_OUT) != 0)
  {
    return -1;
  }
  if (send_twice)
  {
    if (wd_royl_send_ata28_ccc(regs, buffer, sizeof(buffer), WD_ROYL_DIR_OUT) != 0)
    {
      return -1;
    }
  }
  return 0;
}

static int wd_royl_read_sectors_ccc(uint8_t sectors, void *data)
{
  const uint8_t regs[7] = {0xd5, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  uint8_t local_regs[7];

  memcpy(local_regs, regs, sizeof(local_regs));
  local_regs[1] = sectors;
  return wd_royl_send_ata28_ccc(local_regs, data, (size_t)sectors * WD_ROYL_SECTOR_SIZE, WD_ROYL_DIR_IN);
}

static int wd_royl_write_sectors_ccc(uint8_t sectors, const void *data)
{
  const uint8_t regs[7] = {0xd6, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  uint8_t local_regs[7];

  memcpy(local_regs, regs, sizeof(local_regs));
  local_regs[1] = sectors;
  return wd_royl_send_ata28_ccc(local_regs, (void *)data, (size_t)sectors * WD_ROYL_SECTOR_SIZE, WD_ROYL_DIR_OUT);
}

int wd_royl_enable_vsc_ccc(void)
{
  const uint8_t regs[7] = {0x45, 0x0b, 0x00, 0x44, 0x57, 0xa0, 0x80};

  return wd_royl_send_ata28_ccc(regs, NULL, 0, WD_ROYL_DIR_NONE);
}

int wd_royl_disable_vsc_ccc(void)
{
  const uint8_t regs[7] = {0x44, 0x0b, 0x00, 0x44, 0x57, 0xa0, 0x80};

  return wd_royl_send_ata28_ccc(regs, NULL, 0, WD_ROYL_DIR_NONE);
}

static int wd_royl_read_data_table_internal_ccc(uint8_t table[WD_ROYL_SECTOR_SIZE])
{
  uint8_t request[16];

  memset(request, 0, sizeof(request));
  request[0] = 0x0d;
  request[1] = 0x00;
  request[2] = 0x01;
  request[3] = 0x00;

  if (wd_royl_prepare_ccc(request, true) != 0)
  {
    return -1;
  }
  return wd_royl_read_sectors_ccc(1, table);
}

int wd_royl_read_data_table_ccc(uint8_t table[WD_ROYL_SECTOR_SIZE])
{
  int result = -1;

  if (table == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no data table buffer.\n");
    return -1;
  }
  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  if (wd_royl_read_data_table_internal_ccc(table) == 0)
  {
    result = 0;
  }
  if (wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The data table was read, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_get_rom_size_ccc(size_t *rom_size)
{
  uint8_t table[WD_ROYL_SECTOR_SIZE];
  int result = -1;

  if (rom_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no ROM size destination.\n");
    return -1;
  }
  *rom_size = 0;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  if (wd_royl_read_data_table_internal_ccc(table) != 0)
  {
    goto finish;
  }
  if (table[WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET] == 0)
  {
    wd_royl_set_error_ccc("The drive reported zero ROM blocks. Vendor specific commands may not "
                          "be supported by this drive.\n");
    goto finish;
  }
  *rom_size = (size_t)table[WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET] * WD_ROYL_ROM_BLOCK_SIZE;
  result = 0;

finish:
  if (wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The ROM size was read, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_read_rom_ccc(uint8_t **rom, size_t *rom_size, wd_royl_progress_callback_ccc progress)
{
  uint8_t table[WD_ROYL_SECTOR_SIZE];
  uint8_t request[16];
  uint8_t *buffer = NULL;
  size_t size = 0;
  size_t offset = 0;
  size_t chunk_size = 0;
  int result = -1;
  bool enabled = false;

  if (rom == NULL || rom_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no ROM destination.\n");
    return -1;
  }
  *rom = NULL;
  *rom_size = 0;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  wd_royl_progress_ccc(progress, "Reading drive data table", 0, 1);
  if (wd_royl_read_data_table_internal_ccc(table) != 0)
  {
    goto finish;
  }
  if (table[WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET] == 0)
  {
    wd_royl_set_error_ccc("The drive reported zero ROM blocks. Vendor specific commands may not "
                          "be supported by this drive.\n");
    goto finish;
  }

  size = (size_t)table[WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET] * WD_ROYL_ROM_BLOCK_SIZE;
  buffer = (uint8_t *)malloc(size);
  if (buffer == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %zu bytes for the ROM.\n", size);
    goto finish;
  }
  memset(buffer, 0, size);

  memset(request, 0, sizeof(request));
  request[0] = 0x24;
  request[1] = 0x00;
  request[2] = 0x01;
  request[3] = 0x00;
  if (wd_royl_prepare_ccc(request, true) != 0)
  {
    goto finish;
  }

  if ((size % 65536) == 0)
  {
    chunk_size = 65536;
  }
  else
  {
    chunk_size = 16384;
  }

  while (offset < size)
  {
    size_t current = chunk_size;
    if (current > size - offset)
    {
      current = size - offset;
    }
    if ((current % WD_ROYL_SECTOR_SIZE) != 0 || (current / WD_ROYL_SECTOR_SIZE) > 255)
    {
      wd_royl_set_error_ccc("Internal error: bad ROM chunk size %zu.\n", current);
      goto finish;
    }
    if (wd_royl_read_sectors_ccc((uint8_t)(current / WD_ROYL_SECTOR_SIZE), buffer + offset) != 0)
    {
      goto finish;
    }
    offset += current;
    wd_royl_progress_ccc(progress, "Reading ROM", offset, size);
  }

  *rom = buffer;
  *rom_size = size;
  buffer = NULL;
  result = 0;

finish:
  free(buffer);
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The ROM was read, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_write_rom_ccc(const uint8_t *rom, size_t rom_size, bool slow_and_stupid, wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  size_t offset = 0;
  size_t chunk_size = 0;
  int result = -1;
  bool enabled = false;

  if (rom == NULL || rom_size == 0)
  {
    wd_royl_set_error_ccc("There is no ROM data to write.\n");
    return -1;
  }
  if ((rom_size % WD_ROYL_ROM_BLOCK_SIZE) != 0)
  {
    wd_royl_set_error_ccc("The ROM size %zu is not a multiple of the 16384 byte ROM block "
                          "size.\n",
                          rom_size);
    return -1;
  }
  if (rom_size > WD_ROYL_MAX_FILE_SIZE)
  {
    wd_royl_set_error_ccc("The ROM size %zu is too large.\n", rom_size);
    return -1;
  }

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  wd_royl_progress_ccc(progress, "Erasing ROM", 0, 1);
  memset(request, 0, sizeof(request));
  request[0] = 0x24;
  request[1] = 0x00;
  request[2] = 0x03;
  request[3] = 0x00;
  if (wd_royl_prepare_ccc(request, false) != 0)
  {
    goto finish;
  }

  wd_royl_progress_ccc(progress, "Preparing to write ROM", 0, 1);
  memset(request, 0, sizeof(request));
  request[0] = 0x24;
  request[1] = 0x00;
  request[2] = 0x02;
  request[3] = 0x00;
  if (wd_royl_prepare_ccc(request, true) != 0)
  {
    goto finish;
  }

  if (slow_and_stupid)
  {
    chunk_size = WD_ROYL_SECTOR_SIZE;
  }
  else if ((rom_size % 65536) == 0)
  {
    chunk_size = 65536;
  }
  else
  {
    chunk_size = 16384;
  }

  while (offset < rom_size)
  {
    size_t current = chunk_size;
    if (current > rom_size - offset)
    {
      current = rom_size - offset;
    }
    if ((current % WD_ROYL_SECTOR_SIZE) != 0 || (current / WD_ROYL_SECTOR_SIZE) > 255)
    {
      wd_royl_set_error_ccc("Internal error: bad ROM chunk size %zu.\n", current);
      goto finish;
    }
    if (wd_royl_write_sectors_ccc((uint8_t)(current / WD_ROYL_SECTOR_SIZE), rom + offset) != 0)
    {
      goto finish;
    }
    offset += current;
    wd_royl_progress_ccc(progress, "Writing ROM", offset, rom_size);
  }

  result = 0;

finish:
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The ROM was written, but vendor specific commands could not be "
                          "disabled. Power cycle the drive before using it.\n");
    result = -1;
  }
  return result;
}

int wd_royl_verify_rom_ccc(const uint8_t *rom, size_t rom_size, wd_royl_progress_callback_ccc progress)
{
  uint8_t *read_rom = NULL;
  size_t read_size = 0;
  size_t offset = 0;
  int result = -1;

  if (rom == NULL || rom_size == 0)
  {
    wd_royl_set_error_ccc("There is no ROM data to verify.\n");
    return -1;
  }

  if (wd_royl_read_rom_ccc(&read_rom, &read_size, progress) != 0)
  {
    return -1;
  }

  if (read_size != rom_size)
  {
    wd_royl_set_error_ccc("ROM verification failed: expected %zu bytes but read %zu bytes.\n", rom_size, read_size);
    goto finish;
  }

  while (offset < rom_size)
  {
    if (read_rom[offset] != rom[offset])
    {
      wd_royl_set_error_ccc("ROM verification failed at offset 0x%zx: expected 0x%02x but "
                            "read 0x%02x.\n",
                            offset, rom[offset], read_rom[offset]);
      goto finish;
    }
    offset++;
    if ((offset % 65536) == 0 || offset == rom_size)
    {
      wd_royl_progress_ccc(progress, "Verifying ROM", offset, rom_size);
    }
  }

  result = 0;

finish:
  free(read_rom);
  return result;
}

int wd_royl_validate_rom_ccc(const uint8_t *rom, size_t rom_size)
{
  unsigned int block_number = 0;
  size_t table_offset;
  size_t position;
  uint32_t checksum_location;
  uint32_t block_size;
  uint32_t block_start;
  uint32_t checksum_length;
  uint32_t calculated;
  uint32_t stored;

  if (rom == NULL || rom_size == 0)
  {
    wd_royl_set_error_ccc("There is no ROM data to validate.\n");
    return -1;
  }
  if ((rom_size % WD_ROYL_ROM_BLOCK_SIZE) != 0)
  {
    wd_royl_set_error_ccc("The ROM size %zu is not a multiple of the 16384 byte ROM block "
                          "size.\n",
                          rom_size);
    return -1;
  }
  if (rom[0] != 0x5a)
  {
    wd_royl_set_error_ccc("The ROM does not start with the expected 0x5a block signature.\n");
    return -1;
  }

  while (1)
  {
    table_offset = (size_t)block_number * 32;
    if (table_offset + 32 > rom_size)
    {
      wd_royl_set_error_ccc("The ROM block table runs past the end of the ROM at block %u.\n", block_number);
      return -1;
    }
    if (block_number != 0 && rom[table_offset] != (uint8_t)block_number)
    {
      break;
    }

    checksum_location = wd_royl_le32_ccc(rom + table_offset + 4);
    block_size = wd_royl_le32_ccc(rom + table_offset + 8);
    block_start = wd_royl_le32_ccc(rom + table_offset + 0x0c);
    if (checksum_location < block_size)
    {
      wd_royl_set_error_ccc("The ROM block %u has an invalid size (0x%x) and checksum "
                            "location (0x%x).\n",
                            block_number, block_size, checksum_location);
      return -1;
    }
    checksum_length = checksum_location - block_size;
    if ((uint64_t)block_start + checksum_location > rom_size)
    {
      wd_royl_set_error_ccc("The ROM block %u data is outside the ROM (start 0x%x, end "
                            "0x%llx).\n",
                            block_number, block_start, (unsigned long long)block_start + checksum_location);
      return -1;
    }

    calculated = 0;
    for (position = table_offset; position < table_offset + 31; position++)
    {
      calculated += rom[position];
    }
    if ((calculated & 0xff) != rom[table_offset + 0x1f])
    {
      wd_royl_set_error_ccc("The ROM block %u table checksum is invalid (stored 0x%02x, "
                            "calculated 0x%02x).\n",
                            block_number, rom[table_offset + 0x1f], calculated & 0xff);
      return -1;
    }

    calculated = 0;
    if (checksum_length == 1)
    {
      for (position = block_start; position < block_start + block_size; position++)
      {
        calculated += rom[position];
      }
      stored = rom[block_start + block_size];
      calculated &= 0xff;
    }
    else if (checksum_length == 2)
    {
      if ((block_size % 2) != 0)
      {
        wd_royl_set_error_ccc("The ROM block %u has an odd size (0x%x) for a 16 bit "
                              "checksum.\n",
                              block_number, block_size);
        return -1;
      }
      for (position = block_start; position < block_start + block_size; position += 2)
      {
        calculated += wd_royl_le16_ccc(rom + position);
      }
      stored = wd_royl_le16_ccc(rom + block_start + block_size);
      calculated &= 0xffff;
    }
    else
    {
      wd_royl_set_error_ccc("The ROM block %u has an unsupported checksum length of %u.\n", block_number, checksum_length);
      return -1;
    }
    if (stored != calculated)
    {
      wd_royl_set_error_ccc("The ROM block %u checksum is invalid (stored 0x%x, calculated "
                            "0x%x).\n",
                            block_number, stored, calculated);
      return -1;
    }

    block_number++;
  }

  return 0;
}

uint16_t wd_royl_get_module_id_ccc(const uint8_t *module)
{
  if (module == NULL)
  {
    return 0;
  }
  return wd_royl_le16_ccc(module + 0x08);
}

uint16_t wd_royl_get_module_sector_count_ccc(const uint8_t *module)
{
  if (module == NULL)
  {
    return 0;
  }
  return (uint16_t)module[0x0a] | ((uint16_t)module[0x0b] << 8);
}

uint16_t wd_royl_get_module_record_count_ccc(const uint8_t *module)
{
  if (module == NULL)
  {
    return 0;
  }
  return wd_royl_le16_ccc(module + 0x30);
}

uint16_t wd_royl_get_module_table_offset_ccc(const uint8_t *module)
{
  if (module == NULL)
  {
    return 0;
  }
  return wd_royl_le16_ccc(module + 0x30);
}

uint32_t wd_royl_get_module_checksum_ccc(const uint8_t *module)
{
  if (module == NULL)
  {
    return 0;
  }
  return wd_royl_le32_ccc(module + 0x0c);
}

uint32_t wd_royl_calculate_checksum_ccc(const uint8_t *data, size_t size)
{
  uint32_t sum = 0;
  size_t offset;

  if (data == NULL)
  {
    return 0;
  }
  for (offset = 0; (offset + 4) <= size; offset += 4)
  {
    if (offset == 0x0c)
    {
      continue;
    }
    sum += wd_royl_le32_ccc(data + offset);
  }
  return (uint32_t)(0u - sum);
}

void wd_royl_update_checksum_ccc(uint8_t *data, size_t size)
{
  uint32_t checksum;

  if (data == NULL || size < 0x10)
  {
    return;
  }
  checksum = wd_royl_calculate_checksum_ccc(data, size);
  wd_royl_put_le32_ccc(data + 0x0c, checksum);
}

static bool wd_royl_module_valid_ccc(const uint8_t *data, size_t size, uint16_t expected_module)
{
  uint16_t sectors;

  if (data == NULL || size < 0x32)
  {
    return false;
  }
  if (memcmp(data, "ROYL", 4) != 0)
  {
    return false;
  }
  if ((size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    return false;
  }
  sectors = wd_royl_get_module_sector_count_ccc(data);
  if (sectors == 0 || (size_t)sectors * WD_ROYL_SECTOR_SIZE != size)
  {
    return false;
  }
  if (expected_module != 0 && wd_royl_get_module_id_ccc(data) != expected_module)
  {
    return false;
  }
  if (wd_royl_get_module_checksum_ccc(data) != wd_royl_calculate_checksum_ccc(data, size))
  {
    return false;
  }
  return true;
}

int wd_royl_validate_module_header_ccc(const uint8_t *data, size_t size, uint16_t expected_module)
{
  uint16_t sectors;

  if (data == NULL || size < 0x32)
  {
    wd_royl_set_error_ccc("The module data is too small.\n");
    return -1;
  }
  if (memcmp(data, "ROYL", 4) != 0)
  {
    wd_royl_set_error_ccc("The module does not have a ROYL header.\n");
    return -1;
  }
  if ((size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("The module size %zu is not a multiple of the sector size.\n", size);
    return -1;
  }
  sectors = wd_royl_get_module_sector_count_ccc(data);
  if (sectors == 0 || (size_t)sectors * WD_ROYL_SECTOR_SIZE != size)
  {
    wd_royl_set_error_ccc("The module header reports %u sectors, but the data is %zu bytes.\n", sectors, size);
    return -1;
  }
  if (expected_module != 0 && wd_royl_get_module_id_ccc(data) != expected_module)
  {
    wd_royl_set_error_ccc("Expected module 0x%04x but the module data is module 0x%04x.\n", expected_module, wd_royl_get_module_id_ccc(data));
    return -1;
  }
  return 0;
}

int wd_royl_validate_module_ccc(const uint8_t *data, size_t size, uint16_t expected_module)
{
  uint32_t stored_checksum;
  uint32_t calculated_checksum;

  if (wd_royl_validate_module_header_ccc(data, size, expected_module) != 0)
  {
    return -1;
  }
  stored_checksum = wd_royl_get_module_checksum_ccc(data);
  calculated_checksum = wd_royl_calculate_checksum_ccc(data, size);
  if (stored_checksum != calculated_checksum)
  {
    wd_royl_set_error_ccc("Module 0x%04x checksum mismatch: header 0x%08x, calculated "
                          "0x%08x.\n",
                          wd_royl_get_module_id_ccc(data), stored_checksum, calculated_checksum);
    return -1;
  }
  return 0;
}

int wd_royl_get_record_ccc(const uint8_t *data, size_t size, unsigned int record_number, uint16_t *record_offset, uint16_t *record_size)
{
  size_t offset;
  uint16_t record_start;
  uint16_t record_length;

  if (data == NULL || record_number == 0 || record_offset == NULL || record_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: invalid module record request.\n");
    return -1;
  }
  offset = 0x32 + ((size_t)record_number - 1) * 4;
  if ((offset + 4) > size)
  {
    wd_royl_set_error_ccc("Module record %u is past the end of the module.\n", record_number);
    return -1;
  }
  record_start = wd_royl_le16_ccc(data + offset);
  record_length = wd_royl_le16_ccc(data + offset + 2);
  if (record_length == 0 || (size_t)record_start + (size_t)record_length > size)
  {
    wd_royl_set_error_ccc("Module record %u is outside the module data (offset 0x%04x, size "
                          "0x%04x).\n",
                          record_number, record_start, record_length);
    return -1;
  }
  *record_offset = record_start;
  *record_size = record_length;
  return 0;
}

int wd_royl_parse_module_directory_ccc(const uint8_t *data, size_t size, wd_royl_module_entry_ccc **entries, size_t *entry_count)
{
  wd_royl_module_entry_ccc *list = NULL;
  size_t capacity = 0;
  size_t count = 0;
  size_t offset;
  unsigned int record_count;
  unsigned int record_number;
  int result = -1;

  if (entries == NULL || entry_count == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no destination for the module directory.\n");
    return -1;
  }
  *entries = NULL;
  *entry_count = 0;

  if (wd_royl_validate_module_header_ccc(data, size, WD_ROYL_MODULE_01) != 0)
  {
    return -1;
  }

  record_count = wd_royl_get_module_record_count_ccc(data);
  offset = 0x32;
  for (record_number = 1; record_number <= record_count; record_number++)
  {
    uint8_t record_length;
    uint16_t module_id;
    uint16_t sectors;
    wd_royl_module_entry_ccc *entry;

    if (offset >= size)
    {
      wd_royl_set_error_ccc("Directory record %u is past the end of module 0x0001.\n", record_number);
      goto finish;
    }
    record_length = data[offset];
    if (record_length < 6 || (size_t)record_length > size - offset)
    {
      wd_royl_set_error_ccc("Directory record %u has an invalid length (%u bytes).\n", record_number, record_length);
      goto finish;
    }

    module_id = wd_royl_le16_ccc(data + offset + 2);
    sectors = wd_royl_le16_ccc(data + offset + 4);
    if (module_id != 0)
    {
      if (count == capacity)
      {
        size_t new_capacity = capacity == 0 ? 16 : capacity * 2;
        wd_royl_module_entry_ccc *new_list;

        if (new_capacity < capacity || new_capacity > SIZE_MAX / sizeof(*new_list))
        {
          wd_royl_set_error_ccc("The module directory is too large to process.\n");
          goto finish;
        }
        new_list = realloc(list, new_capacity * sizeof(*new_list));
        if (new_list == NULL)
        {
          wd_royl_set_error_ccc("Unable to allocate memory for the module directory.\n");
          goto finish;
        }
        list = new_list;
        capacity = new_capacity;
      }

      entry = &list[count++];
      memset(entry, 0, sizeof(*entry));
      entry->module_id = module_id;
      entry->sectors = sectors;
      entry->copies = data[offset + 1];
      if (record_length >= 0x12)
      {
        entry->location1 = wd_royl_le32_ccc(data + offset + 0x0a);
        entry->location2 = wd_royl_le32_ccc(data + offset + 0x0e);
      }
    }

    offset += record_length;
  }

  *entries = list;
  *entry_count = count;
  list = NULL;
  result = 0;

finish:
  free(list);
  return result;
}

int wd_royl_patch_module_02_ccc(uint8_t *data, size_t size)
{
  uint16_t record_offset = 0;
  uint16_t record_size = 0;

  if (data == NULL || size < 0x32 || memcmp(data, "ROYL", 4) != 0)
  {
    wd_royl_set_error_ccc("The module 02 data is not valid.\n");
    return -1;
  }
  if (wd_royl_get_module_id_ccc(data) != WD_ROYL_MODULE_02)
  {
    wd_royl_set_error_ccc("Expected module 02 but found module 0x%04x.\n", wd_royl_get_module_id_ccc(data));
    return -1;
  }
  if (wd_royl_get_record_ccc(data, size, 27, &record_offset, &record_size) != 0)
  {
    return -1;
  }
  if (record_size < 3)
  {
    wd_royl_set_error_ccc("Module 02 record 27 is too small (%u bytes) for the slow fix.\n", record_size);
    return -1;
  }
  data[record_offset + 2] = 0x00;
  wd_royl_update_checksum_ccc(data, size);
  return 0;
}

int wd_royl_patch_module_32_ccc(uint8_t *data, size_t size)
{
  uint16_t table_offset;

  if (data == NULL || size < 0x32 || memcmp(data, "ROYL", 4) != 0)
  {
    wd_royl_set_error_ccc("The module 32 data is not valid.\n");
    return -1;
  }
  if (wd_royl_get_module_id_ccc(data) != WD_ROYL_MODULE_32)
  {
    wd_royl_set_error_ccc("Expected module 32 but found module 0x%04x.\n", wd_royl_get_module_id_ccc(data));
    return -1;
  }
  table_offset = wd_royl_get_module_table_offset_ccc(data);
  if (table_offset == 0 || (size_t)table_offset >= size)
  {
    wd_royl_set_error_ccc("Module 32 has an invalid table offset 0x%04x.\n", table_offset);
    return -1;
  }
  memset(data + table_offset, 0, size - table_offset);
  wd_royl_update_checksum_ccc(data, size);
  return 0;
}

int wd_royl_read_module_ccc(uint16_t module, uint8_t **data, size_t *size, wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  uint8_t first_sector[WD_ROYL_SECTOR_SIZE];
  uint8_t *buffer = NULL;
  uint16_t sectors;
  size_t module_size;
  unsigned int sector;
  int result = -1;
  bool enabled = false;

  if (data == NULL || size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no module destination.\n");
    return -1;
  }
  *data = NULL;
  *size = 0;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  memset(request, 0, sizeof(request));
  request[0] = 0x08;
  request[1] = 0x00;
  request[2] = 0x01;
  request[3] = 0x00;
  request[4] = (uint8_t)(module & 0xff);
  request[5] = (uint8_t)(module >> 8);
  wd_royl_progress_ccc(progress, "Reading module header", 0, 1);
  if (wd_royl_prepare_ccc(request, true) != 0)
  {
    goto finish;
  }
  if (wd_royl_read_sectors_ccc(1, first_sector) != 0)
  {
    goto finish;
  }
  if (memcmp(first_sector, "ROYL", 4) != 0)
  {
    wd_royl_set_error_ccc("The drive did not return a ROYL module header for module 0x%04x.\n", module);
    goto finish;
  }

  sectors = wd_royl_get_module_sector_count_ccc(first_sector);
  if (sectors == 0)
  {
    wd_royl_set_error_ccc("Module 0x%04x reports zero sectors.\n", module);
    goto finish;
  }
  if (wd_royl_get_module_id_ccc(first_sector) != module)
  {
    wd_royl_set_error_ccc("Expected module 0x%04x but the drive returned module 0x%04x.\n", module, wd_royl_get_module_id_ccc(first_sector));
    goto finish;
  }
  module_size = (size_t)sectors * WD_ROYL_SECTOR_SIZE;
  if (module_size > WD_ROYL_MAX_FILE_SIZE)
  {
    wd_royl_set_error_ccc("Module 0x%04x reports an unreasonable size of %zu bytes.\n", module, module_size);
    goto finish;
  }

  buffer = (uint8_t *)malloc(module_size);
  if (buffer == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %zu bytes for module 0x%04x.\n", module_size, module);
    goto finish;
  }
  memcpy(buffer, first_sector, sizeof(first_sector));
  wd_royl_progress_ccc(progress, "Reading module", WD_ROYL_SECTOR_SIZE, module_size);

  for (sector = 1; sector < sectors; sector++)
  {
    if (wd_royl_read_sectors_ccc(1, buffer + (size_t)sector * WD_ROYL_SECTOR_SIZE) != 0)
    {
      goto finish;
    }
    wd_royl_progress_ccc(progress, "Reading module", (unsigned long long)(sector + 1) * WD_ROYL_SECTOR_SIZE, module_size);
  }

  *data = buffer;
  *size = module_size;
  buffer = NULL;
  result = 0;

finish:
  free(buffer);
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("Module 0x%04x was read, but vendor specific commands could not be "
                          "disabled.\n",
                          module);
    result = -1;
  }
  return result;
}

int wd_royl_write_module_ccc(uint16_t module, const uint8_t *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  size_t sectors;
  size_t sector;
  int result = -1;
  bool enabled = false;

  if (data == NULL || size == 0)
  {
    wd_royl_set_error_ccc("There is no module data to write.\n");
    return -1;
  }
  if ((size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("The module size %zu is not a multiple of the sector size.\n", size);
    return -1;
  }
  if (size > WD_ROYL_MAX_FILE_SIZE)
  {
    wd_royl_set_error_ccc("The module size %zu is too large.\n", size);
    return -1;
  }
  if (wd_royl_validate_module_ccc(data, size, module) != 0)
  {
    return -1;
  }
  sectors = size / WD_ROYL_SECTOR_SIZE;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  memset(request, 0, sizeof(request));
  request[0] = 0x08;
  request[1] = 0x00;
  request[2] = 0x02;
  request[3] = 0x00;
  request[4] = (uint8_t)(module & 0xff);
  request[5] = (uint8_t)(module >> 8);
  wd_royl_progress_ccc(progress, "Preparing to write module", 0, 1);
  if (wd_royl_prepare_ccc(request, true) != 0)
  {
    goto finish;
  }

  for (sector = 0; sector < sectors; sector++)
  {
    if (wd_royl_write_sectors_ccc(1, data + sector * WD_ROYL_SECTOR_SIZE) != 0)
    {
      goto finish;
    }
    wd_royl_progress_ccc(progress, "Writing module", (unsigned long long)(sector + 1) * WD_ROYL_SECTOR_SIZE, size);
  }

  result = 0;

finish:
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("Module 0x%04x was written, but vendor specific commands could not be "
                          "disabled.\n",
                          module);
    result = -1;
  }
  return result;
}

int wd_royl_verify_module_ccc(uint16_t module, const uint8_t *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  uint8_t *read_data = NULL;
  size_t read_size = 0;
  int result = -1;

  if (data == NULL || size == 0)
  {
    wd_royl_set_error_ccc("There is no module data to verify.\n");
    return -1;
  }

  if (wd_royl_read_module_ccc(module, &read_data, &read_size, progress) != 0)
  {
    return -1;
  }

  if (read_size != size)
  {
    wd_royl_set_error_ccc("Module 0x%04x verification failed: expected %zu bytes but read %zu "
                          "bytes.\n",
                          module, size, read_size);
    goto finish;
  }
  if (memcmp(read_data, data, size) != 0)
  {
    size_t offset;
    for (offset = 0; offset < size; offset++)
    {
      if (read_data[offset] != data[offset])
      {
        wd_royl_set_error_ccc("Module 0x%04x verification failed at offset 0x%zx: expected "
                              "0x%02x but read 0x%02x.\n",
                              module, offset, data[offset], read_data[offset]);
        break;
      }
    }
    goto finish;
  }
  result = 0;

finish:
  free(read_data);
  return result;
}

static int wd_royl_read_ram_internal_ccc(uint64_t address, void *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  uint8_t regs[7] = {WD_ROYL_ATA_CMD_D5, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  char detail[WD_ROYL_ERROR_SIZE];
  size_t sectors;

  if (data == NULL || size == 0 || (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM read size %zu.\n", size);
    return -1;
  }
  if (address > 0xffffffffULL || size > 0xffffffffULL)
  {
    wd_royl_set_error_ccc("RAM read is outside the 32 bit address space.\n");
    return -1;
  }

  memset(request, 0, sizeof(request));
  request[0] = 0x13;
  request[1] = 0x00;
  request[2] = 0x01;
  request[3] = 0x00;
  wd_royl_put_le32_ccc(request + 4, (uint32_t)address);
  wd_royl_put_le32_ccc(request + 8, (uint32_t)size);
  // Some firmware only accepts a RAM read after the key sector was sent twice.
  if (wd_royl_prepare_ccc(request, true) != 0)
  {
    snprintf(detail, sizeof(detail), "%s", wd_royl_error_ccc);
    wd_royl_set_error_ccc("Could not send the RAM read key for address 0x%08llx: %s",
                          (unsigned long long)address, detail);
    return -1;
  }

  sectors = size / WD_ROYL_SECTOR_SIZE;
  if (sectors > 255)
  {
    wd_royl_set_error_ccc("Internal error: RAM read is too large.\n");
    return -1;
  }
  regs[1] = (uint8_t)sectors;
  if (wd_royl_send_ata28_ccc(regs, data, size, WD_ROYL_DIR_IN) != 0)
  {
    snprintf(detail, sizeof(detail), "%s", wd_royl_error_ccc);
    wd_royl_set_error_ccc("The RAM read key for address 0x%08llx was accepted, but the %zu byte "
                          "RAM read failed: %s",
                          (unsigned long long)address, size, detail);
    return -1;
  }

  wd_royl_progress_ccc(progress, "Reading RAM", 0, 0);
  return 0;
}

static int wd_royl_write_ram_internal_ccc(uint64_t address, const void *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  uint8_t regs[7] = {WD_ROYL_ATA_CMD_D6, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  char detail[WD_ROYL_ERROR_SIZE];
  size_t sectors;

  if (data == NULL || size == 0 || (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM write size %zu.\n", size);
    return -1;
  }
  if (address > 0xffffffffULL || size > 0xffffffffULL)
  {
    wd_royl_set_error_ccc("RAM write is outside the 32 bit address space.\n");
    return -1;
  }

  memset(request, 0, sizeof(request));
  request[0] = 0x13;
  request[1] = 0x00;
  request[2] = 0x02;
  request[3] = 0x00;
  wd_royl_put_le32_ccc(request + 4, (uint32_t)address);
  wd_royl_put_le32_ccc(request + 8, (uint32_t)size);
  if (wd_royl_prepare_ccc(request, false) != 0)
  {
    snprintf(detail, sizeof(detail), "%s", wd_royl_error_ccc);
    wd_royl_set_error_ccc("Could not send the RAM write key for address 0x%08llx: %s",
                          (unsigned long long)address, detail);
    return -1;
  }

  sectors = size / WD_ROYL_SECTOR_SIZE;
  if (sectors > 255)
  {
    wd_royl_set_error_ccc("Internal error: RAM write is too large.\n");
    return -1;
  }
  regs[1] = (uint8_t)sectors;
  if (wd_royl_send_ata28_ccc(regs, (void *)data, size, WD_ROYL_DIR_OUT) != 0)
  {
    snprintf(detail, sizeof(detail), "%s", wd_royl_error_ccc);
    wd_royl_set_error_ccc("The RAM write key for address 0x%08llx was accepted, but the %zu byte "
                          "RAM write failed: %s",
                          (unsigned long long)address, size, detail);
    return -1;
  }

  wd_royl_progress_ccc(progress, "Writing RAM", 0, 0);
  return 0;
}

// ATA28 can transfer at most 255 sectors in one command, so larger RAM
// operations are split into aligned chunks. The chunk size is also used for
// progress reporting.
#define WD_ROYL_RAM_CHUNK_SIZE ((size_t)WD_ROYL_RAM_BLOCK_SIZE)

static int wd_royl_read_ram_chunked_ccc(uint64_t address, void *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  size_t offset;

  if (data == NULL || size == 0 || (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM read size %zu.\n", size);
    return -1;
  }

  for (offset = 0; offset < size; offset += WD_ROYL_RAM_CHUNK_SIZE)
  {
    size_t current = WD_ROYL_RAM_CHUNK_SIZE;

    if (current > size - offset)
    {
      current = size - offset;
    }
    if (wd_royl_read_ram_internal_ccc(address + offset, (uint8_t *)data + offset, current, NULL) != 0)
    {
      return -1;
    }
    wd_royl_progress_ccc(progress, "Reading RAM", offset + current, size);
  }
  return 0;
}

// Describes how much of the reported RAM window could actually be read.
typedef struct
{
  uint64_t readable_end;            // address just past the last readable byte
  uint64_t first_gap;               // first unreadable address, 0 when fully readable
  char gap_error[WD_ROYL_ERROR_SIZE];
} wd_royl_ram_scan_info_ccc;

static size_t wd_royl_scanned_ram_size_ccc(const wd_royl_ram_scan_info_ccc *scan, uint64_t ram_size);

static int wd_royl_load_ram_image_ccc(uint8_t **ram, uint64_t *ram_size, wd_royl_ram_scan_info_ccc *info,
                                      wd_royl_progress_callback_ccc progress);

// Some firmware reports a DRAM size in the data table that is larger than the
// address range the memory command can actually reach. A read that far out
// hangs until the drive is soft reset. The scan below therefore treats a read
// failure in the middle of the window as an unreadable gap: the chunk is
// zeroed, reading resumes with the next chunk, and two consecutive failures end
// the scan. One jump over a larger gap is allowed, because a reserved block in
// the middle of DRAM looks the same as the end of the window from the host.
// Zeroed gaps cannot be mistaken for a ROYL module.
#define WD_ROYL_RAM_SCAN_MAX_GAP_CHUNKS 2
#define WD_ROYL_RAM_SCAN_RESYNC_JUMP 0x00100000ULL
#define WD_ROYL_RAM_SCAN_MAX_RESYNCS 1

static int wd_royl_scan_ram_ccc(uint64_t address, uint8_t *data, size_t size, size_t *readable,
                                uint64_t *first_gap, char *gap_error, size_t gap_error_size,
                                wd_royl_progress_callback_ccc progress)
{
  size_t offset = 0;
  size_t failures = 0;
  size_t resyncs = 0;

  *readable = 0;
  if (first_gap != NULL)
  {
    *first_gap = 0;
  }
  if (gap_error != NULL && gap_error_size > 0)
  {
    gap_error[0] = '\0';
  }
  if (data == NULL || size == 0 || (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM scan size %zu.\n", size);
    return -1;
  }

  while (offset < size)
  {
    size_t current = WD_ROYL_RAM_CHUNK_SIZE;

    if (current > size - offset)
    {
      current = size - offset;
    }
    if (wd_royl_read_ram_internal_ccc(address + offset, data + offset, current, NULL) != 0)
    {
      if (first_gap != NULL && *first_gap == 0)
      {
        *first_gap = address + offset;
        if (gap_error != NULL && gap_error_size > 0)
        {
          size_t length;

          snprintf(gap_error, gap_error_size, "%s", wd_royl_error_ccc);
          // The stored error ends with a newline; keep the message inside one
          // sentence when it is embedded in a larger report.
          length = strlen(gap_error);
          while (length > 0 && (gap_error[length - 1] == '\n' || gap_error[length - 1] == '\r' ||
                                gap_error[length - 1] == ' '))
          {
            gap_error[--length] = '\0';
          }
        }
      }
      memset(data + offset, 0, current);
      // A failed read leaves the drive soft reset, so restore VSC mode before
      // the next chunk is attempted.
      wd_royl_enable_vsc_ccc();
      failures++;
      if (failures >= WD_ROYL_RAM_SCAN_MAX_GAP_CHUNKS)
      {
        uint64_t next;

        if (resyncs >= WD_ROYL_RAM_SCAN_MAX_RESYNCS)
        {
          break;
        }
        resyncs++;
        failures = 0;
        next = (address + offset + WD_ROYL_RAM_SCAN_RESYNC_JUMP) &
               ~(WD_ROYL_RAM_SCAN_RESYNC_JUMP - 1);
        if (next <= address + offset)
        {
          next += WD_ROYL_RAM_SCAN_RESYNC_JUMP;
        }
        if (next >= address + size)
        {
          break;
        }
        offset = (size_t)(next - address);
        continue;
      }
      offset += current;
      continue;
    }
    failures = 0;
    *readable = offset + current;
    wd_royl_progress_ccc(progress, "Reading RAM", offset + current, size);
    offset += current;
  }

  if (*readable == 0)
  {
    wd_royl_set_error_ccc("Reading the drive RAM at 0x%08llx failed: %s",
                          (unsigned long long)address,
                          (gap_error != NULL && gap_error[0] != '\0') ? gap_error : "unknown error");
    return -1;
  }
  wd_royl_progress_ccc(progress, "Reading RAM", *readable, size);
  return 0;
}

// Build a short, human readable list of the module IDs present in a RAM image.
// It is only used to explain a head map search that found nothing.
static void wd_royl_describe_ram_modules_ccc(const uint8_t *data, size_t size, char *text, size_t text_size)
{
  uint16_t seen[8];
  size_t seen_count = 0;
  size_t used = 0;
  size_t offset = 0;

  if (text_size == 0)
  {
    return;
  }
  text[0] = '\0';

  while (offset + 12 <= size && seen_count < sizeof(seen) / sizeof(seen[0]))
  {
    uint16_t id;
    size_t i;
    bool duplicate = false;

    if (memcmp(data + offset, "ROYL", 4) != 0)
    {
      offset++;
      continue;
    }
    id = wd_royl_get_module_id_ccc(data + offset);
    for (i = 0; i < seen_count; i++)
    {
      if (seen[i] == id)
      {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
    {
      int written;

      seen[seen_count++] = id;
      written = snprintf(text + used, text_size - used, "%s0x%04x@0x%08llx", used ? ", " : "", id,
                         (unsigned long long)(wd_royl_ram_base_ccc + offset));
      if (written < 0 || (size_t)written >= text_size - used)
      {
        text[text_size - 1] = '\0';
        return;
      }
      used += (size_t)written;
    }
    offset++;
  }
  if (seen_count == 0)
  {
    snprintf(text, text_size, "no ROYL module headers");
  }
}

static int wd_royl_write_ram_chunked_ccc(uint64_t address, const void *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  size_t offset;

  if (data == NULL || size == 0 || (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM write size %zu.\n", size);
    return -1;
  }

  for (offset = 0; offset < size; offset += WD_ROYL_RAM_CHUNK_SIZE)
  {
    size_t current = WD_ROYL_RAM_CHUNK_SIZE;

    if (current > size - offset)
    {
      current = size - offset;
    }
    if (wd_royl_write_ram_internal_ccc(address + offset, (const uint8_t *)data + offset, current, NULL) != 0)
    {
      return -1;
    }
    wd_royl_progress_ccc(progress, "Writing RAM", offset + current, size);
  }
  return 0;
}

// Read the DRAM size from the drive data table. This is the preferred way to
// find the RAM window because it avoids probing addresses that may not be
// mapped, which can make the drive hang until it is soft reset.
static bool wd_royl_get_data_table_dram_size_ccc(uint64_t *ram_size)
{
  uint8_t table[WD_ROYL_SECTOR_SIZE];
  uint64_t size;
  uint16_t megabytes;

  if (wd_royl_read_data_table_internal_ccc(table) != 0)
  {
    return false;
  }
  megabytes = wd_royl_le16_ccc(table + WD_ROYL_DATA_TABLE_DRAM_SIZE_OFFSET);
  size = (uint64_t)megabytes * 1024ULL * 1024ULL;
  if (size < WD_ROYL_RAM_MIN_SIZE || size > WD_ROYL_RAM_MAX_SIZE)
  {
    wd_royl_set_error_ccc("The drive data table reported an unusable DRAM size of %u MB.\n",
                          (unsigned int)megabytes);
    return false;
  }
  *ram_size = size;
  return true;
}

// A RAM window is accepted when both its base and the top of a 1 MiB probe
// range are readable. Small boot-ROM or register windows fail the second read.
#define WD_ROYL_RAM_WINDOW_CHECK_SIZE 0x00100000ULL

static const uint64_t wd_royl_ram_base_candidates[] = {
  0x00000000ULL, 0x08000000ULL, 0x04000000ULL, 0x0c000000ULL, 0x10000000ULL,
  0x20000000ULL, 0x40000000ULL, 0x80000000ULL, 0xc0000000ULL,
};

// Returns 0 when the window is readable, -1 when the base read failed, and -2
// when the base was readable but the upper probe failed.
static int wd_royl_check_ram_window_ccc(uint64_t base, uint64_t size_hint, uint8_t first[WD_ROYL_SECTOR_SIZE],
                                        char *error, size_t error_size)
{
  uint8_t high[WD_ROYL_SECTOR_SIZE];
  uint64_t check_size = WD_ROYL_RAM_WINDOW_CHECK_SIZE;
  uint64_t high_address;

  if (size_hint != 0 && size_hint < check_size)
  {
    check_size = size_hint;
  }
  if (check_size <= WD_ROYL_SECTOR_SIZE)
  {
    check_size = WD_ROYL_SECTOR_SIZE;
  }
  high_address = base + check_size - WD_ROYL_SECTOR_SIZE;

  if (wd_royl_read_ram_internal_ccc(base, first, WD_ROYL_SECTOR_SIZE, NULL) != 0)
  {
    snprintf(error, error_size, "%s", wd_royl_error_ccc);
    // The failed read may have ended in an automatic soft reset, so restore
    // VSC mode before the next attempt.
    wd_royl_enable_vsc_ccc();
    return -1;
  }
  if (wd_royl_read_ram_internal_ccc(high_address, high, sizeof(high), NULL) != 0)
  {
    snprintf(error, error_size, "%s", wd_royl_error_ccc);
    wd_royl_enable_vsc_ccc();
    return -2;
  }
  return 0;
}

// Find the RAM window base. `table_detail` describes the data table attempt
// for the error message. On success first[] holds the first sector at the base
// and wd_royl_ram_base_ccc is updated.
static int wd_royl_find_ram_base_ccc(uint64_t size_hint, uint8_t first[WD_ROYL_SECTOR_SIZE], const char *table_detail)
{
  size_t i;
  int fd = wd_royl_get_disk_fd_ccc();
  char error[WD_ROYL_ERROR_SIZE];
  char last_error[WD_ROYL_ERROR_SIZE];
  bool any_base_readable = false;
  size_t candidate_count = sizeof(wd_royl_ram_base_candidates) / sizeof(wd_royl_ram_base_candidates[0]);

  last_error[0] = '\0';

  if (wd_royl_ram_base_known_ccc && wd_royl_ram_base_fd_ccc == fd)
  {
    int result = wd_royl_check_ram_window_ccc(wd_royl_ram_base_ccc, size_hint, first, error, sizeof(error));

    if (result == 0)
    {
      return 0;
    }
    if (result == -2)
    {
      any_base_readable = true;
    }
    snprintf(last_error, sizeof(last_error), "%s", error);
    wd_royl_ram_base_known_ccc = false;
  }

  for (i = 0; i < candidate_count; i++)
  {
    int result = wd_royl_check_ram_window_ccc(wd_royl_ram_base_candidates[i], size_hint, first, error, sizeof(error));

    if (result == 0)
    {
      wd_royl_ram_base_ccc = wd_royl_ram_base_candidates[i];
      wd_royl_ram_base_known_ccc = true;
      wd_royl_ram_base_fd_ccc = fd;
      return 0;
    }
    if (result == -2)
    {
      any_base_readable = true;
    }
    snprintf(last_error, sizeof(last_error), "%s", error);
  }

  wd_royl_set_error_ccc("Could not find a usable RAM window. The data table request %s. None of the known "
                        "RAM base addresses gave a readable window; the last error was: %s%s\n",
                        table_detail, last_error,
                        any_base_readable
                          ? ""
                          : " The memory command may need a loader (module 0x0011) before RAM access "
                            "works.");
  return -1;
}

static int wd_royl_detect_ram_size_internal_ccc(uint64_t *ram_size, wd_royl_progress_callback_ccc progress)
{
  uint8_t base_data[WD_ROYL_SECTOR_SIZE];
  uint8_t probe_data[WD_ROYL_SECTOR_SIZE];
  uint64_t size;
  uint64_t base;
  bool have_size;
  char table_detail[WD_ROYL_ERROR_SIZE + 16];

  *ram_size = 0;
  table_detail[0] = '\0';

  wd_royl_progress_ccc(progress, "Detecting drive RAM", 0, 1);
  have_size = wd_royl_get_data_table_dram_size_ccc(ram_size);
  if (have_size)
  {
    snprintf(table_detail, sizeof(table_detail), "succeeded");
  }
  else
  {
    snprintf(table_detail, sizeof(table_detail), "failed (%s)", wd_royl_error_ccc);
    // A failed data table request may have ended in an automatic soft reset,
    // so make sure VSC mode is enabled again before the next request.
    wd_royl_enable_vsc_ccc();
  }

  if (wd_royl_find_ram_base_ccc(have_size ? *ram_size : 0, base_data, table_detail) != 0)
  {
    return -1;
  }
  base = wd_royl_ram_base_ccc;

  if (!have_size)
  {
    // Probe the RAM window in power of two steps. A failed probe is treated
    // as the end of the readable RAM instead of aborting, because reading an
    // unmapped address is a normal way for this probe to end. The last known
    // good offset is used as a conservative size.
    size = WD_ROYL_RAM_MIN_SIZE;
    while (size <= WD_ROYL_RAM_MAX_SIZE)
    {
      if (wd_royl_read_ram_internal_ccc(base + size, probe_data, sizeof(probe_data), progress) != 0)
      {
        // The failed read leaves the drive soft reset, so restore VSC mode
        // before the caller continues to read the RAM it just detected.
        wd_royl_enable_vsc_ccc();
        *ram_size = (size > WD_ROYL_RAM_MIN_SIZE) ? (size / 2) : size;
        wd_royl_progress_ccc(progress, "Detecting drive RAM", 1, 1);
        return 0;
      }
      if (memcmp(base_data, probe_data, sizeof(base_data)) == 0)
      {
        *ram_size = size;
        wd_royl_progress_ccc(progress, "Detecting drive RAM", 1, 1);
        return 0;
      }
      size *= 2;
    }
    *ram_size = WD_ROYL_RAM_MAX_SIZE;
  }

  wd_royl_progress_ccc(progress, "Detecting drive RAM", 1, 1);
  return 0;
}

int wd_royl_find_ram_size_ccc(uint64_t *ram_size, wd_royl_progress_callback_ccc progress)
{
  int result = -1;
  bool enabled = false;

  if (ram_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no RAM size destination.\n");
    return -1;
  }
  *ram_size = 0;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  if (wd_royl_detect_ram_size_internal_ccc(ram_size, progress) == 0)
  {
    result = 0;
  }

  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The RAM size was detected, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_read_ram_ccc(uint64_t address, void *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  int result = -1;
  bool enabled = false;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;
  if (wd_royl_read_ram_chunked_ccc(address, data, size, progress) == 0)
  {
    result = 0;
  }
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("RAM was read, but vendor specific commands could not be disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_write_ram_ccc(uint64_t address, const void *data, size_t size, wd_royl_progress_callback_ccc progress)
{
  int result = -1;
  bool enabled = false;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;
  if (wd_royl_write_ram_chunked_ccc(address, data, size, progress) == 0)
  {
    result = 0;
  }
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("RAM was written, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_patch_ram_modules_ccc(unsigned int module_mask, unsigned int *module_02_count,
                                  unsigned int *module_32_count, uint64_t *scanned_end, uint64_t *first_gap,
                                  wd_royl_progress_callback_ccc progress)
{
  uint8_t *ram = NULL;
  uint8_t *verify = NULL;
  uint64_t ram_size = 0;
  uint64_t address;
  wd_royl_ram_scan_info_ccc scan;
  char gap_note[96];
  size_t scanned_size;
  size_t offset;
  unsigned int count_02 = 0;
  unsigned int count_32 = 0;
  int result = -1;
  bool enabled = false;

  if (module_mask == 0)
  {
    wd_royl_set_error_ccc("No slow fix modules were selected.\n");
    return -1;
  }
  if (module_02_count != NULL)
  {
    *module_02_count = 0;
  }
  if (module_32_count != NULL)
  {
    *module_32_count = 0;
  }
  if (scanned_end != NULL)
  {
    *scanned_end = 0;
  }
  if (first_gap != NULL)
  {
    *first_gap = 0;
  }

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  // Use the same gap tolerant scan as the head map reader. Some firmware
  // reports more DRAM than the memory command can reach.
  if (wd_royl_load_ram_image_ccc(&ram, &ram_size, &scan, progress) != 0)
  {
    goto finish;
  }
  if (scanned_end != NULL)
  {
    *scanned_end = scan.readable_end;
  }
  if (first_gap != NULL)
  {
    *first_gap = scan.first_gap;
  }
  scanned_size = wd_royl_scanned_ram_size_ccc(&scan, ram_size);
  if (scan.first_gap != 0)
  {
    snprintf(gap_note, sizeof(gap_note), "; the first unreadable address was 0x%08llx",
             (unsigned long long)scan.first_gap);
  }
  else
  {
    gap_note[0] = '\0';
  }

  verify = (uint8_t *)malloc(WD_ROYL_MAX_FILE_SIZE);
  if (verify == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate the RAM verify buffer.\n");
    goto finish;
  }

  offset = 0;
  while (offset + WD_ROYL_SECTOR_SIZE <= scanned_size)
  {
    uint8_t *module = ram + offset;
    uint16_t module_id;
    uint16_t sectors;
    size_t module_size;
    int patch_result = 0;

    if (memcmp(module, "ROYL", 4) != 0)
    {
      offset += WD_ROYL_SECTOR_SIZE;
      continue;
    }

    module_id = wd_royl_get_module_id_ccc(module);
    if (!((module_id == WD_ROYL_MODULE_02 && (module_mask & WD_ROYL_MODULE_MASK_02)) ||
          (module_id == WD_ROYL_MODULE_32 && (module_mask & WD_ROYL_MODULE_MASK_32))))
    {
      offset += WD_ROYL_SECTOR_SIZE;
      continue;
    }

    sectors = wd_royl_get_module_sector_count_ccc(module);
    module_size = (size_t)sectors * WD_ROYL_SECTOR_SIZE;
    if (sectors == 0 || module_size > WD_ROYL_MAX_FILE_SIZE || offset + module_size > scanned_size)
    {
      offset += WD_ROYL_SECTOR_SIZE;
      continue;
    }
    if (!wd_royl_module_valid_ccc(module, module_size, module_id))
    {
      offset += WD_ROYL_SECTOR_SIZE;
      continue;
    }

    wd_royl_progress_ccc(progress, "Patching RAM module", offset, ram_size);
    if (module_id == WD_ROYL_MODULE_02)
    {
      patch_result = wd_royl_patch_module_02_ccc(module, module_size);
    }
    else
    {
      patch_result = wd_royl_patch_module_32_ccc(module, module_size);
    }
    if (patch_result != 0)
    {
      goto finish;
    }

    address = wd_royl_ram_base_ccc + offset;
    if (wd_royl_write_ram_chunked_ccc(address, module, module_size, progress) != 0)
    {
      goto finish;
    }
    if (wd_royl_read_ram_chunked_ccc(address, verify, module_size, progress) != 0)
    {
      goto finish;
    }
    if (memcmp(verify, module, module_size) != 0)
    {
      wd_royl_set_error_ccc("RAM verification failed for module 0x%04x at address "
                            "0x%08llx.\n",
                            module_id, (unsigned long long)address);
      goto finish;
    }

    if (module_id == WD_ROYL_MODULE_02)
    {
      count_02++;
    }
    else
    {
      count_32++;
    }
    offset += module_size;
  }

  if (((module_mask & WD_ROYL_MODULE_MASK_02) && count_02 == 0) ||
      ((module_mask & WD_ROYL_MODULE_MASK_32) && count_32 == 0))
  {
    bool missing_02 = (module_mask & WD_ROYL_MODULE_MASK_02) && count_02 == 0;
    bool missing_32 = (module_mask & WD_ROYL_MODULE_MASK_32) && count_32 == 0;
    const char *written = (count_02 != 0 || count_32 != 0) ? "The other module was patched and verified"
                                                           : "Nothing was written";

    if (missing_02 && missing_32)
    {
      wd_royl_set_error_ccc("Neither module 02 nor module 32 was found in the drive RAM "
                            "(scanned 0x%08llx to 0x%08llx%s). %s.\n",
                            (unsigned long long)wd_royl_ram_base_ccc,
                            (unsigned long long)scan.readable_end, gap_note, written);
    }
    else if (missing_02)
    {
      wd_royl_set_error_ccc("Module 02 was not found in the drive RAM (scanned 0x%08llx to "
                            "0x%08llx%s). %s.\n",
                            (unsigned long long)wd_royl_ram_base_ccc,
                            (unsigned long long)scan.readable_end, gap_note, written);
    }
    else
    {
      wd_royl_set_error_ccc("Module 32 was not found in the drive RAM (scanned 0x%08llx to "
                            "0x%08llx%s). %s.\n",
                            (unsigned long long)wd_royl_ram_base_ccc,
                            (unsigned long long)scan.readable_end, gap_note, written);
    }
    goto finish;
  }

  if (module_02_count != NULL)
  {
    *module_02_count = count_02;
  }
  if (module_32_count != NULL)
  {
    *module_32_count = count_32;
  }
  result = 0;

finish:
  free(ram);
  free(verify);
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The RAM patch completed, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

static unsigned int wd_royl_head_map_popcount_ccc(uint16_t value)
{
  unsigned int count = 0;

  while (value != 0)
  {
    count += value & 1u;
    value >>= 1;
  }
  return count;
}

// Validate a requested head map before it is written. Only the physical map,
// the active map, and the maximum head count are constrained here; the in-use
// count is always recomputed from the active map.
static bool wd_royl_head_map_request_valid_ccc(const wd_royl_head_map_ccc *map)
{
  if (map->physical_count < 1 || map->physical_count > WD_ROYL_HEAD_MAP_MAX_HEADS ||
      map->physical_map == 0)
  {
    wd_royl_set_error_ccc("The requested physical head map is not valid.\n");
    return false;
  }
  if (map->active_map == 0)
  {
    wd_royl_set_error_ccc("At least one head must remain enabled.\n");
    return false;
  }
  if ((map->active_map & ~map->physical_map) != 0)
  {
    wd_royl_set_error_ccc("The requested active head map contains a head that is not present.\n");
    return false;
  }
  return true;
}

// Build the active map for a live copy. Heads that differ between the base and
// the requested map are taken from the request; every other head keeps the
// state the live copy already has.
static uint16_t wd_royl_head_map_merge_active_ccc(uint16_t base_active, uint16_t requested_active,
                                                  uint16_t live_active)
{
  uint16_t changed = base_active ^ requested_active;

  return (uint16_t)((live_active & ~changed) | (requested_active & changed));
}

static bool wd_royl_head_map_header_valid_ccc(const uint8_t *module, size_t size, uint16_t expected_module)
{
  uint16_t sectors;

  if (module == NULL || size < WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET + 2)
  {
    return false;
  }
  if (memcmp(module, "ROYL", 4) != 0)
  {
    return false;
  }
  if (expected_module != 0 && wd_royl_get_module_id_ccc(module) != expected_module)
  {
    return false;
  }
  sectors = wd_royl_get_module_sector_count_ccc(module);
  // Some ROM images store a module compactly and end it at the next ROYL
  // header instead of padding it to sectors * 512 bytes. Accept any length
  // up to the declared size; the caller determines the actual stored length.
  if (sectors == 0 || size > (size_t)sectors * WD_ROYL_SECTOR_SIZE)
  {
    return false;
  }
  return true;
}

static bool wd_royl_head_map_parse_internal_ccc(const uint8_t *module, size_t size, uint16_t expected_module,
                                                wd_royl_head_map_ccc *map)
{
  unsigned int physical_bits;
  unsigned int active_bits;

  if (map == NULL)
  {
    return false;
  }

  memset(map, 0, sizeof(*map));
  map->header_valid = wd_royl_head_map_header_valid_ccc(module, size, expected_module);
  if (!map->header_valid)
  {
    return false;
  }

  map->physical_count = module[WD_ROYL_HEAD_MAP_PHYSICAL_COUNT_OFFSET];
  map->in_use_count = module[WD_ROYL_HEAD_MAP_IN_USE_COUNT_OFFSET];
  map->active_map = wd_royl_le16_ccc(module + WD_ROYL_HEAD_MAP_ACTIVE_MAP_OFFSET);
  map->physical_map = wd_royl_le16_ccc(module + WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET);
  map->vendor_field_3c = wd_royl_le16_ccc(module + WD_ROYL_HEAD_MAP_VENDOR_FIELD_OFFSET);
  map->module_checksum_valid =
    (wd_royl_get_module_checksum_ccc(module) == wd_royl_calculate_checksum_ccc(module, size));

  physical_bits = wd_royl_head_map_popcount_ccc(map->physical_map);
  active_bits = wd_royl_head_map_popcount_ccc(map->active_map);
  map->counts_match_maps = (physical_bits == map->physical_count && active_bits == map->in_use_count);

  map->valid = (map->physical_count >= 1 && map->physical_count <= WD_ROYL_HEAD_MAP_MAX_HEADS &&
                map->in_use_count >= 1 && map->in_use_count <= map->physical_count &&
                map->physical_map != 0 && map->active_map != 0 &&
                (map->active_map & ~map->physical_map) == 0);
  return map->valid;
}

int wd_royl_head_map_parse_ccc(const uint8_t *module, size_t size, uint16_t expected_module, wd_royl_head_map_ccc *map)
{
  if (!wd_royl_head_map_parse_internal_ccc(module, size, expected_module, map))
  {
    wd_royl_set_error_ccc("The module does not contain a valid WD ROYL head map.\n");
    return -1;
  }
  return 0;
}

int wd_royl_head_map_build_ccc(uint8_t *module, size_t size, uint16_t expected_module,
                               const wd_royl_head_map_ccc *map)
{
  unsigned int active_bits;

  if (module == NULL || map == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no head map data to build.\n");
    return -1;
  }
  if (!wd_royl_head_map_header_valid_ccc(module, size, expected_module))
  {
    wd_royl_set_error_ccc("The module does not have a valid WD ROYL head map header.\n");
    return -1;
  }
  if (!wd_royl_head_map_request_valid_ccc(map))
  {
    return -1;
  }
  if (module[WD_ROYL_HEAD_MAP_PHYSICAL_COUNT_OFFSET] != map->physical_count ||
      wd_royl_le16_ccc(module + WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET) != map->physical_map)
  {
    wd_royl_set_error_ccc("The requested head map does not match the module's physical head map.\n");
    return -1;
  }

  module[WD_ROYL_HEAD_MAP_ACTIVE_MAP_OFFSET] = (uint8_t)(map->active_map & 0xff);
  module[WD_ROYL_HEAD_MAP_ACTIVE_MAP_OFFSET + 1] = (uint8_t)(map->active_map >> 8);
  active_bits = wd_royl_head_map_popcount_ccc(map->active_map);
  module[WD_ROYL_HEAD_MAP_IN_USE_COUNT_OFFSET] = (uint8_t)active_bits;

  // The field at 0x3C is opaque vendor data and is left untouched.
  wd_royl_update_checksum_ccc(module, size);
  return 0;
}

// Determine the stored length of the module at buffer+offset. Most modules
// occupy sectors * 512 bytes, but some ROM images store a module compactly and
// place the next ROYL header directly after its last byte. Try the declared
// length first, then any plausible ROYL boundary inside it. *checksum_valid is
// set to whether the module checksum matches over the chosen length.
static bool wd_royl_module_stored_size_ccc(const uint8_t *buffer, size_t size, size_t offset,
                                           size_t *module_size, bool *checksum_valid)
{
  uint16_t sectors;
  size_t available;
  size_t declared;
  size_t limit;
  size_t boundary;
  size_t compact_size = 0;

  if (buffer == NULL || module_size == NULL || offset + 0x0c > size)
  {
    return false;
  }
  sectors = wd_royl_get_module_sector_count_ccc(buffer + offset);
  if (sectors == 0)
  {
    return false;
  }
  available = size - offset;
  declared = (size_t)sectors * WD_ROYL_SECTOR_SIZE;

  if (declared <= available &&
      wd_royl_get_module_checksum_ccc(buffer + offset) ==
        wd_royl_calculate_checksum_ccc(buffer + offset, declared))
  {
    *module_size = declared;
    if (checksum_valid != NULL)
    {
      *checksum_valid = true;
    }
    return true;
  }

  limit = declared < available ? declared : available;

  // A caller may supply exactly the stored bytes of a compact module, with no
  // following header to mark its end.
  if (declared > available && available >= WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET + 2 &&
      wd_royl_get_module_checksum_ccc(buffer + offset) ==
        wd_royl_calculate_checksum_ccc(buffer + offset, available))
  {
    *module_size = available;
    if (checksum_valid != NULL)
    {
      *checksum_valid = true;
    }
    return true;
  }

  for (boundary = WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET + 2; boundary + 0x0c <= limit; boundary++)
  {
    if (memcmp(buffer + offset + boundary, "ROYL", 4) != 0)
    {
      continue;
    }
    if (wd_royl_get_module_sector_count_ccc(buffer + offset + boundary) == 0)
    {
      continue;
    }
    if (compact_size == 0)
    {
      compact_size = boundary;
    }
    if (wd_royl_get_module_checksum_ccc(buffer + offset) ==
        wd_royl_calculate_checksum_ccc(buffer + offset, boundary))
    {
      *module_size = boundary;
      if (checksum_valid != NULL)
      {
        *checksum_valid = true;
      }
      return true;
    }
  }

  if (declared <= available)
  {
    *module_size = declared;
    if (checksum_valid != NULL)
    {
      *checksum_valid = false;
    }
    return true;
  }
  if (compact_size != 0)
  {
    *module_size = compact_size;
    if (checksum_valid != NULL)
    {
      *checksum_valid = false;
    }
    return true;
  }
  return false;
}

static bool wd_royl_find_module_in_buffer_internal_ccc(const uint8_t *buffer, size_t size, uint16_t module_id,
                                                       size_t *module_offset, size_t *module_size,
                                                       bool require_checksum)
{
  size_t offset = 0;

  while (offset + 0x0c <= size)
  {
    size_t current_size = 0;
    bool checksum_valid = false;

    if (memcmp(buffer + offset, "ROYL", 4) != 0)
    {
      offset++;
      continue;
    }
    if (wd_royl_get_module_id_ccc(buffer + offset) != module_id)
    {
      offset++;
      continue;
    }
    if (size - offset < WD_ROYL_HEAD_MAP_PHYSICAL_MAP_OFFSET + 2)
    {
      break;
    }
    if (!wd_royl_module_stored_size_ccc(buffer, size, offset, &current_size, &checksum_valid))
    {
      offset++;
      continue;
    }
    if ((!require_checksum || checksum_valid) &&
        wd_royl_head_map_header_valid_ccc(buffer + offset, current_size, module_id))
    {
      *module_offset = offset;
      *module_size = current_size;
      return true;
    }
    offset += current_size;
  }
  return false;
}

int wd_royl_find_module_in_buffer_ccc(const uint8_t *buffer, size_t size, uint16_t module_id, size_t *module_offset,
                                      size_t *module_size)
{
  if (buffer == NULL || size == 0 || module_offset == NULL || module_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: invalid module search request.\n");
    return -1;
  }
  if (!wd_royl_find_module_in_buffer_internal_ccc(buffer, size, module_id, module_offset, module_size, false))
  {
    wd_royl_set_error_ccc("Module 0x%04x was not found in the buffer.\n", module_id);
    return -1;
  }
  return 0;
}

typedef struct
{
  size_t offset;
  size_t size;
  wd_royl_head_map_ccc map;
} wd_royl_head_map_candidate_ccc;

// Collect every module copy that parses as a sane head map. A ROM can contain
// more than one copy of module 0x0A, including an erased factory template with
// zero maps, so callers must not stop at the first matching header.
static size_t wd_royl_collect_head_map_candidates_ccc(const uint8_t *buffer, size_t size, uint16_t module_id,
                                                      wd_royl_head_map_candidate_ccc *candidates,
                                                      size_t max_candidates)
{
  size_t offset = 0;
  size_t count = 0;

  while (count < max_candidates && offset + 0x0c <= size)
  {
    size_t module_size = 0;

    if (memcmp(buffer + offset, "ROYL", 4) != 0)
    {
      offset++;
      continue;
    }
    if (wd_royl_get_module_id_ccc(buffer + offset) != module_id)
    {
      offset++;
      continue;
    }
    if (!wd_royl_module_stored_size_ccc(buffer, size, offset, &module_size, NULL))
    {
      offset++;
      continue;
    }
    {
      wd_royl_head_map_ccc map;

      if (wd_royl_head_map_parse_internal_ccc(buffer + offset, module_size, module_id, &map))
      {
        candidates[count].offset = offset;
        candidates[count].size = module_size;
        candidates[count].map = map;
        count++;
        offset += module_size;
        continue;
      }
    }
    // A rejected header may sit inside a larger valid module, so only skip the
    // header itself instead of its declared size.
    offset++;
  }
  return count;
}

// Prefer a copy with a valid module checksum and self-consistent counts. Later
// copies win ties, which matches the usual layout where the active copy follows
// the erased template.
static size_t wd_royl_select_head_map_candidate_ccc(const wd_royl_head_map_candidate_ccc *candidates, size_t count)
{
  size_t selected = 0;
  size_t i;
  int best_score = -1;

  for (i = 0; i < count; i++)
  {
    int score = 0;

    if (candidates[i].map.module_checksum_valid)
    {
      score += 4;
    }
    if (candidates[i].map.counts_match_maps)
    {
      score += 2;
    }
    if (score >= best_score)
    {
      best_score = score;
      selected = i;
    }
  }
  return selected;
}

int wd_royl_find_rom_head_map_ccc(const uint8_t *rom, size_t rom_size, size_t *module_offset, size_t *module_size)
{
  wd_royl_head_map_candidate_ccc candidates[16];
  size_t count;
  size_t selected;

  if (rom == NULL || rom_size == 0 || module_offset == NULL || module_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: invalid head map search request.\n");
    return -1;
  }
  count = wd_royl_collect_head_map_candidates_ccc(rom, rom_size, WD_ROYL_MODULE_0A, candidates,
                                                  sizeof(candidates) / sizeof(candidates[0]));
  if (count == 0)
  {
    wd_royl_set_error_ccc("No usable head map module 0x000A was found in the buffer.\n");
    return -1;
  }
  selected = wd_royl_select_head_map_candidate_ccc(candidates, count);
  *module_offset = candidates[selected].offset;
  *module_size = candidates[selected].size;
  return 0;
}

static int wd_royl_read_rom_head_map_ccc(uint8_t **module, size_t *module_size, uint64_t *address,
                                         wd_royl_progress_callback_ccc progress)
{
  uint8_t *rom = NULL;
  size_t rom_size = 0;
  wd_royl_head_map_candidate_ccc candidates[16];
  size_t count;
  size_t selected;
  int result = -1;

  *module = NULL;
  *module_size = 0;
  if (address != NULL)
  {
    *address = 0;
  }

  if (wd_royl_read_rom_ccc(&rom, &rom_size, progress) != 0)
  {
    free(rom);
    return -1;
  }
  count = wd_royl_collect_head_map_candidates_ccc(rom, rom_size, WD_ROYL_MODULE_0A, candidates,
                                                  sizeof(candidates) / sizeof(candidates[0]));
  if (count == 0)
  {
    wd_royl_set_error_ccc("No usable head map module 0x000A was found in the drive ROM.\n");
    goto finish;
  }
  selected = wd_royl_select_head_map_candidate_ccc(candidates, count);

  *module = (uint8_t *)malloc(candidates[selected].size);
  if (*module == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %zu bytes for the ROM head map.\n", candidates[selected].size);
    goto finish;
  }
  memcpy(*module, rom + candidates[selected].offset, candidates[selected].size);
  *module_size = candidates[selected].size;
  if (address != NULL)
  {
    *address = candidates[selected].offset;
  }
  result = 0;

finish:
  free(rom);
  return result;
}

static int wd_royl_read_sa_head_map_ccc(uint8_t **module, size_t *module_size, uint64_t *address,
                                        wd_royl_progress_callback_ccc progress)
{
  uint8_t *data = NULL;
  size_t size = 0;
  wd_royl_head_map_ccc map;

  *module = NULL;
  *module_size = 0;
  if (address != NULL)
  {
    *address = 0;
  }

  if (wd_royl_read_module_ccc(WD_ROYL_MODULE_0A_SA, &data, &size, progress) != 0)
  {
    free(data);
    return -1;
  }
  if (size >= 0x18 && memcmp(data + 0x10, "NOT_INIT", 8) == 0)
  {
    free(data);
    wd_royl_set_error_ccc("Service area module 0x0102 is a NOT_INIT placeholder. This drive does "
                          "not store a usable head map there; read the ROM module 0x000A instead.\n");
    return -1;
  }
  if (!wd_royl_head_map_parse_internal_ccc(data, size, WD_ROYL_MODULE_0A_SA, &map))
  {
    free(data);
    wd_royl_set_error_ccc("The service area head map module 0x0102 does not contain a valid head "
                          "map.\n");
    return -1;
  }

  *module = data;
  *module_size = size;
  return 0;
}

// Load the RAM window into memory. The caller must have VSC enabled. The image
// always has the size reported by the drive so that offsets inside it map
// directly to addresses; unreadable gaps are left zeroed.
static int wd_royl_load_ram_image_ccc(uint8_t **ram, uint64_t *ram_size, wd_royl_ram_scan_info_ccc *info,
                                      wd_royl_progress_callback_ccc progress)
{
  uint8_t *buffer = NULL;
  uint64_t size = 0;
  size_t readable = 0;

  *ram = NULL;
  *ram_size = 0;
  if (info != NULL)
  {
    info->readable_end = 0;
    info->first_gap = 0;
    info->gap_error[0] = '\0';
  }

  if (wd_royl_detect_ram_size_internal_ccc(&size, progress) != 0)
  {
    return -1;
  }
  buffer = (uint8_t *)malloc((size_t)size);
  if (buffer == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %llu bytes for the RAM dump.\n", (unsigned long long)size);
    return -1;
  }
  memset(buffer, 0, (size_t)size);

  wd_royl_progress_ccc(progress, "Reading RAM", 0, size);
  if (wd_royl_scan_ram_ccc(wd_royl_ram_base_ccc, buffer, (size_t)size, &readable,
                           info != NULL ? &info->first_gap : NULL,
                           info != NULL ? info->gap_error : NULL,
                           info != NULL ? sizeof(info->gap_error) : 0, progress) != 0)
  {
    free(buffer);
    return -1;
  }

  if (info != NULL)
  {
    info->readable_end = wd_royl_ram_base_ccc + readable;
  }
  *ram = buffer;
  *ram_size = size;
  return 0;
}

static int wd_royl_read_ram_image_ccc(uint8_t **ram, uint64_t *ram_size, wd_royl_ram_scan_info_ccc *info,
                                      wd_royl_progress_callback_ccc progress)
{
  int result;

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  result = wd_royl_load_ram_image_ccc(ram, ram_size, info, progress);
  if (wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The RAM was read, but vendor specific commands could not be disabled.\n");
    result = -1;
  }
  if (result != 0)
  {
    free(*ram);
    *ram = NULL;
    *ram_size = 0;
  }
  return result;
}

// The RAM image is always allocated at the full reported size so that offsets
// map directly to addresses. Only the part up to the last readable address can
// contain data, so searches must not run over the zero filled remainder.
static size_t wd_royl_scanned_ram_size_ccc(const wd_royl_ram_scan_info_ccc *scan, uint64_t ram_size)
{
  uint64_t readable;

  if (scan == NULL || scan->readable_end <= wd_royl_ram_base_ccc)
  {
    return 0;
  }
  readable = scan->readable_end - wd_royl_ram_base_ccc;
  return (size_t)(readable < ram_size ? readable : ram_size);
}

static void wd_royl_set_ram_head_map_missing_error_ccc(const uint8_t *ram, size_t ram_size,
                                                       const wd_royl_ram_scan_info_ccc *scan)
{
  char modules[256];

  wd_royl_describe_ram_modules_ccc(ram, ram_size, modules, sizeof(modules));
  if (scan != NULL && scan->first_gap != 0)
  {
    wd_royl_set_error_ccc("No usable head map module 0x000A was found in the drive RAM. "
                          "The scan read 0x%08llx to 0x%08llx; the first unreadable address was "
                          "0x%08llx (%s). ROYL modules found: %s. Nothing was written.\n",
                          (unsigned long long)wd_royl_ram_base_ccc,
                          (unsigned long long)(wd_royl_ram_base_ccc + ram_size),
                          (unsigned long long)scan->first_gap,
                          scan->gap_error[0] != '\0' ? scan->gap_error : "unknown error", modules);
  }
  else
  {
    wd_royl_set_error_ccc("No usable head map module 0x000A was found in the drive RAM. "
                          "The scan read 0x%08llx to 0x%08llx without read errors. "
                          "ROYL modules found: %s. Nothing was written.\n",
                          (unsigned long long)wd_royl_ram_base_ccc,
                          (unsigned long long)(wd_royl_ram_base_ccc + ram_size), modules);
  }
}

static int wd_royl_read_ram_head_map_ccc(uint8_t **module, size_t *module_size, uint64_t *address,
                                         wd_royl_progress_callback_ccc progress)
{
  uint8_t *ram = NULL;
  uint64_t ram_size = 0;
  wd_royl_head_map_candidate_ccc candidates[64];
  wd_royl_ram_scan_info_ccc scan;
  size_t scanned_size;
  size_t count;
  size_t selected;
  int result = -1;

  *module = NULL;
  *module_size = 0;
  if (address != NULL)
  {
    *address = 0;
  }

  if (wd_royl_read_ram_image_ccc(&ram, &ram_size, &scan, progress) != 0)
  {
    return -1;
  }
  scanned_size = wd_royl_scanned_ram_size_ccc(&scan, ram_size);
  count = wd_royl_collect_head_map_candidates_ccc(ram, scanned_size, WD_ROYL_MODULE_0A, candidates,
                                                  sizeof(candidates) / sizeof(candidates[0]));
  if (count == 0)
  {
    wd_royl_set_ram_head_map_missing_error_ccc(ram, scanned_size, &scan);
    goto finish;
  }
  selected = wd_royl_select_head_map_candidate_ccc(candidates, count);

  *module = (uint8_t *)malloc(candidates[selected].size);
  if (*module == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %zu bytes for the RAM head map.\n", candidates[selected].size);
    goto finish;
  }
  memcpy(*module, ram + candidates[selected].offset, candidates[selected].size);
  *module_size = candidates[selected].size;
  if (address != NULL)
  {
    *address = wd_royl_ram_base_ccc + candidates[selected].offset;
  }
  result = 0;

finish:
  free(ram);
  return result;
}

int wd_royl_read_head_map_ccc(wd_royl_head_map_source_ccc source, uint8_t **module, size_t *module_size,
                              uint64_t *address, wd_royl_head_map_source_ccc *actual_source,
                              wd_royl_progress_callback_ccc progress)
{
  if (module == NULL || module_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: no head map destination.\n");
    return -1;
  }
  *module = NULL;
  *module_size = 0;
  if (address != NULL)
  {
    *address = 0;
  }
  if (actual_source != NULL)
  {
    *actual_source = source;
  }

  if (source == WD_ROYL_HEAD_MAP_SOURCE_AUTO)
  {
    uint8_t *fast_module = NULL;
    size_t fast_size = 0;
    wd_royl_head_map_ccc map;

    // Some firmware exposes ROM module 0x0A through the normal module read
    // command. This is much faster than reading the whole ROM.
    if (wd_royl_read_module_ccc(WD_ROYL_MODULE_0A, &fast_module, &fast_size, progress) == 0 &&
        wd_royl_head_map_parse_internal_ccc(fast_module, fast_size, WD_ROYL_MODULE_0A, &map))
    {
      *module = fast_module;
      *module_size = fast_size;
      if (actual_source != NULL)
      {
        *actual_source = WD_ROYL_HEAD_MAP_SOURCE_ROM;
      }
      return 0;
    }
    free(fast_module);

    if (wd_royl_read_rom_head_map_ccc(module, module_size, address, progress) == 0)
    {
      if (actual_source != NULL)
      {
        *actual_source = WD_ROYL_HEAD_MAP_SOURCE_ROM;
      }
      return 0;
    }
    if (wd_royl_read_sa_head_map_ccc(module, module_size, address, progress) == 0)
    {
      if (actual_source != NULL)
      {
        *actual_source = WD_ROYL_HEAD_MAP_SOURCE_SA;
      }
      return 0;
    }

    wd_royl_set_error_ccc("No WD ROYL head map was found in ROM module 0x000A or service area "
                          "module 0x0102.\n");
    return -1;
  }

  switch (source)
  {
    case WD_ROYL_HEAD_MAP_SOURCE_ROM:
      return wd_royl_read_rom_head_map_ccc(module, module_size, address, progress);
    case WD_ROYL_HEAD_MAP_SOURCE_SA:
      return wd_royl_read_sa_head_map_ccc(module, module_size, address, progress);
    case WD_ROYL_HEAD_MAP_SOURCE_RAM:
      return wd_royl_read_ram_head_map_ccc(module, module_size, address, progress);
    default:
      wd_royl_set_error_ccc("Internal error: unknown head map source.\n");
      return -1;
  }
}

int wd_royl_apply_head_map_ram_ccc(const wd_royl_head_map_ccc *base_map, const wd_royl_head_map_ccc *map,
                                   unsigned int *copy_count, uint16_t *applied_active,
                                   wd_royl_progress_callback_ccc progress)
{
  uint8_t *ram = NULL;
  uint64_t ram_size = 0;
  wd_royl_head_map_candidate_ccc candidates[64];
  wd_royl_ram_scan_info_ccc scan;
  size_t scanned_size;
  size_t count;
  size_t selected;
  size_t i;
  unsigned int patched = 0;
  int result = -1;
  bool enabled = false;

  if (copy_count != NULL)
  {
    *copy_count = 0;
  }
  if (applied_active != NULL)
  {
    *applied_active = 0;
  }
  if (base_map == NULL || map == NULL)
  {
    wd_royl_set_error_ccc("There is no head map to apply.\n");
    return -1;
  }
  if (!wd_royl_head_map_request_valid_ccc(map))
  {
    return -1;
  }
  if (base_map->physical_count != map->physical_count || base_map->physical_map != map->physical_map)
  {
    wd_royl_set_error_ccc("The requested head map does not match the loaded physical head map.\n");
    return -1;
  }

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  if (wd_royl_load_ram_image_ccc(&ram, &ram_size, &scan, progress) != 0)
  {
    goto finish;
  }

  scanned_size = wd_royl_scanned_ram_size_ccc(&scan, ram_size);
  count = wd_royl_collect_head_map_candidates_ccc(ram, scanned_size, WD_ROYL_MODULE_0A, candidates,
                                                  sizeof(candidates) / sizeof(candidates[0]));
  if (count == 0)
  {
    wd_royl_set_ram_head_map_missing_error_ccc(ram, scanned_size, &scan);
    goto finish;
  }
  selected = wd_royl_select_head_map_candidate_ccc(candidates, count);

  // Every candidate must describe the same physical head stack. Otherwise
  // the map belongs to a different firmware copy and must not be patched.
  // Merge the requested changes into each live copy before anything is
  // written, so an invalid result aborts without touching the drive.
  for (i = 0; i < count; i++)
  {
    uint16_t merged;

    if (candidates[i].map.physical_count != map->physical_count ||
        candidates[i].map.physical_map != map->physical_map)
    {
      wd_royl_set_error_ccc("The RAM head map does not match the loaded physical head map.\n");
      goto finish;
    }
    merged = wd_royl_head_map_merge_active_ccc(base_map->active_map, map->active_map,
                                               candidates[i].map.active_map);
    if (merged == 0)
    {
      wd_royl_set_error_ccc("The requested head map would leave a RAM copy with no enabled heads.\n");
      goto finish;
    }
    if ((merged & ~map->physical_map) != 0)
    {
      wd_royl_set_error_ccc("The RAM head map contains a head that is not present.\n");
      goto finish;
    }
    candidates[i].map.active_map = merged;
  }

  for (i = 0; i < count; i++)
  {
    size_t aligned_start = candidates[i].offset & ~((size_t)WD_ROYL_SECTOR_SIZE - 1);
    size_t aligned_end = (candidates[i].offset + candidates[i].size + WD_ROYL_SECTOR_SIZE - 1) &
                         ~((size_t)WD_ROYL_SECTOR_SIZE - 1);
    size_t aligned_size = aligned_end - aligned_start;
    size_t module_offset = candidates[i].offset - aligned_start;
    uint8_t *sector = NULL;
    uint8_t *verify = NULL;

    if (aligned_end > scanned_size)
    {
      wd_royl_set_error_ccc("The RAM head map at 0x%08llx reaches outside the readable RAM.\n",
                            (unsigned long long)(wd_royl_ram_base_ccc + candidates[i].offset));
      goto finish;
    }

    sector = (uint8_t *)malloc(aligned_size);
    verify = (uint8_t *)malloc(aligned_size);
    if (sector == NULL || verify == NULL)
    {
      free(sector);
      free(verify);
      wd_royl_set_error_ccc("Unable to allocate the RAM head map patch buffer.\n");
      goto finish;
    }

    memcpy(sector, ram + aligned_start, aligned_size);
    if (wd_royl_head_map_build_ccc(sector + module_offset, candidates[i].size, WD_ROYL_MODULE_0A,
                                   &candidates[i].map) != 0)
    {
      free(sector);
      free(verify);
      goto finish;
    }

    wd_royl_progress_ccc(progress, "Patching RAM head map", i, count);
    if (wd_royl_write_ram_chunked_ccc(wd_royl_ram_base_ccc + aligned_start, sector, aligned_size, progress) != 0)
    {
      free(sector);
      free(verify);
      goto finish;
    }
    if (wd_royl_read_ram_chunked_ccc(wd_royl_ram_base_ccc + aligned_start, verify, aligned_size, progress) != 0)
    {
      free(sector);
      free(verify);
      goto finish;
    }
    if (memcmp(verify, sector, aligned_size) != 0)
    {
      free(sector);
      free(verify);
      wd_royl_set_error_ccc("RAM verification failed for the head map at 0x%08llx.\n",
                            (unsigned long long)(wd_royl_ram_base_ccc + candidates[i].offset));
      goto finish;
    }

    // Keep the in-memory image current in case two candidates share a sector.
    memcpy(ram + aligned_start, sector, aligned_size);
    free(sector);
    free(verify);
    patched++;
  }

  if (copy_count != NULL)
  {
    *copy_count = patched;
  }
  if (applied_active != NULL)
  {
    *applied_active = candidates[selected].map.active_map;
  }
  result = 0;

finish:
  free(ram);
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc("The RAM head map was patched, but vendor specific commands could not be "
                          "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_read_file_ccc(const char *filename, uint8_t **data, size_t *size)
{
  FILE *file = NULL;
  long file_size;
  uint8_t *buffer = NULL;

  if (filename == NULL || data == NULL || size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: invalid file read request.\n");
    return -1;
  }
  *data = NULL;
  *size = 0;

  file = fopen(filename, "rb");
  if (file == NULL)
  {
    wd_royl_set_error_ccc("Unable to open %s for reading (%s).\n", filename, strerror(errno));
    return -1;
  }
  if (fseek(file, 0, SEEK_END) != 0)
  {
    wd_royl_set_error_ccc("Unable to seek in %s (%s).\n", filename, strerror(errno));
    goto finish;
  }
  file_size = ftell(file);
  if (file_size < 0)
  {
    wd_royl_set_error_ccc("Unable to get the size of %s (%s).\n", filename, strerror(errno));
    goto finish;
  }
  if (file_size == 0 || (unsigned long long)file_size > WD_ROYL_MAX_FILE_SIZE)
  {
    wd_royl_set_error_ccc("The file %s has an invalid size of %ld bytes.\n", filename, file_size);
    goto finish;
  }
  if (fseek(file, 0, SEEK_SET) != 0)
  {
    wd_royl_set_error_ccc("Unable to rewind %s (%s).\n", filename, strerror(errno));
    goto finish;
  }

  buffer = (uint8_t *)malloc((size_t)file_size);
  if (buffer == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %ld bytes for %s.\n", file_size, filename);
    goto finish;
  }
  if (fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size)
  {
    wd_royl_set_error_ccc("Unable to read all of %s (%s).\n", filename, strerror(errno));
    goto finish;
  }

  fclose(file);
  file = NULL;
  *data = buffer;
  *size = (size_t)file_size;
  return 0;

finish:
  if (file != NULL)
  {
    fclose(file);
  }
  free(buffer);
  return -1;
}

int wd_royl_write_file_ccc(const char *filename, const uint8_t *data, size_t size)
{
  FILE *file;

  if (filename == NULL || data == NULL || size == 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid file write request.\n");
    return -1;
  }

  file = fopen(filename, "wb");
  if (file == NULL)
  {
    wd_royl_set_error_ccc("Unable to open %s for writing (%s).\n", filename, strerror(errno));
    return -1;
  }
  if (fwrite(data, 1, size, file) != size)
  {
    wd_royl_set_error_ccc("Unable to write all of %s (%s).\n", filename, strerror(errno));
    fclose(file);
    return -1;
  }
  if (fclose(file) != 0)
  {
    wd_royl_set_error_ccc("Unable to close %s (%s).\n", filename, strerror(errno));
    return -1;
  }
  return 0;
}
