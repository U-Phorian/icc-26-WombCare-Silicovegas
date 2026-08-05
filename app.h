#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>

#include "wombcare_dsp.h"

/*--------------------------------------------------------------------
 * APPLICATION STATE
 *
 * There is deliberately no shared "result" struct here. The DSP
 * feature vector is a local in app_process_action(); the ML result
 * uses WombCareResult_t from wombcare_ml.h; and BLE keeps its own
 * last-payload copy. (app.h previously defined its own
 * WombCareResult_t, which collided with wombcare_ml.h's — see B1 in
 * app.c — so it was removed.)
 *-------------------------------------------------------------------*/

/*
 * True while WombCare is actively monitoring (BTN0 toggled ON).
 * False while asleep/idle. Toggled by the BTN0 button press.
 */
extern volatile bool is_monitoring_active;

/*--------------------------------------------------------------------
 * APPLICATION API
 *-------------------------------------------------------------------*/

/*
 * Initialize complete firmware.
 */
void app_init(void);

/*
 * Main application scheduler.
 */
void app_process_action(void);

/*--------------------------------------------------------------------
 * Silicon Labs baremetal compatibility layer (defined in app_bm.c).
 * These prototypes are required by autogen/sl_event_handler.c and the
 * SDK super-loop. They were lost when this custom app.h replaced the
 * stock SoC app.h — re-declaring them here fixes the
 * "implicit declaration of function 'app_init_bt'" build error.
 *-------------------------------------------------------------------*/
void app_init_bt(void);
void app_proceed(void);
bool app_is_process_required(void);
bool app_mutex_acquire(void);
void app_mutex_release(void);

#endif /* APP_H */

/*toolchain_settings:
  - value: -mfp16-format=ieee
    option: gcc_compiler_option

config_file:
  - path: config/tflite/inst0.mlconf
    directory: tflite
    override:
      component: ml_model
      file_id: ml_compiler_config
      instance: inst0 */