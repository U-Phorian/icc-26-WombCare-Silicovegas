#include "wombcare_sensors.h"
#include "wombcare_battery.h"

#include "em_cmu.h"
#include <em_gpio.h>
#include "em_iadc.h"
#include "em_ldma.h"
#include "em_letimer.h"
#include "em_prs.h"

#include "sl_dma_manager.h"
#include "sl_dma_manager_instances.h"

#include <stdint.h>

uint16_t adcBufferPing[DMA_BUFFER_SIZE];
uint16_t adcBufferPong[DMA_BUFFER_SIZE];

volatile bool ping_buffer_ready = false;
volatile bool pong_buffer_ready = false;

/* Keep descriptors private to this file */
static LDMA_Descriptor_t ldmaDescriptors[2];

#define LDMA_CHANNEL   0U

/* DMA transfer-complete callback (dispatched by the DMA Manager ISR). */
static void wombcare_ldma_callback(void);

void wombcare_hardware_init(void)
{
    CMU_ClockEnable(cmuClock_IADC0, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_LDMA, true);
    CMU_ClockEnable(cmuClock_LETIMER0, true);

    CMU_ClockSelectSet(cmuClock_EM23GRPACLK, cmuSelect_LFRCO);

    /*---------------------------------------------------------------
    * Configure Analog GPIO Pins
    *--------------------------------------------------------------*/
    GPIO_PinModeSet(gpioPortA, 2, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortA, 3, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortB, 7, gpioModeDisabled, 0);

    //----------------------------------------------------------------------
    // LETIMER
    //----------------------------------------------------------------------
    LETIMER_Init_TypeDef letimerInit = LETIMER_INIT_DEFAULT;

/* Free-running periodic timer */
letimerInit.enable = false;
letimerInit.comp0Top = true;
letimerInit.repMode = letimerRepeatFree;

/* Generate PRS pulse on every underflow */
letimerInit.ufoa0 = letimerUFOAPulse;

LETIMER_Init(
    LETIMER0,
    &letimerInit);

    uint32_t topValue = (32768U / SAMPLE_RATE_HZ) - 1U;

    LETIMER_CompareSet(LETIMER0, 0, topValue);

    //----------------------------------------------------------------------
    // PRS
    //----------------------------------------------------------------------
    PRS_SourceAsyncSignalSet(
        0,
        PRS_ASYNC_CH_CTRL_SOURCESEL_LETIMER0,
        PRS_ASYNC_CH_CTRL_SIGSEL_LETIMER0CH0);

    PRS_ConnectConsumer(
        0,
        prsTypeAsync,
        prsConsumerIADC0_SCANTRIGGER);

IADC_Init_t init = IADC_INIT_DEFAULT;

IADC_AllConfigs_t allConfigs = IADC_ALLCONFIGS_DEFAULT;

IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;

/*----------------------------------------------------------
 * Single Queue
 *
 * Reserved for low-rate battery measurements.
 * It is completely independent of the Scan Queue.
 *---------------------------------------------------------*/
IADC_InitSingle_t initSingle = IADC_INITSINGLE_DEFAULT;


/*----------------------------------------------------------
 * Single Queue Behaviour
 *
 * Battery measurements must never interrupt the
 * 250 Hz Scan Queue used for ECG acquisition.
 *---------------------------------------------------------*/

initSingle.singleTailgate = true;

/* Software-triggered one-shot conversion */
initSingle.triggerSelect = iadcTriggerSelImmediate;
initSingle.triggerAction = iadcTriggerActionOnce;

/* Do not automatically start conversions */
initSingle.start = false;

/*----------------------------------------------------------
 * Battery Single Queue Input Selection
 *---------------------------------------------------------*/
IADC_SingleInput_t singleInput = IADC_SINGLEINPUT_DEFAULT;

IADC_ScanTable_t scanTable = IADC_SCANTABLE_DEFAULT;

/*----------------------------------------------------------
 * Config 0
 *
 * High-speed ECG/PVDF Scan Queue
 *---------------------------------------------------------*/
allConfigs.configs[0].reference = iadcCfgReferenceVddx;

/*----------------------------------------------------------
 * Config 1
 *
 * Battery Single Queue
 *---------------------------------------------------------*/
allConfigs.configs[1].reference = iadcCfgReferenceInt1V2;
allConfigs.configs[1].vRef = 1210;
allConfigs.configs[1].analogGain = iadcCfgAnalogGain1x;

/*----------------------------------------------------------
 * Single Queue Configuration
 *
 * This queue will later measure the internal AVDD supply.
 * It is NOT triggered yet.
 *---------------------------------------------------------*/

/* One conversion per request */
initSingle.dataValidLevel = iadcFifoCfgDvl1;

singleInput.posInput = iadcPosInputAvdd;
singleInput.negInput = iadcNegInputGnd;
/* Battery uses Config 1 */
singleInput.configId = 1;

    /* Triggered by PRS */
initScan.triggerSelect = iadcTriggerSelPrs0PosEdge;

/* One scan for every PRS pulse */
initScan.triggerAction = iadcTriggerActionOnce;

/* DMA wakes after all 3 channels */
initScan.dataValidLevel = iadcFifoCfgDvl3;

initScan.fifoDmaWakeup = true;

    scanTable.entries[CH_MOTHER_ECG].posInput = IADC_INPUT_MOTHER_ECG;
    scanTable.entries[CH_MOTHER_ECG].negInput = iadcNegInputGnd;
    scanTable.entries[CH_MOTHER_ECG].includeInScan = true;

    scanTable.entries[CH_FETAL_ECG].posInput = IADC_INPUT_FETAL_ECG;
    scanTable.entries[CH_FETAL_ECG].negInput = iadcNegInputGnd;
    scanTable.entries[CH_FETAL_ECG].includeInScan = true;

    scanTable.entries[CH_PVDF_KICK].posInput = IADC_INPUT_PVDF_KICK;
    scanTable.entries[CH_PVDF_KICK].negInput = iadcNegInputGnd;
    scanTable.entries[CH_PVDF_KICK].includeInScan = true;

    IADC_init(IADC0, &init, &allConfigs);

/* High-speed ECG Scan Queue */
IADC_initScan(IADC0, &initScan, &scanTable);

/* Low-speed Battery Single Queue */
IADC_initSingle(
    IADC0,
    &initSingle,
    &singleInput);

    //----------------------------------------------------------------------
    // LDMA
    //----------------------------------------------------------------------
    LDMA_Init_t ldmaInit = LDMA_INIT_DEFAULT;
    LDMA_Init(&ldmaInit);

    ldmaDescriptors[0].xfer.structType = ldmaCtrlStructTypeXfer;
    ldmaDescriptors[0].xfer.srcAddr = (uint32_t)&IADC0->SCANFIFODATA;
    ldmaDescriptors[0].xfer.dstAddr = (uint32_t)adcBufferPing;
    ldmaDescriptors[0].xfer.xferCnt = DMA_BUFFER_SIZE - 1;
    ldmaDescriptors[0].xfer.size = ldmaCtrlSizeHalf;
    ldmaDescriptors[0].xfer.srcInc = ldmaCtrlSrcIncNone;
    ldmaDescriptors[0].xfer.dstInc = ldmaCtrlDstIncOne;
    ldmaDescriptors[0].xfer.reqMode = ldmaCtrlReqModeBlock;
    ldmaDescriptors[0].xfer.blockSize = ldmaCtrlBlockSizeUnit1;
    ldmaDescriptors[0].xfer.doneIfs = 1;
    ldmaDescriptors[0].xfer.link = 1;
    ldmaDescriptors[0].xfer.linkMode = ldmaLinkModeRel;
    ldmaDescriptors[0].xfer.linkAddr = 1;

    /* Duplicate Ping descriptor to create Pong descriptor */
ldmaDescriptors[1] = ldmaDescriptors[0];
    ldmaDescriptors[1].xfer.dstAddr = (uint32_t)adcBufferPong;

    /* End of ping-pong chain */
    ldmaDescriptors[1].xfer.linkAddr = -1;

    /*
     * The DMA Manager component owns the single LDMA_IRQHandler and
     * dispatches per-channel. Reserve our fixed channel out of its
     * dynamic pool and register our completion callback so we get
     * called from the shared ISR.
     */
    (void)sl_dma_manager_register_channel_irq_callback(
    &sl_dma_handle_ldma0,
    LDMA_CHANNEL,
    wombcare_ldma_callback);

NVIC_ClearPendingIRQ(LDMA_IRQn);

/*----------------------------------------------------------
 * Enable Battery Single Queue Interrupt
 *---------------------------------------------------------*/
IADC_clearInt(IADC0, _IADC_IF_MASK);

IADC_enableInt(IADC0, IADC_IEN_SINGLEDONE);

NVIC_ClearPendingIRQ(IADC_IRQn);
NVIC_EnableIRQ(IADC_IRQn);


}








