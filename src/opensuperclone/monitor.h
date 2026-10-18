// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef OSC_MONITOR_H
#define OSC_MONITOR_H

#include <stdint.h>

#include "asclepius.h"

// Ring buffer length. At a 100 ms poll this holds ~6.8 minutes,
// enough for the longest selectable graph window.
#define MON_HISTORY_MAX 4096

typedef struct
{
    float v12;     // 12V rail voltage (V)
    float v5;      // 5V rail voltage (V)
    float a12;     // 12V rail current (A)
    float a5;      // 5V rail current (A)
    float ausb;    // USB 5V current (A)
    uint8_t oos12; // 1 when the 12V rail is out of spec
    uint8_t oos5;  // 1 when the 5V rail is out of spec
    uint8_t ocp12;
    uint8_t ocp5;
    uint8_t ocpusb;
    uint32_t t_ms; // monotonic time at sample time (ms)
} mon_sample_t;

typedef struct
{
    mon_sample_t buf[MON_HISTORY_MAX];
    int head; // next write index
    int len;  // number of valid samples (0..MON_HISTORY_MAX)
} mon_history_t;

typedef struct
{
    float vmin, vmax;
    double vsum;
    int vcount;
    float amin, amax;
    double asum;
    int acount;
} mon_stats_t;

typedef struct
{
    mon_history_t hist;
    mon_stats_t st12, st5, stusb;
    uint32_t sample_count;
} mon_t;

// Live sample history, fed by the Asclepius status poller thread
extern mon_t asclepius_monitor;

void mon_init(mon_t *m);

void mon_reset(mon_t *m);

void mon_push(mon_t *m, const struct asclepius_status_t *st, uint32_t t_ms);

// Sample i of the ring, 0 = oldest, len-1 = newest.
const mon_sample_t *mon_sample(const mon_history_t *h, int i);

#endif
