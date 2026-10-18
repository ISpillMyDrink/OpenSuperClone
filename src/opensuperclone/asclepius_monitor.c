// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

// The Asclepius Monitor window: live voltage/current plots, per-rail power
// control and rolling min/avg/max statistics.
//
// Threading: this file only ever touches GTK from the GUI thread. The poller
// thread owns the serial port and appends to asclepius_monitor; every read of
// that buffer goes through the mon_copy_* helpers, which hold the monitor lock
// only for a memcpy, so a redraw never blocks a poll.

#include "asclepius_monitor.h"

#include "asclepius.h"
#include "monitor.h"

// The glade XML is embedded as a byte array in clone_gui.c; only declare it
// here so the definition stays in one translation unit.
extern unsigned char opensuperclone_glade[];
extern unsigned int opensuperclone_glade_len;

// gettext, same convention as the rest of the app
#include <libintl.h>
#include <locale.h>

#ifndef _
#define _(str) gettext(str)
#endif
#ifndef N_
#define N_(str) (str)
#endif

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Rail trace colors, matching the Asclepius Monitor's defaults.
#define MON_COL_12V_R (240.0 / 255.0)
#define MON_COL_12V_G (192.0 / 255.0)
#define MON_COL_12V_B (72.0 / 255.0)
#define MON_COL_5V_R (79.0 / 255.0)
#define MON_COL_5V_G (195.0 / 255.0)
#define MON_COL_5V_B (163.0 / 255.0)
#define MON_COL_USB_R (74.0 / 255.0)
#define MON_COL_USB_G (159.0 / 255.0)
#define MON_COL_USB_B (232.0 / 255.0)

// Plot geometry, in pixels.
#define MON_PLOT_LEFT 46
#define MON_PLOT_RIGHT 8
#define MON_PLOT_TOP 16
#define MON_PLOT_BOTTOM 20

// Selected time window, in milliseconds. 0 means "all time".
static const unsigned int mon_window_ms_ccc[] = {
  10000, 30000, 60000, 300000, 0
};
#define MON_NUM_WINDOWS (sizeof(mon_window_ms_ccc) / sizeof(mon_window_ms_ccc[0]))

// Fills out with the localized name for each selectable window. The literals
// sit inside this function so xgettext can extract them for translation.
static void mon_window_names(char out[MON_NUM_WINDOWS][32])
{
  static const char *const names[MON_NUM_WINDOWS] = {
    N_("Last 10 seconds"),
    N_("Last 30 seconds"),
    N_("Last 1 minute"),
    N_("Last 5 minutes"),
    N_("All time")
  };

  for(size_t i = 0; i < MON_NUM_WINDOWS; i++)
  {
    snprintf(out[i], 32, "%s", _(names[i]));
  }
}

bool asclepius_monitor_open = false;
unsigned int asclepius_monitor_window_index_ccc = 2; // last 1 minute

static GtkWidget *asclepius_monitor_dialog_ccc = NULL;
static GtkWidget *asclepius_mon_window_combobox_ccc = NULL;
static GtkWidget *asclepius_mon_status_label_ccc = NULL;
static GtkWidget *asclepius_mon_voltage_stats_label_ccc = NULL;
static GtkWidget *asclepius_mon_current_stats_label_ccc = NULL;
static GtkWidget *asclepius_mon_power_buttons_ccc[5] = { NULL, NULL, NULL, NULL, NULL };

// Refresh timer, so it can be removed on close.
static guint asclepius_monitor_timer_id = 0;

static GtkWidget *asclepius_mon_v12_drawing_ccc = NULL;
static GtkWidget *asclepius_mon_v5_drawing_ccc = NULL;
static GtkWidget *asclepius_mon_current_drawing_ccc = NULL;

// Scratch buffer for one plot's worth of samples. Decimated to roughly one
// point per horizontal pixel column, so this stays small.
#define MON_MAX_POINTS 2048
static mon_sample_t *mon_points_ccc = NULL;

// Hover state for the crosshair read-out.
static bool mon_hover_active_ccc = false;
static double mon_hover_x_ccc = 0;
static unsigned int mon_hover_t_ms_ccc = 0;

// Which series a hover was captured over, so the tooltip can name it.
typedef enum
{
    MON_HOVER_NONE = 0,
    MON_HOVER_V12,
    MON_HOVER_V5,
    MON_HOVER_CURRENT
} mon_hover_kind_t;

static mon_hover_kind_t mon_hover_kind_ccc = MON_HOVER_NONE;

// ---------------------------------------------------------------- utilities

static void mon_set_source(cairo_t *cr, double r, double g, double b)
{
    cairo_set_source_rgb(cr, r, g, b);
}

// Sets the source colour with an explicit alpha. cairo_set_source_alpha() is
// newer than the cairo we build against, so use the RGBA form directly.
static void mon_set_source_rgba(cairo_t *cr, double r, double g, double b, double alpha)
{
    cairo_set_source_rgba(cr, r, g, b, alpha);
}

// Formats a duration as the monitor's relative labels: now / -5s / -1:05
static void mon_format_age(char *out, size_t out_size, unsigned int age_ms)
{
    if(age_ms < 1000)
    {
        snprintf(out, out_size, "now");
    }
    else if(age_ms < 60000)
    {
        snprintf(out, out_size, "-%us", (unsigned int)(age_ms / 1000));
    }
    else
    {
        unsigned int total = age_ms / 1000;
        snprintf(out, out_size, "-%u:%02u", total / 60, total % 60);
    }
}

// ------------------------------------------------------------------ drawing

// Shared axis/scale state for one plot.
typedef struct
{
    double x0, y0;   // plot area origin
    double w, h;     // plot area size
    double ymin;
    double ymax;
    uint32_t t_min;
    uint32_t t_max;
    int count;       // points in mon_points_ccc
} mon_plot_t;

static void mon_setup_plot(mon_plot_t *p, int width, int height, double ymin, double ymax, uint32_t t_min, uint32_t t_max)
{
    p->x0 = MON_PLOT_LEFT;
    p->y0 = MON_PLOT_TOP;
    p->w = width - MON_PLOT_LEFT - MON_PLOT_RIGHT;
    p->h = height - MON_PLOT_TOP - MON_PLOT_BOTTOM;
    p->ymin = ymin;
    p->ymax = ymax;
    p->t_min = t_min;
    p->t_max = t_max;
    p->count = mon_copy_range(&asclepius_monitor, t_min, t_max, mon_points_ccc, MON_MAX_POINTS);
}

