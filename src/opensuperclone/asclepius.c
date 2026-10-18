// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "asclepius.h"
#include "monitor.h"

#include <ctype.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

bool asclepius_connected = false;
int asclepius_serial_port = -1;

_Atomic unsigned int asclepius_poll_interval_ms = ASCLEPIUS_POLL_INTERVAL_DEFAULT_MS;

struct asclepius_ocp_rail_t asclepius_ocp_12v_ccc = { false, 0, ASCLEPIUS_OCP_LIMIT_DEFAULT_MA, ASCLEPIUS_OCP_CUT };
struct asclepius_ocp_rail_t asclepius_ocp_5v_ccc = { false, 0, ASCLEPIUS_OCP_LIMIT_DEFAULT_MA, ASCLEPIUS_OCP_CUT };
struct asclepius_ocp_rail_t asclepius_ocp_usb_ccc = { false, 0, ASCLEPIUS_OCP_LIMIT_DEFAULT_MA, ASCLEPIUS_OCP_CUT };
char asclepius_soft_ocp_status[256] = "";

static pthread_mutex_t asclepius_serial_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t asclepius_poller_thread;
static bool asclepius_poller_running = false;
static volatile bool asclepius_stop_poller = false;

char asclepius_tty_device_ccc[256] = "";
char asclepius_ext_tty_device_ccc[256] = "";

bool asclepius_ext_connected = false;
int asclepius_ext_serial_port = -1;
int asclepius_ext_baud_rate_ccc = 38400;
void (*asclepius_ext_output_ccc)(const char *text, int byte_count) = NULL;

struct asclepius_command_t asclepius_command;
struct asclepius_status_t asclepius_status;

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
    struct timespec deadline;
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += ASCLEPIUS_STATUS_TIMEOUT_MS / 1000;
    deadline.tv_nsec += (long)(ASCLEPIUS_STATUS_TIMEOUT_MS % 1000) * 1000000L;
    if(deadline.tv_nsec >= 1000000000L)
    {
        deadline.tv_sec += deadline.tv_nsec / 1000000000L;
        deadline.tv_nsec %= 1000000000L;
    }

    while(offset < length)
    {
        struct pollfd poll_fd;
        struct timespec remaining;
        int poll_result;

        clock_gettime(CLOCK_MONOTONIC, &now);
        if(now.tv_sec > deadline.tv_sec || (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec))
        {
            printf("Error reading: timed out after %zu of %zu bytes\n", offset, length);
            return false;
        }

        remaining.tv_sec = deadline.tv_sec - now.tv_sec;
        remaining.tv_nsec = deadline.tv_nsec - now.tv_nsec;
        if(remaining.tv_nsec < 0)
        {
            remaining.tv_nsec += 1000000000L;
            remaining.tv_sec -= 1;
        }

        poll_fd.fd = asclepius_serial_port;
        poll_fd.events = POLLIN;
        poll_fd.revents = 0;

        poll_result = poll(&poll_fd, 1, (int)(remaining.tv_sec * 1000 + remaining.tv_nsec / 1000000L));

        if(poll_result < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }

            printf("Error reading: %s\n", strerror(errno));
            return false;
        }

        if(poll_result == 0)
        {
            printf("Error reading: timed out after %zu of %zu bytes\n", offset, length);
            return false;
        }

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

    tty.c_cc[VTIME] = 0;    // No per-read timeout; poll() enforces the read deadline
    tty.c_cc[VMIN] = 0;     // Non-blocking reads

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

    while(nanosleep(&ts, &rem) != 0)
    {
        if(errno != EINTR)
        {
            break;
        }
        ts = rem;
    }
}

