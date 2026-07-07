#ifndef WOMBCARE_IMU_H
#define WOMBCARE_IMU_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_sleeptimer.h"

// ---------------------------------------------------------
// IMU HARDWARE PINS (BRD2608A)
// ---------------------------------------------------------
#define IMU_ENABLE_PORT  gpioPortA
#define IMU_ENABLE_PIN   10

// ---------------------------------------------------------
// TIMING & THRESHOLD CONFIGURATIONS
// ---------------------------------------------------------
#define MACRO_SLEEP_MINUTES    30
#define EVAL_WINDOW_SECONDS    30
#define IMU_SAMPLE_RATE_HZ     26
#define TOTAL_EVAL_SAMPLES     (EVAL_WINDOW_SECONDS * IMU_SAMPLE_RATE_HZ)

// Variance threshold for X/Y movement (needs tuning via field testing)
#define MOVEMENT_VARIANCE_THRESHOLD  0.15f 

typedef enum {
    SYSTEM_STATE_DEEP_SLEEP,     // 30 min cooldown
    SYSTEM_STATE_IMU_EVAL,       // 30 sec checking window
    SYSTEM_STATE_DATA_ACQ        // Mother at rest, running Phase 1-3
} wombcare_state_t;

extern volatile wombcare_state_t current_system_state;

void wombcare_imu_init(void);
void wombcare_imu_power_on(void);
void wombcare_imu_power_off(void);
void wombcare_evaluate_rest_state(void);

#endif // WOMBCARE_IMU_H