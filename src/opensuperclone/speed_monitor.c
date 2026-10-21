// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

// The Clone Speed Graph: one live throughput plot with a five-minute average
// reference line, a whole-run average reference line and markers where the
// clone phase or pass changes.
//
// The sample ring is fed by update_display_ccc() once per display interval.
// Both the sampler and the dialog run on the GTK thread (clone work pumps GTK
// events while it runs), so the ring needs no lock.

#include "speed_monitor.h"

#include <gtk/gtk.h>
#include <cairo.h>
#include <libintl.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _
#define _(str) gettext(str)
#endif
#ifndef N_
#define N_(str) (str)
#endif

// The glade XML is embedded as a byte array in clone_gui.c; only declare it
// here so the definition stays in one translation unit.
extern unsigned char opensuperclone_glade[];
extern unsigned int opensuperclone_glade_len;

// ------------------------------------------------------------- sample history

static speed_sample_t speed_history[SPEED_HISTORY_MAX];
static int speed_head = 0;
static int speed_len = 0;

static speed_event_t speed_event_history[SPEED_EVENT_MAX];
static int speed_event_head = 0;
static int speed_event_len = 0;

static uint32_t speed_last_status = 0;
static char speed_last_label[48] = "";
static bool speed_have_status = false;

static const speed_sample_t *speed_sample_at(int i)
{
  return &speed_history[(speed_head - speed_len + i + 2 * SPEED_HISTORY_MAX) % SPEED_HISTORY_MAX];
}

static const speed_event_t *speed_event_at(int i)
{
  return &speed_event_history[(speed_event_head - speed_event_len + i + 2 * SPEED_EVENT_MAX) % SPEED_EVENT_MAX];
}

void speed_monitor_clear(void)
{
  memset(speed_history, 0, sizeof(speed_history));
  speed_head = 0;
  speed_len = 0;

  memset(speed_event_history, 0, sizeof(speed_event_history));
  speed_event_head = 0;
  speed_event_len = 0;

  speed_last_status = 0;
  speed_last_label[0] = '\0';
  speed_have_status = false;
}

void speed_monitor_push(uint32_t t_ms, uint32_t dt_ms, double bytes_per_second, long long status, const char *status_label)
{
  speed_sample_t *sample;
  bool had_samples = speed_len > 0;

  if (bytes_per_second < 0.0)
  {
    bytes_per_second = 0.0;
  }
  if (dt_ms == 0)
  {
    dt_ms = 1;
  }

  sample = &speed_history[speed_head];
  sample->t_ms = t_ms;
  sample->dt_ms = dt_ms;
  sample->rate = (float)bytes_per_second;
  sample->status = (uint32_t)status;

  speed_head = (speed_head + 1) % SPEED_HISTORY_MAX;
  if (speed_len < SPEED_HISTORY_MAX)
  {
    speed_len++;
  }

  if (status_label == NULL)
  {
    status_label = "";
  }

  // A phase marker belongs where the status changes, not at the very first
  // sample of the run.
  if (had_samples && speed_have_status && (sample->status != speed_last_status || strcmp(status_label, speed_last_label) != 0))
  {
    speed_event_t *event = &speed_event_history[speed_event_head];

    event->t_ms = t_ms;
    event->status = sample->status;
    snprintf(event->label, sizeof(event->label), "%s", status_label);

    speed_event_head = (speed_event_head + 1) % SPEED_EVENT_MAX;
    if (speed_event_len < SPEED_EVENT_MAX)
    {
      speed_event_len++;
    }
  }

  speed_last_status = sample->status;
  snprintf(speed_last_label, sizeof(speed_last_label), "%s", status_label);
  speed_have_status = true;
}

bool speed_monitor_time_span(uint32_t *oldest_ms, uint32_t *newest_ms)
{
  if (speed_len == 0)
  {
    return false;
  }

  if (oldest_ms != NULL)
  {
    *oldest_ms = speed_sample_at(0)->t_ms;
  }
  if (newest_ms != NULL)
  {
    *newest_ms = speed_sample_at(speed_len - 1)->t_ms;
  }

  return true;
}

int speed_monitor_copy_range(uint32_t t_min_ms, uint32_t t_max_ms, speed_sample_t *out, int max_out)
{
  int first = -1;
  int last = -1;
  int in_range = 0;
  int count = 0;
  int i;

  if (out == NULL || max_out <= 0)
  {
    return 0;
  }

  for (i = 0; i < speed_len; i++)
  {
    uint32_t t = speed_sample_at(i)->t_ms;

    if (t >= t_min_ms && t <= t_max_ms)
    {
      if (first < 0)
      {
        first = i;
      }
      last = i;
      in_range++;
    }
  }

  if (in_range == 0)
  {
    return 0;
  }

  if (in_range <= max_out)
  {
    for (i = first; i <= last; i++)
    {
      out[count++] = *speed_sample_at(i);
    }
  }
  else if (max_out == 1)
  {
    out[count++] = *speed_sample_at(last);
  }
  else
  {
    int span = last - first;
    int step = span / (max_out - 1);

    if (step < 1)
    {
      step = 1;
    }

    for (i = first; i < last && count < max_out - 1; i += step)
    {
      out[count++] = *speed_sample_at(i);
    }

    out[count++] = *speed_sample_at(last);
  }

  return count;
}

int speed_monitor_copy_events(uint32_t t_min_ms, uint32_t t_max_ms, speed_event_t *out, int max_out)
{
  int count = 0;
  int i;

  if (out == NULL || max_out <= 0)
  {
    return 0;
  }

  for (i = 0; i < speed_event_len && count < max_out; i++)
  {
    const speed_event_t *event = speed_event_at(i);

    if (event->t_ms >= t_min_ms && event->t_ms <= t_max_ms)
    {
      out[count++] = *event;
    }
  }

  return count;
}

