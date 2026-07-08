#include "wombcare_dsp.h"
#include "wombcare_buffer.h"
#include "arm_math.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#define NUM_TAPS 32
#define MAX_BEATS 300

// ---------------------------------------------------------
// DSP PIPELINE INTERNAL STATE
// ---------------------------------------------------------
static arm_lms_norm_instance_f32 lms_instance;
static float lms_state[RING_BUFFER_CAPACITY + NUM_TAPS - 1];
static float lms_coeffs[NUM_TAPS] = {0};

static float flat_mother_ecg[RING_BUFFER_CAPACITY];
static float flat_abdom_ecg[RING_BUFFER_CAPACITY];
static float clean_fetal_ecg[RING_BUFFER_CAPACITY];
static float flat_pvdf[RING_BUFFER_CAPACITY];
static float rr_intervals_ms[MAX_BEATS];

// ---------------------------------------------------------
// HELPER FUNCTIONS
// ---------------------------------------------------------
// Helper: Linearize the ring buffer
static void unwrap_buffer(float *ring, float *flat, uint32_t head) {
    uint32_t idx = 0;
    for (uint32_t i = head; i < RING_BUFFER_CAPACITY; i++) flat[idx++] = ring[i];
    for (uint32_t i = 0; i < head; i++) flat[idx++] = ring[i];
}

// ---------------------------------------------------------
// PUBLIC DSP API
// ---------------------------------------------------------
void wombcare_dsp_init(void) {
    arm_lms_norm_init_f32(&lms_instance, NUM_TAPS, lms_coeffs, lms_state, 0.01f, RING_BUFFER_CAPACITY);
}

bool wombcare_dsp_run_pipeline(WombCareFeatures_t *output_features) {
    // Verify buffers are primed
    if (!tracker_mother.is_primed) return false;

    // Linearize ring buffers for processing
    unwrap_buffer(ring_mother_ecg, flat_mother_ecg, tracker_mother.head);
    unwrap_buffer(ring_fetal_ecg, flat_abdom_ecg, tracker_fetal.head);
    unwrap_buffer(ring_pvdf_kick, flat_pvdf, tracker_pvdf.head);

    // 1. Maternal Cancellation
    float error_out[RING_BUFFER_CAPACITY]; 
    arm_lms_norm_f32(&lms_instance, flat_mother_ecg, flat_abdom_ecg, clean_fetal_ecg, error_out, RING_BUFFER_CAPACITY);

    // 2. Dynamic RMS Threshold for Pan-Tompkins
    float signal_rms = 0;
    arm_rms_f32(clean_fetal_ecg, RING_BUFFER_CAPACITY, &signal_rms);
    float dynamic_peak_threshold = signal_rms * 1.5f; 

    // 3. Fetal Peak Tracking (250 Hz scale)
    uint16_t beat_count = 0;
    uint32_t last_peak_idx = 0;

    for (uint32_t i = 1; i < RING_BUFFER_CAPACITY - 1; i++) {
        if (clean_fetal_ecg[i] > dynamic_peak_threshold && 
            clean_fetal_ecg[i] > clean_fetal_ecg[i-1] && 
            clean_fetal_ecg[i] > clean_fetal_ecg[i+1]) {
            
            if (last_peak_idx > 0 && beat_count < MAX_BEATS) {
                rr_intervals_ms[beat_count] = (float)(i - last_peak_idx) * 4.0f; // 4ms per sample at 250Hz
                beat_count++;
            }
            last_peak_idx = i;
            i += 50; // 200ms refractory period to avoid T-wave double counting
        }
    }

    if (beat_count < 10) return false;

    // 4. Exact 8-Feature Vector Generation
    float mean_rr = 0;
    arm_mean_f32(rr_intervals_ms, beat_count, &mean_rr);
    output_features->lb_bpm = 60000.0f / mean_rr;
    output_features->mean_hr_bpm = output_features->lb_bpm; 

    arm_var_f32(rr_intervals_ms, beat_count, &output_features->hr_variance);

    float sum_diff = 0;
    for(uint16_t i = 0; i < beat_count - 1; i++) {
        sum_diff += fabsf(rr_intervals_ms[i+1] - rr_intervals_ms[i]);
    }
    output_features->mstv_ms = sum_diff / (beat_count - 1);

    // MLTV: Divide beats into 10-second segments
    float mltv_sum = 0;
    int segments = 0;
    for(uint16_t i = 0; i < beat_count; i += 15) { // Approx 15 beats in 10s
        if (i + 15 < beat_count) {
            float max_rr = rr_intervals_ms[i], min_rr = rr_intervals_ms[i];
            for(int j = i; j < i + 15; j++) {
                if(rr_intervals_ms[j] > max_rr) max_rr = rr_intervals_ms[j];
                if(rr_intervals_ms[j] < min_rr) min_rr = rr_intervals_ms[j];
            }
            mltv_sum += (max_rr - min_rr);
            segments++;
        }
    }
    output_features->mltv_ms = segments > 0 ? (mltv_sum / segments) : 0;

    // Clinical Accelerations & Decelerations (Must be sustained >= 15 seconds)
    output_features->accel_count = 0;
    output_features->decel_count = 0;
    float accel_time_ms = 0, decel_time_ms = 0;

    for(uint16_t i = 0; i < beat_count; i++) {
        float instant_bpm = 60000.0f / rr_intervals_ms[i];
        
        if (instant_bpm >= (output_features->lb_bpm + 15.0f)) {
            accel_time_ms += rr_intervals_ms[i];
            decel_time_ms = 0;
            if (accel_time_ms >= 15000.0f) { 
                output_features->accel_count++; 
                accel_time_ms = 0; 
            }
        } else if (instant_bpm <= (output_features->lb_bpm - 15.0f)) {
            decel_time_ms += rr_intervals_ms[i];
            accel_time_ms = 0;
            if (decel_time_ms >= 15000.0f) { 
                output_features->decel_count++; 
                decel_time_ms = 0; 
            }
        } else {
            accel_time_ms = 0; 
            decel_time_ms = 0;
        }
    }

    // PVDF Kick Count (Adaptive Threshold)
    float pvdf_rms = 0;
    arm_rms_f32(flat_pvdf, RING_BUFFER_CAPACITY, &pvdf_rms);
    output_features->fetal_movements = 0; 
    
    for (uint32_t i = 0; i < RING_BUFFER_CAPACITY; i++) {
        if (flat_pvdf[i] > (pvdf_rms * 4.0f)) {
            output_features->fetal_movements++;
            i += 250; // 1-second physical recovery lockout
        }
    }
    return true; 
}