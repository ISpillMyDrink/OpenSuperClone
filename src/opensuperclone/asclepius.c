// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "asclepius.h"
#include "monitor.h"

#include <ctype.h>
#include <dirent.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

_Atomic bool asclepius_connected = false;
int asclepius_serial_port = -1;

_Atomic unsigned int asclepius_poll_interval_ms = ASCLEPIUS_POLL_INTERVAL_DEFAULT_MS;

struct asclepius_ocp_rail_t asclepius_ocp_12v_ccc = {
  .limit_ma = ASCLEPIUS_OCP_LIMIT_DEFAULT_MA,
  .action = ASCLEPIUS_OCP_OFF,
  .trip_delay_ms = ASCLEPIUS_OCP_TRIP_DELAY_DEFAULT_MS,
  .startup_grace_ms = ASCLEPIUS_OCP_STARTUP_GRACE_DEFAULT_MS,
  .clear_delay_ms = ASCLEPIUS_OCP_CLEAR_DELAY_DEFAULT_MS,
};
struct asclepius_ocp_rail_t asclepius_ocp_5v_ccc = {
  .limit_ma = ASCLEPIUS_OCP_LIMIT_DEFAULT_MA,
  .action = ASCLEPIUS_OCP_OFF,
  .trip_delay_ms = ASCLEPIUS_OCP_TRIP_DELAY_DEFAULT_MS,
  .startup_grace_ms = ASCLEPIUS_OCP_STARTUP_GRACE_DEFAULT_MS,
  .clear_delay_ms = ASCLEPIUS_OCP_CLEAR_DELAY_DEFAULT_MS,
};
struct asclepius_ocp_rail_t asclepius_ocp_usb_ccc = {
  .limit_ma = ASCLEPIUS_OCP_LIMIT_DEFAULT_MA,
  .action = ASCLEPIUS_OCP_OFF,
  .trip_delay_ms = ASCLEPIUS_OCP_TRIP_DELAY_DEFAULT_MS,
  .startup_grace_ms = ASCLEPIUS_OCP_STARTUP_GRACE_DEFAULT_MS,
  .clear_delay_ms = ASCLEPIUS_OCP_CLEAR_DELAY_DEFAULT_MS,
};
char asclepius_soft_ocp_status[256] = "";

static pthread_mutex_t asclepius_serial_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t asclepius_port_claim_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t asclepius_poller_thread;
static bool asclepius_poller_running = false;
static volatile bool asclepius_stop_poller = false;

// One physical tty may only be opened by one role. The control port and the
// terminal passthrough are separate interfaces on the same board, but their
// paths can be swapped or aliased; tracking the resolved device prevents both
// from reading the same byte stream.
typedef enum
{
  ASCLEPIUS_PORT_CONTROL = 0,
  ASCLEPIUS_PORT_TERMINAL
} asclepius_port_role_t;

static bool asclepius_port_claimed[2] = {false, false};
static dev_t asclepius_port_rdev[2];

char asclepius_tty_device_ccc[256] = "";
char asclepius_ext_tty_device_ccc[256] = "";

bool asclepius_ext_connected = false;
int asclepius_ext_serial_port = -1;
int asclepius_ext_baud_rate_ccc = 38400;
void (*asclepius_ext_output_ccc)(const char *text, int byte_count) = NULL;

struct asclepius_command_t asclepius_command;
struct asclepius_status_t asclepius_status;

static bool asclepius_resolve_device(const char *path, dev_t *rdev)
{
  struct stat st;

  if (path == NULL || path[0] == '\0')
  {
    return false;
  }

  if (stat(path, &st) != 0)
  {
    return false;
  }

  *rdev = st.st_rdev;
  return true;
}

// Claim a tty for one role before opening it. The terminal may never claim the
// configured control port, even while the control side is disconnected: opening
// it would consume protocol bytes as soon as the poller reconnects.
static bool asclepius_serial_claim(asclepius_port_role_t role, const char *path)
{
  asclepius_port_role_t other = role == ASCLEPIUS_PORT_CONTROL ? ASCLEPIUS_PORT_TERMINAL : ASCLEPIUS_PORT_CONTROL;
  dev_t rdev;
  dev_t control_rdev;
  bool allowed = true;

  if (!asclepius_resolve_device(path, &rdev))
  {
    return false;
  }

  pthread_mutex_lock(&asclepius_port_claim_mutex);

  asclepius_port_claimed[role] = false;

  if (asclepius_port_claimed[other] && asclepius_port_rdev[other] == rdev)
  {
    allowed = false;
  }

  if (allowed && role == ASCLEPIUS_PORT_TERMINAL && asclepius_resolve_device(asclepius_tty_device_ccc, &control_rdev) && control_rdev == rdev)
  {
    allowed = false;
  }

  if (allowed)
  {
    asclepius_port_claimed[role] = true;
    asclepius_port_rdev[role] = rdev;
  }
  else
  {
    errno = EBUSY;
  }

  pthread_mutex_unlock(&asclepius_port_claim_mutex);

  return allowed;
}

static void asclepius_serial_release(asclepius_port_role_t role)
{
  pthread_mutex_lock(&asclepius_port_claim_mutex);
  asclepius_port_claimed[role] = false;
  pthread_mutex_unlock(&asclepius_port_claim_mutex);
}

