#ifndef WOMBCARE_DEBUG_H
#define WOMBCARE_DEBUG_H

/*--------------------------------------------------------------------
 * Debug logging shim.
 *
 * The bring-up instrumentation prints over VCOM through the "Log"
 * (app_log) component. That component is optional: this header makes
 * every debug site compile whether or not it is installed, so the
 * firmware never fails to build just because logging was removed.
 *
 * To get real output, add these components to the project and
 * regenerate:
 *      Application > Utility > Log
 *      Services > IO Stream > Driver > IO Stream: EUSART   (vcom)
 *
 * Set WOMBCARE_DEBUG_LOGGING to 0 to silence all of it in one place
 * without deleting the call sites.
 *-------------------------------------------------------------------*/

#include "sl_component_catalog.h"

#if defined(SL_CATALOG_APP_LOG_PRESENT)
  #define WOMBCARE_DEBUG_LOGGING   1
#else
  #define WOMBCARE_DEBUG_LOGGING   0
#endif

#if WOMBCARE_DEBUG_LOGGING
  #include "app_log.h"
  #define WC_LOG(...)   app_log_info(__VA_ARGS__)
#else
  #define WC_LOG(...)   do { } while (0)
#endif

#endif /* WOMBCARE_DEBUG_H */
