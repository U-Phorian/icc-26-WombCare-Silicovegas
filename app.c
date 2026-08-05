#include "app.h"
#include "em_iadc.h"
#include "wombcare_battery.h"
#include "wombcare_sensors.h"
#include "wombcare_buffer.h"
#include "wombcare_dsp.h"
#include "wombcare_imu.h"
#include "wombcare_ml.h"    /* TinyML NSP classifier (extern "C")   */
#include "wombcare_ble.h"   /* BLE clinical-update notification      */


/* Simple Button driver (instance "btn0" created in the GATT/.slcp GUI). */
#include "sl_simple_button_instances.h"

/* Sleeptimer: used to pace IMU sampling to ~26 Hz. */
#include "sl_sleeptimer.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*--------------------------------------------------------------------
 * FEATURE GATES
 *
 * The TinyML and BLE stages are fully wired below, but each depends on
 * a component that is not in this build yet, so both default OFF and
 * the project still links. Set a macro to 1 once its dependency is in
 * place (e.g. as a -D flag or in the .slcp):
 *
 *   WOMBCARE_ENABLE_ML  requires:
 *       - the "TensorFlow Lite Micro" component (provides the
 *         tensorflow/lite/micro headers), and
 *       - wombcare_ml.cc, scaler.c, model_data.c added to the build.
 *
 *   WOMBCARE_ENABLE_BLE requires:
 *       - the "Clinical Update" GATT characteristic in the GATT
 *         Configurator (emits gattdb_clinical_update), and
 *       - wombcare_ble.c added to the build (and its own TODOs fixed).
 *     BLE also consumes the ML result, so it implies WOMBCARE_ENABLE_ML.
 *-------------------------------------------------------------------*/
#ifndef WOMBCARE_ENABLE_ML
#define WOMBCARE_ENABLE_ML   0
#endif

#ifndef WOMBCARE_ENABLE_BLE
#define WOMBCARE_ENABLE_BLE  1
#endif



static uint8_t s_battery_update_counter = 0U;


/*--------------------------------------------------------------------
 * POWER / MONITORING STATE
 *
 * WombCare Edge boots ASLEEP to save battery. BTN0 acts as a single
 * hardware toggle switch:
 *
 *      press once   -> wake IMU + analog sensors, begin monitoring
 *      press again  -> shut IMU + analog sensors down
 *
 * When the sensors and timers are off, nothing holds the MCU in EM1,
 * so the Silicon Labs power manager lets the EFR32MG26 drop into EM2
 * Deep Sleep (microamp range). The BTN0 GPIO interrupt wakes it again.
 *
 * The button callback runs in interrupt context, so it only flips a
 * request flag. The actual (heavier) hardware power transition is
 * performed in app_process_action() below, in thread context.
 *-------------------------------------------------------------------*/

/* True while sensors are running and we are collecting data.
 * Reflects the real hardware state; other modules (e.g. BLE) may read it. */
volatile bool is_monitoring_active = true;

/* Toggled by the BTN0 ISR; app_process_action() reconciles hardware to it. */
static volatile bool s_monitoring_requested = true;

/* Sleeptimer tick of the last IMU sample, used to pace sampling to ~26 Hz. */
static uint32_t s_imu_last_tick = 0U;

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
     * Initialize hardware (clocks, IADC, LDMA, LETIMER, IMU GPIO).
     *
     * NOTE: the sensors are configured but NOT started here. The device
     * wakes up in deep sleep waiting for the mother to press BTN0.
     * (Previously app_init() called wombcare_imu_power_on() and
     * wombcare_analog_start() — those now happen on the button press.)
     */

    wombcare_hardware_init();


    wombcare_battery_init();

    /*
     * Set up the TinyML interpreter once at startup. (BLE advertising
     * is started from the system_boot event in wombcare_ble.c, so no
     * BLE init is needed here.)
     */
#if WOMBCARE_ENABLE_ML
    wombcare_ml_init();
#endif
}