static bool asclepius_write_all(const void *buffer, size_t length)
{
  const uint8_t *data = buffer;
  size_t offset = 0;
  struct timespec deadline;

  clock_gettime(CLOCK_MONOTONIC, &deadline);
  deadline.tv_sec += ASCLEPIUS_CONTROL_WRITE_TIMEOUT_MS / 1000;
  deadline.tv_nsec += (long)(ASCLEPIUS_CONTROL_WRITE_TIMEOUT_MS % 1000) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L)
  {
    deadline.tv_sec += deadline.tv_nsec / 1000000000L;
    deadline.tv_nsec %= 1000000000L;
  }

  while (offset < length)
  {
    ssize_t bytes_written = write(asclepius_serial_port, data + offset, length - offset);

    if (bytes_written < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        struct timespec now;
        struct pollfd poll_fd;
        long remaining_ms;
        int poll_result;

        clock_gettime(CLOCK_MONOTONIC, &now);
        remaining_ms = (long)(deadline.tv_sec - now.tv_sec) * 1000L + (deadline.tv_nsec - now.tv_nsec) / 1000000L;

        if (remaining_ms <= 0)
        {
          printf("Error writing: timed out after %zu of %zu bytes\n", offset, length);
          return false;
        }

        poll_fd.fd = asclepius_serial_port;
        poll_fd.events = POLLOUT;
        poll_fd.revents = 0;

        poll_result = poll(&poll_fd, 1, (int)remaining_ms);

        if (poll_result < 0)
        {
          if (errno == EINTR)
          {
            continue;
          }

          printf("Error writing: %s\n", strerror(errno));
          return false;
        }

        if (poll_result == 0)
        {
          printf("Error writing: timed out after %zu of %zu bytes\n", offset, length);
          return false;
        }

        if ((poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        {
          printf("Error writing: port hung up\n");
          return false;
        }

        continue;
      }

      printf("Error writing: %s\n", strerror(errno));
      return false;
    }

    if (bytes_written == 0)
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
  struct timespec deadline;
  struct timespec now;

  clock_gettime(CLOCK_MONOTONIC, &deadline);
  deadline.tv_sec += ASCLEPIUS_STATUS_TIMEOUT_MS / 1000;
  deadline.tv_nsec += (long)(ASCLEPIUS_STATUS_TIMEOUT_MS % 1000) * 1000000L;
  if (deadline.tv_nsec >= 1000000000L)
  {
    deadline.tv_sec += deadline.tv_nsec / 1000000000L;
    deadline.tv_nsec %= 1000000000L;
  }

  while (offset < length)
  {
    struct pollfd poll_fd;
    struct timespec remaining;
    int poll_result;

    clock_gettime(CLOCK_MONOTONIC, &now);
    if (now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
    {
      printf("Error reading: timed out after %zu of %zu bytes\n", offset, length);
      return false;
    }

    remaining.tv_sec = deadline.tv_sec - now.tv_sec;
    remaining.tv_nsec = deadline.tv_nsec - now.tv_nsec;
    if (remaining.tv_nsec < 0)
    {
      remaining.tv_nsec += 1000000000L;
      remaining.tv_sec -= 1;
    }

    poll_fd.fd = asclepius_serial_port;
    poll_fd.events = POLLIN;
    poll_fd.revents = 0;

    poll_result = poll(&poll_fd, 1, (int)(remaining.tv_sec * 1000 + remaining.tv_nsec / 1000000L));

    if (poll_result < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

      printf("Error reading: %s\n", strerror(errno));
      return false;
    }

    if (poll_result == 0)
    {
      printf("Error reading: timed out after %zu of %zu bytes\n", offset, length);
      return false;
    }

    ssize_t bytes_read = read(asclepius_serial_port, data + offset, length - offset);

    if (bytes_read < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

      printf("Error reading: %s\n", strerror(errno));
      return false;
    }

    if (bytes_read == 0)
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
  return ((uint32_t)buffer[0]) | ((uint32_t)buffer[1] << 8) | ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);
}

static bool asclepius_read_status_packet(void)
{
  uint8_t packet[ASCLEPIUS_STATUS_PACKET_SIZE];

  if (!asclepius_read_all(packet, sizeof(packet)))
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

  if (tcgetattr(asclepius_serial_port, &tty) != 0)
  {
    printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    return -1;
  }

  tty.c_cflag &= ~PARENB; // No parity
  tty.c_cflag &= ~CSTOPB; // 1 stop bit
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;            // 8 bits per byte
  tty.c_cflag &= ~CRTSCTS;       // No flow control
  tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines

  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO;   // Disable echo
  tty.c_lflag &= ~ECHOE;  // Disable erasure
  tty.c_lflag &= ~ECHONL; // Disable new-line echo
  tty.c_lflag &= ~ISIG;   // Disable interpretation of INTR, QUIT and SUSP

  tty.c_iflag &= ~(IXON | IXOFF | IXANY);                                      // Turn off s/w flow ctrl
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); // Disable special handling of received bytes

  tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes
  tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

  tty.c_cc[VTIME] = 0; // No per-read timeout; poll() enforces the read deadline
  tty.c_cc[VMIN] = 0;  // Non-blocking reads

  cfsetispeed(&tty, ASCLEPIUS_BAUDRATE);
  cfsetospeed(&tty, ASCLEPIUS_BAUDRATE);

  if (tcsetattr(asclepius_serial_port, TCSANOW, &tty) != 0)
  {
    printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    return -1;
  }

  tcflush(asclepius_serial_port, TCIOFLUSH);

  return 0;
}

static bool asclepius_reconnect(void)
{
  // The poller retries once a second, forever. Reporting every attempt buries
  // whatever else is on the console, so only speak up when the outcome
  // changes: the first failure, a different error, or recovery.
  static int last_errno = 0;

  if (asclepius_serial_port >= 0)
  {
    close(asclepius_serial_port);
    asclepius_serial_port = -1;
  }

  asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);

  if (!asclepius_serial_claim(ASCLEPIUS_PORT_CONTROL, asclepius_tty_device_ccc))
  {
    if (errno != last_errno)
    {
      if (errno == EBUSY)
      {
        printf("Error: Asclepius control port %s is already in use by the UART terminal\n", asclepius_tty_device_ccc);
      }
      else
      {
        printf("Error claiming Asclepius control port %s: %s\n", asclepius_tty_device_ccc, strerror(errno));
      }
      last_errno = errno;
    }
    return false;
  }

  asclepius_serial_port = open(asclepius_tty_device_ccc, O_RDWR | O_NOCTTY | O_NONBLOCK);

  if (asclepius_serial_port < 0)
  {
    if (errno != last_errno)
    {
      // Name the device: a bare errno does not say what to go and fix,
      // and this is the message a user sees when the board has moved to
      // a different tty after re-enumerating.
      printf("Error %i opening Asclepius port %s: %s\n", errno, asclepius_tty_device_ccc, strerror(errno));
      last_errno = errno;
    }
    asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);
    return false;
  }

  if (asclepius_setup_tty() != 0)
  {
    close(asclepius_serial_port);
    asclepius_serial_port = -1;
    asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);
    return false;
  }

  if (last_errno != 0)
  {
    printf("Asclepius: reconnected to %s\n", asclepius_tty_device_ccc);
    last_errno = 0;
  }

  return true;
}

void asclepius_lock(void)
{
  pthread_mutex_lock(&asclepius_serial_mutex);
}

void asclepius_unlock(void)
{
  pthread_mutex_unlock(&asclepius_serial_mutex);
}

static uint32_t asclepius_monotonic_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);

  return (uint32_t)((uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000L);
}

static void asclepius_sleep_ms(unsigned int ms)
{
  struct timespec ts;
  struct timespec rem;

  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (long)(ms % 1000) * 1000000L;

  while (nanosleep(&ts, &rem) != 0)
  {
    if (errno != EINTR)
    {
      break;
    }
    ts = rem;
  }
}

static struct asclepius_ocp_rail_t *asclepius_ocp_rail(uint8_t channel)
{
  switch (channel)
  {
  case ASCLEPIUS_MAIN12V:
    return &asclepius_ocp_12v_ccc;
  case ASCLEPIUS_MAIN5V:
    return &asclepius_ocp_5v_ccc;
  case ASCLEPIUS_USB5V:
    return &asclepius_ocp_usb_ccc;
  default:
    return NULL;
  }
}

static const char *asclepius_ocp_rail_name(uint8_t channel)
{
  switch (channel)
  {
  case ASCLEPIUS_MAIN12V:
    return "12V";
  case ASCLEPIUS_MAIN5V:
    return "5V";
  case ASCLEPIUS_USB5V:
    return "USB";
  default:
    return "unknown";
  }
}

// ---------------------------------------------------------------------------
// Clone fault guard
//
// The monitor already detects hardware OCP, software OCP trips, and debounced
// voltage excursions. This layer records the rail state at the moment each
// enabled event is seen, and lets the clone engine stop at a chunk boundary.
// It deliberately does not take any power action: the rails are left exactly
// as the fault found them.
// ---------------------------------------------------------------------------

#define ASCLEPIUS_CLONE_RAIL_COUNT 3
#define ASCLEPIUS_CLONE_EVENT_BIT(event) (1u << (event))

static const uint8_t asclepius_clone_channels[ASCLEPIUS_CLONE_RAIL_COUNT] = {ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V};

struct asclepius_clone_settings_t asclepius_clone_settings_ccc = {
  .events =
    {
      {false, ASCLEPIUS_CLONE_ACTION_STOP},
      {false, ASCLEPIUS_CLONE_ACTION_STOP},
      {false, ASCLEPIUS_CLONE_ACTION_LOG},
      {false, ASCLEPIUS_CLONE_ACTION_STOP},
    },
  .voltage_debounce_ms = ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_DEFAULT_MS,
};

struct asclepius_clone_rail_snapshot_t
{
  uint32_t events;
  uint32_t voltage_mv;
  uint32_t current_ma;
  bool enabled;
  struct asclepius_ocp_rail_t ocp;
};

struct asclepius_clone_snapshot_t
{
  struct asclepius_clone_rail_snapshot_t rails[ASCLEPIUS_CLONE_RAIL_COUNT];
};

static struct asclepius_clone_snapshot_t asclepius_clone_latch;
static struct asclepius_clone_snapshot_t asclepius_clone_observed;
static bool asclepius_clone_run_active = false;
static bool asclepius_clone_stop_requested = false;
static bool asclepius_clone_block_written = false;
static bool asclepius_clone_observed_any = false;
static bool asclepius_clone_have_last_chunk = false;
static long long asclepius_clone_last_lba = 0;
static long long asclepius_clone_last_size = 0;
static long long asclepius_clone_last_check_position = 0;
static int asclepius_clone_last_direction = 0;
static char asclepius_clone_last_phase[128] = "";
static char asclepius_clone_message[4096] = "";

asclepius_clone_action_t asclepius_clone_clamp_action(int action)
{
  return action == ASCLEPIUS_CLONE_ACTION_STOP ? ASCLEPIUS_CLONE_ACTION_STOP : ASCLEPIUS_CLONE_ACTION_LOG;
}

unsigned int asclepius_clone_clamp_voltage_debounce(unsigned int debounce_ms)
{
  // The minimum is zero, so an unsigned value is already at or above it.
  if (debounce_ms > ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_MAX_MS)
  {
    debounce_ms = ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_MAX_MS;
  }

  debounce_ms = ((debounce_ms + (ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_STEP_MS / 2)) / ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_STEP_MS) *
                ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_STEP_MS;

  if (debounce_ms > ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_MAX_MS)
  {
    debounce_ms = ASCLEPIUS_CLONE_VOLTAGE_DEBOUNCE_MAX_MS;
  }

  return debounce_ms;
}

