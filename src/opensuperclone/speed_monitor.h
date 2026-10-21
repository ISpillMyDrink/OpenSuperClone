// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef OSC_SPEED_MONITOR_H
#define OSC_SPEED_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

// 24 hours at the normal once-per-second sample rate. The ring overwrites the
// oldest sample once it is full, so "All time" means the retained 24 hours.
#define SPEED_HISTORY_MAX 86400

// One plot is decimated to roughly one point per horizontal pixel column.
#define SPEED_POINT_MAX 2048

// Phase/pass transitions kept for the vertical markers. A whole run normally
// produces only a few dozen, so this is generous.
#define SPEED_EVENT_MAX 1024

// The Recent average reference line covers this many milliseconds.
#define SPEED_RECENT_WINDOW_MS 300000

typedef struct
{
  uint32_t t_ms;  // monotonic time at sample time (ms)
  uint32_t dt_ms; // interval the rate was measured over (ms)
  float rate;     // completed bytes per second
  uint32_t status;
} speed_sample_t;

typedef struct
{
  uint32_t t_ms;
  uint32_t status;
  char label[48];
} speed_event_t;

typedef struct
{
  double min;
  double max;
  double avg;
  double current;
  uint32_t peak_t_ms;
  int count;
} speed_stats_t;

// All of the following are called from the GUI thread. Clone work also runs on
// that thread and pumps GTK events, so no locking is needed.
void speed_monitor_clear(void);

void speed_monitor_push(uint32_t t_ms, uint32_t dt_ms, double bytes_per_second, long long status, const char *status_label);

bool speed_monitor_time_span(uint32_t *oldest_ms, uint32_t *newest_ms);

// Copies samples spanning [t_min_ms, t_max_ms] into out, oldest first. When the
// range holds more than max_out samples, the range is decimated evenly and the
// first and last in-range samples are always included.
int speed_monitor_copy_range(uint32_t t_min_ms, uint32_t t_max_ms, speed_sample_t *out, int max_out);

// Copies phase/pass transition events in the time range, oldest first.
int speed_monitor_copy_events(uint32_t t_min_ms, uint32_t t_max_ms, speed_event_t *out, int max_out);

// Min/avg/max and the latest value over the inclusive time range. Averages are
// weighted by each sample's measurement interval.
void speed_monitor_stats_range(uint32_t t_min_ms, uint32_t t_max_ms, speed_stats_t *stats);

// Current five-minute rolling average and the whole-run average at t_max_ms.
void speed_monitor_reference_rates(uint32_t t_max_ms, double *recent_avg, double *total_avg);

// Top-level dialog, opened from the Tools menu.
void open_speed_monitor_dialog_ccc(void);

#endif
