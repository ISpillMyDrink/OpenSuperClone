// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#define ASCLEPIUS_TTY_DEVICE "/dev/ttyascl"
#define ASCLEPIUS_BAUDRATE B115200

#define ASCLEPIUS_IOCMD_DELAY 1

#define ASCLEPIUS_STATUS_IOCMD 1
#define ASCLEPIUS_TURNON_IOCMD 2
#define ASCLEPIUS_TURNOFF_IOCMD 3

#define ASCLEPIUS_MAIN12V 1
#define ASCLEPIUS_MAIN5V 2
#define ASCLEPIUS_USB5V 3
#define ASCLEPIUS_MOLEX 4
#define ASCLEPIUS_ALL 5

struct
{
    uint8_t command;
    uint8_t channel;
} asclepius_command;

struct
{
    bool main12v_ocp;
    bool main5v_ocp;
    bool usb5v_ocp;
    uint32_t main12v_voltage;
    uint32_t main5v_voltage;
    uint32_t main12v_current;
    uint32_t main5v_current;
    uint32_t usb5v_current;
    bool main12v_enabled;
    bool main5v_enabled;
    bool usb5v_enabled;
} asclepius_status;

void asclepius_connect(void);

void asclepius_disconnect(void);

bool asclepius_get_connection_status(void);

void asclepius_enable_channel(uint8_t channel);

void asclepius_disable_channel(uint8_t channel);

void asclepius_get_status(void);