void asclepius_clone_set_settings(const struct asclepius_clone_settings_t *settings)
{
  struct asclepius_clone_settings_t copy;
  size_t i;

  if (settings == NULL)
  {
    return;
  }

  copy = *settings;
  for (i = 0; i < ASCLEPIUS_CLONE_FAULT_COUNT; i++)
  {
    copy.events[i].action = asclepius_clone_clamp_action(copy.events[i].action);
  }
  copy.voltage_debounce_ms = asclepius_clone_clamp_voltage_debounce(copy.voltage_debounce_ms);

  asclepius_lock();
  asclepius_clone_settings_ccc = copy;
  asclepius_unlock();
}

void asclepius_clone_get_settings(struct asclepius_clone_settings_t *settings)
{
  if (settings == NULL)
  {
    return;
  }

  asclepius_lock();
  *settings = asclepius_clone_settings_ccc;
  asclepius_unlock();
}

static int asclepius_clone_rail_index(uint8_t channel)
{
  size_t i;

  for (i = 0; i < ASCLEPIUS_CLONE_RAIL_COUNT; i++)
  {
    if (asclepius_clone_channels[i] == channel)
    {
      return (int)i;
    }
  }

  return -1;
}

static const char *asclepius_clone_event_name(int event)
{
  switch (event)
  {
  case ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP:
    return "hardware OCP";
  case ASCLEPIUS_CLONE_FAULT_SOFT_OCP_CUT:
    return "software OCP cut";
  case ASCLEPIUS_CLONE_FAULT_SOFT_OCP_WARN:
    return "software OCP warn";
  case ASCLEPIUS_CLONE_FAULT_VOLTAGE:
    return "voltage out of spec";
  default:
    return "unknown event";
  }
}

// Capture the rail state for one event. Must be called with the lock held.
static void asclepius_clone_rail_snapshot_locked(int rail_index, int event)
{
  struct asclepius_clone_rail_snapshot_t *snapshot;
  const struct asclepius_ocp_rail_t *rail;

  if (rail_index < 0 || rail_index >= ASCLEPIUS_CLONE_RAIL_COUNT)
  {
    return;
  }

  snapshot = &asclepius_clone_latch.rails[rail_index];
  snapshot->events |= ASCLEPIUS_CLONE_EVENT_BIT(event);

  switch (asclepius_clone_channels[rail_index])
  {
  case ASCLEPIUS_MAIN12V:
    snapshot->enabled = asclepius_status.main12v_enabled;
    snapshot->voltage_mv = asclepius_status.main12v_voltage;
    snapshot->current_ma = asclepius_status.main12v_current;
    break;
  case ASCLEPIUS_MAIN5V:
    snapshot->enabled = asclepius_status.main5v_enabled;
    snapshot->voltage_mv = asclepius_status.main5v_voltage;
    snapshot->current_ma = asclepius_status.main5v_current;
    break;
  case ASCLEPIUS_USB5V:
    snapshot->enabled = asclepius_status.usb5v_enabled;
    snapshot->voltage_mv = 0;
    snapshot->current_ma = asclepius_status.usb5v_current;
    break;
  default:
    snapshot->enabled = false;
    snapshot->voltage_mv = 0;
    snapshot->current_ma = 0;
    break;
  }

  rail = asclepius_ocp_rail(asclepius_clone_channels[rail_index]);
  if (rail != NULL)
  {
    snapshot->ocp = *rail;
  }
  else
  {
    memset(&snapshot->ocp, 0, sizeof(snapshot->ocp));
  }
}

// Must be called with the lock held.
static void asclepius_clone_latch_hardware_ocp_locked(void)
{
  if (!asclepius_clone_run_active || !asclepius_connected)
  {
    return;
  }

  if (asclepius_status.main12v_ocp)
  {
    asclepius_clone_rail_snapshot_locked(0, ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP);
  }
  if (asclepius_status.main5v_ocp)
  {
    asclepius_clone_rail_snapshot_locked(1, ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP);
  }
  if (asclepius_status.usb5v_ocp)
  {
    asclepius_clone_rail_snapshot_locked(2, ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP);
  }
}

// Must be called with the lock held.
static void asclepius_clone_latch_soft_ocp_locked(int rail_index, asclepius_ocp_action_t action)
{
  if (!asclepius_clone_run_active)
  {
    return;
  }

  if (action == ASCLEPIUS_OCP_CUT)
  {
    asclepius_clone_rail_snapshot_locked(rail_index, ASCLEPIUS_CLONE_FAULT_SOFT_OCP_CUT);
  }
  else if (action == ASCLEPIUS_OCP_WARN)
  {
    asclepius_clone_rail_snapshot_locked(rail_index, ASCLEPIUS_CLONE_FAULT_SOFT_OCP_WARN);
  }
}

// Voltage events are found by the poller after its debounce interval. This
// helper takes the lock itself because asclepius_event_check() has already
// released it when it reports an excursion.
static void asclepius_clone_latch_voltage(uint8_t channel, uint32_t voltage_mv)
{
  int rail_index = asclepius_clone_rail_index(channel);

  if (rail_index < 0)
  {
    return;
  }

  asclepius_lock();
  if (asclepius_clone_run_active)
  {
    asclepius_clone_rail_snapshot_locked(rail_index, ASCLEPIUS_CLONE_FAULT_VOLTAGE);
    asclepius_clone_latch.rails[rail_index].voltage_mv = voltage_mv;
  }
  asclepius_unlock();
}

// Must be called with the lock held. Startup includes voltage so a rail that is
// already outside its limits refuses the run; normal checks do not rescan
// voltage, because the poller's debounce is the authority for excursions.
static void asclepius_clone_scan_locked(bool include_voltage)
{
  size_t i;

  if (!asclepius_connected)
  {
    return;
  }

  for (i = 0; i < ASCLEPIUS_CLONE_RAIL_COUNT; i++)
  {
    struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(asclepius_clone_channels[i]);
    bool out_of_spec = false;

    if (asclepius_clone_channels[i] == ASCLEPIUS_MAIN12V && asclepius_status.main12v_ocp)
    {
      asclepius_clone_rail_snapshot_locked((int)i, ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP);
    }
    else if (asclepius_clone_channels[i] == ASCLEPIUS_MAIN5V && asclepius_status.main5v_ocp)
    {
      asclepius_clone_rail_snapshot_locked((int)i, ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP);
    }
    else if (asclepius_clone_channels[i] == ASCLEPIUS_USB5V && asclepius_status.usb5v_ocp)
    {
      asclepius_clone_rail_snapshot_locked((int)i, ASCLEPIUS_CLONE_FAULT_HARDWARE_OCP);
    }

    if (rail == NULL || !rail->tripped)
    {
      if (!include_voltage)
      {
        continue;
      }
    }
    else
    {
      asclepius_clone_latch_soft_ocp_locked((int)i, rail->action);
    }

    if (!include_voltage)
    {
      continue;
    }

    if (asclepius_clone_channels[i] == ASCLEPIUS_MAIN12V)
    {
      out_of_spec = asclepius_status.main12v_enabled &&
                    (asclepius_status.main12v_voltage < ASCLEPIUS_V12_MIN_MV || asclepius_status.main12v_voltage > ASCLEPIUS_V12_MAX_MV);
    }
    else if (asclepius_clone_channels[i] == ASCLEPIUS_MAIN5V)
    {
      out_of_spec = asclepius_status.main5v_enabled &&
                    (asclepius_status.main5v_voltage < ASCLEPIUS_V5_MIN_MV || asclepius_status.main5v_voltage > ASCLEPIUS_V5_MAX_MV);
    }

    if (out_of_spec)
    {
      uint32_t voltage_mv = asclepius_clone_channels[i] == ASCLEPIUS_MAIN12V ? asclepius_status.main12v_voltage : asclepius_status.main5v_voltage;
      asclepius_clone_rail_snapshot_locked((int)i, ASCLEPIUS_CLONE_FAULT_VOLTAGE);
      asclepius_clone_latch.rails[i].voltage_mv = voltage_mv;
    }
  }
}

