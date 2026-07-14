#include "spidrv.h"
#include "sl_spidrv_instances.h"
#include "sl_assert.h"


#include "sl_spidrv_eusart_IMU_SPIDRV_config.h"
SPIDRV_HandleData_t sl_spidrv_eusart_IMU_SPIDRV_handle_data;
SPIDRV_Handle_t sl_spidrv_eusart_IMU_SPIDRV_handle = &sl_spidrv_eusart_IMU_SPIDRV_handle_data;

static SPIDRV_Handle_t sli_spidrv_default_handle = NULL;

SPIDRV_Init_t sl_spidrv_eusart_init_IMU_SPIDRV = {
  .port = SL_SPIDRV_EUSART_IMU_SPIDRV_PERIPHERAL,
  .portTx = SL_SPIDRV_EUSART_IMU_SPIDRV_TX_PORT,
  .portRx = SL_SPIDRV_EUSART_IMU_SPIDRV_RX_PORT,
  .portClk = SL_SPIDRV_EUSART_IMU_SPIDRV_SCLK_PORT,
#if defined(SL_SPIDRV_EUSART_IMU_SPIDRV_CS_PORT)
  .portCs = SL_SPIDRV_EUSART_IMU_SPIDRV_CS_PORT,
#endif
  .pinTx = SL_SPIDRV_EUSART_IMU_SPIDRV_TX_PIN,
  .pinRx = SL_SPIDRV_EUSART_IMU_SPIDRV_RX_PIN,
  .pinClk = SL_SPIDRV_EUSART_IMU_SPIDRV_SCLK_PIN,
#if defined(SL_SPIDRV_EUSART_IMU_SPIDRV_CS_PIN)
  .pinCs = SL_SPIDRV_EUSART_IMU_SPIDRV_CS_PIN,
#endif
  .bitRate = SL_SPIDRV_EUSART_IMU_SPIDRV_BITRATE,
  .frameLength = SL_SPIDRV_EUSART_IMU_SPIDRV_FRAME_LENGTH,
  .dummyTxValue = 0,
  .type = SL_SPIDRV_EUSART_IMU_SPIDRV_TYPE,
  .bitOrder = SL_SPIDRV_EUSART_IMU_SPIDRV_BIT_ORDER,
  .clockMode = SL_SPIDRV_EUSART_IMU_SPIDRV_CLOCK_MODE,
  .csControl = SL_SPIDRV_EUSART_IMU_SPIDRV_CS_CONTROL,
  .slaveStartMode = SL_SPIDRV_EUSART_IMU_SPIDRV_SLAVE_START_MODE,
};

void sl_spidrv_init_instances(void) {
#if !defined(SL_SPIDRV_EUSART_IMU_SPIDRV_CS_PIN)
  EFM_ASSERT(sl_spidrv_eusart_init_IMU_SPIDRV.csControl == spidrvCsControlApplication);
#endif 
  SPIDRV_Init(sl_spidrv_eusart_IMU_SPIDRV_handle, &sl_spidrv_eusart_init_IMU_SPIDRV);
  sl_spidrv_set_default(sl_spidrv_eusart_IMU_SPIDRV_handle);

}


sl_status_t sl_spidrv_set_default(SPIDRV_Handle_t handle)
{
  sl_status_t status = SL_STATUS_INVALID_HANDLE;

  if (handle != NULL) {
    sli_spidrv_default_handle = handle;
    status = SL_STATUS_OK;
  }

  return status;
}

SPIDRV_Handle_t sl_spidrv_get_default(void)
{
  return sli_spidrv_default_handle;
}

