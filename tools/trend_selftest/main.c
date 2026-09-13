/*
 * Host self-test for the rolling beat trend and the feature vector built
 * on top of it.
 *
 * The real wombcare_dsp.c is #included rather than linked so that the
 * static compute_features() can be driven directly with a synthetic beat
 * series. That is the whole point: the existing dsp_selftest never gets
 * a window accepted (a synthetic abdominal mix does not yield a fetal
 * lock), so it cannot reach the feature code at all.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "wombcare_buffer.h"

/*
 * The ring buffers normally live in wombcare_buffer.c, which is not part
 * of this bench. compute_features() never touches them, but the rest of
 * the pipeline in the same translation unit does, so they have to exist
 * to link. Same arrangement as tools/dsp_selftest/main.c.
 */
RingBufferTracker_t tracker_mother, tracker_fetal, tracker_pvdf;
volatile bool       minute_window_ready;
uint16_t            ring_mother_ecg[RING_BUFFER_CAPACITY];
uint16_t            ring_fetal_ecg[RING_BUFFER_CAPACITY];
uint16_t            ring_pvdf_kick[RING_BUFFER_CAPACITY];

#include "dsp_under_test.c"

static int checks = 0;
static int failures = 0;

static void check(int cond, const char *what)
{
    checks++;
    if (cond)
    {
        printf("  PASS  %s\n", what);
    }
    else
    {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

/*--------------------------------------------------------------------
 * Synthetic beat series
 *-------------------------------------------------------------------*/
#define CAP 2048

typedef float (*bpm_fn)(float t_ms);

static float g_fhr[CAP];
static float g_t[CAP];

static uint16_t gen_beats(bpm_fn f, float start_ms, float end_ms,
                          float *fhr, float *t_ms)
{
    uint16_t n = 0U;
    float    t = start_ms;

    while ((t < end_ms) && (n < CAP))
    {
        const float bpm = f(t);
        fhr[n] = bpm;
        t_ms[n] = t;
        n++;
        t += 60000.0f / bpm;
    }
    return n;
}

/* Flat 140 bpm baseline. */
static float flat140(float t_ms) { (void)t_ms; return 140.0f; }

/*
 * 140 bpm baseline with a +25 bpm acceleration running 50 s -> 70 s,
 * i.e. 20 s long and straddling the 60 s window boundary.
 */
static float straddle(float t_ms)
{
    if ((t_ms >= 50000.0f) && (t_ms < 70000.0f))
    {
        return 165.0f;
    }
    return 140.0f;
}

/* Each successive minute has a wider FHR swing: 10, 20, 30, 40 bpm. */
static float widening(float t_ms)
{
    const int   minute = (int)(t_ms / 60000.0f);
    const float amp    = 5.0f * (float)(minute + 1);   /* half-range */
    const float phase  = fmodf(t_ms, 60000.0f) / 60000.0f;
    return 140.0f + amp * ((phase < 0.5f) ? 1.0f : -1.0f);
}

/*--------------------------------------------------------------------
 * Helpers
 *-------------------------------------------------------------------*/
static void feed_window(const float *fhr, const float *t_ms, uint16_t n,
                        float win_start_ms, float kicks)
{
    static float    wf[CAP];
    static uint32_t wi[CAP];
    uint16_t        m = 0U;

    for (uint16_t i = 0U; i < n; i++)
    {
        const float rel = t_ms[i] - win_start_ms;
        if ((rel >= 0.0f) && (rel < 60000.0f))
        {
            wf[m] = fhr[i];
            wi[m] = (uint32_t)(rel / SAMPLE_PERIOD_MS);
            m++;
        }
    }

    wombcare_trend_add_beats(wf, wi, m, SAMPLE_PERIOD_MS, kicks);
}

static uint16_t slice(const float *fhr, const float *t_ms, uint16_t n,
                      float from_ms, float to_ms,
                      float *out_fhr, float *out_t)
{
    uint16_t m = 0U;
    for (uint16_t i = 0U; i < n; i++)
    {
        if ((t_ms[i] >= from_ms) && (t_ms[i] < to_ms))
        {
            out_fhr[m] = fhr[i];
            out_t[m]   = t_ms[i];
            m++;
        }
    }
    return m;
}

/*====================================================================*/

static void test_straddling_episode(void)
{
    printf("\n-- a 20 s acceleration across a window boundary --\n");

    const uint16_t n = gen_beats(straddle, 0.0f, 120000.0f, g_fhr, g_t);

    /* OLD behaviour: each 60 s window scored on its own, results summed. */
    float sfhr[CAP], st[CAP];
    WombCareFeatures_t f1 = {0}, f2 = {0};

    uint16_t m1 = slice(g_fhr, g_t, n, 0.0f, 60000.0f, sfhr, st);
    compute_features(sfhr, st, m1, 1.0f, 0.0f, &f1);

    uint16_t m2 = slice(g_fhr, g_t, n, 60000.0f, 120000.0f, sfhr, st);
    compute_features(sfhr, st, m2, 1.0f, 0.0f, &f2);

    const float old_total = f1.accel_count * 1.0f + f2.accel_count * 1.0f;

    /* NEW behaviour: one continuous 2-minute series. */
    WombCareFeatures_t fw = {0};
    compute_features(g_fhr, g_t, n, 2.0f, 0.0f, &fw);
    const float new_total = fw.accel_count * 2.0f;

    printf("     per-60s-window, summed : %.0f accelerations\n", old_total);
    printf("     over the 2-minute span : %.0f accelerations\n", new_total);

    check(old_total == 0.0f,
          "60 s windows miss the straddling episode entirely (the old bug)");
    check(new_total == 1.0f,
          "the trend sees it as one complete 20 s acceleration");
}

static void test_rate_denominator(void)
{
    printf("\n-- rate denominator counts observed time, not elapsed --\n");

    wombcare_trend_reset();

    const uint16_t n = gen_beats(flat140, 0.0f, 240000.0f, g_fhr, g_t);

    /* Four windows open; only the 1st and 3rd produce usable beats. */
    for (int w = 0; w < 4; w++)
    {
        wombcare_trend_begin_window(60000.0f);
        if ((w % 2) == 0)
        {
            feed_window(g_fhr, g_t, n, (float)w * 60000.0f, 3.0f);
        }
    }

    const float obs = wombcare_trend_observed_minutes();
    printf("     accepted windows = 2, elapsed = 4 min, observed = %.1f min\n", obs);

    check(fabsf(obs - 2.0f) < 1e-3f,
          "observed time is 2 min, not the 4 min of elapsed wall clock");
    check(fabsf(wombcare_trend_kicks() - 6.0f) < 1e-3f,
          "kicks accumulate across accepted windows only (6)");
}

static void test_gap_breaks_episode(void)
{
    printf("\n-- an excursion is not joined across a rejected window --\n");

    /*
     * 12 s of raised FHR at the end of window 0 and another 12 s at the
     * start of window 2. Window 1 is rejected, so nothing is known about
     * the minute between them. Fusing them would manufacture a 72 s
     * "acceleration" out of two sub-threshold blips.
     */
    float fhr[CAP], t[CAP];
    uint16_t n = 0U;

    for (float tm = 48000.0f; tm < 60000.0f; tm += 400.0f)
    {
        fhr[n] = 165.0f; t[n] = tm; n++;
    }
    for (float tm = 120000.0f; tm < 132000.0f; tm += 400.0f)
    {
        fhr[n] = 165.0f; t[n] = tm; n++;
    }
    for (float tm = 132000.0f; tm < 180000.0f; tm += 400.0f)
    {
        fhr[n] = 140.0f; t[n] = tm; n++;
    }

    WombCareFeatures_t f = {0};
    compute_features(fhr, t, n, 3.0f, 0.0f, &f);

    printf("     accelerations reported: %.0f\n", f.accel_count * 3.0f);
    check(f.accel_count == 0.0f,
          "two 12 s blips either side of a gap stay two sub-threshold blips");
}

static void test_eviction(void)
{
    printf("\n-- the trend retains one span, no more --\n");

    wombcare_trend_reset();

    const uint16_t n = gen_beats(flat140, 0.0f, 480000.0f, g_fhr, g_t);

    for (int w = 0; w < 8; w++)
    {
        wombcare_trend_begin_window(60000.0f);
        feed_window(g_fhr, g_t, n, (float)w * 60000.0f, 1.0f);
    }

    const float obs   = wombcare_trend_observed_minutes();
    const uint16_t bc = wombcare_trend_beat_count();
    const float *tt   = wombcare_trend_time_ms();
    const float span  = (bc > 1U) ? (tt[bc - 1U] - tt[0]) : 0.0f;

    printf("     8 windows fed; observed=%.1f min, beats=%u, span=%.1f s\n",
           obs, (unsigned)bc, span / 1000.0f);

    check(fabsf(obs - 4.0f) < 1e-3f,
          "observed time saturates at the 4-minute span");
    check(span <= TREND_SPAN_MS,
          "retained beats never exceed the span");
    check(bc < TREND_MAX_BEATS,
          "beat store stays within its bound");
}

static void test_mltv_blocks(void)
{
    printf("\n-- MLTV averages every 60 s block in the span --\n");

    const uint16_t n = gen_beats(widening, 0.0f, 240000.0f, g_fhr, g_t);

    float sfhr[CAP], st[CAP];
    WombCareFeatures_t one = {0}, four = {0};

    uint16_t m1 = slice(g_fhr, g_t, n, 0.0f, 60000.0f, sfhr, st);
    compute_features(sfhr, st, m1, 1.0f, 0.0f, &one);

    compute_features(g_fhr, g_t, n, 4.0f, 0.0f, &four);

    printf("     MLTV over 1 min = %.1f bpm   over 4 min = %.1f bpm\n",
           one.mltv_ms, four.mltv_ms);

    check(fabsf(one.mltv_ms - 10.0f) < 2.0f,
          "a single window can only ever report its own block (~10)");
    check(four.mltv_ms > 20.0f,
          "the span averages the later, wider blocks too (>20)");
}

static void test_fm_rate(void)
{
    printf("\n-- kick rate is per observed minute --\n");

    const uint16_t n = gen_beats(flat140, 0.0f, 240000.0f, g_fhr, g_t);

    WombCareFeatures_t f = {0};
    compute_features(g_fhr, g_t, n, 4.0f, 8.0f, &f);

    printf("     8 kicks over 4 min -> %.2f /min\n", f.fetal_movements);
    check(fabsf(f.fetal_movements - 2.0f) < 1e-3f,
          "8 kicks / 4 min = 2.0 per minute");
}

static void test_baseline_unchanged(void)
{
    printf("\n-- unchanged definitions still produce unchanged values --\n");

    const uint16_t n = gen_beats(flat140, 0.0f, 240000.0f, g_fhr, g_t);

    WombCareFeatures_t f = {0};
    compute_features(g_fhr, g_t, n, 4.0f, 0.0f, &f);

    printf("     LB=%.1f MeanHR=%.1f Var=%.3f MSTV=%.3f\n",
           f.lb_bpm, f.mean_hr_bpm, f.hr_variance, f.mstv_ms);

    check(fabsf(f.lb_bpm - 140.0f) < 0.5f,   "LB is the flat baseline");
    check(fabsf(f.mean_hr_bpm - 140.0f) < 0.5f, "MeanHR is the flat baseline");
    check(f.hr_variance < 0.01f,             "variance of a flat trace is ~0");
    check(f.mstv_ms < 0.01f,                 "MSTV of a flat trace is ~0");
}

int main(void)
{
    printf("========================================================\n");
    printf(" TREND + FEATURE SELF-TEST\n");
    printf("========================================================\n");

    wombcare_trend_reset();

    test_straddling_episode();
    test_gap_breaks_episode();
    test_rate_denominator();
    test_eviction();
    test_mltv_blocks();
    test_fm_rate();
    test_baseline_unchanged();

    printf("\n%d/%d checks passed\n", checks - failures, checks);
    return (failures == 0) ? 0 : 1;
}
