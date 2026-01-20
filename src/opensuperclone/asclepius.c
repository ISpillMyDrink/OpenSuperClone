// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "asclepius.h"

bool asclepius_connected = false;
int asclepius_serial_port = -1;

void asclepius_connect(void)
{
    if(asclepius_connected)
    {
        asclepius_disconnect();
    }

    asclepius_serial_port = open(ASCLEPIUS_TTY_DEVICE, O_RDWR);

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

    tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received
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

    asclepius_connected = true;

    sleep(ASCLEPIUS_IOCMD_DELAY);
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

    write(asclepius_serial_port, &asclepius_command, sizeof(asclepius_command));

    sleep(ASCLEPIUS_IOCMD_DELAY);
}

void asclepius_disable_channel(uint8_t channel)
{
    if(!asclepius_connected)
    {
        return;
    }

    asclepius_command.command = ASCLEPIUS_TURNOFF_IOCMD;
    asclepius_command.channel = channel;

    write(asclepius_serial_port, &asclepius_command, sizeof(asclepius_command));

    sleep(ASCLEPIUS_IOCMD_DELAY);
}

void asclepius_get_status(void)
{
    if(!asclepius_connected)
    {
        return;
    }

    asclepius_command.command = ASCLEPIUS_STATUS_IOCMD;

    write(asclepius_serial_port, &asclepius_command, sizeof(asclepius_command));

    int num_bytes = read(asclepius_serial_port, &asclepius_status, sizeof(asclepius_status));

    if(num_bytes < 0) {
        printf("Error reading: %s\n", strerror(errno));
    } else if(num_bytes == 0) {
        printf("No data read\n");
    }
}
