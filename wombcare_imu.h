#ifndef WOMBCARE_IMU_H
#define WOMBCARE_IMU_H

#include <stdint.h>
#include <stdbool.h>

#include <em_gpio.h>

/*--------------------------------------------------------------------
 * IMU Hardware Configuration
 *-------------------------------------------------------------------*/

#define IMU_ENABLE_PORT         gpioPortA
#define IMU_ENABLE_PIN          10U

/*--------------------------------------------------------------------
 * IMU Acquisition Configuration
 *-------------------------------------------------------------------*/

/* Continuous IMU sampling rate */
#define IMU_SAMPLE_RATE_HZ      26U

/* One-minute processing window */
#define IMU_WINDOW_SECONDS      60U

/* Total IMU samples collected each minute */
#define IMU_WINDOW_SAMPLES \
    (IMU_SAMPLE_RATE_HZ * IMU_WINDOW_SECONDS)

/*
 * Movement confidence tuning.
 *
 * Variance below MIN corresponds to
 * confidence = 100.
 *
 * Variance above MAX corresponds to
 * confidence = 0.
 *
 * These values are expected to be tuned
 * during real-world validation.
 */
#define IMU_VARIANCE_MIN        0.0025f
#define IMU_VARIANCE_MAX        0.2000f

/*--------------------------------------------------------------------
 * IMU Result Structure
 *-------------------------------------------------------------------*/

typedef struct
{
    float variance_x;
    float variance_y;
    float variance_z;

    float total_variance;

    uint8_t confidence_score;

} IMU_Result_t;

/*--------------------------------------------------------------------
 * Global Result
 *-------------------------------------------------------------------*/

extern IMU_Result_t imu_result;

/*--------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------*/

/*
 * Initialize IMU hardware.
 */
void wombcare_imu_init(void);

/*
 * Power IMU ON.
 */
void wombcare_imu_power_on(void);

/*
 * Power IMU OFF.
 */
void wombcare_imu_power_off(void);

/*
 * Acquire one IMU sample.
 *
 * Called periodically (26 Hz).
 */
void wombcare_imu_sample(void);

/*
 * Compute movement confidence using
 * the previous one-minute window.
 */
void wombcare_imu_compute_confidence(void);

/*
 * Get latest confidence score.
 */
uint8_t wombcare_imu_get_confidence(void);

#endif /* WOMBCARE_IMU_H */