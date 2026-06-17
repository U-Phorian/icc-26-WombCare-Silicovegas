#include "sl_component_catalog.h"
#ifdef SL_CATALOG_KERNEL_PRESENT
#include "app_rta_internal.h"
#else // SL_CATALOG_KERNEL_PRESENT
#include "app_rta_internal_bm.h"
#endif // SL_CATALOG_KERNEL_PRESENT
#include "throughput_central_rta.h"
#include "throughput_peripheral_rta.h"

void app_rta_init_contributors(void)
{
  throughput_central_rta_init();
  throughput_peripheral_rta_init();
}

void app_rta_ready(void)
{
  throughput_central_rta_ready();
  throughput_peripheral_rta_ready();
}
