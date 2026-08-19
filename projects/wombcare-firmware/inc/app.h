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
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

#include "wombcare_dsp.h"

/*--------------------------------------------------------------------
 * APPLICATION RESULT
 *-------------------------------------------------------------------*/

/*
 * Everything the mobile application needs every minute.
 */

typedef struct
{
    WombCareFeatures_t features;

    uint8_t imu_confidence;

    uint8_t ml_prediction;

} WombCareResult_t;

/*--------------------------------------------------------------------
 * GLOBAL RESULT
 *-------------------------------------------------------------------*/

extern WombCareResult_t current_result;

/*--------------------------------------------------------------------
 * APPLICATION API
 *-------------------------------------------------------------------*/

/*
 * Initialize complete firmware.
 */
void app_init(void);

/*
 * Main application scheduler.
 */
void app_process_action(void);

#endif /* APP_H */