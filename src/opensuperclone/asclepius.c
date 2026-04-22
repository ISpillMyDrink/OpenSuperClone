// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "asclepius.h"

bool asclepius_connected = false;
int asclepius_serial_port = -1;

static bool asclepius_write_all(const void *buffer, size_t length)
{
    const uint8_t *data = buffer;
    size_t offset = 0;

    while(offset < length)
    {
        ssize_t bytes_written = write(asclepius_serial_port, data + offset, length - offset);

        if(bytes_written < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }

            printf("Error writing: %s\n", strerror(errno));
            return false;
        }

        if(bytes_written == 0)
        {
            printf("Error writing: short write\n");
            return false;
        }

        offset += (size_t)bytes_written;
    }

    return true;
}

static bool asclepius_read_all(void *buffer, size_t length)
{
    uint8_t *data = buffer;
    size_t offset = 0;

    while(offset < length)
    {
        ssize_t bytes_read = read(asclepius_serial_port, data + offset, length - offset);

        if(bytes_read < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }

            printf("Error reading: %s\n", strerror(errno));
            return false;
        }

        if(bytes_read == 0)
        {
            printf("Error reading: timed out after %zu of %zu bytes\n", offset, length);
            return false;
        }

        offset += (size_t)bytes_read;
    }

    return true;
}

static uint32_t asclepius_decode_u32_le(const uint8_t *buffer)
{
    return ((uint32_t)buffer[0]) |
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

static bool asclepius_read_status_packet(void)
{
    uint8_t packet[ASCLEPIUS_STATUS_PACKET_SIZE];

    if(!asclepius_read_all(packet, sizeof(packet)))
    {
        return false;
    }

    asclepius_status.main12v_ocp = packet[0] != 0;
    asclepius_status.main5v_ocp = packet[1] != 0;
    asclepius_status.usb5v_ocp = packet[2] != 0;
    asclepius_status.main12v_voltage = asclepius_decode_u32_le(packet + 4);
    asclepius_status.main5v_voltage = asclepius_decode_u32_le(packet + 8);
    asclepius_status.main12v_current = asclepius_decode_u32_le(packet + 12);
    asclepius_status.main5v_current = asclepius_decode_u32_le(packet + 16);
    asclepius_status.usb5v_current = asclepius_decode_u32_le(packet + 20);
    asclepius_status.main12v_enabled = packet[24] != 0;
    asclepius_status.main5v_enabled = packet[25] != 0;
    asclepius_status.usb5v_enabled = packet[26] != 0;

    return true;
}

void asclepius_connect(void)
{
    if(asclepius_connected)
    {
        asclepius_disconnect();
    }

    asclepius_serial_port = open(ASCLEPIUS_TTY_DEVICE, O_RDWR | O_NOCTTY);

    if(asclepius_serial_port < 0)
    {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        asclepius_connected = false;
        return;
    }

    struct termios tty;

    if(tcgetattr(asclepius_serial_port, &tty) != 0)
    {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        asclepius_connected = false;
        close(asclepius_serial_port);
        return;
    }

    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8 bits per byte
    tty.c_cflag &= ~CRTSCTS; // No flow control
    tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;    // Disable echo
    tty.c_lflag &= ~ECHOE;   // Disable erasure
    tty.c_lflag &= ~ECHONL;  // Disable new-line echo
    tty.c_lflag &= ~ISIG;    // Disable interpretation of INTR, QUIT and SUSP

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable special handling of received bytes

    tty.c_oflag &= ~OPOST;   // Prevent special interpretation of output bytes
    tty.c_oflag &= ~ONLCR;   // Prevent conversion of newline to carriage return/line feed

    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds) for each read
    tty.c_cc[VMIN] = 0;

    cfsetispeed(&tty, ASCLEPIUS_BAUDRATE);
    cfsetospeed(&tty, ASCLEPIUS_BAUDRATE);

    if(tcsetattr(asclepius_serial_port, TCSANOW, &tty) != 0)
    {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        asclepius_connected = false;
        close(asclepius_serial_port);
        return;
    }

    tcflush(asclepius_serial_port, TCIOFLUSH);

    asclepius_connected = true;
}

void asclepius_disconnect(void)
{
    if(!asclepius_connected)
    {
        return;
    }
    
    close(asclepius_serial_port);

    asclepius_connected = false;
    asclepius_serial_port = -1;
}

bool asclepius_get_connection_status()
{
    return asclepius_connected;
}

void asclepius_enable_channel(uint8_t channel)
{
    if(!asclepius_connected)
    {
        return;
    }

    asclepius_command.command = ASCLEPIUS_TURNON_IOCMD;
    asclepius_command.channel = channel;

    asclepius_write_all(&asclepius_command, sizeof(asclepius_command));
}

void asclepius_disable_channel(uint8_t channel)
{
    if(!asclepius_connected)
    {
        return;
    }

    asclepius_command.command = ASCLEPIUS_TURNOFF_IOCMD;
    asclepius_command.channel = channel;

    asclepius_write_all(&asclepius_command, sizeof(asclepius_command));
}

void asclepius_get_status(void)
{
    if(!asclepius_connected)
    {
        return;
    }

    asclepius_command.command = ASCLEPIUS_STATUS_IOCMD;
    asclepius_command.channel = 0;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
    {
        return;
    }

    asclepius_read_status_packet();
}
