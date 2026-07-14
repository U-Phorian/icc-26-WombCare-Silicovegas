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

/* Derived timing constants */
#define SAMPLE_PERIOD_MS      (1000.0f / SAMPLE_RATE_HZ)
#define REFRACTORY_SAMPLES    (SAMPLE_RATE_HZ / 5U)



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

/* Working buffers */
static float flat_mother_ecg[RING_BUFFER_CAPACITY];
static float flat_abdom_ecg[RING_BUFFER_CAPACITY];
static float flat_pvdf[RING_BUFFER_CAPACITY];

/*
 * Output of the adaptive filter.
 * This is the estimated maternal ECG present in the abdomen.
 */


/*
 * LMS error signal.
 * This is the maternal-cancelled fetal ECG used by all
 * downstream processing stages.
 */
static float estimated_maternal_ecg[RING_BUFFER_CAPACITY];
static float clean_fetal_ecg[RING_BUFFER_CAPACITY];


static float rr_intervals_ms[MAX_BEATS];

/*--------------------------------------------------------------------
 * HELPER FUNCTIONS
 *-------------------------------------------------------------------*/

/*
 * Convert circular buffer into a linear buffer.
 */
static void unwrap_buffer(const float *ring,
                          float *flat,
                          uint32_t head)
{
    uint32_t idx = 0U;

    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++)
    {
        flat[idx++] = ring[i];
    }

    for (uint32_t i = 0U; i < head; i++)
    {
        flat[idx++] = ring[i];
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
     * Linearize ring buffers
     *-------------------------------------------------------------*/
    unwrap_buffer(ring_mother_ecg,
                  flat_mother_ecg,
                  tracker_mother.head);

    unwrap_buffer(ring_fetal_ecg,
                  flat_abdom_ecg,
                  tracker_fetal.head);

    unwrap_buffer(ring_pvdf_kick,
                  flat_pvdf,
                  tracker_pvdf.head);

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
    float signal_rms = 0.0f;

    arm_rms_f32(
    clean_fetal_ecg,
    RING_BUFFER_CAPACITY,
    &signal_rms);

if (signal_rms < 1.0f)
{
    return false;
}

float dynamic_peak_threshold =
    signal_rms * 1.5f;

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

                    /* Reject physiologically impossible RR intervals */
                    if ((rr_ms >= 250.0f) &&
                        (rr_ms <= 800.0f))
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
     * Heart Rate
     *-------------------------------------------------------------*/
    float mean_rr = 0.0f;

    arm_mean_f32(
        rr_intervals_ms,
        beat_count,
        &mean_rr);

    if (mean_rr <= 0.0f)
    {
        return false;
    }

    output_features->lb_bpm = 60000.0f / mean_rr;
    output_features->mean_hr_bpm = output_features->lb_bpm;

    arm_var_f32(
        rr_intervals_ms,
        beat_count,
        &output_features->hr_variance);

    /*--------------------------------------------------------------
     * MSTV
     *-------------------------------------------------------------*/
    float sum_diff = 0.0f;

    for (uint16_t i = 0U;
         i < (beat_count - 1U);
         i++)
    {
        sum_diff += fabsf(
            rr_intervals_ms[i + 1U] -
            rr_intervals_ms[i]);
    }

    output_features->mstv_ms =
        sum_diff / (float)(beat_count - 1U);

    /*--------------------------------------------------------------
     * MLTV
     *-------------------------------------------------------------*/
    float mltv_sum = 0.0f;
    uint16_t segments = 0U;

    for (uint16_t i = 0U;
         i < beat_count;
         i += 15U)
    {
        if ((i + 15U) < beat_count)
        {
            float max_rr = rr_intervals_ms[i];
            float min_rr = rr_intervals_ms[i];

            for (uint16_t j = i;
                 j < (i + 15U);
                 j++)
            {
                if (rr_intervals_ms[j] > max_rr)
                {
                    max_rr = rr_intervals_ms[j];
                }

                if (rr_intervals_ms[j] < min_rr)
                {
                    min_rr = rr_intervals_ms[j];
                }
            }

            mltv_sum += (max_rr - min_rr);
            segments++;
        }
    }

    if (segments > 0U)
    {
        output_features->mltv_ms =
            mltv_sum / (float)segments;
    }
    else
    {
        output_features->mltv_ms = 0.0f;
    }

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
    float instant_bpm =
        60000.0f / rr_intervals_ms[i];

    /*----------------------------------------------------------
     * ACCELERATION
     *---------------------------------------------------------*/
    if (instant_bpm >=
        (output_features->lb_bpm + 15.0f))
    {
        accel_duration_ms += rr_intervals_ms[i];
        accel_active = true;

        /* End any running deceleration */
        if (decel_active)
        {
            if (decel_duration_ms >= 15000.0f)
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
             (output_features->lb_bpm - 15.0f))
    {
        decel_duration_ms += rr_intervals_ms[i];
        decel_active = true;

        /* End any running acceleration */
        if (accel_active)
        {
            if (accel_duration_ms >= 15000.0f)
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
            if (accel_duration_ms >= 15000.0f)
            {
                output_features->accel_count += 1.0f;
            }

            accel_active = false;
            accel_duration_ms = 0.0f;
        }

        if (decel_active)
        {
            if (decel_duration_ms >= 15000.0f)
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
    accel_duration_ms >= 15000.0f)
{
    output_features->accel_count += 1.0f;
}

if (decel_active &&
    decel_duration_ms >= 15000.0f)
{
    output_features->decel_count += 1.0f;
}
    /*--------------------------------------------------------------
     * PVDF Kick Count
     *-------------------------------------------------------------*/
    float pvdf_rms = 0.0f;

    arm_rms_f32(
        flat_pvdf,
        RING_BUFFER_CAPACITY,
        &pvdf_rms);

    output_features->fetal_movements = 0.0f;

float pvdf_threshold = pvdf_rms * 4.0f;

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
/* One-minute feature vector has been consumed. */
    minute_window_ready = false;
    return true;
}