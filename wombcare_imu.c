#include "wombcare_imu.h"

#include "em_gpio.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * Replace this include with the Silicon Labs
 * IMU driver used on the BRD2608A.
 *
 * Example:
 *
 * #include "sl_imu.h"
 */

/*--------------------------------------------------------------------
 * GLOBAL RESULT
 *-------------------------------------------------------------------*/

IMU_Result_t imu_result;

/*--------------------------------------------------------------------
 * INTERNAL SAMPLE STORAGE
 *-------------------------------------------------------------------*/

/*
 * One-minute acceleration buffers.
 *
 * These are intentionally static so they
 * remain allocated only once.
 */

static float accel_x_buffer[IMU_WINDOW_SAMPLES];
static float accel_y_buffer[IMU_WINDOW_SAMPLES];
static float accel_z_buffer[IMU_WINDOW_SAMPLES];

/*
 * Current write position.
 */

static uint32_t imu_sample_index = 0U;

/*
 * Indicates one complete minute
 * of IMU samples is available.
 */

static volatile bool imu_window_ready = false;

/*--------------------------------------------------------------------
 * INITIALIZATION
 *-------------------------------------------------------------------*/

void wombcare_imu_init(void)
{
    GPIO_PinModeSet(
        IMU_ENABLE_PORT,
        IMU_ENABLE_PIN,
        gpioModePushPull,
        0);

    memset(&imu_result, 0, sizeof(IMU_Result_t));

    memset(accel_x_buffer, 0, sizeof(accel_x_buffer));
    memset(accel_y_buffer, 0, sizeof(accel_y_buffer));
    memset(accel_z_buffer, 0, sizeof(accel_z_buffer));

    imu_sample_index = 0U;
    imu_window_ready = false;
}

/*--------------------------------------------------------------------
 * POWER CONTROL
 *-------------------------------------------------------------------*/

void wombcare_imu_power_on(void)
{
    GPIO_PinOutSet(
        IMU_ENABLE_PORT,
        IMU_ENABLE_PIN);

    /*
     * Allow sensor startup.
     */

    /*
     * TODO
     * sl_imu_init();
     */
}

void wombcare_imu_power_off(void)
{
    /*
     * TODO
     * sl_imu_deinit();
     */

    GPIO_PinOutClear(
        IMU_ENABLE_PORT,
        IMU_ENABLE_PIN);
}

/*--------------------------------------------------------------------
 * HARDWARE ABSTRACTION
 *-------------------------------------------------------------------*/

/*
 * Reads one acceleration sample from the IMU.
 *
 * Returns:
 *      true  -> New sample available
 *      false -> Read failed
 *
 * This function is the ONLY place where the
 * Silicon Labs IMU driver will be used.
 */
static bool imu_read_acceleration(
    float *accel_x,
    float *accel_y,
    float *accel_z)
{
    /*
     * TODO
     *
     * Replace with Silicon Labs IMU API.
     *
     * Example:
     *
     * sl_imu_update();
     * sl_imu_get_acceleration(accel_x,
     *                         accel_y,
     *                         accel_z);
     */

    *accel_x = 0.0f;
    *accel_y = 0.0f;
    *accel_z = 1.0f;

    return true;
}

/*--------------------------------------------------------------------
 * CONTINUOUS SAMPLE ACQUISITION
 *-------------------------------------------------------------------*/

void wombcare_imu_sample(void)
{
    float accel_x;
    float accel_y;
    float accel_z;

    /*
     * Acquire one sample.
     */

    if (!imu_read_acceleration(
            &accel_x,
            &accel_y,
            &accel_z))
    {
        return;
    }

    /*
     * Store into one-minute buffers.
     */

    accel_x_buffer[imu_sample_index] = accel_x;
    accel_y_buffer[imu_sample_index] = accel_y;
    accel_z_buffer[imu_sample_index] = accel_z;

    imu_sample_index++;

    /*
     * One complete minute collected.
     */

    if (imu_sample_index >= IMU_WINDOW_SAMPLES)
    {
        imu_sample_index = 0U;

        imu_window_ready = true;
    }
}

/*--------------------------------------------------------------------
 * MOVEMENT CONFIDENCE COMPUTATION
 *-------------------------------------------------------------------*/

void wombcare_imu_compute_confidence(void)
{
    if (!imu_window_ready)
    {
        return;
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float sum_z = 0.0f;

    /*
     * Mean Calculation
     */

    for (uint32_t i = 0U;
         i < IMU_WINDOW_SAMPLES;
         i++)
    {
        sum_x += accel_x_buffer[i];
        sum_y += accel_y_buffer[i];
        sum_z += accel_z_buffer[i];
    }

    float mean_x = sum_x / (float)IMU_WINDOW_SAMPLES;
    float mean_y = sum_y / (float)IMU_WINDOW_SAMPLES;
    float mean_z = sum_z / (float)IMU_WINDOW_SAMPLES;

    /*
     * Variance Calculation
     */

    float variance_x = 0.0f;
    float variance_y = 0.0f;
    float variance_z = 0.0f;

    for (uint32_t i = 0U;
         i < IMU_WINDOW_SAMPLES;
         i++)
    {
        float dx = accel_x_buffer[i] - mean_x;
        float dy = accel_y_buffer[i] - mean_y;
        float dz = accel_z_buffer[i] - mean_z;

        variance_x += dx * dx;
        variance_y += dy * dy;
        variance_z += dz * dz;
    }

    variance_x /= (float)IMU_WINDOW_SAMPLES;
    variance_y /= (float)IMU_WINDOW_SAMPLES;
    variance_z /= (float)IMU_WINDOW_SAMPLES;

    /*
     * Save individual variances.
     */

    imu_result.variance_x = variance_x;
    imu_result.variance_y = variance_y;
    imu_result.variance_z = variance_z;

    /*
     * Total movement metric.
     */

    imu_result.total_variance =
        variance_x +
        variance_y +
        variance_z;

    /*
     * Convert movement variance
     * into confidence.
     */

    float confidence;

    if (imu_result.total_variance <= IMU_VARIANCE_MIN)
    {
        confidence = 100.0f;
    }
    else if (imu_result.total_variance >= IMU_VARIANCE_MAX)
    {
        confidence = 0.0f;
    }
    else
    {
        confidence =
            100.0f *
            (1.0f -
             ((imu_result.total_variance -
               IMU_VARIANCE_MIN) /
              (IMU_VARIANCE_MAX -
               IMU_VARIANCE_MIN)));
    }

    /*
     * Clamp.
     */

    if (confidence < 0.0f)
    {
        confidence = 0.0f;
    }

    if (confidence > 100.0f)
    {
        confidence = 100.0f;
    }

    imu_result.confidence_score =
        (uint8_t)(confidence + 0.5f);

    /*
     * Window processed.
     */

    imu_window_ready = false;
}

/*--------------------------------------------------------------------
 * PUBLIC API
 *-------------------------------------------------------------------*/

/*
 * Returns the latest movement confidence score.
 *
 * Range:
 *      0   -> Very high maternal movement
 *      100 -> Very stable / resting
 */
uint8_t wombcare_imu_get_confidence(void)
{
    return imu_result.confidence_score;
}