static struct asclepius_ocp_rail_t *asclepius_ocp_rail(uint8_t channel)
{
    switch(channel)
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
    switch(channel)
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

unsigned int asclepius_poll_clamp_interval_ms(unsigned int interval_ms)
{
    if(interval_ms < ASCLEPIUS_POLL_INTERVAL_MIN_MS)
    {
        return ASCLEPIUS_POLL_INTERVAL_MIN_MS;
    }

    if(interval_ms > ASCLEPIUS_POLL_INTERVAL_MAX_MS)
    {
        return ASCLEPIUS_POLL_INTERVAL_MAX_MS;
    }

    return interval_ms;
}

unsigned int asclepius_ocp_clamp_limit_mA(unsigned int limit_ma)
{
    if(limit_ma < ASCLEPIUS_OCP_LIMIT_MIN_MA)
    {
        return ASCLEPIUS_OCP_LIMIT_MIN_MA;
    }

    if(limit_ma > ASCLEPIUS_OCP_LIMIT_MAX_MA)
    {
        return ASCLEPIUS_OCP_LIMIT_MAX_MA;
    }

    return limit_ma;
}

asclepius_ocp_action_t asclepius_ocp_clamp_action(int action)
{
    if(action < ASCLEPIUS_OCP_OFF || action > ASCLEPIUS_OCP_CUT)
    {
        return ASCLEPIUS_OCP_OFF;
    }

    return (asclepius_ocp_action_t)action;
}

void asclepius_ocp_get_rail_state(uint8_t channel, struct asclepius_ocp_rail_t *state)
{
    struct asclepius_ocp_rail_t *rail;

    if(state == NULL)
    {
        return;
    }

    asclepius_lock();

    rail = asclepius_ocp_rail(channel);
    if(rail != NULL)
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
    static const uint8_t channels[] = { ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V };
    size_t used = 0;
    size_t i;

    asclepius_soft_ocp_status[0] = '\0';

    for(i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
    {
        const struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(channels[i]);
        int written;

        if(rail == NULL || !rail->tripped)
        {
            continue;
        }

        written = snprintf(asclepius_soft_ocp_status + used, sizeof(asclepius_soft_ocp_status) - used, "%sSoftware OCP: %s tripped at %.2f A%s", used > 0 ? "; " : "", asclepius_ocp_rail_name(channels[i]), rail->trip_ma / 1000.0f, rail->action == ASCLEPIUS_OCP_CUT ? " (channel off)" : "");

        if(written < 0)
        {
            break;
        }

        used += (size_t)written;
        if(used >= sizeof(asclepius_soft_ocp_status))
        {
            used = sizeof(asclepius_soft_ocp_status) - 1;
            break;
        }
    }
}

void asclepius_ocp_reset(void)
{
    static const uint8_t channels[] = { ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V };
    size_t i;

    asclepius_lock();

    for(i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
    {
        struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(channels[i]);

        if(rail != NULL)
        {
            rail->tripped = false;
            rail->trip_ma = 0;
        }
    }

    asclepius_ocp_build_status();

    asclepius_unlock();
}

void asclepius_ocp_clear_channel(uint8_t channel)
{
    struct asclepius_ocp_rail_t *rail;

    asclepius_lock();

    rail = asclepius_ocp_rail(channel);
    if(rail != NULL)
    {
        rail->tripped = false;
        rail->trip_ma = 0;
    }

    asclepius_ocp_build_status();

    asclepius_unlock();
}

// Software OCP override. NOT fast-acting hardware protection: this runs on the
// poll cadence, so a trip is seen on the sample after the overcurrent.
static void asclepius_ocp_check(void)
{
    static const uint8_t channels[] = { ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V };
    uint32_t currents[3] = { 0, 0, 0 };
    bool enabled[3] = { false, false, false };
    uint8_t cut_channels[3] = { 0, 0, 0 };
    int cut_count = 0;
    bool have_status;
    size_t i;

    asclepius_lock();

    have_status = asclepius_connected;
    if(have_status)
    {
        currents[0] = asclepius_status.main12v_current;
        currents[1] = asclepius_status.main5v_current;
        currents[2] = asclepius_status.usb5v_current;
        enabled[0] = asclepius_status.main12v_enabled;
        enabled[1] = asclepius_status.main5v_enabled;
        enabled[2] = asclepius_status.usb5v_enabled;
    }

    for(i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
    {
        struct asclepius_ocp_rail_t *rail = asclepius_ocp_rail(channels[i]);
        bool over;

        if(rail == NULL)
        {
            continue;
        }

        if(!have_status)
        {
            // The board went away: a latched trip no longer describes anything
            rail->tripped = false;
            rail->trip_ma = 0;
            continue;
        }

        over = rail->action != ASCLEPIUS_OCP_OFF && enabled[i] && currents[i] > rail->limit_ma;

        if(over)
        {
            // The rail is on and drawing more than its limit. Latch the trip and
            // queue a cut. If it was already latched, the user must have
            // re-enabled it while the fault was still present: cut it again and
            // refresh the reason.
            rail->tripped = true;
            rail->trip_ma = currents[i];

            if(rail->action == ASCLEPIUS_OCP_CUT)
            {
                cut_channels[cut_count++] = channels[i];
            }
        }
        else if(rail->tripped && !enabled[i] && rail->action == ASCLEPIUS_OCP_CUT)
        {
            // The rail is still off because software OCP shut it down: keep the
            // latch until the user re-enables the rail or changes the action.
        }
        else
        {
            rail->tripped = false;
            rail->trip_ma = 0;
        }
    }

    asclepius_ocp_build_status();

    asclepius_unlock();

    // Cuts run outside the lock: asclepius_disable_channel() takes it itself.
    for(i = 0; i < (size_t)cut_count; i++)
    {
        if(asclepius_disable_channel(cut_channels[i]) != 0)
        {
            printf("Software OCP: failed to cut %s\n", asclepius_ocp_rail_name(cut_channels[i]));
        }
    }
}

static void *asclepius_poller_main(void *unused)
{
    (void)unused;

    while(!asclepius_stop_poller)
    {
        if(asclepius_connected)
        {
            if(asclepius_get_status() == 0)
            {
                mon_push(&asclepius_monitor, &asclepius_status, asclepius_monotonic_ms());
                asclepius_ocp_check();
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
            if(!asclepius_connected && asclepius_reconnect())
            {
                asclepius_connected = true;
            }
            pthread_mutex_unlock(&asclepius_serial_mutex);

            asclepius_sleep_ms(1000);
        }
    }

    return NULL;
}

int asclepius_connect(void)
{
    if(asclepius_connected)
    {
        asclepius_disconnect();
    }

    pthread_mutex_lock(&asclepius_serial_mutex);

    asclepius_serial_port = open(asclepius_tty_device_ccc, O_RDWR | O_NOCTTY);

    if(asclepius_serial_port < 0)
    {
        printf("Error %i from open: %s\n", errno, strerror(errno));
        asclepius_connected = false;
        pthread_mutex_unlock(&asclepius_serial_mutex);
        return -1;
    }

    if(asclepius_setup_tty() != 0)
    {
        printf("Error setting up TTY for %s\n", asclepius_tty_device_ccc);
        asclepius_connected = false;
        close(asclepius_serial_port);
        asclepius_serial_port = -1;
        pthread_mutex_unlock(&asclepius_serial_mutex);
        return -1;
    }

    asclepius_connected = true;

    pthread_mutex_unlock(&asclepius_serial_mutex);

    // A fresh session starts with an empty history so plotted windows and
    // min/avg/max never mix readings from a previous connection.
    mon_reset(&asclepius_monitor);

    if(!asclepius_poller_running)
    {
        asclepius_stop_poller = false;
        if(pthread_create(&asclepius_poller_thread, NULL, asclepius_poller_main, NULL) == 0)
        {
            asclepius_poller_running = true;
        }
    }

    return 0;
}

void asclepius_disconnect(void)
{
    if(asclepius_poller_running)
    {
        asclepius_stop_poller = true;
        pthread_join(asclepius_poller_thread, NULL);
        asclepius_poller_running = false;
    }

    pthread_mutex_lock(&asclepius_serial_mutex);

    if(asclepius_connected)
    {
        close(asclepius_serial_port);
        asclepius_serial_port = -1;
        asclepius_connected = false;
    }

    // The board is gone: any latched software OCP trip no longer applies
    asclepius_ocp_12v_ccc.tripped = false;
    asclepius_ocp_12v_ccc.trip_ma = 0;
    asclepius_ocp_5v_ccc.tripped = false;
    asclepius_ocp_5v_ccc.trip_ma = 0;
    asclepius_ocp_usb_ccc.tripped = false;
    asclepius_ocp_usb_ccc.trip_ma = 0;
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

    if(!asclepius_connected)
    {
        pthread_mutex_unlock(&asclepius_serial_mutex);
        return -1;
    }

    asclepius_command.command = ASCLEPIUS_TURNON_IOCMD;
    asclepius_command.channel = channel;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
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

    if(!asclepius_connected)
    {
        pthread_mutex_unlock(&asclepius_serial_mutex);
        return -1;
    }

    asclepius_command.command = ASCLEPIUS_TURNOFF_IOCMD;
    asclepius_command.channel = channel;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
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

    if(!asclepius_connected)
    {
        pthread_mutex_unlock(&asclepius_serial_mutex);
        return -1;
    }

    asclepius_command.command = ASCLEPIUS_STATUS_IOCMD;
    asclepius_command.channel = 0;

    if(!asclepius_write_all(&asclepius_command, sizeof(asclepius_command)))
    {
        asclepius_connected = false;
        result = -1;
    }
    else if(!asclepius_read_status_packet())
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
        printf("Error setting up TTY for %s\n", asclepius_ext_tty_device_ccc);
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
