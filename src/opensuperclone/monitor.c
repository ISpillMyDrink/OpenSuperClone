// Copyright (C) OpenSuperClone contributors.
// You may use/distribute/modify this freely, under the terms of
// the GNU General Public License version 2 or later version.
// This software is distributed WITHOUT ANY WARRANTY.

#include "monitor.h"

#include <pthread.h>
#include <string.h>

// ATX spec voltage limits (in volts), +/-5%
#define V12_MIN_V 11.4f
#define V12_MAX_V 12.6f
#define V5_MIN_V 4.75f
#define V5_MAX_V 5.25f

// The poller thread appends samples while the GUI thread reads them, so every
// access is serialized here. Callers never take this lock themselves: the
// copy helpers below hold it only for the duration of a memcpy, so drawing a
// plot never blocks a poll.
static pthread_mutex_t mon_mutex = PTHREAD_MUTEX_INITIALIZER;

mon_t asclepius_monitor;

void mon_init(mon_t *m)
{
    memset(m, 0, sizeof(*m));
}

void mon_reset(mon_t *m)
{
    pthread_mutex_lock(&mon_mutex);
    memset(m, 0, sizeof(*m));
    pthread_mutex_unlock(&mon_mutex);
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
    mon_sample_t *s;

    pthread_mutex_lock(&mon_mutex);

    s = &m->hist.buf[m->hist.head];

    s->t_ms = t_ms;
    s->v12 = st->main12v_voltage / 1000.0f;
    s->v5 = st->main5v_voltage / 1000.0f;
    s->a12 = st->main12v_current / 1000.0f;
    s->a5 = st->main5v_current / 1000.0f;
    s->ausb = st->usb5v_current / 1000.0f;
    s->ocp12 = st->main12v_ocp ? 1 : 0;
    s->ocp5 = st->main5v_ocp ? 1 : 0;
    s->ocpusb = st->usb5v_ocp ? 1 : 0;
    s->enabled12 = st->main12v_enabled ? 1 : 0;
    s->enabled5 = st->main5v_enabled ? 1 : 0;
    s->enabledusb = st->usb5v_enabled ? 1 : 0;
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

    pthread_mutex_unlock(&mon_mutex);
}

uint32_t mon_sample_count(const mon_t *m)
{
    uint32_t count;

    pthread_mutex_lock(&mon_mutex);
    count = m->sample_count;
    pthread_mutex_unlock(&mon_mutex);

    return count;
}

int mon_history_len(const mon_t *m)
{
    int len;

    pthread_mutex_lock(&mon_mutex);
    len = m->hist.len;
    pthread_mutex_unlock(&mon_mutex);

    return len;
}

void mon_copy_stats(const mon_t *m, mon_stats_t *st12, mon_stats_t *st5, mon_stats_t *stusb)
{
    pthread_mutex_lock(&mon_mutex);

    if(st12 != NULL)
    {
        *st12 = m->st12;
    }
    if(st5 != NULL)
    {
        *st5 = m->st5;
    }
    if(stusb != NULL)
    {
        *stusb = m->stusb;
    }

    pthread_mutex_unlock(&mon_mutex);
}

void mon_copy_stats_range(const mon_t *m, uint32_t t_min_ms, uint32_t t_max_ms,
                          mon_stats_t *st12, mon_stats_t *st5, mon_stats_t *stusb)
{
    int i;

    if(st12 != NULL)
    {
        memset(st12, 0, sizeof(*st12));
    }
    if(st5 != NULL)
    {
        memset(st5, 0, sizeof(*st5));
    }
    if(stusb != NULL)
    {
        memset(stusb, 0, sizeof(*stusb));
    }

    pthread_mutex_lock(&mon_mutex);

    for(i = 0; i < m->hist.len; i++)
    {
        const mon_sample_t *sample = &m->hist.buf[(m->hist.head - m->hist.len + i + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];

        if(sample->t_ms < t_min_ms || sample->t_ms > t_max_ms)
        {
            continue;
        }

        if(st12 != NULL)
        {
            stats_update(st12, sample->enabled12 != 0, sample->v12, sample->a12);
        }
        if(st5 != NULL)
        {
            stats_update(st5, sample->enabled5 != 0, sample->v5, sample->a5);
        }
        if(stusb != NULL)
        {
            stats_update(stusb, sample->enabledusb != 0, sample->v5, sample->ausb);
        }
    }

    pthread_mutex_unlock(&mon_mutex);
}

// Copies samples spanning [t_min_ms, t_max_ms] into out, oldest first.
//
// When the range holds more samples than max_out, this decimates evenly
// across the whole range rather than returning the oldest max_out: a plot
// must cover its entire window, not just its start. The returned samples
// always include the first and last sample in range.
int mon_copy_range(const mon_t *m, uint32_t t_min_ms, uint32_t t_max_ms, mon_sample_t *out, int max_out)
{
    int first = -1;
    int last = -1;
    int in_range = 0;
    int i;
    int count = 0;
    int step = 1;

    if(out == NULL || max_out <= 0)
    {
        return 0;
    }

    pthread_mutex_lock(&mon_mutex);

    for(i = 0; i < m->hist.len; i++)
    {
        uint32_t t = m->hist.buf[(m->hist.head - m->hist.len + i + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX].t_ms;

        if(t >= t_min_ms && t <= t_max_ms)
        {
            if(first < 0)
            {
                first = i;
            }
            last = i;
            in_range++;
        }
    }

    if(in_range > 0 && in_range <= max_out)
    {
        for(i = first; i <= last; i++)
        {
            out[count++] = m->hist.buf[(m->hist.head - m->hist.len + i + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];
        }
    }
    else if(in_range > max_out)
    {
        // Decimate evenly, reserving the final slot for the newest sample so
        // the trace reaches the right-hand edge of the plot.
        int span = last - first;
        step = span / (max_out - 1);

        if(step < 1)
        {
            step = 1;
        }

        for(i = first; i < last && count < max_out - 1; i += step)
        {
            out[count++] = m->hist.buf[(m->hist.head - m->hist.len + i + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];
        }

        out[count++] = m->hist.buf[(m->hist.head - m->hist.len + last + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];
    }

    pthread_mutex_unlock(&mon_mutex);

    return count;
}

// Monotonic timestamp of the oldest and newest retained samples. Returns
// false when the buffer is empty.
bool mon_time_span(const mon_t *m, uint32_t *oldest_ms, uint32_t *newest_ms)
{
    const mon_sample_t *first;
    const mon_sample_t *last;
    bool ok = false;

    pthread_mutex_lock(&mon_mutex);

    if(m->hist.len > 0)
    {
        first = &m->hist.buf[(m->hist.head - m->hist.len + 0 + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];
        last = &m->hist.buf[(m->hist.head - 1 + 2 * MON_HISTORY_MAX) % MON_HISTORY_MAX];

        if(oldest_ms != NULL)
        {
            *oldest_ms = first->t_ms;
        }
        if(newest_ms != NULL)
        {
            *newest_ms = last->t_ms;
        }

        ok = true;
    }

    pthread_mutex_unlock(&mon_mutex);

    return ok;
}