void speed_monitor_stats_range(uint32_t t_min_ms, uint32_t t_max_ms, speed_stats_t *stats)
{
  double sum = 0.0;
  uint64_t weight = 0;
  int i;

  if (stats == NULL)
  {
    return;
  }

  memset(stats, 0, sizeof(*stats));

  for (i = 0; i < speed_len; i++)
  {
    const speed_sample_t *sample = speed_sample_at(i);
    double rate;

    if (sample->t_ms < t_min_ms || sample->t_ms > t_max_ms)
    {
      continue;
    }

    rate = sample->rate;

    if (stats->count == 0 || rate < stats->min)
    {
      stats->min = rate;
    }
    if (stats->count == 0 || rate > stats->max)
    {
      stats->max = rate;
      stats->peak_t_ms = sample->t_ms;
    }

    sum += rate * (double)sample->dt_ms;
    weight += sample->dt_ms;
    stats->current = rate;
    stats->count++;
  }

  if (stats->count > 0 && weight > 0)
  {
    stats->avg = sum / (double)weight;
  }
}

void speed_monitor_reference_rates(uint32_t t_max_ms, double *recent_avg, double *total_avg)
{
  uint32_t recent_min = 0;
  double recent_sum = 0.0;
  double total_sum = 0.0;
  uint64_t recent_weight = 0;
  uint64_t total_weight = 0;
  int i;

  if (t_max_ms > SPEED_RECENT_WINDOW_MS)
  {
    recent_min = t_max_ms - SPEED_RECENT_WINDOW_MS;
  }

  for (i = 0; i < speed_len; i++)
  {
    const speed_sample_t *sample = speed_sample_at(i);
    double weighted;

    if (sample->t_ms > t_max_ms)
    {
      continue;
    }

    weighted = (double)sample->rate * (double)sample->dt_ms;

    total_sum += weighted;
    total_weight += sample->dt_ms;

    if (sample->t_ms >= recent_min)
    {
      recent_sum += weighted;
      recent_weight += sample->dt_ms;
    }
  }

  if (recent_avg != NULL)
  {
    *recent_avg = recent_weight > 0 ? recent_sum / (double)recent_weight : 0.0;
  }
  if (total_avg != NULL)
  {
    *total_avg = total_weight > 0 ? total_sum / (double)total_weight : 0.0;
  }
}

// ------------------------------------------------------------------- drawing

// Trace colours: the Asclepius rail palette, so the speed graph reads like the
// current/voltage monitor.
#define SPEED_COL_CURRENT_R (255.0 / 255.0)
#define SPEED_COL_CURRENT_G (155.0 / 255.0)
#define SPEED_COL_CURRENT_B (0.0 / 255.0)
#define SPEED_COL_RECENT_R (0.0 / 255.0)
#define SPEED_COL_RECENT_G (255.0 / 255.0)
#define SPEED_COL_RECENT_B (0.0 / 255.0)
#define SPEED_COL_TOTAL_R (79.0 / 255.0)
#define SPEED_COL_TOTAL_G (195.0 / 255.0)
#define SPEED_COL_TOTAL_B (247.0 / 255.0)

// Peak marker, matching the monitor's flagged-sample dots.
#define SPEED_COL_MARKER_R (230.0 / 255.0)
#define SPEED_COL_MARKER_G (38.0 / 255.0)
#define SPEED_COL_MARKER_B (38.0 / 255.0)

// Plot furniture, same values as asclepius_monitor.c.
#define SPEED_COL_GRID_R (115.0 / 255.0)
#define SPEED_COL_GRID_G (115.0 / 255.0)
#define SPEED_COL_GRID_B (115.0 / 255.0)
#define SPEED_COL_AXIS_R (140.0 / 255.0)
#define SPEED_COL_AXIS_G (140.0 / 255.0)
#define SPEED_COL_AXIS_B (140.0 / 255.0)
#define SPEED_COL_BORDER_R (89.0 / 255.0)
#define SPEED_COL_BORDER_G (89.0 / 255.0)
#define SPEED_COL_BORDER_B (89.0 / 255.0)
#define SPEED_COL_TITLE_R (158.0 / 255.0)
#define SPEED_COL_TITLE_G (158.0 / 255.0)
#define SPEED_COL_TITLE_B (158.0 / 255.0)
#define SPEED_COL_LEGEND_R (179.0 / 255.0)
#define SPEED_COL_LEGEND_G (179.0 / 255.0)
#define SPEED_COL_LEGEND_B (179.0 / 255.0)

// Phase markers use a quiet gray so they do not compete with the traces.
#define SPEED_COL_PHASE_R (170.0 / 255.0)
#define SPEED_COL_PHASE_G (170.0 / 255.0)
#define SPEED_COL_PHASE_B (170.0 / 255.0)

// Hover crosshair and tooltip.
#define SPEED_COL_CROSSHAIR_R (191.0 / 255.0)
#define SPEED_COL_CROSSHAIR_G (191.0 / 255.0)
#define SPEED_COL_CROSSHAIR_B (191.0 / 255.0)
#define SPEED_COL_TOOLTIP_BG_R (26.0 / 255.0)
#define SPEED_COL_TOOLTIP_BG_G (26.0 / 255.0)
#define SPEED_COL_TOOLTIP_BG_B (26.0 / 255.0)
#define SPEED_COL_TOOLTIP_BG_A (0.88)
#define SPEED_COL_TOOLTIP_FG_R (235.0 / 255.0)
#define SPEED_COL_TOOLTIP_FG_G (235.0 / 255.0)
#define SPEED_COL_TOOLTIP_FG_B (235.0 / 255.0)

