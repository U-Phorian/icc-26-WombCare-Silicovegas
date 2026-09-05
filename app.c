#include "app.h"
#include "wombcare_battery.h"
#include "wombcare_sensors.h"
#include "wombcare_buffer.h"
#include "wombcare_dsp.h"
#include "wombcare_trend.h" /* rolling beat trend behind the features */
#include "wombcare_imu.h"
#include "wombcare_ml.h"    /* TinyML NSP classifier (extern "C")   */
#include "wombcare_ble.h"   /* BLE clinical-update notification      */
#include "wombcare_debug.h" /* app_log shim, compiles either way     */

#include "em_iadc.h"
#include "em_letimer.h"

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
 * The TinyML and BLE stages can each be switched off independently and
 * the project still links.
 *
 *   WOMBCARE_ENABLE_ML  requires (all present as of this build):
 *       - the aiml "Tensorflow Lite Micro" + "ML Model" components,
 *         which generate autogen/sl_ml_model_wombcare_nsp.h, and
 *       - wombcare_ml.cc + scaler.c in the build
 *         (cmake_gcc/Wombcare_PreFinal_project.cmake).
 *     The model itself is embedded by the ML Model component; the old
 *     hand-rolled model_data.c is gone.
 *
 *   WOMBCARE_ENABLE_BLE requires:
 *       - the "Clinical Update" GATT characteristic in the GATT
 *         Configurator (emits gattdb_clinical_update), and
 *       - wombcare_ble.c added to the build.
 *-------------------------------------------------------------------*/
#ifndef WOMBCARE_ENABLE_ML
#define WOMBCARE_ENABLE_ML   1
#endif

#ifndef WOMBCARE_ENABLE_BLE
#define WOMBCARE_ENABLE_BLE  1
#endif

/* How often to sample the battery, in milliseconds. */
#define BATTERY_INTERVAL_MS   10000U

/*--------------------------------------------------------------------
 * CLINICAL FLAG THRESHOLDS  (payload v3, wombcare_ble.h)
 *
 * The two temporal flags are the only things in the packet that describe
 * anything beyond the current window, so they need history the rest of the
 * firmware does not keep. Both are measured in elapsed TIME rather than in
 * windows, because the DSP trigger fires once per second
 * (DSP_TRIGGER_INTERVAL_SAMPLES in wombcare_buffer.c), not once per minute --
 * counting windows would make "2 minutes" mean 2 seconds.
 *
 * The bpm/minute figures below are standard FIGO-style values. They are NOT
 * mine to set: have the clinical owner confirm them before this ships.
 *-------------------------------------------------------------------*/
/* Expressed in SECONDS and converted with the timer frequency, not with
 * sl_sleeptimer_ms_to_tick() -- that helper takes a uint16_t, so anything
 * past 65535 ms silently wraps (120000 ms becomes 54464). */
#define PERSIST_ALERT_S       120U      /* Pathologic held >= 2 min    */
#define SUST_BRADY_S          300U      /* LB under threshold >= 5 min */
#define BRADY_LB_BPM          110.0f    /* FIGO lower bound of normal  */

#define SECONDS_TO_TICKS(s)   (sl_sleeptimer_get_timer_frequency() * (uint32_t)(s))

/*--------------------------------------------------------------------
 * FETAL-LOSS DETECTION  (payload v4, flags2 bit 0)
 *
 * "The fetal heart rate has been very low, or absent, for a while -- and
 * the bad stretches do not have to be consecutive."
 *
 * That last part is why this is a ratio over a rolling horizon rather than
 * an unbroken run like the two flags above. A loose electrode gives a few
 * good seconds scattered through a bad minute, and an unbroken-run rule
 * resets on every one of them and never fires.
 *
 * The horizon is in SECONDS because a frame is produced every second, not
 * every minute -- the same trap the comment above describes. Three of every
 * five seconds in the last five minutes must be bad before this raises,
 * which is roughly three minutes of mostly-absent heartbeat.
 *
 * Raise and clear are deliberately different levels. With one threshold the
 * flag chatters on and off around the boundary, which on a phone screen
 * reads as a device fault rather than as a measurement.
 *-------------------------------------------------------------------*/
