/*
 * wombcare_ble.c — BLE GATT service for WombCare (SCAFFOLD, owner: Nikolaos)
 * --------------------------------------------------------------------------
 * This is a working skeleton: it compiles once the GATT Configurator has emitted
 * the characteristic handle (gattdb_clinical_update) and the Bluetooth component
 * is added. Every spot that needs Nikolaos's input is marked  // >>> TODO(O2) /
 * // >>> TODO. See wombcare_ble.h for the full GATT spec + checklist.
 */

#include "wombcare_ble.h"

#include <string.h>

/* Silicon Labs Bluetooth stack + generated GATT database. */
#include <sl_bluetooth.h>
#include <sl_bt_api.h>
#include <gatt_db.h>            /* provides gattdb_clinical_update after config */

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

static uint8_t clamp_u8(float v)
{
    if (v < 0.0f)   return 0u;
    if (v > 255.0f) return 255u;
    return (uint8_t)(v + 0.5f);
}

void wombcare_ble_send_clinical_update(WombCareNSP_t nsp,
                                       uint8_t confidence,
                                       float   fhr_bpm,
                                       float   kick_count)
{
    /* Pack the 8-byte payload (schema in wombcare_ble.h). */
    WombCareBlePayload_t p;
    p.version    = WOMBCARE_BLE_PAYLOAD_VERSION;
    p.nsp        = (uint8_t)nsp;
    p.confidence = (confidence > 100u) ? 100u : confidence;
    p.fhr_bpm    = clamp_u8(fhr_bpm);
    p.kick_count = clamp_u8(kick_count);
    p.flags      = 0u;
    if (nsp == WOMBCARE_PATHOLOGIC) p.flags |= WOMBCARE_FLAG_ALERT;
    /* >>> TODO: set WOMBCARE_FLAG_SIGNAL_LOW / _ACTIVE from IMU trust if desired */
    p.timestamp  = s_tick_minutes++;

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
 * Nikolaos fills the marked cases.
 * ------------------------------------------------------------------------ */
void sl_bt_on_event(sl_bt_msg_t *evt)
{
    switch (SL_BT_MSG_ID(evt->header))
    {
        /* ---- Stack booted: set up + start advertising -------------------- */
        case sl_bt_evt_system_boot_id:
        {
            wombcare_ble_init();

            // >>> TODO(O2): create advertiser set + set timing, then start.
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
            // if (evt->data.evt_gatt_server_characteristic_status.characteristic
            //         == gattdb_clinical_update &&
            //     evt->data.evt_gatt_server_characteristic_status.status_flags
            //         == sl_bt_gatt_server_client_config) {
            //     uint16_t cfg = evt->data.evt_gatt_server_characteristic_status
            //                        .client_config_flags;
            //     s_notifications_on = (cfg & sl_bt_gatt_notification);
            // }
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

        default:
            break;
    }
}