#define SPEED_PLOT_LEFT 46
#define SPEED_PLOT_RIGHT 8
#define SPEED_PLOT_TOP 16
#define SPEED_PLOT_BOTTOM 20

// Selectable windows. Both the monitor's short presets and longer clone-run
// presets are offered; 0 means all retained time.
static const unsigned int speed_window_ms_ccc[] = {10000, 30000, 60000, 300000, 900000, 3600000, 0};
#define SPEED_NUM_WINDOWS (sizeof(speed_window_ms_ccc) / sizeof(speed_window_ms_ccc[0]))

static unsigned int speed_monitor_window_index_ccc = 2; // last 1 minute

static GtkWidget *speed_monitor_dialog_ccc = NULL;
static GtkWidget *speed_monitor_window_combobox_ccc = NULL;
static GtkWidget *speed_monitor_pause_button_ccc = NULL;
static GtkWidget *speed_monitor_stats_label_ccc = NULL;
static GtkWidget *speed_monitor_drawing_ccc = NULL;
static bool speed_monitor_open_ccc = false;
static guint speed_monitor_timer_id_ccc = 0;

static bool speed_monitor_paused_ccc = false;
static uint32_t speed_monitor_paused_t_min_ccc = 0;
static uint32_t speed_monitor_paused_t_max_ccc = 1;

static speed_sample_t *speed_points_ccc = NULL;
static speed_event_t *speed_events_ccc = NULL;

static bool speed_hover_active_ccc = false;
static uint32_t speed_hover_t_ms_ccc = 0;

typedef struct
{
  double x0, y0;
  double w, h;
  double ymin;
  double ymax;
  uint32_t t_min;
  uint32_t t_max;
  int count;
  int event_count;
} speed_plot_t;

typedef struct
{
  double scale;
  const char *suffix;
} speed_unit_t;

static void speed_window_names(char out[SPEED_NUM_WINDOWS][32])
{
  static const char *const names[SPEED_NUM_WINDOWS] = {N_("Last 10 seconds"), N_("Last 30 seconds"), N_("Last 1 minute"),
                                                       N_("Last 5 minutes"),  N_("Last 15 minutes"), N_("Last 1 hour"), N_("All time")};

  for (size_t i = 0; i < SPEED_NUM_WINDOWS; i++)
  {
    snprintf(out[i], 32, "%s", _(names[i]));
  }
}

static speed_unit_t speed_pick_unit(double peak)
{
  speed_unit_t unit;

  if (peak >= 1000000.0)
  {
    unit.scale = 1000000.0;
    unit.suffix = "MB/s";
  }
  else if (peak >= 1000.0)
  {
    unit.scale = 1000.0;
    unit.suffix = "KB/s";
  }
  else
  {
    unit.scale = 1.0;
    unit.suffix = "B/s";
  }

  return unit;
}

// Formats just the number, for callers that draw the unit separately (Pango
// spans, for example).
static void speed_format_number(char *out, size_t out_size, double bytes_per_second, const speed_unit_t *unit)
{
  double value = bytes_per_second / unit->scale;

  if (unit->scale >= 1000000.0)
  {
    snprintf(out, out_size, "%.2f", value);
  }
  else if (unit->scale >= 1000.0)
  {
    snprintf(out, out_size, "%.1f", value);
  }
  else
  {
    snprintf(out, out_size, "%.0f", value);
  }
}

static void speed_format_value(char *out, size_t out_size, double bytes_per_second, const speed_unit_t *unit)
{
  char number[32];

  speed_format_number(number, sizeof(number), bytes_per_second, unit);
  snprintf(out, out_size, "%s %s", number, unit->suffix);
}

static void speed_format_markup_value(char *out, size_t out_size, double bytes_per_second, const speed_unit_t *unit)
{
  char number[32];

  speed_format_number(number, sizeof(number), bytes_per_second, unit);
  snprintf(out, out_size, "<span foreground=\"#%02x%02x%02x\">%s</span> %s", (unsigned int)(SPEED_COL_CURRENT_R * 255.0 + 0.5),
           (unsigned int)(SPEED_COL_CURRENT_G * 255.0 + 0.5), (unsigned int)(SPEED_COL_CURRENT_B * 255.0 + 0.5), number, unit->suffix);
}

static void speed_format_age(char *out, size_t out_size, unsigned int age_ms)
{
  if (age_ms < 1000)
  {
    snprintf(out, out_size, "%s", _("now"));
  }
  else if (age_ms < 60000)
  {
    snprintf(out, out_size, "-%us", (unsigned int)(age_ms / 1000));
  }
  else
  {
    unsigned int total = age_ms / 1000;
    snprintf(out, out_size, "-%u:%02u", total / 60, total % 60);
  }
}

static void speed_resolve_window(uint32_t *t_min, uint32_t *t_max);

static void speed_setup_plot(speed_plot_t *p, int width, int height, double ymin, double ymax, uint32_t t_min, uint32_t t_max)
{
  p->x0 = SPEED_PLOT_LEFT;
  p->y0 = SPEED_PLOT_TOP;
  p->w = width - SPEED_PLOT_LEFT - SPEED_PLOT_RIGHT;
  p->h = height - SPEED_PLOT_TOP - SPEED_PLOT_BOTTOM;
  p->ymin = ymin;
  p->ymax = ymax;
  p->t_min = t_min;
  p->t_max = t_max;
  p->count = speed_monitor_copy_range(t_min, t_max, speed_points_ccc, SPEED_POINT_MAX);
  p->event_count = speed_monitor_copy_events(t_min, t_max, speed_events_ccc, SPEED_EVENT_MAX);
}

