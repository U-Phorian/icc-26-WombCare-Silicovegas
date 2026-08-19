/***************************************************************************//**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 ******************************************************************************/
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