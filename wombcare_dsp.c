/*
 * CORRECTED wombcare_dsp.c  (Feature Spec v1 parity fixes)
 * -------------------------------------------------------------------
 * Base = Malay's latest wombcare_dsp.c. Only the feature MATH changed so the
 * device computes MSTV / MLTV / Variance / MeanHR in the SAME units the model
 * was trained on (bpm / bpm^2), by computing them on an FHR array instead of on
 * RR-intervals in ms.
 *
 * What changed vs Malay's version (search "PARITY FIX"):
 *   1. new fhr_bpm[] array built from rr_intervals_ms[]
 *   2. MSTV      -> mean |dFHR| in bpm            (was mean|dRR| in ms)
 *   3. MLTV      -> per-block max-min of FHR bpm  (was on RR ms)
 *   4. Variance  -> var(FHR) in bpm^2             (was var(RR) in ms^2)
 *   5. MeanHR    -> arithmetic mean of FHR bpm    (was 60000/mean_rr, harmonic)
 * Everything else (LMS cancel, peak detect, accel/decel episodes, PVDF) is
 * unchanged from Malay's version. Counts stay raw over the 60 s window = per-min.
 * -------------------------------------------------------------------
 */
#include "wombcare_dsp.h"
#include "wombcare_buffer.h"
#include "wombcare_sensors.h"
#include <string.h>
#include <math.h>
#include <arm_math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NUM_TAPS              32U
#define MAX_BEATS             300U

#define SAMPLE_PERIOD_MS      (1000.0f / SAMPLE_RATE_HZ)
#define REFRACTORY_SAMPLES    (SAMPLE_RATE_HZ / 5U)

static arm_lms_norm_instance_f32 lms_instance;
static float lms_state[RING_BUFFER_CAPACITY + NUM_TAPS - 1U];
static float lms_coeffs[NUM_TAPS] = {0.0f};

static float flat_mother_ecg[RING_BUFFER_CAPACITY];
static float flat_abdom_ecg[RING_BUFFER_CAPACITY];
static float flat_pvdf[RING_BUFFER_CAPACITY];

static float estimated_maternal_ecg[RING_BUFFER_CAPACITY];
static float clean_fetal_ecg[RING_BUFFER_CAPACITY];

static float rr_intervals_ms[MAX_BEATS];
static float fhr_bpm[MAX_BEATS];          /* PARITY FIX 1: FHR (bpm) series */

static void unwrap_buffer(const float *ring, float *flat, uint32_t head)
{
    uint32_t idx = 0U;
    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++) flat[idx++] = ring[i];
    for (uint32_t i = 0U; i < head; i++)                   flat[idx++] = ring[i];
}

void wombcare_dsp_init(void)
{
    arm_lms_norm_init_f32(&lms_instance, NUM_TAPS, lms_coeffs, lms_state,
                          0.01f, RING_BUFFER_CAPACITY);
}