static double speed_map_x(const speed_plot_t *p, uint32_t t_ms)
{
  if (p->t_max <= p->t_min)
  {
    return p->x0;
  }

  return p->x0 + p->w * ((double)(t_ms - p->t_min) / (double)(p->t_max - p->t_min));
}

static double speed_map_y(const speed_plot_t *p, double value)
{
  if (p->ymax <= p->ymin)
  {
    return p->y0 + p->h;
  }

  return p->y0 + p->h * (1.0 - (value - p->ymin) / (p->ymax - p->ymin));
}

static void speed_set_source(cairo_t *cr, double r, double g, double b)
{
  cairo_set_source_rgb(cr, r, g, b);
}

static void speed_set_source_rgba(cairo_t *cr, double r, double g, double b, double alpha)
{
  cairo_set_source_rgba(cr, r, g, b, alpha);
}

static void speed_draw_grid(cairo_t *cr, const speed_plot_t *p, const speed_unit_t *unit)
{
  cairo_save(cr);

  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 10.0);

  for (int i = 0; i <= 4; i++)
  {
    double frac = (double)i / 4.0;
    double value = p->ymax - frac * (p->ymax - p->ymin);
    double y = p->y0 + frac * p->h;

    speed_set_source(cr, SPEED_COL_GRID_R, SPEED_COL_GRID_G, SPEED_COL_GRID_B);
    cairo_set_line_width(cr, 0.5);
    cairo_move_to(cr, p->x0, y);
    cairo_line_to(cr, p->x0 + p->w, y);
    cairo_stroke(cr);

    char text[32];
    speed_format_value(text, sizeof(text), value, unit);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    speed_set_source(cr, SPEED_COL_AXIS_R, SPEED_COL_AXIS_G, SPEED_COL_AXIS_B);
    cairo_move_to(cr, p->x0 - ext.width - 4, y + ext.height / 2.0);
    cairo_show_text(cr, text);
  }

  for (int i = 0; i <= 6; i++)
  {
    double frac = (double)i / 6.0;
    double x = p->x0 + frac * p->w;
    uint32_t t = p->t_min + (uint32_t)(frac * (double)(p->t_max - p->t_min));

    speed_set_source(cr, SPEED_COL_GRID_R, SPEED_COL_GRID_G, SPEED_COL_GRID_B);
    cairo_set_line_width(cr, 0.5);
    cairo_move_to(cr, x, p->y0);
    cairo_line_to(cr, x, p->y0 + p->h);
    cairo_stroke(cr);

    char text[24];
    uint32_t age = p->t_max > t ? p->t_max - t : 0;
    speed_format_age(text, sizeof(text), age);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    speed_set_source(cr, SPEED_COL_AXIS_R, SPEED_COL_AXIS_G, SPEED_COL_AXIS_B);
    cairo_move_to(cr, x - ext.width / 2.0, p->y0 + p->h + ext.height + 3);
    cairo_show_text(cr, text);
  }

  cairo_restore(cr);
}

static void speed_draw_phase_markers(cairo_t *cr, const speed_plot_t *p)
{
  if (p->event_count <= 0)
  {
    return;
  }

  cairo_save(cr);
  cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
  cairo_clip(cr);

  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 8.0);

  double last_x = -1000.0;
  int label_row = 0;

  for (int i = 0; i < p->event_count; i++)
  {
    const speed_event_t *event = &speed_events_ccc[i];
    double x = speed_map_x(p, event->t_ms);

    if (x < p->x0 || x > p->x0 + p->w || fabs(x - last_x) < 2.0)
    {
      continue;
    }
    last_x = x;

    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_dash(cr, (const double[]){2.0, 3.0}, 2, 0);
    speed_set_source_rgba(cr, SPEED_COL_PHASE_R, SPEED_COL_PHASE_G, SPEED_COL_PHASE_B, 0.55);
    cairo_move_to(cr, x, p->y0);
    cairo_line_to(cr, x, p->y0 + p->h);
    cairo_stroke(cr);
    cairo_restore(cr);

    if (event->label[0] != '\0')
    {
      cairo_text_extents_t ext;
      char text[48];
      snprintf(text, sizeof(text), "%.32s", event->label);
      cairo_text_extents(cr, text, &ext);

      double tx = x + 2.0;
      if (tx + ext.width + 2.0 > p->x0 + p->w)
      {
        tx = x - ext.width - 3.0;
      }
      if (tx < p->x0 + 1.0)
      {
        tx = p->x0 + 1.0;
      }

      double ty = p->y0 + 9.0 + (label_row % 2) * 10.0;
      label_row++;

      speed_set_source_rgba(cr, SPEED_COL_TOOLTIP_BG_R, SPEED_COL_TOOLTIP_BG_G, SPEED_COL_TOOLTIP_BG_B, 0.55);
      cairo_rectangle(cr, tx - 2.0, ty - ext.height, ext.width + 4.0, ext.height + 3.0);
      cairo_fill(cr);

      speed_set_source(cr, SPEED_COL_PHASE_R, SPEED_COL_PHASE_G, SPEED_COL_PHASE_B);
      cairo_move_to(cr, tx, ty);
      cairo_show_text(cr, text);
    }
  }

  cairo_restore(cr);
}

