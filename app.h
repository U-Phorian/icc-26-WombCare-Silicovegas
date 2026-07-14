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