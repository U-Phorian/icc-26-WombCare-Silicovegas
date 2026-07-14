#include "app.h"

#include "wombcare_sensors.h"
#include "wombcare_buffer.h"
#include "wombcare_dsp.h"
#include "wombcare_imu.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*--------------------------------------------------------------------
 * APPLICATION RESULT
 *-------------------------------------------------------------------*/

WombCareResult_t current_result;

/*--------------------------------------------------------------------
 * INITIALIZATION
 *-------------------------------------------------------------------*/

void app_init(void)
{
    /*
     * Initialize software modules first.
     */

    wombcare_buffer_init();

    wombcare_dsp_init();

    wombcare_imu_init();

    /*
     * Initialize hardware.
     */

    wombcare_hardware_init();

    wombcare_imu_power_on();

    wombcare_analog_start();

    memset(
        &current_result,
        0,
        sizeof(WombCareResult_t));
}



void app_process_action(void)
{
    /*--------------------------------------------------------------
     * Process completed ADC DMA buffers
     *-------------------------------------------------------------*/

    if (ping_buffer_ready)
    {
        ping_buffer_ready = false;

        wombcare_buffer_ingest(adcBufferPing);
    }

    if (pong_buffer_ready)
    {
        pong_buffer_ready = false;

        wombcare_buffer_ingest(adcBufferPong);
    }

    /*--------------------------------------------------------------
     * Continuous IMU sampling
     *
     * This should be called every 38 ms
     * (26 Hz scheduler or timer callback).
     *-------------------------------------------------------------*/

    wombcare_imu_sample();

    /*--------------------------------------------------------------
     * Wait until one complete minute of ECG/PVDF data
     * has been collected.
     *-------------------------------------------------------------*/

    if (!minute_window_ready)
    {
        return;
    }

    /*--------------------------------------------------------------
     * Run DSP Feature Extraction
     *-------------------------------------------------------------*/

    if (!wombcare_dsp_run_pipeline(
            &current_result.features))
    {
        /*
         * Invalid minute.
         *
         * Wait for next minute.
         */

        return;
    }

    /*--------------------------------------------------------------
     * Compute IMU Confidence
     *-------------------------------------------------------------*/

    wombcare_imu_compute_confidence();

    current_result.imu_confidence =
        wombcare_imu_get_confidence();

    /*--------------------------------------------------------------
     * TinyML Inference
     *-------------------------------------------------------------*/

    /*
     * TODO
     *
     * current_result.ml_prediction =
     * wombcare_ml_run(
     *      &current_result.features);
     */

    /*--------------------------------------------------------------
     * BLE Notification
     *-------------------------------------------------------------*/

    /*
     * TODO
     *
     * wombcare_ble_send(
     *      &current_result);
     */
}