static double mon_map_x(const mon_plot_t *p, uint32_t t_ms)
{
    if(p->t_max <= p->t_min)
    {
        return p->x0;
    }

    return p->x0 + p->w * ((double)(t_ms - p->t_min) / (double)(p->t_max - p->t_min));
}

static double mon_map_y(const mon_plot_t *p, double value)
{
    if(p->ymax <= p->ymin)
    {
        return p->y0 + p->h;
    }

    return p->y0 + p->h * (1.0 - (value - p->ymin) / (p->ymax - p->ymin));
}

// Shaded ATX zone bands: red outside the spec window, green inside.
static void mon_draw_zone_bands(cairo_t *cr, const mon_plot_t *p, double spec_min, double spec_max)
{
    cairo_save(cr);
    cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
    cairo_clip(cr);

    mon_set_source_rgba(cr, 0.78, 0.20, 0.20, 0.15);
    double above_y = mon_map_y(p, spec_max);
    if(above_y > p->y0)
    {
        cairo_rectangle(cr, p->x0, p->y0, p->w, above_y - p->y0);
        cairo_fill(cr);
    }

    double below_y = mon_map_y(p, spec_min);
    if(below_y < p->y0 + p->h)
    {
        cairo_rectangle(cr, p->x0, below_y, p->w, p->y0 + p->h - below_y);
        cairo_fill(cr);
    }

    mon_set_source_rgba(cr, 0.20, 0.72, 0.36, 0.12);
    cairo_rectangle(cr, p->x0, above_y, p->w, below_y - above_y);
    cairo_fill(cr);

    cairo_restore(cr);
}

static void mon_draw_dashed_limit(cairo_t *cr, const mon_plot_t *p, double value)
{
    double y = mon_map_y(p, value);

    if(y < p->y0 || y > p->y0 + p->h)
    {
        return;
    }

    cairo_save(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_set_dash(cr, (const double[]){ 4.0, 3.0 }, 2, 0);
    mon_set_source(cr, 0.85, 0.25, 0.25);
    cairo_move_to(cr, p->x0, y);
    cairo_line_to(cr, p->x0 + p->w, y);
    cairo_stroke(cr);
    cairo_restore(cr);

    char text[24];
    snprintf(text, sizeof(text), "%.2f V", value);
    cairo_save(cr);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    double bx = p->x0 + p->w - ext.width - 4;
    double by = y - ext.height - 1;
    mon_set_source_rgba(cr, 0.85, 0.25, 0.25, 0.85);
    cairo_rectangle(cr, bx - 2, by - 1, ext.width + 4, ext.height + 3);
    cairo_fill(cr);
    mon_set_source(cr, 0.85, 0.25, 0.25);
    cairo_move_to(cr, bx, by + ext.height);
    cairo_show_text(cr, text);
    cairo_restore(cr);
}

static void mon_draw_grid(cairo_t *cr, const mon_plot_t *p, const char *unit)
{
    cairo_save(cr);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);

    for(int i = 0; i <= 4; i++)
    {
        double frac = (double)i / 4.0;
        double value = p->ymax - frac * (p->ymax - p->ymin);
        double y = p->y0 + frac * p->h;

        mon_set_source(cr, 0.45, 0.45, 0.45);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, p->x0, y);
        cairo_line_to(cr, p->x0 + p->w, y);
        cairo_stroke(cr);

        char text[32];
        snprintf(text, sizeof(text), "%.2f %s", value, unit);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, text, &ext);
        mon_set_source(cr, 0.55, 0.55, 0.55);
        cairo_move_to(cr, p->x0 - ext.width - 4, y + ext.height / 2.0);
        cairo_show_text(cr, text);
    }

    for(int i = 0; i <= 6; i++)
    {
        double frac = (double)i / 6.0;
        double x = p->x0 + frac * p->w;
        uint32_t t = p->t_min + (uint32_t)(frac * (double)(p->t_max - p->t_min));

        mon_set_source(cr, 0.45, 0.45, 0.45);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, x, p->y0);
        cairo_line_to(cr, x, p->y0 + p->h);
        cairo_stroke(cr);

        char text[24];
        uint32_t age = p->t_max > t ? p->t_max - t : 0;
        mon_format_age(text, sizeof(text), age);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, text, &ext);
        mon_set_source(cr, 0.55, 0.55, 0.55);
        cairo_move_to(cr, x - ext.width / 2.0, p->y0 + p->h + ext.height + 3);
        cairo_show_text(cr, text);
    }

    cairo_restore(cr);
}

// Draws one series. series picks which field of the sample to read.
typedef double (*mon_series_fn)(const mon_sample_t *s);

static double mon_series_v12(const mon_sample_t *s) { return s->v12; }
static double mon_series_v5(const mon_sample_t *s) { return s->v5; }
static double mon_series_a12(const mon_sample_t *s) { return s->a12; }
static double mon_series_a5(const mon_sample_t *s) { return s->a5; }
static double mon_series_ausb(const mon_sample_t *s) { return s->ausb; }