bool wombcare_dsp_run_pipeline(WombCareFeatures_t *output_features)
{
    if (output_features == NULL) return false;
    memset(output_features, 0, sizeof(WombCareFeatures_t));

    if (!minute_window_ready) return false;
    if (!tracker_mother.is_primed || !tracker_fetal.is_primed || !tracker_pvdf.is_primed)
        return false;

    unwrap_buffer(ring_mother_ecg, flat_mother_ecg, tracker_mother.head);
    unwrap_buffer(ring_fetal_ecg,  flat_abdom_ecg,  tracker_fetal.head);
    unwrap_buffer(ring_pvdf_kick,  flat_pvdf,       tracker_pvdf.head);

    /* Maternal cancellation: fetal ECG comes out in the error output. */
    arm_lms_norm_f32(&lms_instance, flat_mother_ecg, flat_abdom_ecg,
                     estimated_maternal_ecg, clean_fetal_ecg, RING_BUFFER_CAPACITY);

    float signal_rms = 0.0f;
    arm_rms_f32(clean_fetal_ecg, RING_BUFFER_CAPACITY, &signal_rms);
    if (signal_rms < 1.0f) return false;
    float dynamic_peak_threshold = signal_rms * 1.5f;

    /* Peak detection */
    uint16_t beat_count = 0U;
    uint32_t last_peak_idx = 0U;
    for (uint32_t i = 1U; i < (RING_BUFFER_CAPACITY - 1U); i++)
    {
        if ((fabsf(clean_fetal_ecg[i]) > dynamic_peak_threshold) &&
            (fabsf(clean_fetal_ecg[i]) > fabsf(clean_fetal_ecg[i - 1U])) &&
            (fabsf(clean_fetal_ecg[i]) > fabsf(clean_fetal_ecg[i + 1U])))
        {
            if ((last_peak_idx > 0U) && (beat_count < MAX_BEATS))
            {
                float rr_ms = (float)(i - last_peak_idx) * SAMPLE_PERIOD_MS;
                if ((rr_ms >= 250.0f) && (rr_ms <= 800.0f))
                {
                    rr_intervals_ms[beat_count] = rr_ms;
                    beat_count++;
                }
            }
            last_peak_idx = i;
            i += REFRACTORY_SAMPLES;
        }
    }
    if (beat_count < 10U) return false;

    /* PARITY FIX 1: build FHR (bpm) series from RR intervals */
    for (uint16_t i = 0U; i < beat_count; i++)
        fhr_bpm[i] = 60000.0f / rr_intervals_ms[i];

    /* Baseline + Mean HR (bpm). PARITY FIX 5: arithmetic mean of FHR. */
    arm_mean_f32(fhr_bpm, beat_count, &output_features->mean_hr_bpm);
    output_features->lb_bpm = output_features->mean_hr_bpm;   /* median ideal; mean ok */

    /* PARITY FIX 4: variance of FHR in bpm^2 (was RR var in ms^2) */
    arm_var_f32(fhr_bpm, beat_count, &output_features->hr_variance);

    /* PARITY FIX 2: MSTV = mean |dFHR| in bpm (was |dRR| in ms) */
    float sum_diff = 0.0f;
    for (uint16_t i = 0U; i < (beat_count - 1U); i++)
        sum_diff += fabsf(fhr_bpm[i + 1U] - fhr_bpm[i]);
    output_features->mstv_ms = sum_diff / (float)(beat_count - 1U);  /* value is bpm */

    /* PARITY FIX 3: MLTV = mean per-block (max-min) of FHR bpm (was RR ms) */
    float mltv_sum = 0.0f;
    uint16_t segments = 0U;
    for (uint16_t i = 0U; i < beat_count; i += 15U)
    {
        if ((i + 15U) < beat_count)
        {
            float max_fhr = fhr_bpm[i];
            float min_fhr = fhr_bpm[i];
            for (uint16_t j = i; j < (i + 15U); j++)
            {
                if (fhr_bpm[j] > max_fhr) max_fhr = fhr_bpm[j];
                if (fhr_bpm[j] < min_fhr) min_fhr = fhr_bpm[j];
            }
            mltv_sum += (max_fhr - min_fhr);
            segments++;
        }
    }
    output_features->mltv_ms = (segments > 0U) ? (mltv_sum / (float)segments) : 0.0f; /* bpm */

    /*----------------------------------------------------------------
     * Accelerations / Decelerations  (UNCHANGED from Malay -- episodes,
     * per-minute because the window is 60 s).
     *---------------------------------------------------------------*/
    output_features->accel_count = 0.0f;
    output_features->decel_count = 0.0f;
    bool accel_active = false, decel_active = false;
    float accel_duration_ms = 0.0f, decel_duration_ms = 0.0f;

    for (uint16_t i = 0U; i < beat_count; i++)
    {
        float instant_bpm = fhr_bpm[i];
        if (instant_bpm >= (output_features->lb_bpm + 15.0f))
        {
            accel_duration_ms += rr_intervals_ms[i];
            accel_active = true;
            if (decel_active) {
                if (decel_duration_ms >= 15000.0f) output_features->decel_count += 1.0f;
                decel_active = false; decel_duration_ms = 0.0f;
            }
        }
        else if (instant_bpm <= (output_features->lb_bpm - 15.0f))
        {
            decel_duration_ms += rr_intervals_ms[i];
            decel_active = true;
            if (accel_active) {
                if (accel_duration_ms >= 15000.0f) output_features->accel_count += 1.0f;
                accel_active = false; accel_duration_ms = 0.0f;
            }
        }
        else
        {
            if (accel_active) {
                if (accel_duration_ms >= 15000.0f) output_features->accel_count += 1.0f;
                accel_active = false; accel_duration_ms = 0.0f;
            }
            if (decel_active) {
                if (decel_duration_ms >= 15000.0f) output_features->decel_count += 1.0f;
                decel_active = false; decel_duration_ms = 0.0f;
            }
        }
    }
    if (accel_active && accel_duration_ms >= 15000.0f) output_features->accel_count += 1.0f;
    if (decel_active && decel_duration_ms >= 15000.0f) output_features->decel_count += 1.0f;

    /*----------------------------------------------------------------
     * PVDF Kick Count (UNCHANGED from Malay)
     *---------------------------------------------------------------*/
    float pvdf_rms = 0.0f;
    arm_rms_f32(flat_pvdf, RING_BUFFER_CAPACITY, &pvdf_rms);
    output_features->fetal_movements = 0.0f;
    float pvdf_threshold = pvdf_rms * 4.0f;
    for (uint32_t i = 1U; i < (RING_BUFFER_CAPACITY - 1U); i++)
    {
        if ((fabsf(flat_pvdf[i]) > pvdf_threshold) &&
            (fabsf(flat_pvdf[i]) > fabsf(flat_pvdf[i - 1U])) &&
            (fabsf(flat_pvdf[i]) > fabsf(flat_pvdf[i + 1U])))
        {
            output_features->fetal_movements += 1.0f;
            i += SAMPLE_RATE_HZ;   /* 1 s refractory */
        }
    }

    minute_window_ready = false;
    return true;
}