// Must be called with the lock held. Only enabled events are copied into the
// run's observed snapshot; disabled event types are ignored entirely.
static void asclepius_clone_merge_locked(void)
{
  int rail_index;
  int event;

  for (rail_index = 0; rail_index < ASCLEPIUS_CLONE_RAIL_COUNT; rail_index++)
  {
    for (event = 0; event < ASCLEPIUS_CLONE_FAULT_COUNT; event++)
    {
      uint32_t bit = ASCLEPIUS_CLONE_EVENT_BIT(event);

      if (!(asclepius_clone_latch.rails[rail_index].events & bit) || !asclepius_clone_settings_ccc.events[event].enabled)
      {
        continue;
      }

      asclepius_clone_observed.rails[rail_index].events |= bit;
      asclepius_clone_observed.rails[rail_index].voltage_mv = asclepius_clone_latch.rails[rail_index].voltage_mv;
      asclepius_clone_observed.rails[rail_index].current_ma = asclepius_clone_latch.rails[rail_index].current_ma;
      asclepius_clone_observed.rails[rail_index].enabled = asclepius_clone_latch.rails[rail_index].enabled;
      asclepius_clone_observed.rails[rail_index].ocp = asclepius_clone_latch.rails[rail_index].ocp;
      asclepius_clone_observed_any = true;

      if (asclepius_clone_settings_ccc.events[event].action == ASCLEPIUS_CLONE_ACTION_STOP)
      {
        asclepius_clone_stop_requested = true;
      }
    }
  }
}

static void asclepius_clone_appendf(char *buffer, size_t size, size_t *used, const char *format, ...)
{
  va_list args;
  int written;

  if (*used >= size)
  {
    return;
  }

  va_start(args, format);
  written = vsnprintf(buffer + *used, size - *used, format, args);
  va_end(args);

  if (written < 0)
  {
    return;
  }

  *used += (size_t)written;
  if (*used >= size)
  {
    *used = size - 1;
  }
}

// Must be called with the lock held. The same text is used for the console
// block and the GUI modal.
static void asclepius_clone_format_locked(long long current_position, int result)
{
  char timestamp[64] = "";
  const char *phase = asclepius_clone_last_phase[0] != '\0' ? asclepius_clone_last_phase : "startup";
  const char *direction = asclepius_clone_last_direction == 0 ? "forward" : (asclepius_clone_last_direction == 1 ? "reverse" : "unknown");
  time_t now = time(NULL);
  struct tm local_time;
  size_t used = 0;
  int rail_index;
  int event;

  if (localtime_r(&now, &local_time) != NULL)
  {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
  }

  asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Asclepius clone fault guard%s%s\n",
                          timestamp[0] != '\0' ? " - " : "", timestamp);

  if (asclepius_clone_stop_requested)
  {
    if (asclepius_clone_have_last_chunk)
    {
      asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used,
                              "Outcome: clone stopped after the current chunk; no additional power action was taken.\n");
    }
    else
    {
      asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used,
                              "Outcome: clone refused before the first chunk; no additional power action was taken.\n");
    }
  }
  else if (result == 0)
  {
    asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used,
                            "Outcome: clone completed; log-only faults were recorded.\n");
  }
  else
  {
    asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used,
                            "Outcome: clone ended with return code %d; log-only faults were recorded.\n", result);
  }

  asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Phase: %s, direction: %s\n", phase, direction);

  if (asclepius_clone_have_last_chunk)
  {
    asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Completed/attempted chunk: LBA 0x%llx, %lld sectors\n",
                            (unsigned long long)asclepius_clone_last_lba, asclepius_clone_last_size);
  }
  else
  {
    asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Completed/attempted chunk: none\n");
  }

  asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Current position: LBA 0x%llx\n",
                          (unsigned long long)current_position);

  asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Latched events:\n");

  for (rail_index = 0; rail_index < ASCLEPIUS_CLONE_RAIL_COUNT; rail_index++)
  {
    bool first = true;

    if (asclepius_clone_observed.rails[rail_index].events == 0)
    {
      continue;
    }

    asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used,
                            "  %s: ", asclepius_ocp_rail_name(asclepius_clone_channels[rail_index]));

    for (event = 0; event < ASCLEPIUS_CLONE_FAULT_COUNT; event++)
    {
      if (asclepius_clone_observed.rails[rail_index].events & ASCLEPIUS_CLONE_EVENT_BIT(event))
      {
        asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "%s%s", first ? "" : ", ",
                                asclepius_clone_event_name(event));
        first = false;
      }
    }

    asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "\n");
  }

  asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "Rail snapshot at event time:\n");

  for (rail_index = 0; rail_index < ASCLEPIUS_CLONE_RAIL_COUNT; rail_index++)
  {
    const struct asclepius_clone_rail_snapshot_t *snapshot = &asclepius_clone_observed.rails[rail_index];

    if (snapshot->events == 0)
    {
      continue;
    }

    if (asclepius_clone_channels[rail_index] == ASCLEPIUS_USB5V)
    {
      asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "  USB 5V: no voltage reading, %u mA, %s\n",
                              snapshot->current_ma, snapshot->enabled ? "on" : "off");
    }
    else
    {
      asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "  %s: %.3f V, %u mA, %s\n",
                              asclepius_ocp_rail_name(asclepius_clone_channels[rail_index]), snapshot->voltage_mv / 1000.0, snapshot->current_ma,
                              snapshot->enabled ? "on" : "off");
    }

    if (snapshot->ocp.tripped)
    {
      asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used,
                              "    software OCP: tripped at %u mA (limit %u mA, %s)\n", snapshot->ocp.trip_ma, snapshot->ocp.limit_ma,
                              snapshot->ocp.action == ASCLEPIUS_OCP_CUT ? "cut" : "warn");
    }
    else if (snapshot->ocp.action != ASCLEPIUS_OCP_OFF)
    {
      asclepius_clone_appendf(asclepius_clone_message, sizeof(asclepius_clone_message), &used, "    software OCP: armed at %u mA (%s), not tripped\n",
                              snapshot->ocp.limit_ma, snapshot->ocp.action == ASCLEPIUS_OCP_CUT ? "cut" : "warn");
    }
  }
}

void asclepius_clone_run_begin(void)
{
  asclepius_lock();

  asclepius_clone_run_active = true;
  asclepius_clone_stop_requested = false;
  asclepius_clone_block_written = false;
  asclepius_clone_observed_any = false;
  asclepius_clone_have_last_chunk = false;
  asclepius_clone_last_lba = 0;
  asclepius_clone_last_size = 0;
  asclepius_clone_last_check_position = 0;
  asclepius_clone_last_direction = -1;
  asclepius_clone_last_phase[0] = '\0';
  asclepius_clone_message[0] = '\0';
  memset(&asclepius_clone_latch, 0, sizeof(asclepius_clone_latch));
  memset(&asclepius_clone_observed, 0, sizeof(asclepius_clone_observed));

  asclepius_clone_scan_locked(true);

  asclepius_unlock();
}

void asclepius_clone_run_end(int result)
{
  bool write_block = false;
  long long current_position;

  asclepius_lock();

  if (!asclepius_clone_run_active)
  {
    asclepius_unlock();
    return;
  }

  current_position = asclepius_clone_have_last_chunk ? asclepius_clone_last_lba + asclepius_clone_last_size : asclepius_clone_last_check_position;

  if (asclepius_clone_observed_any && !asclepius_clone_block_written)
  {
    asclepius_clone_format_locked(current_position, result);
    asclepius_clone_block_written = true;
    write_block = true;
  }

  asclepius_clone_run_active = false;
  asclepius_clone_stop_requested = false;
  asclepius_clone_observed_any = false;
  memset(&asclepius_clone_latch, 0, sizeof(asclepius_clone_latch));
  memset(&asclepius_clone_observed, 0, sizeof(asclepius_clone_observed));

  asclepius_unlock();

  if (write_block)
  {
    printf("%s", asclepius_clone_message);
    fflush(stdout);
  }
}

int asclepius_clone_fault_check(long long current_position)
{
  bool write_block = false;
  int result;

  asclepius_lock();

  if (!asclepius_clone_run_active)
  {
    asclepius_unlock();
    return ASCLEPIUS_CLONE_CONTINUE;
  }

  asclepius_clone_scan_locked(false);
  asclepius_clone_merge_locked();
  asclepius_clone_last_check_position = current_position;

  if (asclepius_clone_stop_requested && !asclepius_clone_block_written)
  {
    asclepius_clone_format_locked(current_position, ASCLEPIUS_CLONE_FAULT_RETURN_CODE);
    asclepius_clone_block_written = true;
    write_block = true;
  }

  result = asclepius_clone_stop_requested ? ASCLEPIUS_CLONE_STOP : ASCLEPIUS_CLONE_CONTINUE;

  asclepius_unlock();

  if (write_block)
  {
    printf("%s", asclepius_clone_message);
    fflush(stdout);
  }

  return result;
}

void asclepius_clone_note_chunk(long long position, long long size, int direction, const char *phase)
{
  asclepius_lock();

  if (asclepius_clone_run_active)
  {
    asclepius_clone_last_lba = position;
    asclepius_clone_last_size = size;
    asclepius_clone_last_direction = direction;
    asclepius_clone_have_last_chunk = true;
    snprintf(asclepius_clone_last_phase, sizeof(asclepius_clone_last_phase), "%s", phase != NULL ? phase : "");
  }

  asclepius_unlock();
}