static void mon_draw_series(cairo_t *cr, const mon_plot_t *p, mon_series_fn fn, double r, double g, double b)
{
    if(p->count <= 0)
    {
        return;
    }

    cairo_save(cr);
    cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
    cairo_clip(cr);

    cairo_set_line_width(cr, 1.5);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    mon_set_source(cr, r, g, b);

    bool started = false;
    for(int i = 0; i < p->count; i++)
    {
        double x = mon_map_x(p, mon_points_ccc[i].t_ms);
        double y = mon_map_y(p, fn(&mon_points_ccc[i]));

        if(!started)
        {
            cairo_move_to(cr, x, y);
            started = true;
        }
        else
        {
            cairo_line_to(cr, x, y);
        }
    }

    if(started)
    {
        cairo_stroke(cr);

        double lx = mon_map_x(p, mon_points_ccc[p->count - 1].t_ms);
        double ly = mon_map_y(p, fn(&mon_points_ccc[p->count - 1]));
        cairo_arc(cr, lx, ly, 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}

// Red dots on flagged samples: out of spec on the voltage plots, or
// over-current on the current plot. The dot sits on the flagged series, so
// the caller passes which value to centre it on.
typedef double (*mon_value_fn)(const mon_sample_t *s);

static double mon_mark_value_v12(const mon_sample_t *s) { return s->v12; }
static double mon_mark_value_v5(const mon_sample_t *s) { return s->v5; }
static double mon_mark_value_a12(const mon_sample_t *s) { return s->a12; }

static void mon_draw_markers(cairo_t *cr, const mon_plot_t *p, bool (*bad)(const mon_sample_t *s), mon_value_fn value)
{
    if(p->count <= 0)
    {
        return;
    }

    cairo_save(cr);
    cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
    cairo_clip(cr);
    mon_set_source(cr, 0.90, 0.15, 0.15);

    // Dedupe to one marker per pixel column so a long excursion stays legible.
    double last_x = -1000;
    for(int i = 0; i < p->count; i++)
    {
        if(!bad(&mon_points_ccc[i]))
        {
            continue;
        }

        double x = mon_map_x(p, mon_points_ccc[i].t_ms);
        if(fabs(x - last_x) < 2.0)
        {
            continue;
        }
        last_x = x;

        double y = mon_map_y(p, value(&mon_points_ccc[i]));
        cairo_arc(cr, x, y, 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}

static bool mon_bad_v12(const mon_sample_t *s) { return s->oos12 != 0; }
static bool mon_bad_v5(const mon_sample_t *s) { return s->oos5 != 0; }
static bool mon_bad_any_current(const mon_sample_t *s) { return s->ocp12 || s->ocp5 || s->ocpusb; }

static void mon_draw_border(cairo_t *cr, const mon_plot_t *p)
{
    mon_set_source(cr, 0.35, 0.35, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
    cairo_stroke(cr);
}

static void mon_draw_title(cairo_t *cr, const mon_plot_t *p, const char *title)
{
    cairo_save(cr);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    mon_set_source(cr, 0.62, 0.62, 0.62);
    cairo_move_to(cr, p->x0, p->y0 - 5);
    cairo_show_text(cr, title);
    cairo_restore(cr);
}

static void mon_draw_legend(cairo_t *cr, const mon_plot_t *p, const char *const *labels, const double *cols, int n)
{
    cairo_save(cr);
    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);

    double x = p->x0 + p->w;
    for(int i = 0; i < n; i++)
    {
        cairo_text_extents_t ext;
        cairo_text_extents(cr, labels[i], &ext);
        x -= ext.width + 8;

        cairo_set_source_rgb(cr, cols[i * 3], cols[i * 3 + 1], cols[i * 3 + 2]);
        cairo_rectangle(cr, x, p->y0 - 11, 8, 3);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0.70, 0.70, 0.70);
        cairo_move_to(cr, x + 11, p->y0 - 7);
        cairo_show_text(cr, labels[i]);

        x -= 4;
    }

    cairo_restore(cr);
}

// Crosshair plus a read-out tooltip for the hovered sample.
static void mon_draw_hover(cairo_t *cr, const mon_plot_t *p, mon_series_fn fn, double r, double g, double b, const char *name, const char *unit)
{
    if(!mon_hover_active_ccc || mon_points_ccc == NULL || p->count <= 0)
    {
        return;
    }

    int best = 0;
    uint32_t best_delta = UINT32_MAX;
    for(int i = 0; i < p->count; i++)
    {
        uint32_t d = mon_points_ccc[i].t_ms > mon_hover_t_ms_ccc ? mon_points_ccc[i].t_ms - mon_hover_t_ms_ccc : mon_hover_t_ms_ccc - mon_points_ccc[i].t_ms;
        if(d < best_delta)
        {
            best_delta = d;
            best = i;
        }
    }

    const mon_sample_t *s = &mon_points_ccc[best];
    double x = mon_map_x(p, s->t_ms);
    double y = mon_map_y(p, fn(s));

    cairo_save(cr);
    cairo_rectangle(cr, p->x0, p->y0, p->w, p->h);
    cairo_clip(cr);

    mon_set_source(cr, 0.75, 0.75, 0.75);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, x, p->y0);
    cairo_line_to(cr, x, p->y0 + p->h);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr, r, g, b);
    cairo_arc(cr, x, y, 3.5, 0, 2 * M_PI);
    cairo_fill(cr);

    // Tooltip box, flipped to stay inside the plot
    char text[96];
    uint32_t age = mon_hover_t_ms_ccc > s->t_ms ? mon_hover_t_ms_ccc - s->t_ms : 0;
    snprintf(text, sizeof(text), "%s  %.3f %s  (%u ms ago)", name, fn(s), unit, age);

    cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);

    double bx = x + 8;
    if(bx + ext.width + 8 > p->x0 + p->w)
    {
        bx = x - ext.width - 12;
    }
    if(bx < p->x0)
    {
        bx = p->x0 + 2;
    }

    double by = y + 8;
    if(by + ext.height + 6 > p->y0 + p->h)
    {
        by = y - ext.height - 10;
    }
    if(by < p->y0)
    {
        by = p->y0 + 2;
    }

    mon_set_source_rgba(cr, 0.10, 0.10, 0.10, 0.88);
    cairo_rectangle(cr, bx, by, ext.width + 8, ext.height + 6);
    cairo_fill(cr);

    mon_set_source(cr, 0.92, 0.92, 0.92);
    cairo_move_to(cr, bx + 4, by + ext.height + 2);
    cairo_show_text(cr, text);

    cairo_restore(cr);
}

// Per-plot draw callbacks. Each resolves the time window first so all three
// plots share the same horizontal span.
static void mon_resolve_window(uint32_t *t_min, uint32_t *t_max);
static void on_asclepius_monitor_response_ccc(GtkDialog *dialog, int response, gpointer data);
static void on_asclepius_monitor_destroy_ccc(GtkWidget *widget, gpointer data);
static gboolean on_asclepius_monitor_delete_ccc(GtkWidget *widget, GdkEvent *event, gpointer data);
void asclepius_monitor_stop_refresh_ccc(void);

