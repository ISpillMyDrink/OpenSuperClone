// Copyright (C) Scott Dwyer and OpenSuperClone contributors.
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
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
         ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void wd_royl_put_le32_ccc(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8);
  data[2] = (uint8_t)(value >> 16);
  data[3] = (uint8_t)(value >> 24);
}

static void wd_royl_progress_ccc(wd_royl_progress_callback_ccc progress,
                                 const char *stage, unsigned long long done,
                                 unsigned long long total)
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

int wd_royl_send_ata28_ccc(const uint8_t regs[7], void *data,
                           size_t data_size, int direction)
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
  if (direction != WD_ROYL_DIR_NONE && direction != WD_ROYL_DIR_IN &&
      direction != WD_ROYL_DIR_OUT)
  {
    wd_royl_set_error_ccc("Internal error: invalid data direction %d.\n",
                          direction);
    return -1;
  }
  if (direction == WD_ROYL_DIR_NONE && data_size != 0)
  {
    wd_royl_set_error_ccc("Internal error: no-data command with %zu bytes.\n",
                          data_size);
    return -1;
  }
  if (direction != WD_ROYL_DIR_NONE && data == NULL)
  {
    wd_royl_set_error_ccc("Internal error: data command without a buffer.\n");
    return -1;
  }
  if (data_size > MAX_USB_BUFFER_SIZE)
  {
    wd_royl_set_error_ccc("Transfer size %zu is larger than the maximum.\n",
                          data_size);
    return -1;
  }

  if (usb_mode_ccc)
  {
    if (!usb_allow_ata_ccc)
    {
      wd_royl_set_error_ccc(
          "This operation requires ATA pass-through, not USB mass storage "
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
        wd_royl_set_error_ccc(
            "The source drive is not connected. Choose and connect the source "
            "drive before using vendor specific commands.\n");
        return -1;
      }
      if (!ahci_mode_ccc && !io_permissions_ccc)
      {
        wd_royl_set_error_ccc(
            "The source drive is not connected. Choose and connect the source "
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
    wd_royl_set_error_ccc("Unable to set the data buffer size to %zu.\n",
                          data_size);
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
  passthrough_ccc.scsi_cmd[1] =
      (unsigned char)((passthrough_ccc.multiple_count << 5) +
                      (passthrough_ccc.protocol << 1));
  passthrough_ccc.scsi_cmd[2] = (unsigned char)(
      (passthrough_ccc.off_line << 6) +
      (passthrough_ccc.check_condition << 5) +
      (passthrough_ccc.transfer_direction << 3) +
      (passthrough_ccc.byte_block << 2) +
      passthrough_ccc.transfer_length);
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
    if (raw_ret < 0 ||
        (direction == WD_ROYL_DIR_IN && raw_ret != (int)data_size) ||
        (direction == WD_ROYL_DIR_OUT && raw_ret != 0 &&
         raw_ret != (int)data_size) ||
        !usb_csw_valid_ccc || csw_status != 0)
    {
      (void)usb_get_sense_ccc();
    }

    if (raw_ret < 0)
    {
      wd_royl_set_error_ccc(
          "USB ATA command %02x failed with return %d (%s), sense "
          "%02x/%02x/%02x.\n",
          regs[6], raw_ret, strerror(abs(raw_ret)), sense_key_ccc, asc_ccc,
          ascq_ccc);
      goto restore_buffer;
    }
    if (direction == WD_ROYL_DIR_IN && raw_ret != (int)data_size)
    {
      wd_royl_set_error_ccc(
          "USB ATA command %02x returned %d bytes instead of %zu, sense "
          "%02x/%02x/%02x.\n",
          regs[6], raw_ret, data_size, sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
    if (direction == WD_ROYL_DIR_OUT && raw_ret != 0 &&
        raw_ret != (int)data_size)
    {
      wd_royl_set_error_ccc(
          "USB ATA command %02x transferred %d bytes instead of %zu, sense "
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
      wd_royl_set_error_ccc(
          "ATA command %02x failed with return %d, status %02x, sense "
          "%02x/%02x/%02x.\n",
          regs[6], ret, ata_status_ccc, sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
  }

  if (ata_status_ccc & 0x01)
  {
    wd_royl_set_error_ccc(
        "ATA command %02x reported an error, status %02x, sense "
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
      wd_royl_set_error_ccc(
          "ATA command %02x failed with sense %02x/%02x/%02x.\n", regs[6],
          sense_key_ccc, asc_ccc, ascq_ccc);
      goto restore_buffer;
    }
  }

  if (usb_mode_ccc && !usb_csw_valid_ccc)
  {
    wd_royl_set_error_ccc(
        "USB ATA command %02x did not return a valid command status "
        "wrapper.\n",
        regs[6]);
    goto restore_buffer;
  }

  if (usb_mode_ccc && csw_status != 0)
  {
    if (!(sense_key_ccc == 4 && asc_ccc == 0 && ascq_ccc == 0))
    {
      wd_royl_set_error_ccc(
          "USB ATA command %02x failed with CSW status %u and sense "
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

  if (wd_royl_send_ata28_ccc(regs, buffer, sizeof(buffer),
                             WD_ROYL_DIR_OUT) != 0)
  {
    return -1;
  }
  if (send_twice)
  {
    if (wd_royl_send_ata28_ccc(regs, buffer, sizeof(buffer),
                               WD_ROYL_DIR_OUT) != 0)
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
  return wd_royl_send_ata28_ccc(local_regs, data,
                                (size_t)sectors * WD_ROYL_SECTOR_SIZE,
                                WD_ROYL_DIR_IN);
}

static int wd_royl_write_sectors_ccc(uint8_t sectors, const void *data)
{
  const uint8_t regs[7] = {0xd6, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  uint8_t local_regs[7];

  memcpy(local_regs, regs, sizeof(local_regs));
  local_regs[1] = sectors;
  return wd_royl_send_ata28_ccc(local_regs, (void *)data,
                                (size_t)sectors * WD_ROYL_SECTOR_SIZE,
                                WD_ROYL_DIR_OUT);
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
    wd_royl_set_error_ccc(
        "The data table was read, but vendor specific commands could not be "
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
    wd_royl_set_error_ccc(
        "The drive reported zero ROM blocks. Vendor specific commands may not "
        "be supported by this drive.\n");
    goto finish;
  }
  *rom_size = (size_t)table[WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET] *
              WD_ROYL_ROM_BLOCK_SIZE;
  result = 0;

finish:
  if (wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc(
        "The ROM size was read, but vendor specific commands could not be "
        "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_read_rom_ccc(uint8_t **rom, size_t *rom_size,
                         wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "The drive reported zero ROM blocks. Vendor specific commands may not "
        "be supported by this drive.\n");
    goto finish;
  }

  size = (size_t)table[WD_ROYL_ROM_TABLE_BLOCK_COUNT_OFFSET] *
         WD_ROYL_ROM_BLOCK_SIZE;
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
    if ((current % WD_ROYL_SECTOR_SIZE) != 0 ||
        (current / WD_ROYL_SECTOR_SIZE) > 255)
    {
      wd_royl_set_error_ccc("Internal error: bad ROM chunk size %zu.\n",
                            current);
      goto finish;
    }
    if (wd_royl_read_sectors_ccc(
            (uint8_t)(current / WD_ROYL_SECTOR_SIZE), buffer + offset) != 0)
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
    wd_royl_set_error_ccc(
        "The ROM was read, but vendor specific commands could not be "
        "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_write_rom_ccc(const uint8_t *rom, size_t rom_size,
                          bool slow_and_stupid,
                          wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "The ROM size %zu is not a multiple of the 16384 byte ROM block "
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
    if ((current % WD_ROYL_SECTOR_SIZE) != 0 ||
        (current / WD_ROYL_SECTOR_SIZE) > 255)
    {
      wd_royl_set_error_ccc("Internal error: bad ROM chunk size %zu.\n",
                            current);
      goto finish;
    }
    if (wd_royl_write_sectors_ccc(
            (uint8_t)(current / WD_ROYL_SECTOR_SIZE), rom + offset) != 0)
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
    wd_royl_set_error_ccc(
        "The ROM was written, but vendor specific commands could not be "
        "disabled. Power cycle the drive before using it.\n");
    result = -1;
  }
  return result;
}

int wd_royl_verify_rom_ccc(const uint8_t *rom, size_t rom_size,
                           wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "ROM verification failed: expected %zu bytes but read %zu bytes.\n",
        rom_size, read_size);
    goto finish;
  }

  while (offset < rom_size)
  {
    if (read_rom[offset] != rom[offset])
    {
      wd_royl_set_error_ccc(
          "ROM verification failed at offset 0x%zx: expected 0x%02x but "
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
    wd_royl_set_error_ccc(
        "The ROM size %zu is not a multiple of the 16384 byte ROM block "
        "size.\n",
        rom_size);
    return -1;
  }
  if (rom[0] != 0x5a)
  {
    wd_royl_set_error_ccc(
        "The ROM does not start with the expected 0x5a block signature.\n");
    return -1;
  }

  while (1)
  {
    table_offset = (size_t)block_number * 32;
    if (table_offset + 32 > rom_size)
    {
      wd_royl_set_error_ccc(
          "The ROM block table runs past the end of the ROM at block %u.\n",
          block_number);
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
      wd_royl_set_error_ccc(
          "The ROM block %u has an invalid size (0x%x) and checksum "
          "location (0x%x).\n",
          block_number, block_size, checksum_location);
      return -1;
    }
    checksum_length = checksum_location - block_size;
    if ((uint64_t)block_start + checksum_location > rom_size)
    {
      wd_royl_set_error_ccc(
          "The ROM block %u data is outside the ROM (start 0x%x, end "
          "0x%llx).\n",
          block_number, block_start,
          (unsigned long long)block_start + checksum_location);
      return -1;
    }

    calculated = 0;
    for (position = table_offset; position < table_offset + 31; position++)
    {
      calculated += rom[position];
    }
    if ((calculated & 0xff) != rom[table_offset + 0x1f])
    {
      wd_royl_set_error_ccc(
          "The ROM block %u table checksum is invalid (stored 0x%02x, "
          "calculated 0x%02x).\n",
          block_number, rom[table_offset + 0x1f], calculated & 0xff);
      return -1;
    }

    calculated = 0;
    if (checksum_length == 1)
    {
      for (position = block_start; position < block_start + block_size;
           position++)
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
        wd_royl_set_error_ccc(
            "The ROM block %u has an odd size (0x%x) for a 16 bit "
            "checksum.\n",
            block_number, block_size);
        return -1;
      }
      for (position = block_start; position < block_start + block_size;
           position += 2)
      {
        calculated += wd_royl_le16_ccc(rom + position);
      }
      stored = wd_royl_le16_ccc(rom + block_start + block_size);
      calculated &= 0xffff;
    }
    else
    {
      wd_royl_set_error_ccc(
          "The ROM block %u has an unsupported checksum length of %u.\n",
          block_number, checksum_length);
      return -1;
    }
    if (stored != calculated)
    {
      wd_royl_set_error_ccc(
          "The ROM block %u checksum is invalid (stored 0x%x, calculated "
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

static bool wd_royl_module_valid_ccc(const uint8_t *data, size_t size,
                                     uint16_t expected_module)
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
  if (sectors == 0 ||
      (size_t)sectors * WD_ROYL_SECTOR_SIZE != size)
  {
    return false;
  }
  if (expected_module != 0 &&
      wd_royl_get_module_id_ccc(data) != expected_module)
  {
    return false;
  }
  if (wd_royl_get_module_checksum_ccc(data) !=
      wd_royl_calculate_checksum_ccc(data, size))
  {
    return false;
  }
  return true;
}

int wd_royl_validate_module_header_ccc(const uint8_t *data, size_t size,
                                       uint16_t expected_module)
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
    wd_royl_set_error_ccc(
        "The module size %zu is not a multiple of the sector size.\n", size);
    return -1;
  }
  sectors = wd_royl_get_module_sector_count_ccc(data);
  if (sectors == 0 || (size_t)sectors * WD_ROYL_SECTOR_SIZE != size)
  {
    wd_royl_set_error_ccc(
        "The module header reports %u sectors, but the data is %zu bytes.\n",
        sectors, size);
    return -1;
  }
  if (expected_module != 0 &&
      wd_royl_get_module_id_ccc(data) != expected_module)
  {
    wd_royl_set_error_ccc(
        "Expected module 0x%04x but the module data is module 0x%04x.\n",
        expected_module, wd_royl_get_module_id_ccc(data));
    return -1;
  }
  return 0;
}

int wd_royl_validate_module_ccc(const uint8_t *data, size_t size,
                                uint16_t expected_module)
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
    wd_royl_set_error_ccc(
        "Module 0x%04x checksum mismatch: header 0x%08x, calculated "
        "0x%08x.\n",
        wd_royl_get_module_id_ccc(data), stored_checksum,
        calculated_checksum);
    return -1;
  }
  return 0;
}

int wd_royl_get_record_ccc(const uint8_t *data, size_t size,
                           unsigned int record_number, uint16_t *record_offset,
                           uint16_t *record_size)
{
  size_t offset;
  uint16_t record_start;
  uint16_t record_length;

  if (data == NULL || record_number == 0 || record_offset == NULL ||
      record_size == NULL)
  {
    wd_royl_set_error_ccc("Internal error: invalid module record request.\n");
    return -1;
  }
  offset = 0x32 + ((size_t)record_number - 1) * 4;
  if ((offset + 4) > size)
  {
    wd_royl_set_error_ccc(
        "Module record %u is past the end of the module.\n", record_number);
    return -1;
  }
  record_start = wd_royl_le16_ccc(data + offset);
  record_length = wd_royl_le16_ccc(data + offset + 2);
  if (record_length == 0 ||
      (size_t)record_start + (size_t)record_length > size)
  {
    wd_royl_set_error_ccc(
        "Module record %u is outside the module data (offset 0x%04x, size "
        "0x%04x).\n",
        record_number, record_start, record_length);
    return -1;
  }
  *record_offset = record_start;
  *record_size = record_length;
  return 0;
}

int wd_royl_parse_module_directory_ccc(
    const uint8_t *data, size_t size, wd_royl_module_entry_ccc **entries,
    size_t *entry_count)
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
    wd_royl_set_error_ccc(
        "Internal error: no destination for the module directory.\n");
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
      wd_royl_set_error_ccc(
          "Directory record %u is past the end of module 0x0001.\n",
          record_number);
      goto finish;
    }
    record_length = data[offset];
    if (record_length < 6 || (size_t)record_length > size - offset)
    {
      wd_royl_set_error_ccc(
          "Directory record %u has an invalid length (%u bytes).\n",
          record_number, record_length);
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

        if (new_capacity < capacity ||
            new_capacity > SIZE_MAX / sizeof(*new_list))
        {
          wd_royl_set_error_ccc(
              "The module directory is too large to process.\n");
          goto finish;
        }
        new_list = realloc(list, new_capacity * sizeof(*new_list));
        if (new_list == NULL)
        {
          wd_royl_set_error_ccc(
              "Unable to allocate memory for the module directory.\n");
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
    wd_royl_set_error_ccc(
        "Expected module 02 but found module 0x%04x.\n",
        wd_royl_get_module_id_ccc(data));
    return -1;
  }
  if (wd_royl_get_record_ccc(data, size, 27, &record_offset, &record_size) !=
      0)
  {
    return -1;
  }
  if (record_size < 3)
  {
    wd_royl_set_error_ccc(
        "Module 02 record 27 is too small (%u bytes) for the slow fix.\n",
        record_size);
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
    wd_royl_set_error_ccc(
        "Expected module 32 but found module 0x%04x.\n",
        wd_royl_get_module_id_ccc(data));
    return -1;
  }
  table_offset = wd_royl_get_module_table_offset_ccc(data);
  if (table_offset == 0 || (size_t)table_offset >= size)
  {
    wd_royl_set_error_ccc(
        "Module 32 has an invalid table offset 0x%04x.\n", table_offset);
    return -1;
  }
  memset(data + table_offset, 0, size - table_offset);
  wd_royl_update_checksum_ccc(data, size);
  return 0;
}

int wd_royl_read_module_ccc(uint16_t module, uint8_t **data, size_t *size,
                            wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "The drive did not return a ROYL module header for module 0x%04x.\n",
        module);
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
    wd_royl_set_error_ccc(
        "Expected module 0x%04x but the drive returned module 0x%04x.\n",
        module, wd_royl_get_module_id_ccc(first_sector));
    goto finish;
  }
  module_size = (size_t)sectors * WD_ROYL_SECTOR_SIZE;
  if (module_size > WD_ROYL_MAX_FILE_SIZE)
  {
    wd_royl_set_error_ccc(
        "Module 0x%04x reports an unreasonable size of %zu bytes.\n", module,
        module_size);
    goto finish;
  }

  buffer = (uint8_t *)malloc(module_size);
  if (buffer == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %zu bytes for module 0x%04x.\n",
                          module_size, module);
    goto finish;
  }
  memcpy(buffer, first_sector, sizeof(first_sector));
  wd_royl_progress_ccc(progress, "Reading module", WD_ROYL_SECTOR_SIZE,
                       module_size);

  for (sector = 1; sector < sectors; sector++)
  {
    if (wd_royl_read_sectors_ccc(1, buffer + (size_t)sector *
                                         WD_ROYL_SECTOR_SIZE) != 0)
    {
      goto finish;
    }
    wd_royl_progress_ccc(
        progress, "Reading module",
        (unsigned long long)(sector + 1) * WD_ROYL_SECTOR_SIZE, module_size);
  }

  *data = buffer;
  *size = module_size;
  buffer = NULL;
  result = 0;

finish:
  free(buffer);
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc(
        "Module 0x%04x was read, but vendor specific commands could not be "
        "disabled.\n",
        module);
    result = -1;
  }
  return result;
}

int wd_royl_write_module_ccc(uint16_t module, const uint8_t *data, size_t size,
                             wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "The module size %zu is not a multiple of the sector size.\n", size);
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
    if (wd_royl_write_sectors_ccc(
            1, data + sector * WD_ROYL_SECTOR_SIZE) != 0)
    {
      goto finish;
    }
    wd_royl_progress_ccc(
        progress, "Writing module",
        (unsigned long long)(sector + 1) * WD_ROYL_SECTOR_SIZE, size);
  }

  result = 0;

finish:
  if (enabled && wd_royl_disable_vsc_ccc() != 0 && result == 0)
  {
    wd_royl_set_error_ccc(
        "Module 0x%04x was written, but vendor specific commands could not be "
        "disabled.\n",
        module);
    result = -1;
  }
  return result;
}

int wd_royl_verify_module_ccc(uint16_t module, const uint8_t *data, size_t size,
                              wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "Module 0x%04x verification failed: expected %zu bytes but read %zu "
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
        wd_royl_set_error_ccc(
            "Module 0x%04x verification failed at offset 0x%zx: expected "
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

static int wd_royl_read_ram_internal_ccc(uint64_t address, void *data,
                                         size_t size,
                                         wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  uint8_t regs[7] = {WD_ROYL_ATA_CMD_D5, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  size_t sectors;

  if (data == NULL || size == 0 ||
      (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM read size %zu.\n",
                          size);
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
  if (wd_royl_prepare_ccc(request, false) != 0)
  {
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
    return -1;
  }

  wd_royl_progress_ccc(progress, "Reading RAM", 0, 0);
  return 0;
}

static int wd_royl_write_ram_internal_ccc(uint64_t address, const void *data,
                                          size_t size,
                                          wd_royl_progress_callback_ccc progress)
{
  uint8_t request[16];
  uint8_t regs[7] = {WD_ROYL_ATA_CMD_D6, 0, 0xbf, 0x4f, 0xc2, 0xa0, 0xb0};
  size_t sectors;

  if (data == NULL || size == 0 ||
      (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM write size %zu.\n",
                          size);
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
    return -1;
  }

  sectors = size / WD_ROYL_SECTOR_SIZE;
  if (sectors > 255)
  {
    wd_royl_set_error_ccc("Internal error: RAM write is too large.\n");
    return -1;
  }
  regs[1] = (uint8_t)sectors;
  if (wd_royl_send_ata28_ccc(regs, (void *)data, size,
                             WD_ROYL_DIR_OUT) != 0)
  {
    return -1;
  }

  wd_royl_progress_ccc(progress, "Writing RAM", 0, 0);
  return 0;
}

// ATA28 can transfer at most 255 sectors in one command, so larger RAM
// operations are split into aligned chunks. The chunk size is also used for
// progress reporting.
#define WD_ROYL_RAM_CHUNK_SIZE ((size_t)WD_ROYL_RAM_BLOCK_SIZE)

static int wd_royl_read_ram_chunked_ccc(uint64_t address, void *data,
                                        size_t size,
                                        wd_royl_progress_callback_ccc progress)
{
  size_t offset;

  if (data == NULL || size == 0 ||
      (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM read size %zu.\n",
                          size);
    return -1;
  }

  for (offset = 0; offset < size; offset += WD_ROYL_RAM_CHUNK_SIZE)
  {
    size_t current = WD_ROYL_RAM_CHUNK_SIZE;

    if (current > size - offset)
    {
      current = size - offset;
    }
    if (wd_royl_read_ram_internal_ccc(address + offset,
                                      (uint8_t *)data + offset, current,
                                      NULL) != 0)
    {
      return -1;
    }
    wd_royl_progress_ccc(progress, "Reading RAM", offset + current, size);
  }
  return 0;
}

static int wd_royl_write_ram_chunked_ccc(uint64_t address, const void *data,
                                         size_t size,
                                         wd_royl_progress_callback_ccc progress)
{
  size_t offset;

  if (data == NULL || size == 0 ||
      (size % WD_ROYL_SECTOR_SIZE) != 0)
  {
    wd_royl_set_error_ccc("Internal error: invalid RAM write size %zu.\n",
                          size);
    return -1;
  }

  for (offset = 0; offset < size; offset += WD_ROYL_RAM_CHUNK_SIZE)
  {
    size_t current = WD_ROYL_RAM_CHUNK_SIZE;

    if (current > size - offset)
    {
      current = size - offset;
    }
    if (wd_royl_write_ram_internal_ccc(address + offset,
                                       (const uint8_t *)data + offset, current,
                                       NULL) != 0)
    {
      return -1;
    }
    wd_royl_progress_ccc(progress, "Writing RAM", offset + current, size);
  }
  return 0;
}

static int wd_royl_detect_ram_size_internal_ccc(
    uint64_t *ram_size, wd_royl_progress_callback_ccc progress)
{
  uint8_t base_data[WD_ROYL_SECTOR_SIZE];
  uint8_t probe_data[WD_ROYL_SECTOR_SIZE];
  uint64_t size;

  *ram_size = 0;
  wd_royl_progress_ccc(progress, "Detecting drive RAM", 0, 1);
  if (wd_royl_read_ram_internal_ccc(WD_ROYL_RAM_BASE, base_data,
                                    sizeof(base_data), progress) != 0)
  {
    return -1;
  }

  size = WD_ROYL_RAM_MIN_SIZE;
  while (size <= WD_ROYL_RAM_MAX_SIZE)
  {
    if (wd_royl_read_ram_internal_ccc(WD_ROYL_RAM_BASE + size, probe_data,
                                      sizeof(probe_data), progress) != 0)
    {
      return -1;
    }
    if (memcmp(base_data, probe_data, sizeof(base_data)) == 0)
    {
      *ram_size = size;
      return 0;
    }
    size *= 2;
  }

  *ram_size = WD_ROYL_RAM_MAX_SIZE;
  return 0;
}

int wd_royl_find_ram_size_ccc(uint64_t *ram_size,
                              wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "The RAM size was detected, but vendor specific commands could not be "
        "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_read_ram_ccc(uint64_t address, void *data, size_t size,
                         wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "RAM was read, but vendor specific commands could not be disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_write_ram_ccc(uint64_t address, const void *data, size_t size,
                          wd_royl_progress_callback_ccc progress)
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
    wd_royl_set_error_ccc(
        "RAM was written, but vendor specific commands could not be "
        "disabled.\n");
    result = -1;
  }
  return result;
}

int wd_royl_patch_ram_modules_ccc(unsigned int module_mask,
                                  unsigned int *module_02_count,
                                  unsigned int *module_32_count,
                                  wd_royl_progress_callback_ccc progress)
{
  uint8_t *ram = NULL;
  uint8_t *verify = NULL;
  uint64_t ram_size = 0;
  uint64_t address;
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

  if (wd_royl_enable_vsc_ccc() != 0)
  {
    return -1;
  }
  enabled = true;

  if (wd_royl_detect_ram_size_internal_ccc(&ram_size, progress) != 0)
  {
    goto finish;
  }

  ram = (uint8_t *)malloc((size_t)ram_size);
  if (ram == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %llu bytes for the RAM dump.\n",
                          (unsigned long long)ram_size);
    goto finish;
  }

  wd_royl_progress_ccc(progress, "Reading RAM", 0, ram_size);
  if (wd_royl_read_ram_chunked_ccc(WD_ROYL_RAM_BASE, ram, (size_t)ram_size,
                                   progress) != 0)
  {
    goto finish;
  }

  verify = (uint8_t *)malloc(WD_ROYL_MAX_FILE_SIZE);
  if (verify == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate the RAM verify buffer.\n");
    goto finish;
  }

  offset = 0;
  while (offset + WD_ROYL_SECTOR_SIZE <= ram_size)
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
    if (!((module_id == WD_ROYL_MODULE_02 &&
           (module_mask & WD_ROYL_MODULE_MASK_02)) ||
          (module_id == WD_ROYL_MODULE_32 &&
           (module_mask & WD_ROYL_MODULE_MASK_32))))
    {
      offset += WD_ROYL_SECTOR_SIZE;
      continue;
    }

    sectors = wd_royl_get_module_sector_count_ccc(module);
    module_size = (size_t)sectors * WD_ROYL_SECTOR_SIZE;
    if (sectors == 0 || module_size > WD_ROYL_MAX_FILE_SIZE ||
        offset + module_size > ram_size)
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

    address = WD_ROYL_RAM_BASE + offset;
    if (wd_royl_write_ram_chunked_ccc(address, module, module_size,
                                      progress) != 0)
    {
      goto finish;
    }
    if (wd_royl_read_ram_chunked_ccc(address, verify, module_size,
                                     progress) != 0)
    {
      goto finish;
    }
    if (memcmp(verify, module, module_size) != 0)
    {
      wd_royl_set_error_ccc(
          "RAM verification failed for module 0x%04x at address "
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

  if (count_02 == 0 && count_32 == 0)
  {
    if ((module_mask & WD_ROYL_MODULE_MASK_02) &&
        (module_mask & WD_ROYL_MODULE_MASK_32))
    {
      wd_royl_set_error_ccc(
          "Neither module 02 nor module 32 was found in the drive RAM.\n");
    }
    else if (module_mask & WD_ROYL_MODULE_MASK_02)
    {
      wd_royl_set_error_ccc("Module 02 was not found in the drive RAM.\n");
    }
    else
    {
      wd_royl_set_error_ccc("Module 32 was not found in the drive RAM.\n");
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
    wd_royl_set_error_ccc(
        "The RAM patch completed, but vendor specific commands could not be "
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
    wd_royl_set_error_ccc("Unable to open %s for reading (%s).\n", filename,
                          strerror(errno));
    return -1;
  }
  if (fseek(file, 0, SEEK_END) != 0)
  {
    wd_royl_set_error_ccc("Unable to seek in %s (%s).\n", filename,
                          strerror(errno));
    goto finish;
  }
  file_size = ftell(file);
  if (file_size < 0)
  {
    wd_royl_set_error_ccc("Unable to get the size of %s (%s).\n", filename,
                          strerror(errno));
    goto finish;
  }
  if (file_size == 0 ||
      (unsigned long long)file_size > WD_ROYL_MAX_FILE_SIZE)
  {
    wd_royl_set_error_ccc("The file %s has an invalid size of %ld bytes.\n",
                          filename, file_size);
    goto finish;
  }
  if (fseek(file, 0, SEEK_SET) != 0)
  {
    wd_royl_set_error_ccc("Unable to rewind %s (%s).\n", filename,
                          strerror(errno));
    goto finish;
  }

  buffer = (uint8_t *)malloc((size_t)file_size);
  if (buffer == NULL)
  {
    wd_royl_set_error_ccc("Unable to allocate %ld bytes for %s.\n", file_size,
                          filename);
    goto finish;
  }
  if (fread(buffer, 1, (size_t)file_size, file) != (size_t)file_size)
  {
    wd_royl_set_error_ccc("Unable to read all of %s (%s).\n", filename,
                          strerror(errno));
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

int wd_royl_write_file_ccc(const char *filename, const uint8_t *data,
                           size_t size)
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
    wd_royl_set_error_ccc("Unable to open %s for writing (%s).\n", filename,
                          strerror(errno));
    return -1;
  }
  if (fwrite(data, 1, size, file) != size)
  {
    wd_royl_set_error_ccc("Unable to write all of %s (%s).\n", filename,
                          strerror(errno));
    fclose(file);
    return -1;
  }
  if (fclose(file) != 0)
  {
    wd_royl_set_error_ccc("Unable to close %s (%s).\n", filename,
                          strerror(errno));
    return -1;
  }
  return 0;
}
