/*
 
 * This is a working skeleton: it compiles once the GATT Configurator has emitted
 * the characteristic handle (gattdb_clinical_update) and the Bluetooth component
 * is added. Every spot that needs Nikolaos's input is marked  // >>> TODO(O2) /
 * // >>> TODO. See wombcare_ble.h for the full GATT spec + checklist.
 */

#include "wombcare_ble.h"
#include <string.h>
#include <math.h>       /* sqrtf -- variance (bpm^2) is sent as SD (bpm) */

/* Silicon Labs Bluetooth stack + generated GATT database. */
#include <sl_bluetooth.h>
#include <sl_bt_api.h>
#include <gatt_db.h>            /* provides gattdb_clinical_update after config */

#include "wombcare_imu.h"
#include "app.h"                /* is_monitoring_active (device state flag) */

/* Logging is optional: wombcare_debug.h pulls in app_log.h when the
 * "Log" component is installed and no-ops otherwise, so these call
 * sites now produce real serial output instead of always compiling
 * away. Asserts stay stubbed deliberately — a failed BLE status must
 * not halt an in-progress recording. */
#include "wombcare_debug.h"

#if !WOMBCARE_DEBUG_LOGGING
#define app_log_info(...)      ((void)0)
#define app_log_warning(...)   ((void)0)
#endif

#ifndef APP_LOG_NL
#define APP_LOG_NL              ""
#endif

#define app_assert_status(sc)  ((void)(sc))

/* --------------------------------------------------------------------------
 * Connection / subscription state
 * ------------------------------------------------------------------------ */
static uint8_t  s_connection   = 0xFFu;   /* 0xFF = no active connection */
static uint8_t  s_advertiser   = 0xFFu;   /* advertising set handle      */
static bool     s_notifications_on = false;

/* Latest packed payload (kept so a late subscriber can be given last-known). */
static WombCareBlePayload_t s_last_payload;
static uint16_t s_tick_minutes = 0u;      /* simple monotonic timestamp  */

/* --------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------ */

void wombcare_ble_send_battery_level(uint8_t battery_percent)
{
    // Safety check: Battery cannot be over 100%
    if (battery_percent > 100 ) {
        battery_percent = 100;
    }

    if (s_connection != 0xFF)
{
    sl_bt_gatt_server_send_notification(
        s_connection,
        gattdb_battery_level,
        1,
        &battery_percent);
}

    // 2. Push a live Notification to the phone instantly
    // (Note: The connection_handle variable should be whatever you named your 
    // global connection ID inside sl_bt_on_event when the phone connected)
    // Send notification to ALL connected devices (0xFF)
    sl_bt_gatt_server_send_notification(
        0xFF, 
        gattdb_battery_level, 
        1, 
        &battery_percent
    );

}

void wombcare_ble_init(void)
{
    s_connection = 0xFFu;
    s_notifications_on = false;
    memset(&s_last_payload, 0, sizeof(s_last_payload));
    s_last_payload.version = WOMBCARE_BLE_PAYLOAD_VERSION;
    /* Advertising itself is started in the system_boot event (see below). */
}

bool wombcare_ble_is_connected(void)
{
    return (s_connection != 0xFFu) && s_notifications_on;
}

/* A wire-format drift here is silent and clinically dangerous, so make the
 * compiler enforce the size the GATT characteristic declares. */
_Static_assert(sizeof(WombCareBlePayload_t) == WOMBCARE_BLE_PAYLOAD_SIZE,
               "payload must match <value length=...> in gatt_configuration.btconf");

/* Scale a float feature into a byte, clamped. */
static uint8_t scale_u8(float v, float k)
{
    float s = v * k;
    if (s < 0.0f)   return 0u;
    if (s > 255.0f) return 255u;
    return (uint8_t)(s + 0.5f);
}