static void mon_resolve_window(uint32_t *t_min, uint32_t *t_max)
{
    uint32_t oldest = 0;
    uint32_t newest = 0;

    if(!mon_time_span(&asclepius_monitor, &oldest, &newest))
    {
        *t_min = 0;
        *t_max = 1;
        return;
    }

    *t_max = newest;
    unsigned int window = mon_window_ms_ccc[asclepius_monitor_window_index_ccc];

    if(window == 0 || (uint32_t)window > newest - oldest)
    {
        *t_min = oldest;
    }
    else
    {
        *t_min = newest - window;
    }
}

// Which plot to draw. The GTK draw callbacks are thin wrappers around
// mon_draw_plot so the rendering can also be exercised headlessly.
typedef enum
{
    MON_PLOT_V12 = 0,
    MON_PLOT_V5,
    MON_PLOT_CURRENT
} mon_plot_kind_t;

// Core renderer: draws one plot into the given cairo context at the given
// pixel size. Returns false without drawing if the area is too small.
static bool mon_draw_plot(cairo_t *cr, int width, int height, mon_plot_kind_t kind)
{
    if(width <= MON_PLOT_LEFT + MON_PLOT_RIGHT || height <= MON_PLOT_TOP + MON_PLOT_BOTTOM)
    {
        return false;
    }

    uint32_t t_min, t_max;
    mon_resolve_window(&t_min, &t_max);

    if(kind == MON_PLOT_V12)
    {
        static const char *const labels[] = { "12V", "Spec" };
        const double cols[] = { MON_COL_12V_R, MON_COL_12V_G, MON_COL_12V_B, 0.85, 0.25, 0.25 };

        mon_plot_t plot;
        mon_setup_plot(&plot, width, height, 10.0, 14.0, t_min, t_max);

        mon_draw_zone_bands(cr, &plot, ASCLEPIUS_V12_MIN_MV / 1000.0, ASCLEPIUS_V12_MAX_MV / 1000.0);
        mon_draw_grid(cr, &plot, "V");
        mon_draw_dashed_limit(cr, &plot, ASCLEPIUS_V12_MIN_MV / 1000.0);
        mon_draw_dashed_limit(cr, &plot, ASCLEPIUS_V12_MAX_MV / 1000.0);
        mon_draw_markers(cr, &plot, mon_bad_v12, mon_mark_value_v12);
        mon_draw_series(cr, &plot, mon_series_v12, MON_COL_12V_R, MON_COL_12V_G, MON_COL_12V_B);
        if(mon_hover_kind_ccc == MON_HOVER_V12)
        {
            mon_draw_hover(cr, &plot, mon_series_v12, MON_COL_12V_R, MON_COL_12V_G, MON_COL_12V_B, "12V", "V");
        }
        mon_draw_border(cr, &plot);
        mon_draw_title(cr, &plot, "12V voltage");
        mon_draw_legend(cr, &plot, labels, cols, 2);

        return true;
    }

    if(kind == MON_PLOT_V5)
    {
        static const char *const labels[] = { "5V", "Spec" };
        const double cols[] = { MON_COL_5V_R, MON_COL_5V_G, MON_COL_5V_B, 0.85, 0.25, 0.25 };

        mon_plot_t plot;
        mon_setup_plot(&plot, width, height, 4.4, 5.6, t_min, t_max);

        mon_draw_zone_bands(cr, &plot, ASCLEPIUS_V5_MIN_MV / 1000.0, ASCLEPIUS_V5_MAX_MV / 1000.0);
        mon_draw_grid(cr, &plot, "V");
        mon_draw_dashed_limit(cr, &plot, ASCLEPIUS_V5_MIN_MV / 1000.0);
        mon_draw_dashed_limit(cr, &plot, ASCLEPIUS_V5_MAX_MV / 1000.0);
        mon_draw_markers(cr, &plot, mon_bad_v5, mon_mark_value_v5);
        mon_draw_series(cr, &plot, mon_series_v5, MON_COL_5V_R, MON_COL_5V_G, MON_COL_5V_B);
        if(mon_hover_kind_ccc == MON_HOVER_V5)
        {
            mon_draw_hover(cr, &plot, mon_series_v5, MON_COL_5V_R, MON_COL_5V_G, MON_COL_5V_B, "5V", "V");
        }
        mon_draw_border(cr, &plot);
        mon_draw_title(cr, &plot, "5V voltage");
        mon_draw_legend(cr, &plot, labels, cols, 2);

        return true;
    }

    // Combined current plot: three stacked series on one shared scale.
    {
        static const char *const labels[] = { "12V", "5V", "USB" };
        const double cols[] = {
            MON_COL_12V_R, MON_COL_12V_G, MON_COL_12V_B,
            MON_COL_5V_R, MON_COL_5V_G, MON_COL_5V_B,
            MON_COL_USB_R, MON_COL_USB_G, MON_COL_USB_B
        };

        // Auto-scale to the visible data with headroom and a sane floor. This
        // copy is the one mon_setup_plot repeats, so scan it directly.
        int n = mon_copy_range(&asclepius_monitor, t_min, t_max, mon_points_ccc, MON_MAX_POINTS);
        double peak_seen = 0.0;
        for(int i = 0; i < n; i++)
        {
            double peak = mon_points_ccc[i].a12;
            if(mon_points_ccc[i].a5 > peak)
            {
                peak = mon_points_ccc[i].a5;
            }
            if(mon_points_ccc[i].ausb > peak)
            {
                peak = mon_points_ccc[i].ausb;
            }
            if(peak > peak_seen)
            {
                peak_seen = peak;
            }
        }
        double ymax = peak_seen > 0.0 ? peak_seen * 1.08 + 0.02 : 0.1;

        mon_plot_t plot;
        mon_setup_plot(&plot, width, height, 0.0, ymax, t_min, t_max);

        mon_draw_grid(cr, &plot, "A");
        mon_draw_markers(cr, &plot, mon_bad_any_current, mon_mark_value_a12);
        mon_draw_series(cr, &plot, mon_series_a12, MON_COL_12V_R, MON_COL_12V_G, MON_COL_12V_B);
        mon_draw_series(cr, &plot, mon_series_a5, MON_COL_5V_R, MON_COL_5V_G, MON_COL_5V_B);
        mon_draw_series(cr, &plot, mon_series_ausb, MON_COL_USB_R, MON_COL_USB_G, MON_COL_USB_B);

        if(mon_hover_kind_ccc == MON_HOVER_CURRENT)
        {
            // All three rails share this plot, so show a read-out for each.
            mon_draw_hover(cr, &plot, mon_series_a12, MON_COL_12V_R, MON_COL_12V_G, MON_COL_12V_B, "12V", "A");
            mon_draw_hover(cr, &plot, mon_series_a5, MON_COL_5V_R, MON_COL_5V_G, MON_COL_5V_B, "5V", "A");
            mon_draw_hover(cr, &plot, mon_series_ausb, MON_COL_USB_R, MON_COL_USB_G, MON_COL_USB_B, "USB", "A");
        }

        mon_draw_border(cr, &plot);
        mon_draw_title(cr, &plot, "current draw");
        mon_draw_legend(cr, &plot, labels, cols, 3);

        return true;
    }
}

