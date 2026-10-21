// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef ASCLEPIUS_H
#define ASCLEPIUS_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
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
#define ASCLEPIUS_STATUS_TIMEOUT_MS 300

// A control-port write may wait for room in the transmit buffer, but not
// forever: the poller holds the serial lock while writing, so an unresponsive
// USB-serial adapter must fail cleanly instead of freezing the GUI.
#define ASCLEPIUS_CONTROL_WRITE_TIMEOUT_MS 5000

// ATX spec voltage limits (in millivolts), +/-5%
#define ASCLEPIUS_V12_MIN_MV 11400
#define ASCLEPIUS_V12_MAX_MV 12600
#define ASCLEPIUS_V5_MIN_MV 4750
#define ASCLEPIUS_V5_MAX_MV 5250

#define ASCLEPIUS_UI_UPDATE_TIMER 500

extern char asclepius_tty_device_ccc[256];
extern char asclepius_ext_tty_device_ccc[256];
extern int asclepius_ext_baud_rate_ccc;
extern bool asclepius_ext_connected;
extern int asclepius_ext_serial_port;
extern _Atomic bool asclepius_connected;

struct asclepius_command_t
{
  uint8_t command;
  uint8_t channel;
};
extern struct asclepius_command_t asclepius_command;

struct asclepius_status_t
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
};
extern struct asclepius_status_t asclepius_status;

// Clone fault guard. The clone engine asks the monitor whether an enabled
// electrical fault has been latched since the run started. An event that is
// set to Stop ends the run after the chunk in progress has been processed.
#define ASCLEPIUS_CLONE_FAULT_RETURN_CODE (-17)
#define ASCLEPIUS_CLONE_CONTINUE 0
#define ASCLEPIUS_CLONE_STOP 1

#define ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP 0
#define ASCLEPIUS_CLONE_FAULT_SOFT_OCP_CUT 1
#define ASCLEPIUS_CLONE_FAULT_SOFT_OCP_WARN 2
#define ASCLEPIUS_CLONE_FAULT_VOLTAGE 3
#define ASCLEPIUS_CLONE_FAULT_COUNT 4

// How long an out-of-spec voltage reading must hold before the poller reports
// it. A rail sitting on a limit jitters across it every poll; without this the
// event log gets two lines per poll instead of one per excursion. The same
// shared value gates the clone guard's voltage event.
#define ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_MIN_MS 0
#define ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_MAX_MS 5000
#define ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_STEP_MS 50
#define ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_DEFAULT_MS 250

typedef enum
{
  ASCLEPIUS_CLONE_ACTION_LOG = 0,
  ASCLEPIUS_CLONE_ACTION_STOP = 1
} asclepius_clone_action_t;

struct asclepius_clone_event_setting_t
{
  bool enabled;
  asclepius_clone_action_t action;
};

struct asclepius_clone_settings_t
{
  struct asclepius_clone_event_setting_t events[ASCLEPIUS_CLONE_FAULT_COUNT];
  unsigned int voltage_debounce_ms;
};
extern struct asclepius_clone_settings_t asclepius_clone_settings_ccc;

// Clamp a clone action / voltage debounce to the supported range
asclepius_clone_action_t asclepius_clone_clamp_action(int action);

unsigned int asclepius_clone_clamp_voltage_debounce(unsigned int debounce_ms);

// Copy the GUI settings into the monitor. The monitor clamps every value.
void asclepius_clone_set_settings(const struct asclepius_clone_settings_t *settings);

void asclepius_clone_get_settings(struct asclepius_clone_settings_t *settings);

// Start/stop one clone run. Starting resets the per-run latch and takes a
// baseline of any fault that is already active, so a pre-existing trip can
// refuse the run while a recovered transient from an earlier run is ignored.
void asclepius_clone_run_begin(void);

// Finish one clone run. Log events that did not stop the run are written here
// as the single combined console block for the run. result is the clone
// return code, used to describe the outcome.
void asclepius_clone_run_end(int result);

// Check the latched monitor events. Returns ASCLEPIUS_CLONE_STOP when an
// enabled Stop event is pending, otherwise ASCLEPIUS_CLONE_CONTINUE. The first
// Stop check writes the combined console block and stores the modal text.
int asclepius_clone_fault_check(long long current_position);

// Record the chunk currently being read, so a fault that is latched during the
// read can report the completed chunk and the position after it.
void asclepius_clone_note_chunk(long long position, long long size, int direction, const char *phase);

// Text for the GUI modal after an Asclepius fault stop.
char *asclepius_clone_get_message(void);

int asclepius_connect(void);

void asclepius_disconnect(void);

bool asclepius_get_connection_status(void);

int asclepius_enable_channel(uint8_t channel);

int asclepius_disable_channel(uint8_t channel);

int asclepius_get_status(void);

// Status poll interval for the background poller thread, in milliseconds
#define ASCLEPIUS_POLL_INTERVAL_MIN_MS 25
#define ASCLEPIUS_POLL_INTERVAL_MAX_MS 5000
#define ASCLEPIUS_POLL_INTERVAL_DEFAULT_MS 25
extern _Atomic unsigned int asclepius_poll_interval_ms;

// Clamp a poll interval to the supported range
unsigned int asclepius_poll_clamp_interval_ms(unsigned int interval_ms);

// Lock/unlock the control port for a snapshot of the asclepius_status global
void asclepius_lock(void);

void asclepius_unlock(void);

// Software OCP trip limits (mA). The board's sense range tops out at 4.5 A.
// A trip acts on the next poll: this is a safety net for custom limits, NOT
// fast-acting hardware protection.
#define ASCLEPIUS_OCP_LIMIT_MIN_MA 500
#define ASCLEPIUS_OCP_LIMIT_MAX_MA 4500
#define ASCLEPIUS_OCP_LIMIT_DEFAULT_MA 4500
#define ASCLEPIUS_OCP_NUM_LIMITS 9