#define FETAL_LOW_BPM         70.0f     /* below this the second is "bad" */
#define FETAL_LOSS_HORIZON_S  300U      /* rolling 5-minute memory        */
#define FETAL_LOSS_RAISE_S    180U      /* 3-of-5 of the horizon -> raise  */
#define FETAL_LOSS_CLEAR_S    120U      /* 2-of-5 of the horizon -> clear  */

/* Below this IMU trust the window is flagged as motion-contaminated. */
#define IMU_TRUST_LOW         50U

/*----------------------------------------------------
 * POWER / MONITORING STATE
 *
 * The device begins monitoring automatically after power-up: ECG
 * acquisition, IMU sampling and battery monitoring all start in
 * app_init() and keep running.
 *
 * BTN0 remains a manual override. Pressing it stops the sensors and
 * releases the EM1 requirement held by the acquisition chain, letting
 * the Power Manager drop the part into EM2; pressing it again resumes.
 * The callback runs in interrupt context, so it only flips a request
 * flag and app_process_action() reconciles the hardware.
 *---------------------------------------------------*/

volatile bool is_monitoring_active = true;

static volatile bool s_monitoring_requested = true;

/* Sleeptimer tick of the last IMU sample, used to pace sampling to ~26 Hz. */
static uint32_t s_imu_last_tick = 0U;

/* Sleeptimer tick of the last battery measurement request. */
static uint32_t s_last_battery_tick = 0U;

/*
 * Cross-window history for the two temporal clinical flags. Each records the
 * tick at which an unbroken run began; the run is broken (and the flag drops)
 * the moment a window disagrees. Cleared whenever monitoring stops so a new
 * session never inherits the previous one's alert state.
 */
static bool     s_patho_run_active = false;
static uint32_t s_patho_run_start  = 0U;

static bool     s_brady_run_active = false;
static uint32_t s_brady_run_start  = 0U;

/*
 * Rolling history for the fetal-loss flag: one bit per produced frame
 * (~one per second), 1 = that second was bad. A bit array rather than a
 * byte array because 300 bits is 38 bytes and this lives in static RAM
 * alongside ~450 KB of DSP buffers.
 *
 * s_fetal_bad_count is maintained incrementally -- the evicted bit is
 * subtracted and the new one added -- so raising the flag never costs a
 * 300-iteration scan in the super-loop.
 */
static uint8_t  s_fetal_bad_bits[(FETAL_LOSS_HORIZON_S + 7U) / 8U];
static uint16_t s_fetal_bad_count = 0U;
static uint16_t s_fetal_hist_pos  = 0U;
static uint16_t s_fetal_hist_len  = 0U;
static bool     s_fetal_not_detected = false;

/* Forget everything the fetal-loss flag has seen. Called when monitoring
 * stops, for the same reason the alert runs are cleared there: a new
 * session must not start already latched from the previous one. */
static void fetal_loss_reset(void)
{
    memset(s_fetal_bad_bits, 0, sizeof(s_fetal_bad_bits));

    s_fetal_bad_count    = 0U;
    s_fetal_hist_pos     = 0U;
    s_fetal_hist_len     = 0U;
    s_fetal_not_detected = false;
}

/*
 * Record one second's verdict and re-evaluate the flag.
 *
 * Returns true while the fetal heartbeat should be reported as not
 * detected.
 */