// GTK draw callbacks: thin wrappers that supply the allocated size.
static gboolean on_draw_v12(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    (void)data;
    mon_draw_plot(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget), MON_PLOT_V12);
    return FALSE;
}

static gboolean on_draw_v5(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    (void)data;
    mon_draw_plot(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget), MON_PLOT_V5);
    return FALSE;
}

static gboolean on_draw_current(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    (void)data;
    mon_draw_plot(cr, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget), MON_PLOT_CURRENT);
    return FALSE;
}

// ------------------------------------------------------- power + statistics

static uint8_t mon_button_channel(GtkWidget *button)
{
  const char *name = gtk_buildable_get_name(GTK_BUILDABLE(button));

  if(g_strcmp0(name, "asclepius_mon_power_molex") == 0)
  {
    return ASCLEPIUS_MOLEX;
  }
  if(g_strcmp0(name, "asclepius_mon_power_12v") == 0)
  {
    return ASCLEPIUS_MAIN12V;
  }
  if(g_strcmp0(name, "asclepius_mon_power_5v") == 0)
  {
    return ASCLEPIUS_MAIN5V;
  }
  if(g_strcmp0(name, "asclepius_mon_power_usb") == 0)
  {
    return ASCLEPIUS_USB5V;
  }

  return ASCLEPIUS_ALL;
}

// MOLEX is the 12V and 5V relays together; All is every rail.
static bool mon_channel_enabled(uint8_t channel, const struct asclepius_status_t *st)
{
  switch(channel)
  {
    case ASCLEPIUS_MAIN12V:
      return st->main12v_enabled;
    case ASCLEPIUS_MAIN5V:
      return st->main5v_enabled;
    case ASCLEPIUS_USB5V:
      return st->usb5v_enabled;
    case ASCLEPIUS_MOLEX:
      return st->main12v_enabled && st->main5v_enabled;
    case ASCLEPIUS_ALL:
      return st->main12v_enabled && st->main5v_enabled && st->usb5v_enabled;
    default:
      return false;
  }
}

// Rails a button command should clear a latched software OCP trip for.
static void mon_clear_ocp_for_channel(uint8_t channel)
{
  switch(channel)
  {
    case ASCLEPIUS_MAIN12V:
      asclepius_ocp_clear_channel(ASCLEPIUS_MAIN12V);
      break;
    case ASCLEPIUS_MAIN5V:
      asclepius_ocp_clear_channel(ASCLEPIUS_MAIN5V);
      break;
    case ASCLEPIUS_USB5V:
      asclepius_ocp_clear_channel(ASCLEPIUS_USB5V);
      break;
    case ASCLEPIUS_MOLEX:
      asclepius_ocp_clear_channel(ASCLEPIUS_MAIN12V);
      asclepius_ocp_clear_channel(ASCLEPIUS_MAIN5V);
      break;
    case ASCLEPIUS_ALL:
      asclepius_ocp_reset();
      break;
    default:
      break;
  }
}

void asclepius_mon_power_clicked_ccc(GtkWidget *button)
{
  if(!asclepius_get_connection_status())
  {
    printf("Asclepius: not connected, ignoring power command\n");
    return;
  }

  uint8_t channel = mon_button_channel(button);

  // Decide the new state from the board's own enabled flags rather than a
  // local toggle, so a stale GUI can never command the wrong direction.
  struct asclepius_status_t st;
  asclepius_lock();
  st = asclepius_status;
  asclepius_unlock();

  bool turn_on = !mon_channel_enabled(channel, &st);
  int result = turn_on ? asclepius_enable_channel(channel) : asclepius_disable_channel(channel);

  if(result != 0)
  {
    printf("Asclepius: power command failed for %s\n", channel == ASCLEPIUS_MAIN12V ? "12V" : channel == ASCLEPIUS_MAIN5V ? "5V" : channel == ASCLEPIUS_USB5V ? "USB" : channel == ASCLEPIUS_MOLEX ? "MOLEX" : "ALL");
  }
  else
  {
    // A manual power command takes the rail out of any latched soft OCP trip
    mon_clear_ocp_for_channel(channel);
    printf("Asclepius: power %s %s\n", turn_on ? "on" : "off", channel == ASCLEPIUS_MAIN12V ? "12V" : channel == ASCLEPIUS_MAIN5V ? "5V" : channel == ASCLEPIUS_USB5V ? "USB" : channel == ASCLEPIUS_MOLEX ? "MOLEX" : "ALL");
  }

  update_asclepius_monitor_ccc();
}