// Dashed reference line with a value tag at the right edge.
static void speed_draw_reference(cairo_t *cr, const speed_plot_t *p, double value, const speed_unit_t *unit, const char *name, double r, double g,
                                 double b, bool label_above)
{
  char value_text[48];
  char text[80];

  if (value <= 0.0)
  {
    return;
  }

  double y = speed_map_y(p, value);
  if (y < p->y0 || y > p->y0 + p->h)
  {
    return;
  }

  cairo_save(cr);
  cairo_set_line_width(cr, 1.0);
  cairo_set_dash(cr, (const double[]){4.0, 3.0}, 2, 0);
  speed_set_source(cr, r, g, b);
  cairo_move_to(cr, p->x0, y);
  cairo_line_to(cr, p->x0 + p->w, y);
  cairo_stroke(cr);
  cairo_restore(cr);

  speed_format_value(value_text, sizeof(value_text), value, unit);
  snprintf(text, sizeof(text), "%s %s", name, value_text);

  cairo_save(cr);
  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 10.0);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, text, &ext);

  double bx = p->x0 + p->w - ext.width - 4.0;
  double by = label_above ? y - ext.height - 2.0 : y + 2.0;

  if (by < p->y0)
  {
    by = p->y0 + 2.0;
  }
  if (by + ext.height + 3.0 > p->y0 + p->h)
  {
    by = p->y0 + p->h - ext.height - 3.0;
  }

  speed_set_source_rgba(cr, r, g, b, 0.10);
  cairo_rectangle(cr, bx - 2.0, by - 1.0, ext.width + 4.0, ext.height + 3.0);
  cairo_fill(cr);

  speed_set_source(cr, r, g, b);
  cairo_move_to(cr, bx, by + ext.height);
  cairo_show_text(cr, text);
  cairo_restore(cr);
}

static void speed_draw_series(cairo_t *cr, const speed_plot_t *p)
{
  if (p->count <= 0)
  {
    return;
  }

  cairo_save(cr);
  cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
  cairo_clip(cr);

  cairo_set_line_width(cr, 1.5);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
  speed_set_source(cr, SPEED_COL_CURRENT_R, SPEED_COL_CURRENT_G, SPEED_COL_CURRENT_B);

  bool started = false;
  for (int i = 0; i < p->count; i++)
  {
    double x = speed_map_x(p, speed_points_ccc[i].t_ms);
    double y = speed_map_y(p, speed_points_ccc[i].rate);

    if (!started)
    {
      cairo_move_to(cr, x, y);
      started = true;
    }
    else
    {
      cairo_line_to(cr, x, y);
    }
  }

  if (started)
  {
    cairo_stroke(cr);

    double lx = speed_map_x(p, speed_points_ccc[p->count - 1].t_ms);
    double ly = speed_map_y(p, speed_points_ccc[p->count - 1].rate);
    cairo_arc(cr, lx, ly, 2.5, 0, 2 * M_PI);
    cairo_fill(cr);
  }

  cairo_restore(cr);
}

static void speed_draw_peak(cairo_t *cr, const speed_plot_t *p, const speed_stats_t *stats)
{
  if (stats->count <= 0 || stats->max <= 0.0)
  {
    return;
  }

  if (stats->peak_t_ms < p->t_min || stats->peak_t_ms > p->t_max)
  {
    return;
  }

  double x = speed_map_x(p, stats->peak_t_ms);
  double y = speed_map_y(p, stats->max);

  cairo_save(cr);
  cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
  cairo_clip(cr);
  speed_set_source(cr, SPEED_COL_MARKER_R, SPEED_COL_MARKER_G, SPEED_COL_MARKER_B);
  cairo_arc(cr, x, y, 3.0, 0, 2 * M_PI);
  cairo_fill(cr);
  cairo_restore(cr);
}

static void speed_draw_hover(cairo_t *cr, const speed_plot_t *p, const speed_unit_t *unit)
{
  if (!speed_hover_active_ccc || speed_points_ccc == NULL || p->count <= 0)
  {
    return;
  }

  int best = 0;
  uint32_t best_delta = UINT32_MAX;

  for (int i = 0; i < p->count; i++)
  {
    uint32_t d = speed_points_ccc[i].t_ms > speed_hover_t_ms_ccc ? speed_points_ccc[i].t_ms - speed_hover_t_ms_ccc : speed_hover_t_ms_ccc - speed_points_ccc[i].t_ms;
    if (d < best_delta)
    {
      best_delta = d;
      best = i;
    }
  }

  const speed_sample_t *sample = &speed_points_ccc[best];
  double x = speed_map_x(p, sample->t_ms);
  double y = speed_map_y(p, sample->rate);

  cairo_save(cr);
  cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
  cairo_clip(cr);

  speed_set_source(cr, SPEED_COL_CROSSHAIR_R, SPEED_COL_CROSSHAIR_G, SPEED_COL_CROSSHAIR_B);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, x, p->y0);
  cairo_line_to(cr, x, p->y0 + p->h);
  cairo_stroke(cr);

  speed_set_source(cr, SPEED_COL_CURRENT_R, SPEED_COL_CURRENT_G, SPEED_COL_CURRENT_B);
  cairo_arc(cr, x, y, 3.5, 0, 2 * M_PI);
  cairo_fill(cr);

  char value[48];
  char text[128];
  uint32_t age = speed_hover_t_ms_ccc > sample->t_ms ? speed_hover_t_ms_ccc - sample->t_ms : 0;

  speed_format_value(value, sizeof(value), sample->rate, unit);
  snprintf(text, sizeof(text), "%s  %s  (%u ms ago)", _("Speed"), value, age);

  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 10.0);
  cairo_text_extents_t ext;
  cairo_text_extents(cr, text, &ext);

  double bx = x + 8.0;
  if (bx + ext.width + 8.0 > p->x0 + p->w)
  {
    bx = x - ext.width - 12.0;
  }
  if (bx < p->x0)
  {
    bx = p->x0 + 2.0;
  }

  double by = y + 8.0;
  if (by + ext.height + 6.0 > p->y0 + p->h)
  {
    by = y - ext.height - 10.0;
  }
  if (by < p->y0)
  {
    by = p->y0 + 2.0;
  }

  speed_set_source_rgba(cr, SPEED_COL_TOOLTIP_BG_R, SPEED_COL_TOOLTIP_BG_G, SPEED_COL_TOOLTIP_BG_B, SPEED_COL_TOOLTIP_BG_A);
  cairo_rectangle(cr, bx, by, ext.width + 8.0, ext.height + 6.0);
  cairo_fill(cr);

  speed_set_source(cr, SPEED_COL_TOOLTIP_FG_R, SPEED_COL_TOOLTIP_FG_G, SPEED_COL_TOOLTIP_FG_B);
  cairo_move_to(cr, bx + 4.0, by + ext.height + 2.0);
  cairo_show_text(cr, text);

  cairo_restore(cr);
}