static bool fetal_loss_update(bool bad)
{
    const uint16_t byte_i = s_fetal_hist_pos / 8U;
    const uint8_t  mask   = (uint8_t)(1u << (s_fetal_hist_pos % 8U));

    /* Evict the entry this slot is about to overwrite. Only once the ring
     * has wrapped -- before that the slot holds nothing. */
    if (s_fetal_hist_len >= FETAL_LOSS_HORIZON_S)
    {
        if ((s_fetal_bad_bits[byte_i] & mask) != 0u)
        {
            s_fetal_bad_count--;
        }
    }
    else
    {
        s_fetal_hist_len++;
    }

    if (bad)
    {
        s_fetal_bad_bits[byte_i] |= mask;
        s_fetal_bad_count++;
    }
    else
    {
        s_fetal_bad_bits[byte_i] &= (uint8_t)~mask;
    }

    s_fetal_hist_pos = (uint16_t)((s_fetal_hist_pos + 1U)
                                  % FETAL_LOSS_HORIZON_S);

    /* Hysteresis: raise high, clear low, hold in between. */
    if (s_fetal_bad_count >= FETAL_LOSS_RAISE_S)
    {
        s_fetal_not_detected = true;
    }
    else if (s_fetal_bad_count <= FETAL_LOSS_CLEAR_S)
    {
        s_fetal_not_detected = false;
    }

    return s_fetal_not_detected;
}

/*--------------------------------------------------------------------
 * INITIALIZATION
 *-------------------------------------------------------------------*/

void app_init(void)
{
    WC_LOG("=== WOMBCARE APP STARTED ===\r\n");

    /*
     * Software modules first.
     */
    wombcare_buffer_init();

    wombcare_dsp_init();

    wombcare_imu_init();

    /*
     * Hardware next. wombcare_imu_init() runs before this and allocates
     * its SPIDRV channels from the DMA Manager, so the analog chain has
     * to take whatever LDMA channel is left — which is why
     * wombcare_hardware_init() allocates rather than hardcoding one.
     */
    wombcare_hardware_init();

    wombcare_battery_init();

    /* Start monitoring immediately after boot. */
    wombcare_imu_power_on();

    wombcare_analog_start();

    s_imu_last_tick     = sl_sleeptimer_get_tick_count();
    s_last_battery_tick = s_imu_last_tick;

    /*
     * Set up the TinyML interpreter once at startup. (BLE advertising
     * is started from the system_boot event in wombcare_ble.c, so no
     * BLE init is needed here.)
     */
#if WOMBCARE_ENABLE_ML
    wombcare_ml_init();

    /* Optional boot-time proof that the scaler + quant + MVP inference
     * path reproduces the trained model. See wombcare_ml.h. */
#if WOMBCARE_ML_SELFTEST
    wombcare_ml_selftest();
#endif
#endif
}

/*--------------------------------------------------------------------
 * ACQUISITION-CHAIN TRACE
 *
 * One line per second covering every stage the scan path depends on:
 * the LETIMER that drives PRS, the IADC status/flags, and the scan
 * FIFO level. A healthy chain shows LETIMER_CNT moving and SCANFIFO
 * hovering near zero because the LDMA is draining it; a stalled chain
 * shows SCANFIFO pinned at 0 with no DMA buffers arriving, or pinned
 * at its depth because the LDMA is not running.
 *
 * Deliberately rate-limited. Logging on every LETIMER count change
 * means ~32768 lines per second, which floods the UART and starves
 * the super-loop badly enough to look like a separate bug.
 *-------------------------------------------------------------------*/
#if WOMBCARE_DEBUG_LOGGING
static void wombcare_debug_trace(uint32_t now)
{
    static uint32_t last_trace_tick = 0U;

    if ((uint32_t)(now - last_trace_tick) < sl_sleeptimer_ms_to_tick(1000U))
    {
        return;
    }

    last_trace_tick = now;

    WC_LOG("LETIMER_CNT=%lu  IADC_IF=0x%08lX  IADC_STATUS=0x%08lX  SCANFIFO=%lu\r\n",
           (unsigned long)LETIMER_CounterGet(LETIMER0),
           (unsigned long)IADC0->IF,
           (unsigned long)IADC0->STATUS,
           (unsigned long)IADC0->SCANFIFOSTAT);
}
#endif