// One statistics row: live value plus rolling min/avg/max and fault flags.
static void mon_format_stats(char *out, size_t out_size, const char *name, double value, const mon_stats_t *st, bool show_volts, bool overcurrent, bool out_of_spec, bool soft_trip, bool enabled)
{
  if(st == NULL || st->acount == 0)
  {
    snprintf(out, out_size, "%s: no samples", name);
    return;
  }

  char flags[128];
  flags[0] = '\0';

  if(soft_trip)
  {
    strncat(flags, "  OVER-CURRENT (SOFT)", sizeof(flags) - strlen(flags) - 1);
  }
  else if(overcurrent)
  {
    strncat(flags, "  OVER-CURRENT", sizeof(flags) - strlen(flags) - 1);
  }

  if(out_of_spec)
  {
    strncat(flags, "  OUT OF SPEC", sizeof(flags) - strlen(flags) - 1);
  }

  if(!enabled)
  {
    strncat(flags, "  (off)", sizeof(flags) - strlen(flags) - 1);
  }

  if(show_volts)
  {
    snprintf(out, out_size, "%s  %.3f V   min %.3f   avg %.3f   max %.3f%s", name, value, st->vmin, st->vsum / st->vcount, st->vmax, flags);
  }
  else
  {
    // The USB rail has no reported voltage, so show current statistics only.
    snprintf(out, out_size, "%s  %.3f A   min %.3f   avg %.3f   max %.3f%s", name, value, st->amin, st->asum / st->acount, st->amax, flags);
  }
}

void update_asclepius_monitor_ccc(void)
{
  if(!asclepius_monitor_open || asclepius_monitor_dialog_ccc == NULL)
  {
    return;
  }

  struct asclepius_status_t st;
  asclepius_lock();
  st = asclepius_status;
  asclepius_unlock();

  bool connected = asclepius_get_connection_status();

  struct asclepius_ocp_rail_t ocp12v, ocp5v, ocpusb;
  asclepius_ocp_get_rail_state(ASCLEPIUS_MAIN12V, &ocp12v);
  asclepius_ocp_get_rail_state(ASCLEPIUS_MAIN5V, &ocp5v);
  asclepius_ocp_get_rail_state(ASCLEPIUS_USB5V, &ocpusb);

  mon_stats_t s12, s5, susb;
  mon_copy_stats(&asclepius_monitor, &s12, &s5, &susb);

  // Power buttons reflect the board's actual enabled flags.
  static const uint8_t channels[5] = { ASCLEPIUS_MOLEX, ASCLEPIUS_MAIN12V, ASCLEPIUS_MAIN5V, ASCLEPIUS_USB5V, ASCLEPIUS_ALL };
  for(int i = 0; i < 5; i++)
  {
    GtkWidget *button = asclepius_mon_power_buttons_ccc[i];
    if(button == NULL)
    {
      continue;
    }

    bool on = connected && mon_channel_enabled(channels[i], &st);
    const char *style = on ? "asclepius-rail-on" : "asclepius-rail-off";

    if(gtk_widget_get_sensitive(button) != connected)
    {
      gtk_widget_set_sensitive(button, connected);
    }

    GtkStyleContext *context = gtk_widget_get_style_context(button);
    if(!gtk_style_context_has_class(context, style))
    {
      gtk_style_context_remove_class(context, "asclepius-rail-on");
      gtk_style_context_remove_class(context, "asclepius-rail-off");
      gtk_style_context_add_class(context, style);
    }
  }

  if(asclepius_mon_voltage_stats_label_ccc != NULL)
  {
    char row12[256];
    char row5[256];
    char text[600];

    mon_format_stats(row12, sizeof(row12), "12V", st.main12v_voltage / 1000.0, &s12, true, st.main12v_ocp, s12.vcount > 0 && (s12.vmin < ASCLEPIUS_V12_MIN_MV / 1000.0 || s12.vmax > ASCLEPIUS_V12_MAX_MV / 1000.0), ocp12v.tripped, st.main12v_enabled);
    mon_format_stats(row5, sizeof(row5), "5V ", st.main5v_voltage / 1000.0, &s5, true, st.main5v_ocp, s5.vcount > 0 && (s5.vmin < ASCLEPIUS_V5_MIN_MV / 1000.0 || s5.vmax > ASCLEPIUS_V5_MAX_MV / 1000.0), ocp5v.tripped, st.main5v_enabled);

    snprintf(text, sizeof(text), "%s\n%s", row12, row5);
    gtk_label_set_text(GTK_LABEL(asclepius_mon_voltage_stats_label_ccc), text);
  }

  if(asclepius_mon_current_stats_label_ccc != NULL)
  {
    char row12[256];
    char row5[256];
    char rowusb[256];
    char text[850];

    mon_format_stats(row12, sizeof(row12), "12V", st.main12v_current / 1000.0, &s12, false, st.main12v_ocp, false, ocp12v.tripped, st.main12v_enabled);
    mon_format_stats(row5, sizeof(row5), "5V ", st.main5v_current / 1000.0, &s5, false, st.main5v_ocp, false, ocp5v.tripped, st.main5v_enabled);
    mon_format_stats(rowusb, sizeof(rowusb), "USB", st.usb5v_current / 1000.0, &susb, false, st.usb5v_ocp, false, ocpusb.tripped, st.usb5v_enabled);

    snprintf(text, sizeof(text), "%s\n%s\n%s", row12, row5, rowusb);
    gtk_label_set_text(GTK_LABEL(asclepius_mon_current_stats_label_ccc), text);
  }

  // Status line: connection state, sample count, and any latched soft trip.
  if(asclepius_mon_status_label_ccc != NULL)
  {
    char text[400];

    if(!connected)
    {
      snprintf(text, sizeof(text), "%s", _("Disconnected"));
    }
    else if(asclepius_soft_ocp_status[0] != '\0')
    {
      snprintf(text, sizeof(text), "%s", _("Connected - soft OCP"));
    }
    else
    {
      snprintf(text, sizeof(text), _("Connected - %u samples"), mon_sample_count(&asclepius_monitor));
    }

    if(asclepius_soft_ocp_status[0] != '\0')
    {
      // The trip detail is assembled by the poller and is not a catalog entry.
      size_t used = strlen(text);
      snprintf(text + used, sizeof(text) - used, " - %s", asclepius_soft_ocp_status);
    }

    gtk_label_set_text(GTK_LABEL(asclepius_mon_status_label_ccc), text);
  }

  if(asclepius_mon_v12_drawing_ccc != NULL)
  {
    gtk_widget_queue_draw(asclepius_mon_v12_drawing_ccc);
  }
  if(asclepius_mon_v5_drawing_ccc != NULL)
  {
    gtk_widget_queue_draw(asclepius_mon_v5_drawing_ccc);
  }
  if(asclepius_mon_current_drawing_ccc != NULL)
  {
    gtk_widget_queue_draw(asclepius_mon_current_drawing_ccc);
  }
}

