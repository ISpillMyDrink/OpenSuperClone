// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef OSC_MONITOR_H
#define OSC_MONITOR_H

#include <stdint.h>

#include "asclepius.h"

// Ring buffer length. At the 25 ms default poll this holds ~6.8 minutes,
// enough for the longest selectable graph window. Power of two keeps the
// ring index math cheap.
#define MON_HISTORY_MAX 16384

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
  uint8_t enabled12;
  uint8_t enabled5;
  uint8_t enabledusb;
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
// Not synchronized: only safe for a caller that owns a private copy, or when
// no poller is running. The GUI uses the mon_copy_* helpers below instead.
const mon_sample_t *mon_sample(const mon_history_t *h, int i);

// All of the following are safe to call from the GUI thread while the poller
// thread is running: each takes the internal lock only for a memcpy.

uint32_t mon_sample_count(const mon_t *m);

// Number of valid samples currently retained (0..MON_HISTORY_MAX)
int mon_history_len(const mon_t *m);

// Rolling min/avg/max accumulators per rail. Pass NULL for a rail to skip it.
void mon_copy_stats(const mon_t *m, mon_stats_t *st12, mon_stats_t *st5, mon_stats_t *stusb);

// Statistics for samples in the inclusive time range.
void mon_copy_stats_range(const mon_t *m, uint32_t t_min_ms, uint32_t t_max_ms, mon_stats_t *st12, mon_stats_t *st5, mon_stats_t *stusb);

// Copies samples spanning [t_min_ms, t_max_ms] into out, oldest first.
// When the range holds more than max_out samples, the range is decimated
// evenly so the copy still covers the whole window; the first and last
// in-range samples are always included. Returns the number copied.
int mon_copy_range(const mon_t *m, uint32_t t_min_ms, uint32_t t_max_ms, mon_sample_t *out, int max_out);

// Timestamps of the oldest and newest retained samples. False when empty.
bool mon_time_span(const mon_t *m, uint32_t *oldest_ms, uint32_t *newest_ms);

#endif
