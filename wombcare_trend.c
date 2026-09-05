#include "wombcare_trend.h"

#include <string.h>

/*--------------------------------------------------------------------
 * Retained beats.
 *
 * Times are held in a floating frame that is REBASED to the oldest
 * retained beat on every begin_window(). Absolute session time would
 * grow without bound and a float loses millisecond resolution past
 * ~4.6 hours of monitoring -- which is well inside an overnight
 * session. Rebasing keeps every stored time under one span plus one
 * window, where a float is exact to far better than a sample period.
 *-------------------------------------------------------------------*/
static float    g_fhr[TREND_MAX_BEATS];
static float    g_t_ms[TREND_MAX_BEATS];
static uint16_t g_n;

/*
 * One entry per ACCEPTED window still inside the span. This is what
 * makes the rate denominator honest: counts are divided by the summed
 * length of the windows that actually contributed, so a device that
 * rejects half its windows still reports the right events-per-minute.
 */
static float   g_win_t_ms[TREND_MAX_WINDOWS];
static float   g_win_ms[TREND_MAX_WINDOWS];
static float   g_win_kicks[TREND_MAX_WINDOWS];
static uint8_t g_win_n;

/* Start of the window most recently opened, in the same frame. */
static float g_cursor_ms;
static float g_window_ms;
static bool  g_open;

/*--------------------------------------------------------------------
 * Drop the oldest `k` beats.
 *-------------------------------------------------------------------*/
static void drop_oldest_beats(uint16_t k)
{
    if (k == 0U)
    {
        return;
    }

    if (k >= g_n)
    {
        g_n = 0U;
        return;
    }

    memmove(g_fhr,  &g_fhr[k],  (size_t)(g_n - k) * sizeof(g_fhr[0]));
    memmove(g_t_ms, &g_t_ms[k], (size_t)(g_n - k) * sizeof(g_t_ms[0]));
    g_n = (uint16_t)(g_n - k);
}

static void drop_oldest_windows(uint8_t k)
{
    if (k == 0U)
    {
        return;
    }

    if (k >= g_win_n)
    {
        g_win_n = 0U;
        return;
    }

    const size_t rem = (size_t)(g_win_n - k);
    memmove(g_win_t_ms,  &g_win_t_ms[k],  rem * sizeof(g_win_t_ms[0]));
    memmove(g_win_ms,    &g_win_ms[k],    rem * sizeof(g_win_ms[0]));
    memmove(g_win_kicks, &g_win_kicks[k], rem * sizeof(g_win_kicks[0]));
    g_win_n = (uint8_t)(g_win_n - k);
}

/*--------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------*/

void wombcare_trend_reset(void)
{
    g_n         = 0U;
    g_win_n     = 0U;
    g_cursor_ms = 0.0f;
    g_window_ms = 0.0f;
    g_open      = false;
}

void wombcare_trend_begin_window(float window_ms)
{
    if (window_ms <= 0.0f)
    {
        return;
    }

    if (g_open)
    {
        /* Step past the window opened last time. This runs whether or not
         * that window was ever accepted, which is precisely how a rejected
         * window becomes a real gap in the timeline instead of vanishing. */
        g_cursor_ms += g_window_ms;
    }
    else
    {
        g_open = true;
    }

    g_window_ms = window_ms;

    /* Age out everything that has fallen off the back of the span. The
     * cutoff is measured from the END of the window being opened, not
     * from the newest beat, so a long run of rejected windows still
     * retires stale beats instead of freezing them in the trend. */
    const float now    = g_cursor_ms + window_ms;
    const float cutoff = now - TREND_SPAN_MS;

    if (cutoff > 0.0f)
    {
        uint16_t k = 0U;
        while ((k < g_n) && (g_t_ms[k] < cutoff))
        {
            k++;
        }
        drop_oldest_beats(k);

        uint8_t wk = 0U;
        while ((wk < g_win_n) && (g_win_t_ms[wk] < cutoff))
        {
            wk++;
        }
        drop_oldest_windows(wk);
    }

    /*----------------------------------------------------------------
     * Rebase the frame onto the oldest thing still retained.
     *---------------------------------------------------------------*/
    float base;

    if (g_n > 0U)
    {
        base = g_t_ms[0];
    }
    else if (g_win_n > 0U)
    {
        base = g_win_t_ms[0];
    }
    else
    {
        /* Nothing retained at all -- restart the frame at this window. */
        base = g_cursor_ms;
    }

    if (base > 0.0f)
    {
        for (uint16_t i = 0U; i < g_n; i++)
        {
            g_t_ms[i] -= base;
        }

        for (uint8_t i = 0U; i < g_win_n; i++)
        {
            g_win_t_ms[i] -= base;
        }

        g_cursor_ms -= base;
    }
}

void wombcare_trend_add_beats(const float    *fhr_bpm,
                              const uint32_t *beat_idx,
                              uint16_t        n,
                              float           sample_period_ms,
                              float           kick_count)
{
    if (!g_open || (fhr_bpm == NULL) || (beat_idx == NULL) || (n == 0U))
    {
        return;
    }

    /* Make room if this window would overrun the store. The span and the
     * valid-beat ceiling together make this unreachable in practice; if it
     * ever does happen the OLDEST beats are the ones to lose. */
    if ((uint32_t)g_n + (uint32_t)n > TREND_MAX_BEATS)
    {
        uint32_t excess = (uint32_t)g_n + (uint32_t)n - TREND_MAX_BEATS;

        if (excess > g_n)
        {
            excess = g_n;
        }

        drop_oldest_beats((uint16_t)excess);
    }

    for (uint16_t i = 0U; i < n; i++)
    {
        if (g_n >= TREND_MAX_BEATS)
        {
            break;
        }

        g_fhr[g_n]  = fhr_bpm[i];
        g_t_ms[g_n] = g_cursor_ms +
                      ((float)beat_idx[i] * sample_period_ms);
        g_n++;
    }

    if (g_win_n >= TREND_MAX_WINDOWS)
    {
        drop_oldest_windows(1U);
    }

    g_win_t_ms[g_win_n]  = g_cursor_ms;
    g_win_ms[g_win_n]    = g_window_ms;
    g_win_kicks[g_win_n] = kick_count;
    g_win_n++;
}

uint16_t wombcare_trend_beat_count(void)
{
    return g_n;
}

const float *wombcare_trend_fhr(void)
{
    return g_fhr;
}

const float *wombcare_trend_time_ms(void)
{
    return g_t_ms;
}

float wombcare_trend_observed_minutes(void)
{
    float total_ms = 0.0f;

    for (uint8_t i = 0U; i < g_win_n; i++)
    {
        total_ms += g_win_ms[i];
    }

    return total_ms / 60000.0f;
}

float wombcare_trend_kicks(void)
{
    float total = 0.0f;

    for (uint8_t i = 0U; i < g_win_n; i++)
    {
        total += g_win_kicks[i];
    }

    return total;
}