static gboolean mon_refresh_timer(gpointer data)
{
  (void)data;
  update_asclepius_monitor_ccc();

  return G_SOURCE_CONTINUE;
}

// Maps a hover over one of the drawing areas onto a timestamp, so the tooltip
// can find the nearest sample.
static gboolean on_plot_motion(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
  (void)data;

  if(mon_points_ccc == NULL)
  {
    return FALSE;
  }

  int width = gtk_widget_get_allocated_width(widget);
  int height = gtk_widget_get_allocated_height(widget);
  if(width <= MON_PLOT_LEFT + MON_PLOT_RIGHT || height <= MON_PLOT_TOP + MON_PLOT_BOTTOM)
  {
    return FALSE;
  }

  uint32_t t_min, t_max;
  mon_resolve_window(&t_min, &t_max);

  double plot_w = width - MON_PLOT_LEFT - MON_PLOT_RIGHT;
  if(plot_w <= 0 || t_max <= t_min)
  {
    return FALSE;
  }

  double frac = (event->x - MON_PLOT_LEFT) / plot_w;
  if(frac < 0)
  {
    frac = 0;
  }
  if(frac > 1)
  {
    frac = 1;
  }

  const char *name = gtk_buildable_get_name(GTK_BUILDABLE(widget));
  if(g_strcmp0(name, "asclepius_mon_v12_drawing") == 0)
  {
    mon_hover_kind_ccc = MON_HOVER_V12;
  }
  else if(g_strcmp0(name, "asclepius_mon_v5_drawing") == 0)
  {
    mon_hover_kind_ccc = MON_HOVER_V5;
  }
  else
  {
    mon_hover_kind_ccc = MON_HOVER_CURRENT;
  }

  mon_hover_active_ccc = true;
  mon_hover_x_ccc = event->x;
  mon_hover_t_ms_ccc = t_min + (uint32_t)(frac * (double)(t_max - t_min));

  gtk_widget_queue_draw(widget);

  return FALSE;
}

static gboolean on_plot_leave(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  (void)event;
  (void)data;

  mon_hover_active_ccc = false;
  mon_hover_kind_ccc = MON_HOVER_NONE;
  gtk_widget_queue_draw(widget);

  return FALSE;
}

void on_asclepius_monitor_window_changed_ccc(GtkComboBox *combo, gpointer data)
{
  (void)data;

  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
  if(active >= 0 && active < (int)MON_NUM_WINDOWS)
  {
    asclepius_monitor_window_index_ccc = (unsigned int)active;
  }

  update_asclepius_monitor_ccc();
}

// The window manager's close button sends delete-event. GTK destroys the
// window on its own when this returns FALSE, so the timer and cached widget
// pointers are cleared here but the destroy is left to GTK.
//
// Returning TRUE would veto that destroy: the window would stay on screen
// while asclepius_monitor_open went false, so the refresh timer would bail
// out on every tick and only a mouse motion would repaint anything.
static gboolean on_asclepius_monitor_delete_ccc(GtkWidget *widget, GdkEvent *event, gpointer data)
{
  (void)widget;
  (void)event;
  (void)data;

  asclepius_monitor_stop_refresh_ccc();

  return FALSE;
}

// Drops the refresh timer and forgets every cached widget pointer. Safe to
// call more than once: GTK destroys the dialog asynchronously, so both the
// delete-event path and close_asclepius_monitor_ccc() can arrive for one
// close.
void asclepius_monitor_stop_refresh_ccc(void)
{
  if(asclepius_monitor_timer_id != 0)
  {
    g_source_remove(asclepius_monitor_timer_id);
    asclepius_monitor_timer_id = 0;
  }

  asclepius_monitor_open = false;
  asclepius_monitor_dialog_ccc = NULL;

  asclepius_mon_window_combobox_ccc = NULL;
  asclepius_mon_status_label_ccc = NULL;
  asclepius_mon_voltage_stats_label_ccc = NULL;
  asclepius_mon_current_stats_label_ccc = NULL;
  asclepius_mon_v12_drawing_ccc = NULL;
  asclepius_mon_v5_drawing_ccc = NULL;
  asclepius_mon_current_drawing_ccc = NULL;

  for(int i = 0; i < 5; i++)
  {
    asclepius_mon_power_buttons_ccc[i] = NULL;
  }

  mon_hover_active_ccc = false;
  mon_hover_kind_ccc = MON_HOVER_NONE;
}

