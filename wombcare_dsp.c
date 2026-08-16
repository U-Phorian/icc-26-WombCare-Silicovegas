#include "wombcare_dsp.h"
#include "wombcare_buffer.h"
#include "wombcare_sensors.h"
#include <string.h>
#include <math.h>
#include "dsp/filtering_functions.h"
#include <arm_math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NUM_TAPS              32U
#define MAX_BEATS             300U

/* ADC-count -> physical-unit conversion (moved here from the ring buffer,
 * which now stores raw uint16 counts to save RAM). */
#define ADC_CENTER            2048.0f
#define ADC_TO_UV_SCALE       805.8f
#define ECG_FLATLINE_STD_UV   150.0f

#define PVDF_RMS_THRESHOLD_MULTIPLIER 1.001f

#define ECG_PEAK_THRESHOLD_MULTIPLIER 2.0f

/* Derived timing constants */
#define SAMPLE_PERIOD_MS      (1000.0f / SAMPLE_RATE_HZ)
#define REFRACTORY_SAMPLES    (SAMPLE_RATE_HZ / 5U)

/*--------------------------------------------------------------------
 * FEATURE-SPEC PARITY CONSTANTS
 *
 * The model is trained on features computed a specific way; if the
 * device computes them differently it feeds out-of-distribution inputs
 * and the model fails *silently* (still returns a confident answer).
 * These constants pin the definitions to FEATURE_SPEC.md v1.
 *-------------------------------------------------------------------*/

/* §G3/§G4: analysis window, and the divisor that turns every count
 * feature into a per-minute rate. Derived from the ring buffer so the
 * rates stay correct if the window length ever changes. */
#define WINDOW_SECONDS        ((float)RING_BUFFER_CAPACITY / (float)SAMPLE_RATE_HZ)
#define WINDOW_MINUTES        (WINDOW_SECONDS / 60.0f)

/* §G2: valid-beat filter, applied before ANY feature is computed. */
#define FHR_MIN_BPM           50.0f
#define FHR_MAX_BPM           240.0f
#define FHR_MAX_JUMP_BPM      25.0f

/* The FHR limits above expressed as RR bounds for the peak detector. */
#define RR_MIN_MS             (60000.0f / FHR_MAX_BPM)   /*  250 ms */
#define RR_MAX_MS             (60000.0f / FHR_MIN_BPM)   /* 1200 ms */

/* §3.2: MLTV is the mean FHR range over consecutive 60 s blocks. */
#define MLTV_BLOCK_MS         60000.0f

/* §3.3/§3.4: an accel/decel episode is >=15 bpm from LB held >=15 s. */
#define EPISODE_DELTA_BPM     15.0f
#define EPISODE_MIN_MS        15000.0f



/*
 * head always points to the oldest sample
 * because it is the next write location.
 */



/*--------------------------------------------------------------------
 * DSP INTERNAL STATE
 *-------------------------------------------------------------------*/

static arm_lms_norm_instance_f32 lms_instance;

/* LMS filter memory */
static float lms_state[RING_BUFFER_CAPACITY + NUM_TAPS - 1U];
static float lms_coeffs[NUM_TAPS] = {0.0f};

/*
 * Shared full-length working buffers.
 *
 * The DSP pipeline runs one stage at a time, so a small pool of
 * scratch buffers is reused rather than giving every intermediate
 * signal its own array. Aliases below name each stage's usage:
 *
 *   scratch_pool[0] : flat maternal ECG  (LMS source)      -> later PVDF
 *   scratch_pool[1] : flat abdominal ECG (LMS reference)
 *   scratch_pool[2] : estimated maternal (LMS output, discarded)
 *                     -> later the sorted FHR copy used for the LB median
 *   scratch_pool[3] : clean fetal ECG    (LMS error, used downstream)
 *
 * This keeps peak RAM at 4 buffers instead of 5 while never aliasing
 * two live signals within the same stage.
 */
static float scratch_pool[4][RING_BUFFER_CAPACITY];


static float rr_intervals_ms[MAX_BEATS];
static float fhr_bpm[MAX_BEATS];   /* FHR in bpm — variability MUST be computed
                                    * here, not on RR ms, to match the trained
                                    * model (see FEATURE_SPEC.md). */