/* IMU trust (100 = perfectly still) bucketed into the three motion states the
 * app displays. Thresholds are first-pass and expected to move during
 * real-world validation, same caveat as IMU_VARIANCE_MIN/MAX. */
static uint8_t motion_from_trust(uint8_t imu_trust)
{
    if (imu_trust >= 70u) return (uint8_t)WOMBCARE_MOTION_RESTING;
    if (imu_trust >= 30u) return (uint8_t)WOMBCARE_MOTION_SITTING;
    return (uint8_t)WOMBCARE_MOTION_WALKING;
}

void wombcare_ble_send_clinical_update(uint8_t flags,
                                       uint8_t confidence,
                                       const WombCareFeatures_t *features,
                                       uint8_t imu_trust)
{
    WombCareBlePayload_t p;
    memset(&p, 0, sizeof(p));

    p.version      = WOMBCARE_BLE_PAYLOAD_VERSION;
    p.flags        = flags;
    p.confidence   = (confidence > 100u) ? 100u : confidence;
    p.timestamp    = s_tick_minutes++;
    p.motion_state = motion_from_trust(imu_trust);

    /* features == NULL means the DSP rejected this window. Everything derived
     * from it stays zero; FLAG_SENSOR_FAULT (set by app.c) is what tells the
     * app those zeros are "unknown", not "measured zero". */
    if (features != NULL)
    {
        p.fhr_bpm     = scale_u8(features->lb_bpm,          1.0f);
        p.kick_count  = scale_u8(features->fetal_movements, 1.0f);
        p.mstv_x10    = scale_u8(features->mstv_ms,        10.0f);  /* bpm */
        p.mltv_x4     = scale_u8(features->mltv_ms,         4.0f);  /* bpm */
        p.accel_x10   = scale_u8(features->accel_count,    10.0f);
        p.decel_x10   = scale_u8(features->decel_count,    10.0f);
        p.mean_hr_bpm = scale_u8(features->mean_hr_bpm,     1.0f);

        /* Send SD, not variance: bpm^2 overruns a byte on any lively trace,
         * its square root does not. */
        p.hr_sd_x10   = scale_u8(sqrtf(features->hr_variance), 10.0f);
    }

    s_last_payload = p;

    /* Notify the subscribed client. No-op if nobody is connected/subscribed. */
    if (wombcare_ble_is_connected())
    {
        // >>> TODO: confirm the characteristic ID name matches the GATT Configurator
        sl_bt_gatt_server_send_notification(
            s_connection,
            gattdb_clinical_update,             /* generated handle */
            sizeof(p),
            (const uint8_t *)&p);
    }
}

/* --------------------------------------------------------------------------
 * THE Bluetooth event handler (exactly one per project — keep it here).
 * ------------------------------------------------------------------------ */
