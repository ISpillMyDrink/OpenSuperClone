// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "asclepius.h"

#include <ctype.h>
#include <stdlib.h>

bool asclepius_connected = false;
int asclepius_serial_port = -1;

bool asclepius_ext_connected = false;
int asclepius_ext_serial_port = -1;
int asclepius_ext_baud_rate_ccc = 38400;
void (*asclepius_ext_output_ccc)(const char *text, int byte_count) = NULL;

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

static int asclepius_setup_tty(void)
{
    struct termios tty;

    if(tcgetattr(asclepius_serial_port, &tty) != 0)
    {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return -1;
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
        return -1;
    }

    tcflush(asclepius_serial_port, TCIOFLUSH);
    return 0;
}

static bool asclepius_reconnect(void)
{
    if(asclepius_serial_port >= 0)
    {
        close(asclepius_serial_port);
        asclepius_serial_port = -1;
    }

    asclepius_serial_port = open(asclepius_tty_device_ccc, O_RDWR | O_NOCTTY);

    if(asclepius_serial_port < 0)
    {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        return false;
    }

    if(asclepius_setup_tty() != 0)
    {
        close(asclepius_serial_port);
        asclepius_serial_port = -1;
        return false;
    }

    return true;
}

int asclepius_connect(void)
{
    if(asclepius_connected)
    {
        asclepius_disconnect();
    }

    asclepius_serial_port = open(asclepius_tty_device_ccc, O_RDWR | O_NOCTTY);

    if(asclepius_serial_port < 0)
    {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        asclepius_connected = false;
        return -1;
    }

    if(asclepius_setup_tty() != 0)
    {
        asclepius_connected = false;
        close(asclepius_serial_port);
        return -1;
    }

    asclepius_connected = true;
    return 0;
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

int asclepius_enable_channel(uint8_t channel)
{
    if(!asclepius_connected)
    {
        return -1;
    }

    asclepius_command.command = ASCLEPIUS_TURNON_IOCMD;
    asclepius_command.channel = channel;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
    {
        asclepius_connected = false;
        if(asclepius_reconnect())
        {
            asclepius_connected = true;
            if(asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
            {
                return 0;
            }
        }
        return -1;
    }

    return 0;
}

int asclepius_disable_channel(uint8_t channel)
{
    if(!asclepius_connected)
    {
        return -1;
    }

    asclepius_command.command = ASCLEPIUS_TURNOFF_IOCMD;
    asclepius_command.channel = channel;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
    {
        asclepius_connected = false;
        if(asclepius_reconnect())
        {
            asclepius_connected = true;
            if(asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
            {
                return 0;
            }
        }
        return -1;
    }

    return 0;
}

int asclepius_get_status(void)
{
    if(!asclepius_connected)
    {
        return -1;
    }

    asclepius_command.command = ASCLEPIUS_STATUS_IOCMD;
    asclepius_command.channel = 0;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
    {
        asclepius_connected = false;
        if(asclepius_reconnect())
        {
            asclepius_connected = true;
            if(asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
            {
                if(asclepius_read_status_packet())
                {
                    return 0;
                }
            }
        }
        return -1;
    }

    if(!asclepius_read_status_packet())
    {
        asclepius_connected = false;
        if(asclepius_reconnect())
        {
            asclepius_connected = true;
            if(asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
            {
                if(asclepius_read_status_packet())
                {
                    return 0;
                }
            }
        }
        return -1;
    }

    return 0;
}

// -----------------------------------------------------------------------
// Ext UART (secondary serial port)
// -----------------------------------------------------------------------

static int asclepius_ext_baud_to_constant(int baud)
{
    switch (baud)
    {
    case 9600:     return B9600;
    case 19200:    return B19200;
    case 38400:    return B38400;
    case 57600:    return B57600;
    case 115200:   return B115200;
    case 230400:   return B230400;
    case 921600:   return B921600;
    default:       return B38400;
    }
}

static int asclepius_ext_setup_tty(void)
{
    struct termios tty;

    if(tcgetattr(asclepius_ext_serial_port, &tty) != 0)
    {
        printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
        return -1;
    }

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    speed_t speed = asclepius_ext_baud_to_constant(asclepius_ext_baud_rate_ccc);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    if(tcsetattr(asclepius_ext_serial_port, TCSANOW, &tty) != 0)
    {
        printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
        return -1;
    }

    tcflush(asclepius_ext_serial_port, TCIOFLUSH);
    return 0;
}

int asclepius_ext_open(void)
{
    if(asclepius_ext_connected)
    {
        asclepius_ext_close();
    }

    asclepius_ext_serial_port = open(asclepius_ext_tty_device_ccc, O_RDWR | O_NOCTTY | O_NONBLOCK);

    if(asclepius_ext_serial_port < 0)
    {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        asclepius_ext_connected = false;
        return -1;
    }

    if(asclepius_ext_setup_tty() != 0)
    {
        asclepius_ext_connected = false;
        close(asclepius_ext_serial_port);
        asclepius_ext_serial_port = -1;
        return -1;
    }

    asclepius_ext_connected = true;
    return 0;
}

void asclepius_ext_close(void)
{
    if(!asclepius_ext_connected)
    {
        return;
    }

    close(asclepius_ext_serial_port);
    asclepius_ext_connected = false;
    asclepius_ext_serial_port = -1;
}

int asclepius_ext_send(const char *data, size_t len)
{
    if(!asclepius_ext_connected)
    {
        return -1;
    }

    const uint8_t *buf = (const uint8_t *)data;
    size_t offset = 0;

    while(offset < len)
    {
        ssize_t n = write(asclepius_ext_serial_port, buf + offset, len - offset);

        if(n < 0)
        {
            if(errno == EINTR || errno == EAGAIN)
            {
                continue;
            }
            printf("Error writing ext: %s\n", strerror(errno));
            return -1;
        }

        if(n == 0)
        {
            printf("Error writing ext: short write\n");
            return -1;
        }

        offset += (size_t)n;
    }

    return 0;
}

int asclepius_ext_read(char *buffer, size_t max_len)
{
    if(!asclepius_ext_connected)
    {
        return -1;
    }

    ssize_t n = read(asclepius_ext_serial_port, buffer, max_len);

    if(n < 0)
    {
        if(errno == EINTR || errno == EAGAIN)
        {
            return 0;
        }
        printf("Error reading ext: %s\n", strerror(errno));
        return -1;
    }

    return (int)n;
}

int asclepius_parse_escapes(const char *input, char *output, size_t max)
{
    size_t out = 0;
    while(*input && out < max - 1)
    {
        if(*input == '\\')
        {
            input++;
            switch(*input)
            {
            case 'n':  output[out++] = '\n'; break;
            case 'r':  output[out++] = '\r'; break;
            case 't':  output[out++] = '\t'; break;
            case '0':  output[out++] = '\0'; break;
            case '\\': output[out++] = '\\'; break;
            case 'x':
            {
                input++;
                if(isxdigit((unsigned char)input[0]) && isxdigit((unsigned char)input[1]))
                {
                    char hex[3] = {input[0], input[1], '\0'};
                    output[out++] = (char)strtol(hex, NULL, 16);
                    input++;
                }
                break;
            }
            default:
                output[out++] = *input;
                break;
            }
        }
        else
        {
            output[out++] = *input;
        }
        if(*input) input++;
    }
    output[out] = '\0';
    return (int)out;
}