static void speed_draw_border(cairo_t *cr, const speed_plot_t *p)
{
  speed_set_source(cr, SPEED_COL_BORDER_R, SPEED_COL_BORDER_G, SPEED_COL_BORDER_B);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
  cairo_stroke(cr);
}

static void speed_draw_title(cairo_t *cr, const speed_plot_t *p, const char *title)
{
  cairo_save(cr);
  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 11.0);
  speed_set_source(cr, SPEED_COL_TITLE_R, SPEED_COL_TITLE_G, SPEED_COL_TITLE_B);
  cairo_move_to(cr, p->x0, p->y0 - 5.0);
  cairo_show_text(cr, title);
  cairo_restore(cr);
}

static void speed_draw_legend(cairo_t *cr, const speed_plot_t *p, const char *const *labels, const double *cols, int n)
{
  cairo_save(cr);
  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 10.0);

  double x = p->x0 + p->w;
  for (int i = 0; i < n; i++)
  {
    cairo_text_extents_t ext;
    cairo_text_extents(cr, labels[i], &ext);
    x -= ext.width + 8.0;

    cairo_set_source_rgb(cr, cols[i * 3], cols[i * 3 + 1], cols[i * 3 + 2]);
    cairo_rectangle(cr, x, p->y0 - 11.0, 8.0, 3.0);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, SPEED_COL_LEGEND_R, SPEED_COL_LEGEND_G, SPEED_COL_LEGEND_B);
    cairo_move_to(cr, x + 11.0, p->y0 - 7.0);
    cairo_show_text(cr, labels[i]);

    x -= 4.0;
  }

  cairo_restore(cr);
}

static void speed_resolve_window(uint32_t *t_min, uint32_t *t_max)
{
  if (speed_monitor_paused_ccc)
  {
    *t_min = speed_monitor_paused_t_min_ccc;
    *t_max = speed_monitor_paused_t_max_ccc;
    return;
  }

  uint32_t oldest = 0;
  uint32_t newest = 0;

  if (!speed_monitor_time_span(&oldest, &newest))
  {
    *t_min = 0;
    *t_max = 1;
    return;
  }

  *t_max = newest;
  unsigned int window = speed_window_ms_ccc[speed_monitor_window_index_ccc];

  if (window == 0 || (uint32_t)window > newest - oldest)
  {
    *t_min = oldest;
  }
  else
  {
    *t_min = newest - window;
  }
}

static bool speed_draw_plot(cairo_t *cr, int width, int height)
{
  if (width <= SPEED_PLOT_LEFT + SPEED_PLOT_RIGHT || height <= SPEED_PLOT_TOP + SPEED_PLOT_BOTTOM)
  {
    return false;
  }

  uint32_t t_min, t_max;
  speed_resolve_window(&t_min, &t_max);

  speed_stats_t stats;
  speed_monitor_stats_range(t_min, t_max, &stats);

  double recent = 0.0;
  double total = 0.0;
  speed_monitor_reference_rates(t_max, &recent, &total);

  double top = stats.count > 0 ? stats.max : 0.0;
  if (recent > top)
  {
    top = recent;
  }
  if (total > top)
  {
    top = total;
  }
  double ymax = top > 0.0 ? top * 1.08 : 1.0;
  speed_unit_t unit = speed_pick_unit(ymax);

  speed_plot_t plot;
  speed_setup_plot(&plot, width, height, 0.0, ymax, t_min, t_max);

  speed_draw_grid(cr, &plot, &unit);
  speed_draw_phase_markers(cr, &plot);
  speed_draw_reference(cr, &plot, recent, &unit, _("5m avg"), SPEED_COL_RECENT_R, SPEED_COL_RECENT_G, SPEED_COL_RECENT_B, true);
  speed_draw_reference(cr, &plot, total, &unit, _("Total avg"), SPEED_COL_TOTAL_R, SPEED_COL_TOTAL_G, SPEED_COL_TOTAL_B, false);
  speed_draw_peak(cr, &plot, &stats);
  speed_draw_series(cr, &plot);
  speed_draw_hover(cr, &plot, &unit);
  speed_draw_border(cr, &plot);
  speed_draw_title(cr, &plot, _("clone speed"));

  static const double cols[] = {SPEED_COL_CURRENT_R, SPEED_COL_CURRENT_G, SPEED_COL_CURRENT_B, SPEED_COL_RECENT_R,
                                SPEED_COL_RECENT_G,  SPEED_COL_RECENT_B,  SPEED_COL_TOTAL_R,   SPEED_COL_TOTAL_G,
                                SPEED_COL_TOTAL_B};
  const char *labels[] = {_("Speed"), _("5m avg"), _("Total avg")};
  speed_draw_legend(cr, &plot, labels, cols, 3);

  return true;
}