char *asclepius_clone_get_message(void)
{
  return asclepius_clone_message;
}

unsigned int asclepius_poll_clamp_interval_ms(unsigned int interval_ms)
{
  if (interval_ms < ASCLEPIUS_POLL_INTERVAL_MIN_MS)
  {
    return ASCLEPIUS_POLL_INTERVAL_MIN_MS;
  }

  if (interval_ms > ASCLEPIUS_POLL_INTERVAL_MAX_MS)
  {
    return ASCLEPIUS_POLL_INTERVAL_MAX_MS;
  }

  return interval_ms;
}

unsigned int asclepius_ocp_clamp_limit_mA(unsigned int limit_ma)
{
  if (limit_ma < ASCLEPIUS_OCP_LIMIT_MIN_MA)
  {
    return ASCLEPIUS_OCP_LIMIT_MIN_MA;
  }

  if (limit_ma > ASCLEPIUS_OCP_LIMIT_MAX_MA)
  {
    return ASCLEPIUS_OCP_LIMIT_MAX_MA;
  }

  return limit_ma;
}

asclepius_ocp_action_t asclepius_ocp_clamp_action(int action)
{
  if (action < ASCLEPIUS_OCP_OFF || action > ASCLEPIUS_OCP_CUT)
  {
    return ASCLEPIUS_OCP_OFF;
  }

  return (asclepius_ocp_action_t)action;
}

void asclepius_ocp_get_rail_state(uint8_t channel, struct asclepius_ocp_rail_t *state)
{
  struct asclepius_ocp_rail_t *rail;

  if (state == NULL)
  {
    return;
  }

  asclepius_lock();

  rail = asclepius_ocp_rail(channel);
  if (rail != NULL)
  {
    *state = *rail;
  }
  else
  {
    memset(state, 0, sizeof(*state));
  }

  asclepius_unlock();
}