void app_process_action(void)
{
    uint32_t now = sl_sleeptimer_get_tick_count();

#if WOMBCARE_DEBUG_LOGGING
    wombcare_debug_trace(now);
#endif

    /*--------------------------------------------------------------
     * Apply any ON/OFF toggle requested by the BTN0 interrupt.
     *-------------------------------------------------------------*/

    if (s_monitoring_requested != is_monitoring_active)
    {
        is_monitoring_active = s_monitoring_requested;

        if (is_monitoring_active)
        {
            wombcare_imu_power_on();

            wombcare_analog_start();

            /* Reset the pacing clocks so the first sample is prompt. */
            s_imu_last_tick     = now;
            s_last_battery_tick = now;
        }
        else
        {
            wombcare_analog_stop();

            wombcare_imu_power_off();

            /* Start the next session fresh. Clearing minute_window_ready
             * alone is NOT enough: the ring buffers stay primed and would
             * blend stale data from this session into the next one. Fully
             * reset the buffers and IMU window instead. */
            wombcare_buffer_reset();
            wombcare_imu_reset();

            /* The beat trend outlives the ring buffers by design (it is
             * what carries history across windows), so resetting the
             * buffers alone would leave the next session classifying on
             * the last one's beats. */
            wombcare_trend_reset();

            /* Drop any in-progress alert run: the next session must not
             * start already latched from this one. */
            s_patho_run_active = false;
            s_brady_run_active = false;

            fetal_loss_reset();
        }
    }

    /*--------------------------------------------------------------
     * Idle: device is OFF. Do no work so the CPU can go back to sleep.
     *-------------------------------------------------------------*/

    if (!is_monitoring_active)
    {
        return;
    }

    /*--------------------------------------------------------------
     * Battery monitoring.
     *
     * Runs on its own timer, independent of the DSP window, so a
     * stalled or invalid minute never blocks battery reporting.
     * The conversion itself is tailgated onto the Single Queue and
     * cannot interrupt the 250 Hz ECG scan.
     *-------------------------------------------------------------*/

    if ((uint32_t)(now - s_last_battery_tick)
        >= sl_sleeptimer_ms_to_tick(BATTERY_INTERVAL_MS))
    {
        s_last_battery_tick = now;

        wombcare_battery_request_measurement();
    }

    if (wombcare_battery_start_conversion())
    {
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
        uint32_t interval =
            sl_sleeptimer_get_timer_frequency() / IMU_SAMPLE_RATE_HZ;

        if ((uint32_t)(now - s_imu_last_tick) >= interval)
        {
            s_imu_last_tick = now;
            wombcare_imu_sample();
        }
    }

    /*--------------------------------------------------------------
     * Wait until the rolling 60-second window has fresh data.
     * wombcare_dsp_run_pipeline() clears the flag.
     *-------------------------------------------------------------*/

    if (!minute_window_ready)
    {
        return;
    }

    /*--------------------------------------------------------------
     * Run DSP Feature Extraction
     *
     * The feature vector is a local: nothing else needs a shared copy
     * (ML consumes it immediately, BLE keeps its own).
     *-------------------------------------------------------------*/

    WombCareFeatures_t features;
    WombCareVitals_t   vitals;

    bool window_ok = wombcare_dsp_run_pipeline(&features, &vitals);

    if (!window_ok)
    {
        /*
         * The pipeline only clears minute_window_ready on its success path,
         * so a rejected window would otherwise leave the flag set and the
         * whole LMS + peak-detection pass would re-run on every super-loop
         * iteration until the next trigger. Consume the flag here.
         *
         * We deliberately do NOT return: a rejected window is reported as a
         * frame with FLAG_SENSOR_FAULT set. Going silent is indistinguishable
         * from a crash on the phone, and silence is exactly what a bradycardic
         * trace used to produce.
         */
        minute_window_ready = false;
    }

    /*--------------------------------------------------------------
     * Compute IMU Confidence (movement "trust", 0..100)
     *-------------------------------------------------------------*/

    wombcare_imu_compute_confidence();
    app_log_info("IMU Total Variance: %d\r\n", (int)(imu_result.total_variance * 1000.0f));

    uint8_t imu_trust = wombcare_imu_get_confidence();

    /*==============================================================
     * TinyML Inference + BLE Notification
     *
     * app.c only: (a) runs ML on the feature vector, (b) fuses the ML
     * confidence with the IMU trust, (c) maintains the cross-window history
     * the temporal flags need, and (d) pushes ONE clinical update. All
     * SM/GATT/advertising handling lives in sl_bt_on_event() in
     * wombcare_ble.c — nothing of that goes here.
     *
     * ML and BLE are INDEPENDENT gates. With BLE off nothing is transmitted;
     * with ML off the classification is reported as ANALYSIS FAILED rather
     * than Normal, because "no classifier" is not the same as "healthy".
     *==============================================================*/

    WombCareNSP_t nsp        = WOMBCARE_NORMAL;
    bool          nsp_valid  = false;        /* false -> wire value 3 */
    uint8_t       confidence = imu_trust;    /* 0..100 */

#if WOMBCARE_ENABLE_ML

    if (window_ok)
    {
        /* Classify the window: Normal / Suspect / Pathologic + confidence. */
        WombCareResult_t ml = wombcare_ml_run(&features);

        if (ml.ok)
        {
            nsp       = ml.nsp;
            nsp_valid = true;

            /* Fuse ML confidence (0..1) with IMU trust (0..100) into a single
             * 0..100 dashboard confidence. Heavy maternal movement (low trust)
             * pulls the reported confidence down. */
            confidence = (uint8_t)((ml.confidence * (float)imu_trust) + 0.5f);
        }
    }

#endif /* WOMBCARE_ENABLE_ML */

    /*--------------------------------------------------------------
     * Temporal clinical flags.
     *
     * Both track an unbroken run and compare ELAPSED TIME, not a window
     * count -- see the threshold block at the top of this file.
     *-------------------------------------------------------------*/

    if (nsp_valid && (nsp == WOMBCARE_PATHOLOGIC))
    {
        if (!s_patho_run_active)
        {
            s_patho_run_active = true;
            s_patho_run_start  = now;
        }
    }
    else
    {
        s_patho_run_active = false;
    }

    /* A rejected window breaks the bradycardia run: no measurement is not
     * evidence of a normal baseline, but it is not evidence of a low one
     * either, and latching an alert on absent data would be worse. */
    if (window_ok && (features.lb_bpm > 0.0f) &&
        (features.lb_bpm < BRADY_LB_BPM))
    {
        if (!s_brady_run_active)
        {
            s_brady_run_active = true;
            s_brady_run_start  = now;
        }
    }
    else
    {
        s_brady_run_active = false;
    }

    /*--------------------------------------------------------------
     * Assemble the flags byte (layout in wombcare_ble.h).
     *-------------------------------------------------------------*/

    uint8_t flags = 0U;

    if (!(tracker_mother.is_primed &&
          tracker_fetal.is_primed  &&
          tracker_pvdf.is_primed))
    {
        flags |= WOMBCARE_FLAG_INITIALIZING;
    }

    if (is_monitoring_active)
    {
        flags |= WOMBCARE_FLAG_MONITORING;
    }

    if (!window_ok)
    {
        flags |= WOMBCARE_FLAG_SENSOR_FAULT;
    }

    flags |= WOMBCARE_FLAG_NSP(nsp_valid ? (uint8_t)nsp
                                         : WOMBCARE_NSP_WIRE_UNKNOWN);

    if (imu_trust < IMU_TRUST_LOW)
    {
        flags |= WOMBCARE_FLAG_MOTION;
    }

    if (s_patho_run_active &&
        ((uint32_t)(now - s_patho_run_start) >= SECONDS_TO_TICKS(PERSIST_ALERT_S)))
    {
        flags |= WOMBCARE_FLAG_PERSIST_ALERT;
    }

    if (s_brady_run_active &&
        ((uint32_t)(now - s_brady_run_start) >= SECONDS_TO_TICKS(SUST_BRADY_S)))
    {
        flags |= WOMBCARE_FLAG_SUST_BRADY;
    }

    /*--------------------------------------------------------------
     * Assemble flags2 (v4, layout in wombcare_ble.h).
     *-------------------------------------------------------------*/

    uint8_t flags2 = 0U;

    /*
     * Is this second "bad" for the purposes of the fetal-loss rule?
     *
     * Three ways to be bad, and they are deliberately pooled rather than
     * flagged separately: from the mother's point of view "I cannot find
     * the baby's heartbeat" is one situation, whether the cause was a
     * rejected window, no rhythm found, or a rate too low to be credible.
     */
    const bool fetal_low = window_ok &&
                           (features.lb_bpm > 0.0f) &&
                           (features.lb_bpm < FETAL_LOW_BPM);

    const bool fetal_bad = (!window_ok) ||
                           (!vitals.fetal_lock) ||
                           (features.lb_bpm <= 0.0f) ||
                           fetal_low;

    if (fetal_loss_update(fetal_bad))
    {
        flags2 |= WOMBCARE_F2_FETAL_NOT_DETECTED;
    }

    if (fetal_low)
    {
        flags2 |= WOMBCARE_F2_FETAL_HR_LOW;
    }

    if (vitals.mhr_valid)
    {
        flags2 |= WOMBCARE_F2_MHR_VALID;
    }

    /*
     * The DSP now discards beats above its plausible ceiling rather than
     * reporting them. Surfacing that lets the app say "poor signal"
     * instead of leaving a reading unexplained -- these windows used to be
     * published as confident 160-200 bpm readings.
     */
    if (vitals.fhr_high_rejected)
    {
        flags2 |= WOMBCARE_F2_FHR_HIGH_REJECTED;
    }

    /*
     * The abdominal sensor was following the mother rather than the baby.
     * Distinguished from a plain loss of signal because the remedy is
     * different and the user can act on it: move the sensor.
     */
    if (vitals.fetal_is_maternal)
    {
        flags2 |= WOMBCARE_F2_FETAL_IS_MATERNAL;
    }

    WC_LOG("WINDOW: ok=%u nsp=%u conf=%u fhr=%u mhr=%u q=%u kicks=%u "
           "flags=0x%02X flags2=0x%02X bad=%u/%u batt=%u%%\r\n",
           (unsigned)window_ok,
           (unsigned)(nsp_valid ? (uint8_t)nsp : WOMBCARE_NSP_WIRE_UNKNOWN),
           (unsigned)confidence,
           (unsigned)(window_ok ? features.lb_bpm : 0.0f),
           (unsigned)(vitals.mhr_valid ? vitals.mhr_bpm : 0.0f),
           (unsigned)vitals.fetal_quality,
           (unsigned)(window_ok ? features.fetal_movements : 0.0f),
           (unsigned)flags,
           (unsigned)flags2,
           (unsigned)s_fetal_bad_count,
           (unsigned)FETAL_LOSS_HORIZON_S,
           (unsigned)wombcare_battery_get_percentage());

#if WOMBCARE_ENABLE_BLE

    /*
     * Push one clinical update. Safe no-op when no phone is connected /
     * subscribed. Passing NULL for the features zeroes the clinical bytes;
     * FLAG_SENSOR_FAULT is what marks them as unknown rather than measured.
     */
    wombcare_ble_send_clinical_update(
        flags,
        flags2,
        confidence,
        window_ok ? &features : NULL,
        &vitals,                       /* valid even on a rejected window */
        imu_trust);

    wombcare_ble_send_battery_level(
        wombcare_battery_get_percentage());

#else
    (void)flags;
    (void)flags2;
    (void)confidence;
    (void)features;
    (void)vitals;
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

