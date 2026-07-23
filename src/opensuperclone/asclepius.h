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

#define ASCLEPIUS_BAUDRATE B115200

#define ASCLEPIUS_STATUS_IOCMD 1
#define ASCLEPIUS_TURNON_IOCMD 2
#define ASCLEPIUS_TURNOFF_IOCMD 3

#define ASCLEPIUS_MAIN12V 1
#define ASCLEPIUS_MAIN5V 2
#define ASCLEPIUS_USB5V 3
#define ASCLEPIUS_MOLEX 4
#define ASCLEPIUS_ALL 5

#define ASCLEPIUS_STATUS_PACKET_SIZE 28

#define ASCLEPIUS_UI_UPDATE_TIMER 500

extern char asclepius_tty_device_ccc[256];
extern char asclepius_ext_tty_device_ccc[256];
extern int asclepius_ext_baud_rate_ccc;
extern bool asclepius_ext_connected;
extern int asclepius_ext_serial_port;

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

int asclepius_connect(void);

void asclepius_disconnect(void);

bool asclepius_get_connection_status(void);

int asclepius_enable_channel(uint8_t channel);

int asclepius_disable_channel(uint8_t channel);

int asclepius_get_status(void);

int asclepius_ext_open(void);

void asclepius_ext_close(void);

int asclepius_ext_send(const char *data, size_t len);

int asclepius_ext_read(char *buffer, size_t max_len);

int asclepius_parse_escapes(const char *input, char *output, size_t max);

// Function pointer set by GUI to capture serial output to the terminal textview
extern void (*asclepius_ext_output_ccc)(const char *text, int byte_count);
