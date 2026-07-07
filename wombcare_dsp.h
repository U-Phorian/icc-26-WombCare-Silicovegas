#ifndef WOMBCARE_DSP_H
#define WOMBCARE_DSP_H

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------
// TINYML FEATURE VECTOR STRUCTURE
// ---------------------------------------------------------
typedef struct {
    float lb_bpm;           // Baseline Fetal Heart Rate
    float mstv_ms;          // Mean Short-Term Variability
    float mltv_ms;          // Mean Long-Term Variability
    float accel_count;      // Accelerations (AC)
    float decel_count;      // Decelerations (DL/DS)
    float fetal_movements;  // Kick Count (FM)
    float mean_hr_bpm;      // Overall Mean HR
    float hr_variance;      // Heart Rate Variance
} WombCareFeatures_t;

// ---------------------------------------------------------
// PROTOTYPES
// ---------------------------------------------------------
void wombcare_dsp_init(void);
bool wombcare_dsp_run_pipeline(WombCareFeatures_t *output_features);

#endif // WOMBCARE_DSP_H