/*--------------------------------------------------------------------
 * HELPER FUNCTIONS
 *-------------------------------------------------------------------*/

/*
 * Convert a circular ECG ring (raw ADC counts) into a linear buffer,
 * applying the ADC-count -> microvolt conversion on the way out.
 */
static void unwrap_ecg(const uint16_t *ring,
                       float *flat,
                       uint32_t head)
{
    uint32_t idx = 0U;

    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++)
    {
        flat[idx++] = ((float)ring[i] - ADC_CENTER) * ADC_TO_UV_SCALE;
    }

    for (uint32_t i = 0U; i < head; i++)
    {
        flat[idx++] = ((float)ring[i] - ADC_CENTER) * ADC_TO_UV_SCALE;
    }
}

/*
 * Convert a circular ring into a linear buffer, keeping raw counts
 * (used for PVDF, which is normalized later by its own RMS).
 */
static void unwrap_raw(const uint16_t *ring,
                       float *flat,
                       uint32_t head)
{
    uint32_t idx = 0U;

    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++)
    {
        flat[idx++] = (float)ring[i];
    }

    for (uint32_t i = 0U; i < head; i++)
    {
        flat[idx++] = (float)ring[i];
    }
}

/*--------------------------------------------------------------------
 * PUBLIC API
 *-------------------------------------------------------------------*/

void wombcare_dsp_init(void)
{
    arm_lms_norm_init_f32(
        &lms_instance,
        NUM_TAPS,
        lms_coeffs,
        lms_state,
        0.01f,
        RING_BUFFER_CAPACITY);
}