static gboolean on_speed_draw(GtkWidget *widget, cairo_t *cr, gpointer data)
{
  (void)data;
  speed_draw_plot(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
  return FALSE;
}

static void speed_update_monitor(void)
{
  if (!speed_monitor_open_ccc || speed_monitor_dialog_ccc == NULL)
  {
    return;
  }

  uint32_t t_min, t_max;
  speed_resolve_window(&t_min, &t_max);

  speed_stats_t stats;
  speed_monitor_stats_range(t_min, t_max, &stats);

  double recent = 0.0;
  double total = 0.0;
  speed_monitor_reference_rates(t_max, &recent, &total);

  double top = stats.count > 0 ? stats.max : 0.0;
  if (recent > top)
  {
    top = recent;
  }
  if (total > top)
  {
    top = total;
  }
  speed_unit_t unit = speed_pick_unit(top > 0.0 ? top * 1.08 : 1.0);

  if (speed_monitor_stats_label_ccc != NULL)
  {
    if (stats.count == 0)
    {
      gtk_label_set_text(GTK_LABEL(speed_monitor_stats_label_ccc), _("No samples"));
      gtk_widget_set_tooltip_text(speed_monitor_stats_label_ccc, NULL);
    }
    else
    {
      char current[64];
      char minimum[64];
      char average[64];
      char maximum[64];
      char current_plain[64];
      char minimum_plain[64];
      char average_plain[64];
      char maximum_plain[64];
      char text[320];
      char plain[320];

      speed_format_markup_value(current, sizeof(current), stats.current, &unit);
      speed_format_markup_value(minimum, sizeof(minimum), stats.min, &unit);
      speed_format_markup_value(average, sizeof(average), stats.avg, &unit);
      speed_format_markup_value(maximum, sizeof(maximum), stats.max, &unit);

      snprintf(text, sizeof(text), "%s  %s   min %s   avg %s   max %s", _("Speed"), current, minimum, average, maximum);
      gtk_label_set_markup(GTK_LABEL(speed_monitor_stats_label_ccc), text);

      // The label wraps to two lines and then ellipsizes, so keep the complete
      // row reachable as a tooltip when the window is narrow.
      speed_format_value(current_plain, sizeof(current_plain), stats.current, &unit);
      speed_format_value(minimum_plain, sizeof(minimum_plain), stats.min, &unit);
      speed_format_value(average_plain, sizeof(average_plain), stats.avg, &unit);
      speed_format_value(maximum_plain, sizeof(maximum_plain), stats.max, &unit);

      snprintf(plain, sizeof(plain), "%s  %s   min %s   avg %s   max %s", _("Speed"), current_plain, minimum_plain, average_plain, maximum_plain);
      gtk_widget_set_tooltip_text(speed_monitor_stats_label_ccc, plain);
    }
  }

  if (speed_monitor_pause_button_ccc != NULL)
  {
    gtk_button_set_label(GTK_BUTTON(speed_monitor_pause_button_ccc), speed_monitor_paused_ccc ? _("Resume") : _("Pause"));
  }

  if (speed_monitor_drawing_ccc != NULL)
  {
    gtk_widget_queue_draw(speed_monitor_drawing_ccc);
  }
}

static gboolean speed_refresh_timer(gpointer data)
{
  (void)data;

  if (speed_monitor_paused_ccc)
  {
    return G_SOURCE_CONTINUE;
  }

  speed_update_monitor();

  return G_SOURCE_CONTINUE;
}

static void speed_pause_clicked(GtkButton *button, gpointer data)
{
  (void)button;
  (void)data;

  if (!speed_monitor_paused_ccc)
  {
    speed_resolve_window(&speed_monitor_paused_t_min_ccc, &speed_monitor_paused_t_max_ccc);
  }

  speed_monitor_paused_ccc = !speed_monitor_paused_ccc;
  speed_update_monitor();
}

static gboolean on_speed_plot_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
  (void)data;

  if (speed_points_ccc == NULL)
  {
    return FALSE;
  }

  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);
  if (width <= SPEED_PLOT_LEFT + SPEED_PLOT_RIGHT || height <= SPEED_PLOT_TOP + SPEED_PLOT_BOTTOM)
  {
    return FALSE;
  }

  uint32_t t_min, t_max;
  speed_resolve_window(&t_min, &t_max);

  double plot_w = width - SPEED_PLOT_LEFT - SPEED_PLOT_RIGHT;
  if (plot_w <= 0 || t_max <= t_min)
  {
    return FALSE;
  }

  double frac = (event->x - SPEED_PLOT_LEFT) / plot_w;
  if (frac < 0)
  {
    frac = 0;
  }
  if (frac > 1)
  {
    frac = 1;
  }

  speed_hover_active_ccc = true;
  speed_hover_t_ms_ccc = t_min + (uint32_t)(frac * (double)(t_max - t_min));

  gtk_widget_queue_draw(widget);

  return FALSE;
}

static gboolean on_speed_plot_leave(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  (void)event;
  (void)data;

  speed_hover_active_ccc = false;
  gtk_widget_queue_draw(widget);

  return FALSE;
}

static void on_speed_window_changed(GtkComboBox *combo, gpointer data)
{
  (void)data;

  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if (active >= 0 && active < (int)SPEED_NUM_WINDOWS)
  {
    speed_monitor_window_index_ccc = (unsigned int)active;
  }

  speed_update_monitor();
}

