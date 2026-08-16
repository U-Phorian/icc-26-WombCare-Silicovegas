#ifndef WOMBCARE_BLE_H
#define WOMBCARE_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "wombcare_ml.h"   /* WombCareNSP_t -- the values packed into flags bits 4-3 */
#include "wombcare_dsp.h"  /* WombCareFeatures_t */

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

/* ==========================================================================
 *  Payload v3 — 15 bytes
 * --------------------------------------------------------------------------
 *  version MUST stay at byte 0. The app reads it before it knows the layout
 *  (ClinicalUpdateParser.kt: `expectedSizeFor(bytes.u8(0))`), so a version
 *  placed anywhere else is unparseable by construction.
 *
 *  Version numbering: 1 = the old 8-byte frame, 2 is ALREADY CLAIMED by the
 *  app's 10-byte v1+motion+battery layout (docs/BLE_CONTRACT.md §4) even
 *  though firmware never shipped it. Hence 3.
 *
 *  byte  field         notes
 *  ----  ------------  -------------------------------------------------
 *   0    version       == 3
 *   1    flags         see the bit table below
 *   2    confidence    0..100  (ML confidence x IMU trust)
 *   3    fhr_bpm       LB, median FHR in bpm. 0 = invalid / no lock
 *   4    kick_count    fetal movements this window
 *   5    mstv_x10      MSTV in bpm x 10      (Feature Spec: bpm, NOT ms)
 *   6    mltv_x4       MLTV in bpm x 4       (x4 so 0..63.75 fits a byte)
 *   7    accel_x10     accelerations/min x 10
 *   8    decel_x10     decelerations/min x 10
 *   9    mean_hr_bpm   mean FHR in bpm
 *  10    hr_sd_x10     std-dev of FHR in bpm x 10 (SD, not variance: the
 *                      variance in bpm^2 overflows a byte, its root does not
 *                      and is the more readable figure anyway)
 *  11-12 timestamp     u16 LE, device window counter since boot
 *  13    motion_state  0 = resting, 1 = sitting, 2 = walking
 *  14    reserved      0
 *
 *  Bytes 5..10 are zero when the DSP rejected the window; FLAG_SENSOR_FAULT
 *  is set in that case and the frame is still sent, so the app can show
 *  "poor signal" instead of going silent.
 * ========================================================================== */
#define WOMBCARE_BLE_PAYLOAD_VERSION   3u
#define WOMBCARE_BLE_PAYLOAD_SIZE      15u

/*  bit  meaning
 *  ---  --------------------------------------------------------------
 *   7   persistent clinical alert  (Pathologic held >= 2 min)
 *   6   sustained bradycardia      (LB below threshold >= 5 min)
 *   5   motion detected            (low IMU trust, signal less reliable)
 *  4-3  NSP: 0 Normal, 1 Suspect, 2 Pathologic, 3 ANALYSIS FAILED
 *   2   sensor fault / DSP rejected this window
 *   1   monitoring active
 *   0   initializing (ring buffers not primed yet)
 */
#define WOMBCARE_FLAG_INITIALIZING   (1u << 0)
#define WOMBCARE_FLAG_MONITORING     (1u << 1)
#define WOMBCARE_FLAG_SENSOR_FAULT   (1u << 2)
#define WOMBCARE_FLAG_NSP_SHIFT      (3u)
#define WOMBCARE_FLAG_NSP_MASK       (3u << WOMBCARE_FLAG_NSP_SHIFT)
#define WOMBCARE_FLAG_MOTION         (1u << 5)
#define WOMBCARE_FLAG_SUST_BRADY     (1u << 6)
#define WOMBCARE_FLAG_PERSIST_ALERT  (1u << 7)

/* Wire value for "the classifier produced no answer" (inference failed, or
 * the window was rejected). Previously this case was reported as Normal --
 * i.e. the device said HEALTHY when it did not know. */
#define WOMBCARE_NSP_WIRE_UNKNOWN    (3u)

#define WOMBCARE_FLAG_NSP(v) \
    (((uint8_t)(v) << WOMBCARE_FLAG_NSP_SHIFT) & WOMBCARE_FLAG_NSP_MASK)

/* byte 13 */
typedef enum {
    WOMBCARE_MOTION_RESTING = 0,
    WOMBCARE_MOTION_SITTING = 1,
    WOMBCARE_MOTION_WALKING = 2
} WombCareMotion_t;

typedef struct __attribute__((packed))
{
    uint8_t  version;
    uint8_t  flags;
    uint8_t  confidence;
    uint8_t  fhr_bpm;
    uint8_t  kick_count;
    uint8_t  mstv_x10;
    uint8_t  mltv_x4;
    uint8_t  accel_x10;
    uint8_t  decel_x10;
    uint8_t  mean_hr_bpm;
    uint8_t  hr_sd_x10;
    uint16_t timestamp;
    uint8_t  motion_state;
    uint8_t  reserved;
} WombCareBlePayload_t;    /* sizeof == WOMBCARE_BLE_PAYLOAD_SIZE (15) */

void wombcare_ble_send_battery_level(uint8_t battery_percent);
/* --- Public API ------------------------------------------------------------ */

/* Call once (from the system_boot event path). Sets up advertising. */
void wombcare_ble_init(void);

/* Pack + notify one clinical update. Safe no-op if nobody is subscribed.
 *
 * flags     : fully built by app.c -- it owns the monitoring lifecycle and the
 *             cross-window history the temporal bits need, so the whole byte
 *             is assembled in one place rather than half here and half there.
 * features  : NULL when the DSP rejected the window; bytes 5..10 are then 0.
 * imu_trust : 0..100, used only to derive motion_state (byte 13). */
void wombcare_ble_send_clinical_update(uint8_t flags,
                                       uint8_t confidence,
                                       const WombCareFeatures_t *features,
                                       uint8_t imu_trust);

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