bool wombcare_dsp_run_pipeline(WombCareFeatures_t *output_features)
{
    /*--------------------------------------------------------------
     * Defensive Checks
     *-------------------------------------------------------------*/
    if (output_features == NULL)
{
    return false;
}

/* Clear feature structure before filling it */
memset(output_features, 0, sizeof(WombCareFeatures_t));

if (!minute_window_ready)
{
    return false;
}

    if (!tracker_mother.is_primed ||
        !tracker_fetal.is_primed ||
        !tracker_pvdf.is_primed)
    {
        return false;
    }

    /*--------------------------------------------------------------
     * Name the scratch buffers for the ECG stages.
     *-------------------------------------------------------------*/
    float *flat_mother_ecg        = scratch_pool[0];
    float *flat_abdom_ecg         = scratch_pool[1];
    float *estimated_maternal_ecg = scratch_pool[2];
    float *clean_fetal_ecg        = scratch_pool[3];

    /*--------------------------------------------------------------
     * Linearize ECG ring buffers (raw counts -> microvolts).
     * PVDF is unwrapped later, reusing a freed scratch buffer.
     *-------------------------------------------------------------*/
    unwrap_ecg(ring_mother_ecg,
               flat_mother_ecg,
               tracker_mother.head);

    unwrap_ecg(ring_fetal_ecg,
               flat_abdom_ecg,
               tracker_fetal.head);

    /*--------------------------------------------------------------
     * Maternal ECG Cancellation
     *-------------------------------------------------------------*/
    arm_lms_norm_f32(
        &lms_instance,
        flat_mother_ecg,
        flat_abdom_ecg,
        estimated_maternal_ecg,
        clean_fetal_ecg,
        RING_BUFFER_CAPACITY);

    /*--------------------------------------------------------------
     * Dynamic Threshold
     *-------------------------------------------------------------*/
    /*--------------------------------------------------------------
     * Dynamic Threshold & Flatline Detection
     *-------------------------------------------------------------*/
    float signal_std = 0.0f;

    /* 1. Calculate Standard Deviation to completely ignore DC offsets (4095 and 3550)! */
    arm_std_f32(
        clean_fetal_ecg,
        RING_BUFFER_CAPACITY,
        &signal_std);

    /* 2. Flatline / Leads-Off Detection */
    if (signal_std < ECG_FLATLINE_STD_UV) 
    {
        output_features->lb_bpm = 0;
        output_features->fetal_movements = 0;
        minute_window_ready = false; 
        return true; // Push zeroes to the Android App!
    }

    /* 3. Base the Peak Detector strictly on the true AC wave size */
    float dynamic_peak_threshold = signal_std * ECG_PEAK_THRESHOLD_MULTIPLIER;

    /*--------------------------------------------------------------
     * Peak Detection
     *-------------------------------------------------------------*/
    uint16_t beat_count = 0U;
    uint32_t last_peak_idx = 0U;

    for (uint32_t i = 1U;
         i < (RING_BUFFER_CAPACITY - 1U);
         i++)
    {
        if ((fabsf(clean_fetal_ecg[i]) > dynamic_peak_threshold) &&
    (fabsf(clean_fetal_ecg[i]) > fabsf(clean_fetal_ecg[i - 1U])) &&
    (fabsf(clean_fetal_ecg[i]) > fabsf(clean_fetal_ecg[i + 1U])))
        {
            if ((last_peak_idx > 0U) && (beat_count < MAX_BEATS))
            {
                float rr_ms = (float)(i - last_peak_idx) *SAMPLE_PERIOD_MS;

                    /* Reject physiologically impossible RR intervals.
                     * Bounds come from the Feature-Spec FHR range
                     * (50..240 bpm). The old 800 ms ceiling capped the
                     * series at 75 bpm, which discarded exactly the
                     * bradycardic beats a Pathologic call depends on. */
                    if ((rr_ms >= RR_MIN_MS) &&
                        (rr_ms <= RR_MAX_MS))
                        {
                           rr_intervals_ms[beat_count] = rr_ms;
                           beat_count++;
                        }
            }

            last_peak_idx = i;

            /* 200 ms refractory period */
            i += REFRACTORY_SAMPLES;
        }
    }

    if (beat_count < 10U)
    {
        return false;
    }

    /*--------------------------------------------------------------
     * Valid-beat filter (Feature Spec §G2)
     *
     * Build the FHR (bpm) series and drop artifact beats before ANY
     * feature is computed: out-of-range rates, and jumps larger than
     * 25 bpm from the last accepted beat (a missed or doubled R-peak
     * produces exactly that signature).
     *
     * rr_intervals_ms is compacted in step with fhr_bpm so the
     * acceleration/deceleration pass below can still use it for
     * episode timing. Compaction in place is safe because the write
     * index never overtakes the read index.
     *-------------------------------------------------------------*/
    uint16_t valid_count = 0U;

    for (uint16_t i = 0U; i < beat_count; i++)
    {
        float fhr = 60000.0f / rr_intervals_ms[i];

        if ((fhr < FHR_MIN_BPM) || (fhr > FHR_MAX_BPM))
        {
            continue;
        }

        if ((valid_count > 0U) &&
            (fabsf(fhr - fhr_bpm[valid_count - 1U]) > FHR_MAX_JUMP_BPM))
        {
            continue;
        }

        fhr_bpm[valid_count]         = fhr;
        rr_intervals_ms[valid_count] = rr_intervals_ms[i];

        valid_count++;
    }

    beat_count = valid_count;

    if (beat_count < 10U)
    {
        return false;
    }

    /*--------------------------------------------------------------
     * MeanHR + Variance (Feature Spec §3.6, §3.7)
     *-------------------------------------------------------------*/
    arm_mean_f32(fhr_bpm, beat_count, &output_features->mean_hr_bpm);

    /* Variance of FHR in bpm^2 (not RR variance in ms^2). */
    arm_var_f32(
        fhr_bpm,
        beat_count,
        &output_features->hr_variance);

    /*--------------------------------------------------------------
     * LB — baseline FHR (Feature Spec §3.0)
     *
     * The MEDIAN, not the mean. Two reasons: the baseline is supposed
     * to survive accel/decel excursions, and the model was trained on
     * LB and MeanHR as distinct features (training means 133.30 vs
     * 134.61). Setting LB = MeanHR made inputs 0 and 6 identical --
     * a combination that never occurs in the training set.
     *
     * Sorting a copy is cheap here: at most MAX_BEATS entries, once
     * per window, and the LMS estimate buffer is dead at this point.
     *-------------------------------------------------------------*/
    float *fhr_sorted = estimated_maternal_ecg;

    memcpy(fhr_sorted, fhr_bpm, (size_t)beat_count * sizeof(float));

    for (uint16_t i = 1U; i < beat_count; i++)
    {
        float    key = fhr_sorted[i];
        uint16_t j   = i;

        while ((j > 0U) && (fhr_sorted[j - 1U] > key))
        {
            fhr_sorted[j] = fhr_sorted[j - 1U];
            j--;
        }

        fhr_sorted[j] = key;
    }

    output_features->lb_bpm =
        ((beat_count & 1U) != 0U)
            ? fhr_sorted[beat_count / 2U]
            : 0.5f * (fhr_sorted[(beat_count / 2U) - 1U] +
                      fhr_sorted[beat_count / 2U]);

    /*--------------------------------------------------------------
     * MSTV
     *-------------------------------------------------------------*/
    float sum_diff = 0.0f;

    for (uint16_t i = 0U;
         i < (beat_count - 1U);
         i++)
    {
        sum_diff += fabsf(
            fhr_bpm[i + 1U] -
            fhr_bpm[i]);
    }

    output_features->mstv_ms =
        sum_diff / (float)(beat_count - 1U);

    /*--------------------------------------------------------------
     * MLTV — mean long-term variability (Feature Spec §3.2)
     *
     * Split the window into consecutive 60 SECOND blocks using
     * cumulative beat time, take max(FHR) - min(FHR) within each, and
     * average those ranges.
     *
     * This used to slice into fixed 15-BEAT blocks, which span roughly
     * 6 s at a typical fetal rate -- an order of magnitude short of the
     * trained definition, so the value came out far below the training
     * distribution (mean 8.19, std 5.63).
     *-------------------------------------------------------------*/
    float    mltv_sum      = 0.0f;
    uint16_t mltv_blocks   = 0U;
    float    block_elapsed = 0.0f;
    uint16_t block_beats   = 0U;
    float    block_max     = fhr_bpm[0];
    float    block_min     = fhr_bpm[0];

    for (uint16_t i = 0U; i < beat_count; i++)
    {
        if (fhr_bpm[i] > block_max)
        {
            block_max = fhr_bpm[i];
        }

        if (fhr_bpm[i] < block_min)
        {
            block_min = fhr_bpm[i];
        }

        block_beats++;
        block_elapsed += rr_intervals_ms[i];

        if (block_elapsed >= MLTV_BLOCK_MS)
        {
            mltv_sum += (block_max - block_min);
            mltv_blocks++;

            block_elapsed = 0.0f;
            block_beats   = 0U;

            if ((i + 1U) < beat_count)
            {
                block_max = fhr_bpm[i + 1U];
                block_min = fhr_bpm[i + 1U];
            }
        }
    }

    /* Close a trailing partial block. Without this a window whose
     * accepted beats never quite total 60 s would report MLTV = 0,
     * which reads as "no variability at all" rather than "short block". */
    if (block_beats >= 2U)
    {
        mltv_sum += (block_max - block_min);
        mltv_blocks++;
    }

    output_features->mltv_ms =
        (mltv_blocks > 0U)
            ? (mltv_sum / (float)mltv_blocks)
            : 0.0f;

    /*--------------------------------------------------------------
 * Accelerations / Decelerations
 *
 * Counts complete episodes rather than every elapsed 15 seconds.
 * This avoids counting one long acceleration/deceleration
 * multiple times.
 *-------------------------------------------------------------*/

output_features->accel_count = 0.0f;
output_features->decel_count = 0.0f;

bool accel_active = false;
bool decel_active = false;

float accel_duration_ms = 0.0f;
float decel_duration_ms = 0.0f;

for (uint16_t i = 0U; i < beat_count; i++)
{
    /* Use the filtered series; excursions are measured against LB
     * (the median baseline), per Feature Spec §3.3/§3.4. */
    float instant_bpm = fhr_bpm[i];

    /*----------------------------------------------------------
     * ACCELERATION
     *---------------------------------------------------------*/
    if (instant_bpm >=
        (output_features->lb_bpm + EPISODE_DELTA_BPM))
    {
        accel_duration_ms += rr_intervals_ms[i];
        accel_active = true;

        /* End any running deceleration */
        if (decel_active)
        {
            if (decel_duration_ms >= EPISODE_MIN_MS)
            {
                output_features->decel_count += 1.0f;
            }

            decel_active = false;
            decel_duration_ms = 0.0f;
        }
    }

    /*----------------------------------------------------------
     * DECELERATION
     *---------------------------------------------------------*/
    else if (instant_bpm <=
             (output_features->lb_bpm - EPISODE_DELTA_BPM))
    {
        decel_duration_ms += rr_intervals_ms[i];
        decel_active = true;

        /* End any running acceleration */
        if (accel_active)
        {
            if (accel_duration_ms >= EPISODE_MIN_MS)
            {
                output_features->accel_count += 1.0f;
            }

            accel_active = false;
            accel_duration_ms = 0.0f;
        }
    }

    /*----------------------------------------------------------
     * Returned to baseline.
     * Close whichever episode was active.
     *---------------------------------------------------------*/
    else
    {
        if (accel_active)
        {
            if (accel_duration_ms >= EPISODE_MIN_MS)
            {
                output_features->accel_count += 1.0f;
            }

            accel_active = false;
            accel_duration_ms = 0.0f;
        }

        if (decel_active)
        {
            if (decel_duration_ms >= EPISODE_MIN_MS)
            {
                output_features->decel_count += 1.0f;
            }

            decel_active = false;
            decel_duration_ms = 0.0f;
        }
    }
}

/*--------------------------------------------------------------
 * Handle episodes that continue until the end
 * of the one-minute window.
 *-------------------------------------------------------------*/

if (accel_active &&
    accel_duration_ms >= EPISODE_MIN_MS)
{
    output_features->accel_count += 1.0f;
}

if (decel_active &&
    decel_duration_ms >= EPISODE_MIN_MS)
{
    output_features->decel_count += 1.0f;
}

/* Feature Spec §G4: the model consumes per-minute RATES, not raw
 * episode counts. Identity at the current 60 s window, but keeps the
 * features correct if the window is ever widened (§G3 recommends 240 s). */
output_features->accel_count /= WINDOW_MINUTES;
output_features->decel_count /= WINDOW_MINUTES;
    /*--------------------------------------------------------------
     * PVDF Kick Count
     *
     * The ECG stages are finished, so reuse scratch_pool[0]
     * (formerly the flat maternal ECG) for the PVDF signal.
     *-------------------------------------------------------------*/
    float *flat_pvdf = scratch_pool[0];

    unwrap_raw(ring_pvdf_kick,
               flat_pvdf,
               tracker_pvdf.head);

    float pvdf_rms = 0.0f;

    arm_rms_f32(
        flat_pvdf,
        RING_BUFFER_CAPACITY,
        &pvdf_rms);

    output_features->fetal_movements = 0.0f;

float pvdf_threshold = pvdf_rms * PVDF_RMS_THRESHOLD_MULTIPLIER;

    for (uint32_t i = 1U;
     i < (RING_BUFFER_CAPACITY - 1U);
     i++)
{
    if ((fabsf(flat_pvdf[i]) > pvdf_threshold) &&
        (fabsf(flat_pvdf[i]) > fabsf(flat_pvdf[i - 1U])) &&
        (fabsf(flat_pvdf[i]) > fabsf(flat_pvdf[i + 1U])))
    {
        output_features->fetal_movements += 1.0f;

        /* Ignore another kick for one second */
        i += SAMPLE_RATE_HZ;
    }
}

/* Feature Spec §G4: FM is a per-minute rate too. NOTE: the spec also
 * calls for a ~3 Hz high-pass on the PVDF before this RMS threshold, to
 * strip maternal respiration. That is NOT implemented -- FM is the
 * lowest-parity feature in the set and a candidate to drop entirely
 * (spec §4, Set B). Kick count is unchanged for BLE reporting. */
output_features->fetal_movements /= WINDOW_MINUTES;

/* One-minute feature vector has been consumed. */
    minute_window_ready = false;
    return true;
}