static void speed_monitor_stop_refresh(void)
{
  if (speed_monitor_timer_id_ccc != 0)
  {
    g_source_remove(speed_monitor_timer_id_ccc);
    speed_monitor_timer_id_ccc = 0;
  }

  speed_monitor_open_ccc = false;
  speed_monitor_dialog_ccc = NULL;
  speed_monitor_window_combobox_ccc = NULL;
  speed_monitor_pause_button_ccc = NULL;
  speed_monitor_stats_label_ccc = NULL;
  speed_monitor_drawing_ccc = NULL;

  speed_hover_active_ccc = false;
}

static gboolean on_speed_monitor_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  (void)widget;
  (void)event;
  (void)data;

  speed_monitor_stop_refresh();

  return FALSE;
}

static void on_speed_monitor_destroy(GtkWidget *widget, gpointer data)
{
  (void)widget;
  (void)data;

  speed_monitor_stop_refresh();
}

static void on_speed_monitor_response(GtkDialog *dialog, int response, gpointer data)
{
  (void)dialog;
  (void)data;

  if (response != GTK_RESPONSE_CLOSE)
  {
    return;
  }

  GtkWidget *window = speed_monitor_dialog_ccc;

  speed_monitor_stop_refresh();

  if (window != NULL)
  {
    gtk_widget_destroy(window);
  }
}

void open_speed_monitor_dialog_ccc(void)
{
  if (speed_monitor_open_ccc && speed_monitor_dialog_ccc != NULL)
  {
    gtk_window_present(GTK_WINDOW(speed_monitor_dialog_ccc));
    return;
  }

  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, NULL);
  gtk_builder_connect_signals(builder, NULL);

  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "speed_monitor_dialog"));
  if (dialog == NULL)
  {
    g_object_unref(builder);
    return;
  }

  GtkWidget *window_label = GTK_WIDGET(gtk_builder_get_object(builder, "speed_mon_window_label"));
  GtkWidget *window_combo = GTK_WIDGET(gtk_builder_get_object(builder, "speed_mon_window_combobox"));
  GtkWidget *pause_button = GTK_WIDGET(gtk_builder_get_object(builder, "speed_mon_pause_button"));
  GtkWidget *stats_label = GTK_WIDGET(gtk_builder_get_object(builder, "speed_mon_stats_label"));
  GtkWidget *drawing = GTK_WIDGET(gtk_builder_get_object(builder, "speed_mon_drawing"));
  GtkWidget *speed_label = GTK_WIDGET(gtk_builder_get_object(builder, "speed_mon_speed_label"));

  gtk_window_set_title(GTK_WINDOW(dialog), _("Clone Speed Graph"));

  if (window_label != NULL)
  {
    gtk_label_set_text(GTK_LABEL(window_label), _("Window"));
  }
  if (speed_label != NULL)
  {
    gtk_label_set_text(GTK_LABEL(speed_label), _("Speed"));
  }
  if (stats_label != NULL)
  {
    gtk_label_set_text(GTK_LABEL(stats_label), _("No samples"));
  }

  if (window_combo != NULL)
  {
    char names[SPEED_NUM_WINDOWS][32];
    speed_window_names(names);

    for (size_t i = 0; i < SPEED_NUM_WINDOWS; i++)
    {
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window_combo), names[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(window_combo), (int)speed_monitor_window_index_ccc);
    g_signal_connect(window_combo, "changed", G_CALLBACK(on_speed_window_changed), NULL);
  }

  if (pause_button != NULL)
  {
    g_signal_connect(pause_button, "clicked", G_CALLBACK(speed_pause_clicked), NULL);
  }

  if (drawing != NULL)
  {
    g_signal_connect(drawing, "draw", G_CALLBACK(on_speed_draw), NULL);
    g_signal_connect(drawing, "motion-notify-event", G_CALLBACK(on_speed_plot_motion), NULL);
    g_signal_connect(drawing, "leave-notify-event", G_CALLBACK(on_speed_plot_leave), NULL);
    gtk_widget_add_events(drawing, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  }

  if (speed_points_ccc == NULL)
  {
    speed_points_ccc = calloc(SPEED_POINT_MAX, sizeof(speed_sample_t));
  }
  if (speed_events_ccc == NULL)
  {
    speed_events_ccc = calloc(SPEED_EVENT_MAX, sizeof(speed_event_t));
  }
  if (speed_points_ccc == NULL || speed_events_ccc == NULL)
  {
    printf("Speed monitor: out of memory for the plot buffers\n");
    g_object_unref(builder);
    return;
  }

  speed_monitor_dialog_ccc = dialog;
  speed_monitor_window_combobox_ccc = window_combo;
  speed_monitor_pause_button_ccc = pause_button;
  speed_monitor_stats_label_ccc = stats_label;
  speed_monitor_drawing_ccc = drawing;

  g_signal_connect(dialog, "delete-event", G_CALLBACK(on_speed_monitor_delete), NULL);
  g_signal_connect(dialog, "response", G_CALLBACK(on_speed_monitor_response), NULL);
  g_signal_connect(dialog, "destroy", G_CALLBACK(on_speed_monitor_destroy), NULL);

  speed_monitor_open_ccc = true;
  speed_monitor_timer_id_ccc = g_timeout_add(250, speed_refresh_timer, NULL);

  gtk_widget_show_all(dialog);

  // Show the window before dropping the builder, so the window owns its own
  // reference and is not finalized along with the builder.
  g_object_unref(builder);

  speed_update_monitor();
}