// Rebuild the human readable trip reason. Must be called with the lock held.
static void asclepius_ocp_build_status(void)
{
  static const uint8_t channels[] = {ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V};
  size_t used = 0;
  size_t i;

  asclepius_soft_ocp_status[0] = '\0';

  for (i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
  {
    const struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(channels[i]);
    int written;

    if (rail == NULL || !rail->tripped)
    {
      continue;
    }

    written = snprintf(asclepius_soft_ocp_status + used, sizeof(asclepius_soft_ocp_status) - used, "%sSoftware OCP: %s tripped at %.2f A%s",
                       used > 0 ? "; " : "", asclepius_ocp_rail_name(channels[i]), rail->trip_ma / 1000.0f,
                       rail->action == ASCLEPIUS_OCP_CUT ? " (channel off)" : "");

    if (written < 0)
    {
      break;
    }

    used += (size_t)written;
    if (used >= sizeof(asclepius_soft_ocp_status))
    {
      used = sizeof(asclepius_soft_ocp_status) - 1;
      break;
    }
  }
}

void asclepius_ocp_reset(void)
{
  static const uint8_t channels[] = {ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V};
  uint32_t now_ms = asclepius_monotonic_ms();
  size_t i;

  asclepius_lock();

  for (i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
  {
    struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(channels[i]);

    if (rail != NULL)
    {
      rail->tripped = false;
      rail->trip_ma = 0;
      rail->over_pending = false;
      rail->clear_pending = false;
      // A manual reset is also a fresh start for the debounce and grace
      // timers, so the same fault does not re-latch on the next poll.
      if (rail->enabled_was_on)
      {
        rail->enabled_since_ms = now_ms;
      }
    }
  }

  asclepius_ocp_build_status();

  asclepius_unlock();
}

void asclepius_ocp_clear_channel(uint8_t channel)
{
  struct asclepius_ocp_rail_t *rail;
  uint32_t now_ms = asclepius_monotonic_ms();

  asclepius_lock();

  rail = asclepius_ocp_rail(channel);
  if (rail != NULL)
  {
    rail->tripped = false;
    rail->trip_ma = 0;
    rail->over_pending = false;
    rail->clear_pending = false;
    if (rail->enabled_was_on)
    {
      rail->enabled_since_ms = now_ms;
    }
  }

  asclepius_ocp_build_status();

  asclepius_unlock();
}

// Software OCP override. NOT fast-acting hardware protection: this runs on the
// poll cadence, so a trip is seen only after the over-current condition has
// held for the configured debounce interval.
static void asclepius_ocp_check(void)
{
  static const uint8_t channels[] = {ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V};
  uint32_t currents[3] = {0, 0, 0};
  bool enabled[3] = {false, false, false};
  uint8_t cut_channels[3] = {0, 0, 0};
  uint32_t trip_ma[3] = {0, 0, 0};
  unsigned int limit_ma[3] = {0, 0, 0};
  bool new_trip[3] = {false, false, false};
  bool cleared[3] = {false, false, false};
  int cut_count = 0;
  bool have_status;
  uint32_t now_ms;
  size_t i;

  now_ms = asclepius_monotonic_ms();

  asclepius_lock();

  have_status = asclepius_connected;
  if (have_status)
  {
    currents[0] = asclepius_status.main12v_current;
    currents[1] = asclepius_status.main5v_current;
    currents[2] = asclepius_status.usb5v_current;
    enabled[0] = asclepius_status.main12v_enabled;
    enabled[1] = asclepius_status.main5v_enabled;
    enabled[2] = asclepius_status.usb5v_enabled;
  }

  for (i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
  {
    struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(channels[i]);
    bool in_grace;
    bool over;

    if (rail == NULL)
    {
      continue;
    }

    if (!have_status)
    {
      // The board went away: a latched trip no longer describes anything.
      rail->tripped = false;
      rail->trip_ma = 0;
      rail->over_pending = false;
      rail->clear_pending = false;
      rail->enabled_was_on = false;
      continue;
    }

    if (enabled[i] != rail->enabled_was_on)
    {
      rail->enabled_was_on = enabled[i];
      rail->enabled_since_ms = now_ms;
      rail->over_pending = false;
      rail->clear_pending = false;
    }

    // Switching monitoring off is an explicit request to do nothing: drop any
    // latch and stop carrying timing state for this rail.
    if (rail->action == ASCLEPIUS_OCP_OFF)
    {
      cleared[i] = rail->tripped;
      rail->tripped = false;
      rail->trip_ma = 0;
      rail->over_pending = false;
      rail->clear_pending = false;
      continue;
    }

    if (!enabled[i])
    {
      // A CUT trip leaves the rail off, so keep its latch until the user
      // re-enables the rail or changes the action. WARN trips follow the load
      // and can clear normally.
      if (rail->tripped && rail->action != ASCLEPIUS_OCP_CUT)
      {
        cleared[i] = true;
        rail->tripped = false;
        rail->trip_ma = 0;
      }

      rail->over_pending = false;
      rail->clear_pending = false;
      continue;
    }

    over = currents[i] > rail->limit_ma;

    if (!over)
    {
      rail->over_pending = false;

      if (rail->tripped)
      {
        // Require the load to stay back in range before clearing, so crossing
        // the limit repeatedly does not produce a stream of trip/clear events.
        if (!rail->clear_pending)
        {
          rail->clear_pending = true;
          rail->clear_since_ms = now_ms;
        }

        if ((uint32_t)(now_ms - rail->clear_since_ms) >= rail->clear_delay_ms)
        {
          cleared[i] = true;
          rail->tripped = false;
          rail->trip_ma = 0;
          rail->clear_pending = false;
        }
      }
      else
      {
        rail->clear_pending = false;
      }

      continue;
    }

    rail->clear_pending = false;

    // Ignore everything during the post-enable grace window. A drive spinning
    // up legitimately draws far more than its idle current.
    in_grace = (uint32_t)(now_ms - rail->enabled_since_ms) < rail->startup_grace_ms;
    if (in_grace)
    {
      rail->over_pending = false;
      continue;
    }

    if (!rail->over_pending)
    {
      rail->over_pending = true;
      rail->over_since_ms = now_ms;
    }

    if (!rail->tripped && (uint32_t)(now_ms - rail->over_since_ms) >= rail->trip_delay_ms)
    {
      // The condition held for the whole debounce interval. Latch the trip and
      // remember the reading that caused it.
      rail->tripped = true;
      rail->trip_ma = currents[i];
      new_trip[i] = true;
      trip_ma[i] = currents[i];
      limit_ma[i] = rail->limit_ma;
      asclepius_clone_latch_soft_ocp_locked((int)i, rail->action);
    }

    // Keep issuing the cut if the rail is somehow still enabled after a CUT
    // trip (a failed or ignored command), rather than assuming it worked.
    if (rail->tripped && rail->action == ASCLEPIUS_OCP_CUT)
    {
      cut_channels[cut_count++] = channels[i];
    }
  }

  asclepius_ocp_build_status();

  asclepius_unlock();

  // Announced here rather than from the general event log: a failed cut marks
  // the port dead, which would otherwise swallow the notice of a trip this
  // very poll.
  for (i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
  {
    if (new_trip[i])
    {
      printf("Asclepius: soft OCP %s tripped at %.2f A (limit %u mA)\n", asclepius_ocp_rail_name(channels[i]), trip_ma[i] / 1000.0f, limit_ma[i]);
    }
    else if (cleared[i])
    {
      printf("Asclepius: soft OCP %s cleared\n", asclepius_ocp_rail_name(channels[i]));
    }
  }

  // Cuts run outside the lock: asclepius_disable_channel() takes it itself.
  for (i = 0; i < (size_t)cut_count; i++)
  {
    if (asclepius_disable_channel(cut_channels[i]) != 0)
    {
      printf("Software OCP: failed to cut %s\n", asclepius_ocp_rail_name(cut_channels[i]));
    }
  }
}

static void asclepius_event_check(void);

// Poller thread. Owns the control port: nothing else reads or writes it while
// this runs, which is why the GUI can poll the ring buffer without blocking.
// Announce rail state changes to stdout, once per transition.
//
// The poller runs at the configured interval (25 ms by default), so printing
// on every sample would bury the terminal in noise. Everything here is
// edge-triggered against the previous poll. The first poll after connecting
// only primes that baseline: a board that powers up with 12V and 5V already on
// has not "switched" anything.
//
// Rail on/off is discrete and reported immediately. Out-of-spec is an analogue
// reading that can sit right on a limit: a 5V rail measuring 5232-5253 mV
// against a 5250 mV ceiling would otherwise emit two lines per poll, tens of
// thousands a minute. An excursion therefore has to hold for
// the configured voltage debounce before it is announced, which reports a genuine
// standing violation once while ignoring jitter. Excursions shorter than that
// remain visible in the monitor's per-sample markers and statistics; this only
// thins out the log.
static void asclepius_event_check(void)
{
  static const uint8_t channels[] = {ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V};
  static bool primed = false;
  static bool was_enabled[3] = {false, false, false};
  static bool reported_out_of_spec[3] = {false, false, false};
  static bool candidate_out_of_spec[3] = {false, false, false};
  static uint32_t candidate_since_ms[3] = {0, 0, 0};

  bool enabled[3];
  bool out_of_spec[3];
  uint32_t volts[3];
  uint32_t now_ms;
  unsigned int debounce_ms;
  bool connected;
  size_t i;

  now_ms = asclepius_monotonic_ms();

  asclepius_lock();

  connected = asclepius_connected;
  debounce_ms = asclepius_clone_settings_ccc.voltage_debounce_ms;
  if (connected)
  {
    enabled[0] = asclepius_status.main12v_enabled;
    enabled[1] = asclepius_status.main5v_enabled;
    enabled[2] = asclepius_status.usb5v_enabled;
    volts[0] = asclepius_status.main12v_voltage;
    volts[1] = asclepius_status.main5v_voltage;
    volts[2] = 0; // the USB rail is derived from the 5V rail and reports no voltage

    for (i = 0; i < 3; i++)
    {
      if (channels[i] == ASCLEPIUS_MAIN12V)
      {
        out_of_spec[i] = enabled[i] && (volts[i] < ASCLEPIUS_V12_MIN_MV || volts[i] > ASCLEPIUS_V12_MAX_MV);
      }
      else if (channels[i] == ASCLEPIUS_MAIN5V)
      {
        out_of_spec[i] = enabled[i] && (volts[i] < ASCLEPIUS_V5_MIN_MV || volts[i] > ASCLEPIUS_V5_MAX_MV);
      }
      else
      {
        out_of_spec[i] = false; // no voltage to judge the USB rail by
      }
    }
  }

  asclepius_unlock();

  if (!connected)
  {
    // Re-prime silently on the next successful poll rather than reporting
    // every rail as having switched while the board was away.
    primed = false;
    return;
  }

  if (!primed)
  {
    primed = true;
    for (i = 0; i < 3; i++)
    {
      was_enabled[i] = enabled[i];
      reported_out_of_spec[i] = out_of_spec[i];
      candidate_out_of_spec[i] = out_of_spec[i];
      candidate_since_ms[i] = now_ms;
    }
    return;
  }

  // Track the unsettled candidate separately from what was announced, so a
  // reading that crosses the limit and comes straight back never reaches the
  // log at all.
  for (i = 0; i < 3; i++)
  {
    if (out_of_spec[i] != candidate_out_of_spec[i])
    {
      candidate_out_of_spec[i] = out_of_spec[i];
      candidate_since_ms[i] = now_ms;
    }
  }

  for (i = 0; i < 3; i++)
  {
    const char *rail = asclepius_ocp_rail_name(channels[i]);

    if (enabled[i] != was_enabled[i])
    {
      printf("Asclepius: %s %s\n", rail, enabled[i] ? "on" : "off");
      was_enabled[i] = enabled[i];
    }

    if (candidate_out_of_spec[i] != reported_out_of_spec[i] && (uint32_t)(now_ms - candidate_since_ms[i]) >= debounce_ms)
    {
      if (candidate_out_of_spec[i])
      {
        printf("Asclepius: %s out of spec at %u mV\n", rail, volts[i]);
        asclepius_clone_latch_voltage(channels[i], volts[i]);
      }
      else
      {
        printf("Asclepius: %s back in spec at %u mV\n", rail, volts[i]);
      }
      reported_out_of_spec[i] = candidate_out_of_spec[i];
    }
  }
}

static void *asclepius_poller_main(void *unused)
{
  (void)unused;

  while (!asclepius_stop_poller)
  {
    if (asclepius_connected)
    {
      if (asclepius_get_status() == 0)
      {
        mon_push(&asclepius_monitor, &asclepius_status, asclepius_monotonic_ms());
        asclepius_ocp_check();
        asclepius_event_check();
      }
      else
      {
        // Status read failed; asclepius_get_status clears
        // asclepius_connected, so a reconnect is attempted below.
      }

      asclepius_sleep_ms(asclepius_poll_clamp_interval_ms(atomic_load(&asclepius_poll_interval_ms)));
    }
    else
    {
      // Port lost or not yet connected: retry at a fixed 1 s backoff
      pthread_mutex_lock(&asclepius_serial_mutex);
      if (!asclepius_connected && asclepius_reconnect())
      {
        asclepius_connected = true;
      }
      pthread_mutex_unlock(&asclepius_serial_mutex);

      asclepius_sleep_ms(1000);
    }
  }

  return NULL;
}

// Take one reading before handing control back. The poller only starts below,
// so a caller that asks for status immediately after connecting (the GUI status
// line, asclepiusstatus) would otherwise see zeros.
//
// The port being open does not mean the board is already answering: USB serial
// devices commonly need a moment after open() before the first exchange lands.
// Retry briefly so a slow board is not reported as a failed connect. A failure
// that survives this is reported rather than hidden, because get_status() has
// already marked the port dead and pretending to be connected would be a lie.
static int asclepius_initial_status(void)
{
  int attempt;

  for (attempt = 0; attempt < 3; attempt++)
  {
    if (asclepius_get_status() == 0)
    {
      return 0;
    }

    asclepius_sleep_ms(100);
  }

  return -1;
}

int asclepius_connect(void)
{
  if (asclepius_connected)
  {
    asclepius_disconnect();
  }

  pthread_mutex_lock(&asclepius_serial_mutex);

  if (!asclepius_serial_claim(ASCLEPIUS_PORT_CONTROL, asclepius_tty_device_ccc))
  {
    if (errno == EBUSY)
    {
      printf("Error: Asclepius control port %s is already in use by the UART terminal\n", asclepius_tty_device_ccc);
    }
    else
    {
      printf("Error claiming Asclepius control port %s: %s\n", asclepius_tty_device_ccc, strerror(errno));
    }
    asclepius_connected = false;
    pthread_mutex_unlock(&asclepius_serial_mutex);
    return -1;
  }

  asclepius_serial_port = open(asclepius_tty_device_ccc, O_RDWR | O_NOCTTY | O_NONBLOCK);

  if (asclepius_serial_port < 0)
  {
    printf("Error %i from open: %s\n", errno, strerror(errno));
    asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);
    asclepius_connected = false;
    pthread_mutex_unlock(&asclepius_serial_mutex);
    return -1;
  }

  if (asclepius_setup_tty() != 0)
  {
    printf("Error setting up TTY for %s\n", asclepius_tty_device_ccc);
    asclepius_connected = false;
    close(asclepius_serial_port);
    asclepius_serial_port = -1;
    asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);
    pthread_mutex_unlock(&asclepius_serial_mutex);
    return -1;
  }

  asclepius_connected = true;

  pthread_mutex_unlock(&asclepius_serial_mutex);

  // A fresh session starts with an empty history so plotted windows and
  // min/avg/max never mix readings from a previous connection.
  mon_reset(&asclepius_monitor);

  if (asclepius_initial_status() != 0)
  {
    // get_status() has already marked the port dead, so this path returns
    // with the descriptor still open and nothing able to close it:
    // asclepius_disconnect() only closes a port it believes is connected.
    // Without this, every failed Connect leaks one descriptor.
    close(asclepius_serial_port);
    asclepius_serial_port = -1;
    asclepius_connected = false;
    asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);
    return -1;
  }

  mon_push(&asclepius_monitor, &asclepius_status, asclepius_monotonic_ms());

  if (!asclepius_poller_running)
  {
    asclepius_stop_poller = false;
    if (pthread_create(&asclepius_poller_thread, NULL, asclepius_poller_main, NULL) == 0)
    {
      asclepius_poller_running = true;
    }
  }

  return 0;
}

void asclepius_disconnect(void)
{
  if (asclepius_poller_running)
  {
    asclepius_stop_poller = true;
    pthread_join(asclepius_poller_thread, NULL);
    asclepius_poller_running = false;
  }

  pthread_mutex_lock(&asclepius_serial_mutex);

  if (asclepius_serial_port >= 0)
  {
    close(asclepius_serial_port);
    asclepius_serial_port = -1;
  }
  asclepius_connected = false;
  asclepius_serial_release(ASCLEPIUS_PORT_CONTROL);

  // The board is gone: any latched software OCP trip no longer applies
  asclepius_ocp_12v_ccc.tripped = false;
  asclepius_ocp_12v_ccc.trip_ma = 0;
  asclepius_ocp_12v_ccc.over_pending = false;
  asclepius_ocp_12v_ccc.clear_pending = false;
  asclepius_ocp_12v_ccc.enabled_was_on = false;
  asclepius_ocp_5v_ccc.tripped = false;
  asclepius_ocp_5v_ccc.trip_ma = 0;
  asclepius_ocp_5v_ccc.over_pending = false;
  asclepius_ocp_5v_ccc.clear_pending = false;
  asclepius_ocp_5v_ccc.enabled_was_on = false;
  asclepius_ocp_usb_ccc.tripped = false;
  asclepius_ocp_usb_ccc.trip_ma = 0;
  asclepius_ocp_usb_ccc.over_pending = false;
  asclepius_ocp_usb_ccc.clear_pending = false;
  asclepius_ocp_usb_ccc.enabled_was_on = false;
  asclepius_soft_ocp_status[0] = '\0';

  pthread_mutex_unlock(&asclepius_serial_mutex);

  // Drop the history too: the readings belonged to the session that ended
  mon_reset(&asclepius_monitor);
}

bool asclepius_get_connection_status()
{
  return asclepius_connected;
}

int asclepius_enable_channel(uint8_t channel)
{
  int result;

  pthread_mutex_lock(&asclepius_serial_mutex);

  if (!asclepius_connected)
  {
    pthread_mutex_unlock(&asclepius_serial_mutex);
    return -1;
  }

  asclepius_command.command = ASCLEPIUS_TURNON_IOCMD;
  asclepius_command.channel = channel;

  if (!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
  {
    asclepius_connected = false;
    result = -1;
  }
  else
  {
    result = 0;
  }

  pthread_mutex_unlock(&asclepius_serial_mutex);

  return result;
}

int asclepius_disable_channel(uint8_t channel)
{
  int result;

  pthread_mutex_lock(&asclepius_serial_mutex);

  if (!asclepius_connected)
  {
    pthread_mutex_unlock(&asclepius_serial_mutex);
    return -1;
  }

  asclepius_command.command = ASCLEPIUS_TURNOFF_IOCMD;
  asclepius_command.channel = channel;

  if (!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
  {
    asclepius_connected = false;
    result = -1;
  }
  else
  {
    result = 0;
  }

  pthread_mutex_unlock(&asclepius_serial_mutex);

  return result;
}

int asclepius_get_status(void)
{
  int result;

  pthread_mutex_lock(&asclepius_serial_mutex);

  if (!asclepius_connected)
  {
    pthread_mutex_unlock(&asclepius_serial_mutex);
    return -1;
  }

  asclepius_command.command = ASCLEPIUS_STATUS_IOCMD;
  asclepius_command.channel = 0;

  // Discard anything left from an exchange that did not finish. The protocol
  // has no framing, so a partial reply left queued would shift every later
  // read out of alignment, and reopening the port would not clear it because
  // the tty buffer belongs to the device, not the descriptor. The board only
  // ever speaks in reply to a command, so there is nothing else to lose.
  tcflush(asclepius_serial_port, TCIFLUSH);

  if (!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
  {
    asclepius_connected = false;
    result = -1;
  }
  else if (!asclepius_read_status_packet())
  {
    asclepius_connected = false;
    result = -1;
  }
  else
  {
    asclepius_clone_latch_hardware_ocp_locked();
    result = 0;
  }

  pthread_mutex_unlock(&asclepius_serial_mutex);

  return result;
}

static int asclepius_ext_baud_to_constant(int baud)
{
  switch (baud)
  {
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  case 230400:
    return B230400;
  case 921600:
    return B921600;
  default:
    return B38400;
  }
}

static int asclepius_ext_setup_tty(void)
{
  struct termios tty;

  if (tcgetattr(asclepius_ext_serial_port, &tty) != 0)
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

  if (tcsetattr(asclepius_ext_serial_port, TCSANOW, &tty) != 0)
  {
    printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
    return -1;
  }

  tcflush(asclepius_ext_serial_port, TCIOFLUSH);
  return 0;
}

// Candidate serial devices. The kernel names USB CDC ACM ports ttyUSB* and
// ttyACM*; ttyascl* covers the project's udev aliases so a board wired up with
// a stable name is found the same way as one addressed by enumeration order.
static bool asclepius_port_name_matches(const char *name)
{
  static const char *const prefixes[] = {"ttyUSB", "ttyACM", "ttyascl"};

  for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++)
  {
    if (strncmp(name, prefixes[i], strlen(prefixes[i])) == 0)
    {
      return true;
    }
  }

  return false;
}

static int asclepius_port_name_cmp(const void *a, const void *b)
{
  return strcmp((const char *)a, (const char *)b);
}

int asclepius_list_serial_ports(char (*names)[ASCLEPIUS_PORT_NAME_MAX], int max_ports)
{
  DIR *dir;
  struct dirent *entry;
  int found = 0;

  if (names == NULL || max_ports <= 0)
  {
    return 0;
  }

  dir = opendir("/dev");
  if (dir == NULL)
  {
    printf("Error listing serial ports: %s\n", strerror(errno));
    return 0;
  }

  while ((entry = readdir(dir)) != NULL)
  {
    if (!asclepius_port_name_matches(entry->d_name))
    {
      continue;
    }

    if (found < max_ports)
    {
      snprintf(names[found], ASCLEPIUS_PORT_NAME_MAX, "/dev/%s", entry->d_name);
    }

    found++;
  }

  closedir(dir);

  // Sorting on the counted set: the full list is normally far below the cap,
  // and when it is not the caller still gets a stable prefix.
  int usable = found < max_ports ? found : max_ports;
  qsort(names, (size_t)usable, ASCLEPIUS_PORT_NAME_MAX, asclepius_port_name_cmp);

  return found;
}

int asclepius_ext_open(void)
{
  if (asclepius_ext_connected)
  {
    asclepius_ext_close();
  }

  // The passthrough must never be opened on the control port. Two
  // descriptors on one device would have the terminal stealing bytes out of
  // STATUS replies (killing the poller) and serialsend injecting bytes into
  // the protocol stream. The shipped defaults are udev aliases on the same
  // board, so this is easy to set up by accident. The claim compares the
  // resolved device, so /dev/ttyascl and /dev/ttyUSB1 are caught, and it also
  // rejects the configured control path while the control side is disconnected.
  if (!asclepius_serial_claim(ASCLEPIUS_PORT_TERMINAL, asclepius_ext_tty_device_ccc))
  {
    if (errno == EBUSY)
    {
      printf("Error: UART passthrough port %s conflicts with the Asclepius control port\n", asclepius_ext_tty_device_ccc);
    }
    else
    {
      printf("Error claiming UART passthrough port %s: %s\n", asclepius_ext_tty_device_ccc, strerror(errno));
    }
    asclepius_ext_connected = false;
    return -1;
  }

  asclepius_ext_serial_port = open(asclepius_ext_tty_device_ccc, O_RDWR | O_NOCTTY | O_NONBLOCK);

  if (asclepius_ext_serial_port < 0)
  {
    printf("Error %i from open: %s\n", errno, strerror(errno));
    asclepius_ext_connected = false;
    asclepius_serial_release(ASCLEPIUS_PORT_TERMINAL);
    return -1;
  }

  if (asclepius_ext_setup_tty() != 0)
  {
    printf("Error setting up TTY for %s\n", asclepius_ext_tty_device_ccc);
    asclepius_ext_connected = false;
    close(asclepius_ext_serial_port);
    asclepius_ext_serial_port = -1;
    asclepius_serial_release(ASCLEPIUS_PORT_TERMINAL);
    return -1;
  }

  asclepius_ext_connected = true;
  return 0;
}

void asclepius_ext_close(void)
{
  if (asclepius_ext_serial_port >= 0)
  {
    close(asclepius_ext_serial_port);
    asclepius_ext_serial_port = -1;
  }

  asclepius_ext_connected = false;
  asclepius_serial_release(ASCLEPIUS_PORT_TERMINAL);
}

int asclepius_ext_send(const char *data, size_t len)
{
  if (!asclepius_ext_connected)
  {
    return -1;
  }

  const uint8_t *buf = (const uint8_t *)data;
  size_t offset = 0;

  while (offset < len)
  {
    ssize_t n = write(asclepius_ext_serial_port, buf + offset, len - offset);

    if (n < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        // The port is non-blocking, so a full transmit buffer is normal
        // rather than exceptional. Wait for room with a deadline: this
        // used to retry immediately, which pinned a core at 100% and
        // froze the GUI whenever the peer stopped draining (drive
        // powered off, cable unplugged, or XOFF asserted).
        struct pollfd poll_fd;
        poll_fd.fd = asclepius_ext_serial_port;
        poll_fd.events = POLLOUT;
        poll_fd.revents = 0;

        int poll_result = poll(&poll_fd, 1, ASCLEPIUS_EXT_WRITE_TIMEOUT_MS);

        if (poll_result > 0)
        {
          continue;
        }

        if (poll_result == 0)
        {
          printf("Error writing ext: timed out after %u ms with %zu of %zu bytes sent\n", ASCLEPIUS_EXT_WRITE_TIMEOUT_MS, offset, len);
          return -1;
        }

        if (errno == EINTR)
        {
          continue;
        }

        printf("Error writing ext: %s\n", strerror(errno));
        return -1;
      }

      printf("Error writing ext: %s\n", strerror(errno));
      return -1;
    }

    if (n == 0)
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
  if (!asclepius_ext_connected)
  {
    return -1;
  }

  ssize_t n = read(asclepius_ext_serial_port, buffer, max_len);

  if (n < 0)
  {
    if (errno == EINTR || errno == EAGAIN)
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
  while (*input && out < max - 1)
  {
    if (*input == '\\')
    {
      input++;
      switch (*input)
      {
      case 'n':
        output[out++] = '\n';
        break;
      case 'r':
        output[out++] = '\r';
        break;
      case 't':
        output[out++] = '\t';
        break;
      case '0':
        output[out++] = '\0';
        break;
      case '\\':
        output[out++] = '\\';
        break;
      case 'x':
      {
        input++;
        if (isxdigit((unsigned char)input[0]) && isxdigit((unsigned char)input[1]))
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
    if (*input)
      input++;
  }
  output[out] = '\0';
  return (int)out;
}

unsigned char asclepius_ctrl_byte(unsigned char c)
{
  if (c == '?')
  {
    return 0x7F; // Ctrl-? is DEL
  }

  if (c < 0x20)
  {
    return c; // already a control code, leave it alone
  }

  return (unsigned char)(c & 0x1F);
}

int asclepius_parse_hex(const char *input, char *output, size_t max)
{
  size_t out = 0;

  if (input == NULL || output == NULL || max == 0)
  {
    return -1;
  }

  while (*input != '\0')
  {
    if (isspace((unsigned char)*input))
    {
      input++;
      continue;
    }

    if (!isxdigit((unsigned char)input[0]) || !isxdigit((unsigned char)input[1]))
    {
      return -1;
    }

    if (out + 1 >= max)
    {
      return -1;
    }

    char pair[3] = {input[0], input[1], '\0'};
    output[out++] = (char)strtol(pair, NULL, 16);
    input += 2;
  }

  output[out] = '\0';
  return (int)out;
}

void asclepius_linebuf_init(asclepius_linebuf_t *lb)
{
  lb->len = 0;
  lb->buf[0] = '\0';
}

// Shared by feed and flush: drop the terminator and hand over the line. A bare
// CR is not a terminator, so a message split as "...\r" followed by "\n..."
// reassembles into one line instead of two.
static void asclepius_linebuf_emit(asclepius_linebuf_t *lb, asclepius_line_cb cb, void *user)
{
  size_t len = lb->len;

  while (len > 0 && (lb->buf[len - 1] == '\n' || lb->buf[len - 1] == '\r'))
  {
    len--;
  }

  lb->buf[len] = '\0';
  cb(lb->buf, len, user);
  lb->len = 0;
}

void asclepius_linebuf_feed(asclepius_linebuf_t *lb, const char *data, size_t len, asclepius_line_cb cb, void *user)
{
  size_t i;

  if (lb == NULL || cb == NULL)
  {
    return;
  }

  for (i = 0; i < len; i++)
  {
    if (data[i] == '\r')
    {
      // CR is a line terminator in its own right, not just padding. Some
      // boards end a line with LF and start the next one with CR, leaving
      // a stray CR at the head of every message after the first.
      // GtkTextView renders that CR as a line break, so each entry looked
      // like "RCV:" followed by an empty line.
      //
      // CRLF is one terminator, not two.
      if (i + 1 < len && data[i + 1] == '\n')
      {
        i++;
      }

      asclepius_linebuf_emit(lb, cb, user);
      continue;
    }

    if (lb->len + 1 >= sizeof(lb->buf))
    {
      // A message longer than the buffer with no terminator in it. Emit
      // what we have rather than dropping it, then carry on.
      asclepius_linebuf_emit(lb, cb, user);
    }

    lb->buf[lb->len++] = data[i];

    if (data[i] == '\n')
    {
      asclepius_linebuf_emit(lb, cb, user);
    }
  }
}

void asclepius_linebuf_flush(asclepius_linebuf_t *lb, asclepius_line_cb cb, void *user)
{
  if (lb == NULL || cb == NULL || lb->len == 0)
  {
    return;
  }

  asclepius_linebuf_emit(lb, cb, user);
}

int asclepius_format_hex_line(char *out, size_t out_size, const unsigned char *data, size_t len, unsigned long offset_base)
{
  char hex[ASCLEPIUS_HEX_PER_LINE * 3 + 1];
  char ascii[ASCLEPIUS_HEX_PER_LINE + 1];
  size_t i;
  size_t at = 0;
  int written;

  if (out == NULL || out_size == 0)
  {
    return 0;
  }

  if (len > ASCLEPIUS_HEX_PER_LINE)
  {
    len = ASCLEPIUS_HEX_PER_LINE;
  }

  for (i = 0; i < len; i++)
  {
    at += (size_t)snprintf(hex + at, sizeof(hex) - at, "%02x ", data[i]);
    ascii[i] = (data[i] >= 0x20 && data[i] < 0x7F) ? (char)data[i] : '.';
  }
  ascii[len] = '\0';

  // Pad to a fixed width so the hex column and the ASCII gutter stay aligned
  // from line to line, including for a short final line.
  for (i = len; i < ASCLEPIUS_HEX_PER_LINE; i++)
  {
    at += (size_t)snprintf(hex + at, sizeof(hex) - at, "   ");
  }
  hex[at] = '\0';

  written = snprintf(out, out_size, "%08lx  %-*s |%s|", offset_base, (int)(ASCLEPIUS_HEX_PER_LINE * 3), hex, ascii);
  return written < 0 ? 0 : (written >= (int)out_size ? (int)out_size - 1 : written);
}
