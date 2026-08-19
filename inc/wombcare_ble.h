#ifndef WOMBCARE_BLE_H
#define WOMBCARE_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "wombcare_ml.h"   /* WombCareNSP_t */

/* ==========================================================================
 *  WombCare BLE — GATT SERVICE SPECIFICATION  (owner: Nikolaos)
 *  Open item O2: finalize the 128-bit UUIDs and confirm topology
 *  (phone <-> MG26 direct, or via Pi relay).
 *
 *  Everything the app needs is pushed once per 60-second window as ONE compact
 *  notification (WombCareBlePayload_t below). Optional per-field characteristics
 *  are listed too, in case the app team prefers separate reads.
 *
 *  >>> WHAT NIKOLAOS MUST CREATE (checklist) <<<
 *
 *  [1] In the GATT Configurator (gatt_configuration.btconf):
 *      - Custom Service  "WombCare Fetal Wellness"
 *          UUID: TODO-O2  (e.g. e0000000-1000-4000-8000-00805f9b34fb)
 *      - Characteristic  "Clinical Update"   [Notify]  value length = 8 bytes
 *          UUID: TODO-O2
 *          Add the Client Characteristic Configuration Descriptor (CCCD) so the
 *          app can subscribe. Give it the ID `clinical_update` so the generator
 *          emits `gattdb_clinical_update` in gatt_db.h.
 *      - (Optional) Battery Service 0x180F / Battery Level 0x2A19 [Read/Notify].
 *
 *  [2] Implement the event handling in wombcare_ble.c (sl_bt_on_event):
 *      - system_boot            -> create advertiser set, start advertising
 *      - connection_opened      -> store connection handle, stop advertising
 *      - connection_closed      -> clear handle, restart advertising
 *      - gatt_server_characteristic_status -> track CCCD (notifications on/off)
 *
 *  [3] Fill wombcare_ble_send_clinical_update() to pack + notify (skeleton given).
 *
 *  [4] app.c hooks (see APP.C INTEGRATION block below).
 *
 *  [5] Add the "Bluetooth" + "GATT Configurator" components in the .slcp, and
 *      make sure exactly ONE sl_bt_on_event() exists in the project (this file).
 * ========================================================================== */

/* --- Payload: one 8-byte clinical update (matches slide-7 "8 B" schema) ----- */
#define WOMBCARE_BLE_PAYLOAD_VERSION   1u

/* flags bitfield */
#define WOMBCARE_FLAG_ALERT       (1u << 0)   /* NSP == Pathologic (persisted) */
#define WOMBCARE_FLAG_SIGNAL_LOW  (1u << 1)   /* low IMU/signal trust          */
#define WOMBCARE_FLAG_ACTIVE      (1u << 2)   /* mother active (not resting)    */

typedef struct __attribute__((packed))
{
    uint8_t  version;      /* = WOMBCARE_BLE_PAYLOAD_VERSION (app checks first)  */
    uint8_t  nsp;          /* 0=Normal, 1=Suspect, 2=Pathologic                 */
    uint8_t  confidence;   /* 0..100  (fused ML confidence x IMU trust)          */
    uint8_t  fhr_bpm;      /* baseline fetal heart rate (bpm), 0 = invalid       */
    uint8_t  kick_count;   /* fetal movements in the window                      */
    uint8_t  flags;        /* WOMBCARE_FLAG_*                                    */
    uint16_t timestamp;    /* device tick / minutes, little-endian               */
} WombCareBlePayload_t;    /* sizeof == 8 */

/* --- Public API ------------------------------------------------------------ */

/* Call once (from the system_boot event path). Sets up advertising. */
void wombcare_ble_init(void);

/* Pack the latest result into WombCareBlePayload_t and notify the subscribed
 * client. Safe no-op if no client is connected/subscribed.
 * Called by app.c after DSP + ML each minute. (float args are clamped to uint8.) */
void wombcare_ble_send_clinical_update(WombCareNSP_t nsp,
                                       uint8_t confidence,
                                       float   fhr_bpm,
                                       float   kick_count);

/* True while a phone is connected and has enabled notifications. */
bool wombcare_ble_is_connected(void);

/* The single Bluetooth event handler for the project. Nikolaos implements the
 * cases listed in checklist [2]. (In Silicon Labs projects this IS sl_bt_on_event;
 * keep it in this file so there is exactly one.) */
/* void sl_bt_on_event(sl_bt_msg_t *evt);   // defined in wombcare_ble.c */

/* ==========================================================================
 *  APP.C INTEGRATION  — the "space" to keep in app.c (exact hooks)
 * --------------------------------------------------------------------------
 *  #include "wombcare_ble.h"          // near the other wombcare_* includes
 *
 *  // NOTE: do NOT create a second sl_bt_on_event() in app.c — it lives in
 *  //       wombcare_ble.c. The Bluetooth stack calls it automatically.
 *
 *  // (optional) if not using the boot-event path, call once in app_init():
 *  //   wombcare_ble_init();
 *
 *  // In app_process_action(), the existing call is already correct:
 *  //   wombcare_ble_send_clinical_update(
 *  //       ml_output.nsp,
 *  //       final_dashboard_confidence,   // 0..100 (see /100 fix note)
 *  //       clinical_features.lb_bpm,
 *  //       clinical_features.fetal_movements);
 * ========================================================================== */

#endif /* WOMBCARE_BLE_H */
