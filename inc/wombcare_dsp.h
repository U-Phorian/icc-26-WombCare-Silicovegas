#ifndef WOMBCARE_DSP_H
#define WOMBCARE_DSP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*----------------------------------------------------------
 * TinyML Feature Vector
 *
 * Order MUST remain identical to the trained model input.
 *---------------------------------------------------------*/
typedef struct
{
    float lb_bpm;            /* Baseline Fetal Heart Rate (BPM) */
    float mstv_ms;           /* Mean Short-Term Variability */
    float mltv_ms;           /* Mean Long-Term Variability */
    float accel_count;       /* Accelerations */
    float decel_count;       /* Decelerations */
    float fetal_movements;   /* PVDF Kick Count */
    float mean_hr_bpm;       /* Mean Heart Rate */
    float hr_variance;       /* Heart Rate Variance */

} WombCareFeatures_t;

/*----------------------------------------------------------
 * Public API
 *---------------------------------------------------------*/

void wombcare_dsp_init(void);

bool wombcare_dsp_run_pipeline(
    WombCareFeatures_t *output_features);

#endif /* WOMBCARE_DSP_H */