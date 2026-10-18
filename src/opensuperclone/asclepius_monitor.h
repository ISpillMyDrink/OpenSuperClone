// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#ifndef OSC_ASCLEPIUS_MONITOR_H
#define OSC_ASCLEPIUS_MONITOR_H

#include <stdbool.h>
#include <gtk/gtk.h>

// True while the monitor window is on screen. Guards the per-rail refresh
// timer so it only runs while the user can actually see the plots.
extern bool asclepius_monitor_open;

// Index into the selectable plot time windows (10 s .. all time).
extern unsigned int asclepius_monitor_window_index_ccc;

// Opens the monitor window, or brings the existing one to the front.
void open_asclepius_monitor_dialog_ccc(void);

// Refreshes plots, power buttons and statistics from the current poller data.
void update_asclepius_monitor_ccc(void);

// Stops the refresh timer and forgets every cached widget pointer, without
// destroying the window. Used by the close handlers; safe to call twice.
void asclepius_monitor_stop_refresh_ccc(void);

// Stops the refresh timer and destroys the monitor window.
void close_asclepius_monitor_ccc(void);

#endif