void app_process_action(void)
{
    /*--------------------------------------------------------------
     * Apply any ON/OFF toggle requested by the BTN0 interrupt.
     *
     * This runs before the idle check below so a button press while
     * asleep is serviced immediately after the interrupt wakes us.
     *-------------------------------------------------------------*/

    if (s_monitoring_requested != is_monitoring_active)
    {
        is_monitoring_active = s_monitoring_requested;

        if (is_monitoring_active)
        {
            /* Mother started a reading: wake all sensors. */
            wombcare_imu_power_on();

            wombcare_analog_start();

            /* Reset the IMU pacing clock so the first sample is prompt. */
            s_imu_last_tick = sl_sleeptimer_get_tick_count();
        }
        else
        {
            /* Mother stopped: shut everything down to save battery.
             * With the LETIMER/LDMA/IADC idle the MCU can enter EM2. */
            wombcare_analog_stop();

            wombcare_imu_power_off();

            /* Start the next session fresh. Clearing minute_window_ready
             * alone is NOT enough: the ring buffers stay primed and would
             * blend stale data from this session into the next one. Fully
             * reset the buffers and IMU window instead. */
            wombcare_buffer_reset();
            wombcare_imu_reset();
        }
    }

    /*--------------------------------------------------------------
     * Idle: device is OFF. Do no work so the CPU can go back to sleep.
     *-------------------------------------------------------------*/

    if (!is_monitoring_active)
    {
        return;
    }

      // =================================================================
    // 1. RUN BATTERY MONITORING INDEPENDENTLY OF THE 1-MINUTE WINDOW
    // =================================================================
    static uint32_t s_last_battery_tick = 0;
    uint32_t current_tick = sl_sleeptimer_get_tick_count();
    
    // Request a battery measurement periodically (e.g., every 10 seconds)
    if ((current_tick - s_last_battery_tick) >= sl_sleeptimer_ms_to_tick(10000)) {
        s_last_battery_tick = current_tick;
        wombcare_battery_request_measurement();
    }

    // Trigger the conversion if requested
    if (wombcare_battery_start_conversion()) {
        IADC_command(IADC0, iadcCmdStartSingle);
    }  
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
     * IMU sampling, paced to ~26 Hz (every ~38 ms).
     *
     * The super-loop runs far faster than 26 Hz, so gate the sample on
     * the sleeptimer. Without this the one-minute IMU window would fill
     * in milliseconds and the movement confidence would be meaningless.
     *-------------------------------------------------------------*/

    {
        uint32_t now = sl_sleeptimer_get_tick_count();
        uint32_t interval =
            sl_sleeptimer_get_timer_frequency() / IMU_SAMPLE_RATE_HZ;

        if ((uint32_t)(now - s_imu_last_tick) >= interval)
        {
            s_imu_last_tick = now;
            wombcare_imu_sample();
        }
    }

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
     *
     * The one-minute feature vector is a local: nothing else needs a
     * shared copy (ML consumes it immediately, BLE keeps its own).
     *-------------------------------------------------------------*/

    WombCareFeatures_t features;

    if (!wombcare_dsp_run_pipeline(&features))
    {
        /*
         * Invalid minute.
         *
         * Wait for next minute.
         */

        return;
    }

    /*--------------------------------------------------------------
     * Compute IMU Confidence (movement "trust", 0..100)
     *-------------------------------------------------------------*/

    wombcare_imu_compute_confidence();

    uint8_t imu_trust = wombcare_imu_get_confidence();

    /*==============================================================
     * TinyML Inference + BLE Notification
     *
     * app.c only: (a) runs ML on the feature vector, (b) fuses the ML
     * confidence with the IMU trust, and (c) pushes ONE 8-byte clinical
     * update. All SM/GATT/advertising handling lives in sl_bt_on_event()
     * in wombcare_ble.c — nothing of that goes here.
     *
     * ML and BLE are INDEPENDENT gates. With BLE on but ML off, we still
     * push the measured FHR / kick count (class defaults to NORMAL with
     * IMU trust as the confidence), so the phone gets live data before the
     * TensorFlow Lite Micro component is added.
     *==============================================================*/

    /* Defaults for the "no ML" case: report the measured signal only. */
    WombCareNSP_t nsp        = WOMBCARE_NORMAL;
    uint8_t       confidence = imu_trust;   /* 0..100 */

#if WOMBCARE_ENABLE_ML

    /* Classify the minute: Normal / Suspect / Pathologic + confidence. */
    WombCareResult_t ml = wombcare_ml_run(&features);

    if (ml.ok)
    {
        nsp = ml.nsp;

        /* Fuse ML confidence (0..1) with IMU trust (0..100) into a single
         * 0..100 dashboard confidence. Heavy maternal movement (low trust)
         * pulls the reported confidence down. */
        confidence = (uint8_t)((ml.confidence * (float)imu_trust) + 0.5f);
    }

#endif /* WOMBCARE_ENABLE_ML */

#if WOMBCARE_ENABLE_BLE

    /*
     * Push one 8-byte clinical update. Safe no-op when no phone is
     * connected / subscribed.
     */
    wombcare_ble_send_clinical_update(
        nsp,                       /* 0=Normal, 1=Suspect, 2=Pathologic */
        confidence,                /* fused (or IMU-only) confidence     */
        features.lb_bpm,           /* baseline FHR (bpm)                 */
        features.fetal_movements); /* PVDF kick count                    */


    /*--------------------------------------------------------------
 * Request one battery measurement every 60 rolling updates.
 *-------------------------------------------------------------*/
if (++s_battery_update_counter >= 60U)
{
    s_battery_update_counter = 0U;

    wombcare_battery_request_measurement();
}

/*--------------------------------------------------------------
 * Start a pending battery conversion.
 *
 * Tailgating guarantees that the conversion waits until the
 * Scan Queue finishes, so the ECG acquisition is never
 * interrupted.
 *-------------------------------------------------------------*/
if (wombcare_battery_start_conversion())
{
    IADC_command(
        IADC0,
        iadcCmdStartSingle);
}


/*--------------------------------------------------------------
 * Check whether the battery conversion has completed.
 *
 * The conversion runs on the IADC Single Queue.
 * Because Tailgating is enabled, SINGLEDONE will only be
 * asserted after the Scan Queue has safely finished.
 *-------------------------------------------------------------*/
/*--------------------------------------------------------------
 * Check whether the battery conversion has completed.
 *
 * Read the completed result from the Single Queue FIFO.
 *-------------------------------------------------------------*/


    wombcare_ble_send_battery_level(
    wombcare_battery_get_percentage());

    // wombcare_ble_send_battery_level(55);
#else
    (void)nsp;
    (void)confidence;
    (void)features;
#endif /* WOMBCARE_ENABLE_BLE */
}

/*--------------------------------------------------------------------
 * BUTTON INTERRUPT CALLBACK
 *
 * The Simplicity Studio Simple Button driver calls this exact function
 * name on every button state change. It runs in interrupt context, so
 * keep it minimal: just flip the request flag and let the main loop do
 * the real work.
 *-------------------------------------------------------------------*/

void sl_button_on_change(const sl_button_t *handle)
{
    /* Act on the press (button down); ignore the release. */
    if (sl_button_get_state(handle) != SL_SIMPLE_BUTTON_PRESSED)
    {
        return;
    }

    /* Single-button toggle on BTN0. */
    if (handle == &sl_button_btn0)
    {
        s_monitoring_requested = !s_monitoring_requested;
    }
}
