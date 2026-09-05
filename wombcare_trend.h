#ifndef WOMBCARE_TREND_H
#define WOMBCARE_TREND_H

#include <stdint.h>
#include <stdbool.h>

/*--------------------------------------------------------------------
 * Rolling multi-minute beat trend.
 *
 * WHY THIS EXISTS
 * ---------------
 * The NSP model is trained on UCI CTG segments whose MEDIAN duration is
 * 3.3 minutes (mean 3.44, IQR 2.55-4.34), but the DSP window is 60 s.
 * Feeding one-minute features to a model trained on ~3.4-minute features
 * is a train/serve mismatch, and it bites hardest on exactly the features
 * the model leans on:
 *
 *   - AC_rate / DEC_rate are episode COUNTS. At the training mean rates
 *     (0.76 and 0.45 per minute) a 60 s window contains ZERO accelerations
 *     47% of the time and zero decelerations 64% of the time -- by chance,
 *     not because anything is wrong. The model reads that as a warning
 *     sign. Over 240 s the same rates are estimated from ~4x the events.
 *
 *   - An episode must last >= 15 s to count at all. Inside a 60 s window
 *     an episode fits entirely within the window only ~75% of the time
 *     ((60-15)/60); over 240 s it is ~94%. The 60 s window therefore
 *     systematically UNDER-COUNTS both features by roughly a fifth
 *     relative to how the training data was built.
 *
 *   - MLTV is the mean FHR range over consecutive 60 s blocks. A 60 s
 *     window holds exactly ONE block, so "mean over blocks" averaged a
 *     single sample. Four blocks is what the trained definition assumes.
 *
 * Measured cost of the mismatch, scoring the deployed model by segment
 * duration under recording-level CV: 0.810 accuracy on >4 min segments
 * vs 0.720 on <2 min segments. FEATURE_SPEC.md §G3 already recommends
 * 240 s; this module is that change.
 *
 * WHY BEATS, NOT SAMPLES
 * ----------------------
 * Widening the raw ring buffer to 240 s is not affordable: the three
 * rings plus scratch_pool already sit near 330 KB of the part's 512 KB,
 * and quadrupling them is not close to possible. But every feature is
 * computed from the BEAT SERIES, never from the waveform -- so only the
 * beats have to persist. At FHR_MAX_BPM over the span that is under 900
 * beats (~7 KB), and beat detection still runs on the unchanged 60 s ring.
 *
 * USAGE (one call pair per DSP window):
 *      wombcare_trend_begin_window(ms);   // always, even if the window
 *                                         // is about to be rejected
 *      ... detect beats ...
 *      wombcare_trend_add_beats(...);     // only on the accepted path
 *
 * begin_window() advances the clock and ages out stale beats, so a
 * rejected window needs no special handling: it simply contributes no
 * beats and no observation time. That matters -- a rejected window is a
 * real gap, and the beats either side of it must not look adjacent.
 *-------------------------------------------------------------------*/

/* Analysis span presented to the model (FEATURE_SPEC.md §G3). */
#define TREND_SPAN_MS         240000.0f

/*
 * Worst case retained beats. FHR_MAX_BPM (200) sustained for the whole
 * span is 800; the headroom covers the window that pushes the buffer past
 * the span in the moment before the stale beats are evicted.
 */
#define TREND_MAX_BEATS       1024U

/* Accepted windows retained across the span, plus headroom. */
#define TREND_MAX_WINDOWS     8U

/*
 * An episode may not be joined across a discontinuity longer than this.
 * A gap is not evidence that an excursion continued through it, and
 * without this a rejected window in the middle of the trend would fuse
 * two unrelated excursions into one long false episode.
 */
#define TREND_GAP_BREAK_MS    3000.0f

/* Clears every retained beat and restarts the clock. */
void wombcare_trend_reset(void);

/*
 * Open a new window of `window_ms` wall clock. Advances the trend clock
 * and evicts everything that has aged past TREND_SPAN_MS. Call this on
 * EVERY window, before any decision to accept or reject it.
 */
void wombcare_trend_begin_window(float window_ms);

/*
 * Append the accepted beats of the window opened by the last
 * begin_window() call.
 *
 *   fhr_bpm          : valid FHR values, bpm
 *   beat_idx         : each beat's sample index within the DSP window
 *   n                : beat count
 *   sample_period_ms : ms per sample, to turn beat_idx into a time
 *   kick_count       : raw PVDF kicks for this window (a count, not a rate)
 */
void wombcare_trend_add_beats(const float    *fhr_bpm,
                              const uint32_t *beat_idx,
                              uint16_t        n,
                              float           sample_period_ms,
                              float           kick_count);

uint16_t     wombcare_trend_beat_count(void);
const float *wombcare_trend_fhr(void);

/* Beat times in ms, relative to the oldest retained beat. */
const float *wombcare_trend_time_ms(void);

/*
 * Observation time backing the retained beats, in minutes -- the summed
 * wall clock of the ACCEPTED windows still in the trend, not the elapsed
 * time. Rejected windows contribute neither events nor observation time,
 * so dividing counts by this gives an unbiased rate; dividing by elapsed
 * time would halve the reported rate on a device rejecting half its
 * windows. Returns 0 when the trend is empty.
 */
float wombcare_trend_observed_minutes(void);

/* Total PVDF kicks retained across the span (a raw count, not a rate). */
float wombcare_trend_kicks(void);

#endif /* WOMBCARE_TREND_H */
