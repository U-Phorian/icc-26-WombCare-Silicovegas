#include "wombcare_sensors.h"

#include "em_cmu.h"
#include <em_gpio.h>
#include "em_iadc.h"
#include "em_ldma.h"
#include "em_letimer.h"
#include "em_prs.h"

#include <stdint.h>

uint16_t adcBufferPing[DMA_BUFFER_SIZE];
uint16_t adcBufferPong[DMA_BUFFER_SIZE];

volatile bool ping_buffer_ready = false;
volatile bool pong_buffer_ready = false;

/* Keep descriptors private to this file */
static LDMA_Descriptor_t ldmaDescriptors[2];

#define LDMA_CHANNEL   0U

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
    GPIO_PinModeSet(gpioPortB, 7, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortB, 8, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortD, 8, gpioModeDisabled, 0);

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

    //----------------------------------------------------------------------
    // IADC
    //----------------------------------------------------------------------
    IADC_Init_t init = IADC_INIT_DEFAULT;

IADC_AllConfigs_t allConfigs = IADC_ALLCONFIGS_DEFAULT;

IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;

IADC_ScanTable_t scanTable = IADC_SCANTABLE_DEFAULT;

    allConfigs.configs[0].reference = iadcCfgReferenceVddx;

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
    IADC_initScan(IADC0, &initScan, &scanTable);

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

    NVIC_ClearPendingIRQ(LDMA_IRQn);
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

void LDMA_IRQHandler(void)
{
    uint32_t pending = LDMA_IF_Get();

    LDMA_IF_Clear(pending);

    if (pending & (1UL << LDMA_CHANNEL))
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
}