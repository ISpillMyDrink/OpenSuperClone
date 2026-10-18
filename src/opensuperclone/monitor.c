// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "monitor.h"

#include <string.h>

// ATX spec voltage limits (in volts), +/-5%
#define V12_MIN_V 11.4f
#define V12_MAX_V 12.6f
#define V5_MIN_V 4.75f
#define V5_MAX_V 5.25f

mon_t asclepius_monitor;

void mon_init(mon_t *m)
{
    memset(m, 0, sizeof(*m));
}

void mon_reset(mon_t *m)
{
    memset(m, 0, sizeof(*m));
}

const mon_sample_t *mon_sample(const mon_history_t *h, int i)
{
    if(h->len == 0)
    {
        return NULL;
    }

    if(i < 0)
    {
        i = 0;
    }

    if(i >= h->len)
    {
        i = h->len - 1;
    }

    return &h->buf[(h->head - h->len + i + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];
}

static void stats_update(mon_stats_t *s, bool has_v, float v, float a)
{
    if(has_v)
    {
        if(s->vcount == 0)
        {
            s->vmin = s->vmax = v;
        }
        else
        {
            if(v < s->vmin)
            {
                s->vmin = v;
            }
            if(v > s->vmax)
            {
                s->vmax = v;
            }
        }

        s->vsum += v;
        s->vcount++;
    }

    if(s->acount == 0)
    {
        s->amin = s->amax = a;
    }
    else
    {
        if(a < s->amin)
        {
            s->amin = a;
        }
        if(a > s->amax)
        {
            s->amax = a;
        }
    }

    s->asum += a;
    s->acount++;
}

void mon_push(mon_t *m, const struct asclepius_status_t *st, uint32_t t_ms)
{
    mon_sample_t *s = &m->hist.buf[m->hist.head];

    s->t_ms = t_ms;
    s->v12 = st->main12v_voltage / 1000.0f;
    s->v5 = st->main5v_voltage / 1000.0f;
    s->a12 = st->main12v_current / 1000.0f;
    s->a5 = st->main5v_current / 1000.0f;
    s->ausb = st->usb5v_current / 1000.0f;
    s->ocp12 = st->main12v_ocp ? 1 : 0;
    s->ocp5 = st->main5v_ocp ? 1 : 0;
    s->ocpusb = st->usb5v_ocp ? 1 : 0;
    s->oos12 = (st->main12v_enabled && (s->v12 < V12_MIN_V || s->v12 > V12_MAX_V)) ? 1 : 0;
    s->oos5 = (st->main5v_enabled && (s->v5 < V5_MIN_V || s->v5 > V5_MAX_V)) ? 1 : 0;

    m->hist.head = (m->hist.head + 1) % MON_HISTORY_MAX;
    if(m->hist.len < MON_HISTORY_MAX)
    {
        m->hist.len++;
    }
    m->sample_count++;

    // The USB rail is derived from the 5V rail, so its voltage
    // contribution mirrors the 5V enabled flag.
    stats_update(&m->st12, st->main12v_enabled, s->v12, s->a12);
    stats_update(&m->st5, st->main5v_enabled, s->v5, s->a5);
    stats_update(&m->stusb, st->main5v_enabled, s->v5, s->ausb);
}