void wombcare_analog_start(void)
{
    ping_buffer_ready = false;
    pong_buffer_ready = false;

    LETIMER_CounterSet(
        LETIMER0,
        LETIMER_CompareGet(LETIMER0, 0));

    LDMA_TransferCfg_t transferCfg =
        LDMA_TRANSFER_CFG_PERIPHERAL(
            ldmaPeripheralSignal_IADC0_IADC_SCAN);

    LDMA_IntClear(0xFFFFFFFFU);
    LDMA_StartTransfer(
        LDMA_CHANNEL,
        &transferCfg,
        &ldmaDescriptors[0]);

    NVIC_EnableIRQ(LDMA_IRQn);

    LETIMER_Enable(LETIMER0, true);
}

void wombcare_analog_stop(void)
{
    LETIMER_Enable(LETIMER0, false);

    LDMA_StopTransfer(LDMA_CHANNEL);

    LDMA_IntClear(0xFFFFFFFFU);

    NVIC_DisableIRQ(LDMA_IRQn);

    ping_buffer_ready = false;
    pong_buffer_ready = false;

    IADC_command(IADC0, iadcCmdStopScan);
    IADC_reset(IADC0);
}

/*
 * DMA transfer-complete callback.
 *
 * The LDMA_IRQHandler is owned by the DMA Manager component
 * (sl_dma_manager_hal_ldma.c). It clears the interrupt flags and
 * dispatches to the per-channel callback registered below, so this
 * function must NOT touch the interrupt flags itself.
 */
static void wombcare_ldma_callback(void)
{
    static bool isPing = true;

    if (isPing)
    {
        ping_buffer_ready = true;
    }
    else
    {
        pong_buffer_ready = true;
    }

    /* Toggle between Ping and Pong completion notifications */
    isPing = !isPing;
}


/*----------------------------------------------------------
 * IADC Interrupt Handler
 *
 * Handles completion of tailgated Single Queue
 * battery conversions.
 *---------------------------------------------------------*/
void IADC_IRQHandler(void)
{
    uint32_t flags =
        IADC_getEnabledInt(IADC0);

    if (flags & IADC_IF_SINGLEDONE)
    {
        IADC_clearInt(
            IADC0,
            IADC_IF_SINGLEDONE);

        IADC_Result_t result =
            IADC_pullSingleFifoResult(IADC0);

        float battery_voltage =
            ((float)result.data * 1.21f * 4.0f) / 4095.0f;

        wombcare_battery_update(
            battery_voltage);

        // wombcare_battery_update(3.0f);
    }
}