void open_asclepius_monitor_dialog_ccc(void)
{
  if(asclepius_monitor_open && asclepius_monitor_dialog_ccc != NULL)
  {
    gtk_window_present(GTK_WINDOW(asclepius_monitor_dialog_ccc));
    return;
  }

  GtkBuilder *builder = gtk_builder_new();
  gtk_builder_add_from_string(builder, (const gchar *)opensuperclone_glade, opensuperclone_glade_len, NULL);
  gtk_builder_connect_signals(builder, NULL);

  GtkWidget *dialog = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_monitor_dialog"));
  if(dialog == NULL)
  {
    g_object_unref(builder);
    return;
  }

  GtkWidget *power_molex = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_power_molex"));
  GtkWidget *power_12v = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_power_12v"));
  GtkWidget *power_5v = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_power_5v"));
  GtkWidget *power_usb = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_power_usb"));
  GtkWidget *power_all = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_power_all"));

  asclepius_mon_power_buttons_ccc[0] = power_molex;
  asclepius_mon_power_buttons_ccc[1] = power_12v;
  asclepius_mon_power_buttons_ccc[2] = power_5v;
  asclepius_mon_power_buttons_ccc[3] = power_usb;
  asclepius_mon_power_buttons_ccc[4] = power_all;

  GtkWidget *window_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_window_label"));
  GtkWidget *window_combo = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_window_combobox"));
  GtkWidget *status_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_status_label"));
  GtkWidget *voltage_stats = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_voltage_stats_label"));
  GtkWidget *current_stats = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_current_stats_label"));

  GtkWidget *v12 = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_v12_drawing"));
  GtkWidget *v5 = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_v5_drawing"));
  GtkWidget *current = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_current_drawing"));

  // The captions are marked translatable in the glade, which makes GtkBuilder
  // translate them at parse time, but only if their msgids reached the
  // catalog. xgettext never reads the glade, so every string that is not also
  // wrapped in _() here stays untranslated.
  GtkWidget *power_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_power_label"));
  GtkWidget *voltage_label = GTK_WIDGET(gtk_builder_get_object(builder, "asclepius_mon_voltage_label"));

  gtk_window_set_title(GTK_WINDOW(dialog), _("Asclepius Monitor"));
  if(power_label != NULL)
  {
    gtk_label_set_text(GTK_LABEL(power_label), _("Power"));
  }
  if(voltage_label != NULL)
  {
    gtk_label_set_text(GTK_LABEL(voltage_label), _("Voltage"));
  }
  // Shown until the first refresh delivers real statistics.
  if(voltage_stats != NULL)
  {
    gtk_label_set_text(GTK_LABEL(voltage_stats), _("No samples"));
  }
  if(current_stats != NULL)
  {
    gtk_label_set_text(GTK_LABEL(current_stats), _("No samples"));
  }

  if(v12 != NULL)
  {
    g_signal_connect(v12, "draw", G_CALLBACK(on_draw_v12), NULL);
    g_signal_connect(v12, "motion-notify-event", G_CALLBACK(on_plot_motion), NULL);
    g_signal_connect(v12, "leave-notify-event", G_CALLBACK(on_plot_leave), NULL);
    gtk_widget_add_events(v12, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  }
  if(v5 != NULL)
  {
    g_signal_connect(v5, "draw", G_CALLBACK(on_draw_v5), NULL);
    g_signal_connect(v5, "motion-notify-event", G_CALLBACK(on_plot_motion), NULL);
    g_signal_connect(v5, "leave-notify-event", G_CALLBACK(on_plot_leave), NULL);
    gtk_widget_add_events(v5, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  }
  if(current != NULL)
  {
    g_signal_connect(current, "draw", G_CALLBACK(on_draw_current), NULL);
    g_signal_connect(current, "motion-notify-event", G_CALLBACK(on_plot_motion), NULL);
    g_signal_connect(current, "leave-notify-event", G_CALLBACK(on_plot_leave), NULL);
    gtk_widget_add_events(current, GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
  }

  if(window_label != NULL)
  {
    gtk_label_set_text(GTK_LABEL(window_label), _("Window"));
  }
  if(window_combo != NULL)
  {
    char names[MON_NUM_WINDOWS][32];
    mon_window_names(names);

    for(size_t i = 0; i < MON_NUM_WINDOWS; i++)
    {
      gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(window_combo), names[i]);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(window_combo), (int)asclepius_monitor_window_index_ccc);
    g_signal_connect(window_combo, "changed", G_CALLBACK(on_asclepius_monitor_window_changed_ccc), NULL);
  }

  // Allocate the sample scratch buffer once for the life of the dialog.
  if(mon_points_ccc == NULL)
  {
    mon_points_ccc = calloc(MON_MAX_POINTS, sizeof(mon_sample_t));
    if(mon_points_ccc == NULL)
    {
      printf("Asclepius monitor: out of memory for the plot sample buffer\n");
      g_object_unref(builder);
      return;
    }
  }

  asclepius_monitor_dialog_ccc = dialog;
  asclepius_mon_window_combobox_ccc = window_combo;
  asclepius_mon_status_label_ccc = status_label;
  asclepius_mon_voltage_stats_label_ccc = voltage_stats;
  asclepius_mon_current_stats_label_ccc = current_stats;
  asclepius_mon_v12_drawing_ccc = v12;
  asclepius_mon_v5_drawing_ccc = v5;
  asclepius_mon_current_drawing_ccc = current;

  // Rail button styling follows the status panel's on/off vocabulary.
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
                                  "button.asclepius-rail-on { background-image: none; background-color: #2e7d32; color: #ffffff; }\n"
                                  "button.asclepius-rail-off { background-image: none; background-color: #4a4a4a; color: #dddddd; }\n"
                                  "button.asclepius-rail-on:disabled, button.asclepius-rail-off:disabled { color: #888888; }\n",
                                  -1,
                                  NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  // "delete-event" is the window manager's X, "response" is the Close button,
  // and "destroy" catches both plus close_asclepius_monitor_ccc().
  g_signal_connect(dialog, "delete-event", G_CALLBACK(on_asclepius_monitor_delete_ccc), NULL);
  g_signal_connect(dialog, "response", G_CALLBACK(on_asclepius_monitor_response_ccc), NULL);
  g_signal_connect(dialog, "destroy", G_CALLBACK(on_asclepius_monitor_destroy_ccc), NULL);

  asclepius_monitor_open = true;
  asclepius_monitor_timer_id = g_timeout_add(100, mon_refresh_timer, NULL);

  gtk_widget_show_all(dialog);

  update_asclepius_monitor_ccc();
}

// The in-window Close button. close_asclepius_monitor_ccc() destroys the
  // dialog, which runs the "destroy" handler for the shared cleanup.
static void on_asclepius_monitor_response_ccc(GtkDialog *dialog, int response, gpointer data)
{
  (void)dialog;
  (void)data;

  if(response != GTK_RESPONSE_CLOSE)
  {
    return;
  }

  close_asclepius_monitor_ccc();
}

void close_asclepius_monitor_ccc(void)
{
  // GTKDialog's own "destroy" signal covers the window manager's close
  // button, so both routes converge on the same cleanup.
  GtkWidget *dialog = asclepius_monitor_dialog_ccc;

  asclepius_monitor_stop_refresh_ccc();

  if(dialog != NULL)
  {
    gtk_widget_destroy(dialog);
  }
}

// Fires whenever the dialog goes away, whichever route closed it: the window
// manager's X, the Close button, or close_asclepius_monitor_ccc().
static void on_asclepius_monitor_destroy_ccc(GtkWidget *widget, gpointer data)
{
  (void)widget;
  (void)data;

  asclepius_monitor_stop_refresh_ccc();
}