// Action taken when a rail trips its software OCP limit
typedef enum
{
  ASCLEPIUS_OCP_OFF = 0,  // no software monitoring on this rail (default)
  ASCLEPIUS_OCP_WARN = 1, // latch and report, leave the rail powered
  ASCLEPIUS_OCP_CUT = 2   // latch and command the rail off
} asclepius_ocp_action_t;

// Software OCP is a slow supervisory limit. A rail must stay over its limit
// for the full trip delay before it latches, and readings during the startup
// grace window after a rail is switched on are ignored. This keeps a single
// noisy sample or a normal drive spin-up from cutting power.
#define ASCLEPIUS_OCP_TRIP_DELAY_DEFAULT_MS 1000
#define ASCLEPIUS_OCP_STARTUP_GRACE_DEFAULT_MS 2000
#define ASCLEPIUS_OCP_CLEAR_DELAY_DEFAULT_MS 1000

struct asclepius_ocp_rail_t
{
  bool tripped;     // latched until the user intervenes
  uint32_t trip_ma; // current at the moment of the trip
  unsigned int limit_ma;
  asclepius_ocp_action_t action;

  // Timing state, owned by the poller and protected by asclepius_lock().
  unsigned int trip_delay_ms;
  unsigned int startup_grace_ms;
  unsigned int clear_delay_ms;
  bool over_pending;
  uint32_t over_since_ms;
  bool enabled_was_on;
  uint32_t enabled_since_ms;
  bool clear_pending;
  uint32_t clear_since_ms;
};
extern struct asclepius_ocp_rail_t asclepius_ocp_12v_ccc;
extern struct asclepius_ocp_rail_t asclepius_ocp_5v_ccc;
extern struct asclepius_ocp_rail_t asclepius_ocp_usb_ccc;

// Reason for the currently latched trips, empty when nothing is latched
extern char asclepius_soft_ocp_status[256];

// Snapshot of one rail's software OCP state (channel is 12V/5V/USB)
void asclepius_ocp_get_rail_state(uint8_t channel, struct asclepius_ocp_rail_t *state);

// Clamp a limit to the supported range
unsigned int asclepius_ocp_clamp_limit_mA(unsigned int limit_ma);

// Clamp an action to a valid mode
asclepius_ocp_action_t asclepius_ocp_clamp_action(int action);

// Drop every latched trip (board lost, or user reset)
void asclepius_ocp_reset(void);

// Drop the latched trip for one channel after a manual power command
void asclepius_ocp_clear_channel(uint8_t channel);

// Fill names with the serial device paths that could be an Asclepius board,
// counting them. Matches the kernel's USB/ACM serial names plus the project's
// own udev aliases. Returns the number found, up to max_ports; a truncated
// result still reports how many were dropped.
#define ASCLEPIUS_PORT_LIST_MAX 64
#define ASCLEPIUS_PORT_NAME_MAX 256
int asclepius_list_serial_ports(char (*names)[ASCLEPIUS_PORT_NAME_MAX], int max_ports);

int asclepius_ext_open(void);

// How long a passthrough write may wait for room in the transmit buffer before
// giving up. Bounded so an unresponsive peer cannot wedge the caller.
#define ASCLEPIUS_EXT_WRITE_TIMEOUT_MS 5000

void asclepius_ext_close(void);

int asclepius_ext_send(const char *data, size_t len);

int asclepius_ext_read(char *buffer, size_t max_len);

int asclepius_parse_escapes(const char *input, char *output, size_t max);

// Apply terminal Ctrl semantics to one byte: Ctrl-A..Ctrl-Z become 0x01..0x1A
// and every other ASCII byte is masked to its low five bits.
unsigned char asclepius_ctrl_byte(unsigned char c);

// Parse whitespace-separated hex byte pairs, with or without spaces:
// "48 65 6c" and "48656c" both yield "Hel". Returns the byte count, or -1 on a
// malformed token or when the result would not fit.
int asclepius_parse_hex(const char *input, char *output, size_t max);

// Largest retained partial line. A serial read can split a message anywhere,
// so the tail of a chunk is held until its terminator arrives.
#define ASCLEPIUS_RX_LINE_MAX 4096

struct asclepius_linebuf_t
{
  char buf[ASCLEPIUS_RX_LINE_MAX];
  size_t len;
};
typedef struct asclepius_linebuf_t asclepius_linebuf_t;

typedef void (*asclepius_line_cb)(const char *line, size_t line_len, void *user);

void asclepius_linebuf_init(asclepius_linebuf_t *lb);

// Feed received bytes, emitting each complete line through cb. A trailing
// partial line is retained for the next chunk. Treating every read as a whole
// set of lines is what split one reply into several entries.
void asclepius_linebuf_feed(asclepius_linebuf_t *lb, const char *data, size_t len, asclepius_line_cb cb, void *user);

// Emit any retained partial line, for use when the stream stops mid-message.
void asclepius_linebuf_flush(asclepius_linebuf_t *lb, asclepius_line_cb cb, void *user);

// Bytes per line in the hex view.
#define ASCLEPIUS_HEX_PER_LINE 16

// Render one offset-annotated hex line of up to ASCLEPIUS_HEX_PER_LINE bytes,
// as "offset  hex  |ascii|". Returns the length written.
int asclepius_format_hex_line(char *out, size_t out_size, const unsigned char *data, size_t len, unsigned long offset_base);

// Function pointer set by GUI to capture serial output to the terminal textview
extern void (*asclepius_ext_output_ccc)(const char *text, int byte_count);

#endif