void sl_bt_on_event(sl_bt_msg_t *evt)
{
    sl_status_t sc;
    switch (SL_BT_MSG_ID(evt->header))
    {
        /* ---- Stack booted: set up + start advertising -------------------- */
        case sl_bt_evt_system_boot_id:
        {
            wombcare_ble_init();

            /* --- NEW SECURITY CONFIGURATION FOR 6-DIGIT PIN --- */
            
            // 1. Configure Security Manager (0x0F requires MITM protection, bonding, and encryption)
            // "displayonly" tells the phone: "I have the PIN, you must type it in."
            sc = sl_bt_sm_configure(0x0F, sl_bt_sm_io_capability_displayonly);
            app_assert_status(sc);

            // 2. Set the static 6-digit PIN (Must be between 000000 and 999999)
            sc = sl_bt_sm_set_passkey(123456); 
            app_assert_status(sc);

            // 3. Tell the Bluetooth radio it is allowed to accept bonding requests
            sc = sl_bt_sm_set_bondable_mode(1);
            app_assert_status(sc);
            
            /* -------------------------------------------------- */

            sc = sl_bt_advertiser_create_set(&s_advertiser);
            app_assert_status(sc);

            sc = sl_bt_legacy_advertiser_generate_data(
                     s_advertiser,
                     sl_bt_advertiser_general_discoverable);
            app_assert_status(sc);

            sc = sl_bt_advertiser_set_timing(
                     s_advertiser, 160, 160, 0, 0);
            app_assert_status(sc);

            sc = sl_bt_legacy_advertiser_start(
                     s_advertiser,
                     sl_bt_legacy_advertiser_connectable);
            app_assert_status(sc);

            app_log_info("WombCare BLE: advertising started" APP_LOG_NL);
            break;
        }

        /* ---- Phone connected -------------------------------------------- */
        case sl_bt_evt_connection_opened_id:
        {
            s_connection = evt->data.evt_connection_opened.connection;
            // >>> TODO: (optional) stop advertising while connected.
            app_log_info("BLE: phone connected (handle %d)" APP_LOG_NL,
                         s_connection);

            // Stop advertising while connected
            sl_bt_advertiser_stop(s_advertiser);
            break;
        }

        /* ---- Phone disconnected: restart advertising -------------------- */
        case sl_bt_evt_connection_closed_id:
        {
            s_connection = 0xFFu;
            s_notifications_on = false;
            app_log_info("BLE: phone disconnected, restarting advertising"
                         APP_LOG_NL);

            sc = sl_bt_legacy_advertiser_start(
                     s_advertiser,
                     sl_bt_legacy_advertiser_connectable);
            app_assert_status(sc);
            break;
        }
        
        /* ---- Client enabled/disabled notifications (CCCD) --------------- */
        case sl_bt_evt_gatt_server_characteristic_status_id:
        {
            // >>> TODO: verify this is our characteristic + a CCCD change, then:
            if (evt->data.evt_gatt_server_characteristic_status.characteristic
                    == gattdb_clinical_update &&
                evt->data.evt_gatt_server_characteristic_status.status_flags
                    == sl_bt_gatt_server_client_config)
            {
                uint16_t cfg = evt->data
                    .evt_gatt_server_characteristic_status
                    .client_config_flags;
                s_notifications_on = (cfg & sl_bt_gatt_notification) != 0;

                if (s_notifications_on) {
                    app_log_info("BLE: phone subscribed to notifications"
                                 APP_LOG_NL);
                    // Send last known payload immediately
                    sl_bt_gatt_server_send_notification(
                        s_connection,
                        gattdb_clinical_update,
                        sizeof(s_last_payload),
                        (const uint8_t *)&s_last_payload);
                }
            }
            break;
        }

        case sl_bt_evt_sm_confirm_bonding_id:
            sl_bt_sm_bonding_confirm(
                evt->data.evt_sm_confirm_bonding.connection, 1);
            break;

        case sl_bt_evt_sm_confirm_passkey_id:
            sl_bt_sm_passkey_confirm(
                evt->data.evt_sm_confirm_passkey.connection, 1);
            break;

        case sl_bt_evt_sm_bonded_id:
            app_log_info("BLE: bonded and encrypted" APP_LOG_NL);
            break;

        case sl_bt_evt_sm_bonding_failed_id:
{
    uint8_t connection_handle =
        evt->data.evt_sm_bonding_failed.connection;

    uint16_t reason =
        evt->data.evt_sm_bonding_failed.reason;

    app_log_warning(
        "BLE: bonding failed 0x%04X"
        APP_LOG_NL,
        reason);

    if ((reason == SL_STATUS_BT_CTRL_PIN_OR_KEY_MISSING) ||
        (reason == SL_STATUS_BT_SMP_PAIRING_NOT_SUPPORTED))
    {
        app_log_warning(
            "Broken bond detected. Deleting stored bond."
            APP_LOG_NL);

        sc = sl_bt_sm_delete_bondings();
        app_assert_status(sc);
    }

       sl_bt_connection_close(connection_handle);

        break;
        }

        default:
            break;
    }
}
