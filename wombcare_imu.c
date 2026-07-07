#include "wombcare_imu.h"
#include "em_gpio.h"
#include "sl_sleeptimer.h"
#include "sl_power_manager.h"
// Include your GSDK IMU driver header here (e.g., #include "sl_imu.h")

volatile wombcare_state_t current_system_state = SYSTEM_STATE_IMU_EVAL;
sl_sleeptimer_timer_handle_t macro_sleep_timer;

// ---------------------------------------------------------
// TIMING CALLBACKS
// ---------------------------------------------------------
static void macro_sleep_callback(sl_sleeptimer_timer_handle_t *handle, void *data) {
    (void)handle;
    (void)data;
    // 30 minutes have passed. Wake up and evaluate the IMU.
    current_system_state = SYSTEM_STATE_IMU_EVAL;
}

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------
void wombcare_imu_init(void) {
    // Configure PA10 as an output for the IMU Isolation Switch
    GPIO_PinModeSet(IMU_ENABLE_PORT, IMU_ENABLE_PIN, gpioModePushPull, 0);
}

void wombcare_imu_power_on(void) {
    GPIO_PinOutSet(IMU_ENABLE_PORT, IMU_ENABLE_PIN);
    // Allow sensor to boot (~2-5ms usually required)
    sl_sleeptimer_delay_millisecond(5);
    
    // TODO: Call your specific GSDK sl_imu_init() or SPI init here
}

void wombcare_imu_power_off(void) {
    // TODO: Call your specific GSDK sl_imu_deinit() here
    
    // Cut physical power to the IMU isolation switch
    GPIO_PinOutClear(IMU_ENABLE_PORT, IMU_ENABLE_PIN);
}

// ---------------------------------------------------------
// STATISTICAL EVALUATION (Variance of X & Y)
// ---------------------------------------------------------
void wombcare_evaluate_rest_state(void) {
    if (current_system_state != SYSTEM_STATE_IMU_EVAL) return;

    wombcare_imu_power_on();

    float sum_x = 0, sum_y = 0;
    float sum_sq_x = 0, sum_sq_y = 0;
    int16_t raw_x, raw_y, raw_z; 
    
    // The 30-Second Polling Loop
    for (uint32_t i = 0; i < TOTAL_EVAL_SAMPLES; i++) {
        // TODO: Replace with actual GSDK IMU read function:
        // sl_imu_read_accel(&raw_x, &raw_y, &raw_z); 
        
        // Convert to Gs (assuming typical 16-bit +/- 2G scale)
        float accel_x = (float)raw_x / 16384.0f; 
        float accel_y = (float)raw_y / 16384.0f;
        // Z is intentionally ignored as per respiratory rejection logic

        sum_x += accel_x;
        sum_y += accel_y;
        sum_sq_x += (accel_x * accel_x);
        sum_sq_y += (accel_y * accel_y);

        // Sleep to maintain the 26 Hz sample rate (~38ms)
        sl_sleeptimer_delay_millisecond(38);
    }

    wombcare_imu_power_off();

    // Calculate Variance: Var(X) = E(X^2) - (E(X))^2
    float mean_x = sum_x / TOTAL_EVAL_SAMPLES;
    float mean_y = sum_y / TOTAL_EVAL_SAMPLES;
    
    float var_x = (sum_sq_x / TOTAL_EVAL_SAMPLES) - (mean_x * mean_x);
    float var_y = (sum_sq_y / TOTAL_EVAL_SAMPLES) - (mean_y * mean_y);
    
    float total_xy_variance = var_x + var_y;

    // Gate Logic
    if (total_xy_variance > MOVEMENT_VARIANCE_THRESHOLD) {
        // Mother is moving. Go to deep sleep for 30 minutes.
        current_system_state = SYSTEM_STATE_DEEP_SLEEP;
        
        // Ensure IADC/LDMA are paused to save power (add your pause logic here)
        
        uint32_t ticks_30_min = sl_sleeptimer_ms_to_tick(MACRO_SLEEP_MINUTES * 60 * 1000);
        sl_sleeptimer_start_timer(&macro_sleep_timer, ticks_30_min, macro_sleep_callback, NULL, 0, 0);
    } else {
        // Mother is resting. Proceed to data acquisition.
        current_system_state = SYSTEM_STATE_DATA_ACQ;
        
        // Start your Phase 1 IADC/LDMA sequence here
    }
}