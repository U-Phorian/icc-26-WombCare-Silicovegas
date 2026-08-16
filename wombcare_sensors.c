#include "wombcare_sensors.h"
#include "wombcare_battery.h"
#include "wombcare_debug.h"

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_iadc.h"
#include "em_ldma.h"
#include "em_letimer.h"
#include "em_prs.h"

#include "sl_dma_manager.h"
#include "sl_dma_manager_instances.h"
#include "sl_power_manager.h"

#include <stdint.h>
#include <stdbool.h>

uint16_t adcBufferPing[DMA_BUFFER_SIZE];
uint16_t adcBufferPong[DMA_BUFFER_SIZE];

volatile bool ping_buffer_ready = false;
volatile bool pong_buffer_ready = false;

/* Keep descriptors private to this file */
static LDMA_Descriptor_t ldmaDescriptors[2];

/*
 * LDMA channel used for the scan queue.
 *
 * Taken from the DMA Manager rather than hardcoded: SPIDRV (the IMU)
 * allocates its TX/RX channels from the same pool during
 * wombcare_imu_init(), which runs before this module. A fixed channel
 * number is invisible to the allocator and can be handed to another
 * driver later.
 */
static uint8_t s_ldma_channel;
static bool    s_ldma_channel_valid = false;

/* True while an EM1 requirement is held on behalf of the acquisition chain. */
static bool s_em1_held = false;

/* Which of the two ping-pong buffers the next completion belongs to. */
static volatile bool s_next_is_ping = true;

/*
 * FSRCO runs at 20 MHz and, unlike EM01GRPACLK, keeps running in EM2 —
 * see the IADCCLK note in config/sl_clock_manager_tree_config.h.
 */
#define CLK_SRC_ADC_FREQ  20000000UL
#define CLK_ADC_FREQ      10000000UL

/* 12-bit right-aligned results: full scale is 4096 codes. */
#define IADC_FULL_SCALE   4096.0f

/*
 * AVDD reaches the IADC through a fixed 1/4 divider, and the battery
 * config uses the internal 1.21 V reference.
 */
#define BATTERY_VREF_V    1.21f
#define BATTERY_DIVIDER   4.0f

static void wombcare_ldma_callback(void);

void wombcare_hardware_init(void)
{
    CMU_ClockEnable(cmuClock_IADC0, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_LETIMER0, true);

    /*
     * LETIMER runs from the low-frequency domain so it keeps ticking in
     * EM2 and can trigger the IADC while the core sleeps.
     */
    CMU_ClockSelectSet(cmuClock_EM23GRPACLK, cmuSelect_LFRCO);

    /*
     * IADC runs from FSRCO. The Clock Manager already applies this from
     * SL_CLOCK_MANAGER_IADCCLK_SOURCE; repeating it here keeps the
     * acquisition chain correct even if that config is regenerated back
     * to its EM01GRPACLK default, which would silently stop conversions
     * as soon as the device enters EM2.
     */
    CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);

    /*---------------------------------------------------------------
     * Configure Analog GPIO Pins
     *
     * The pins are left disabled (no digital driver) and the analog
     * bus is allocated to ADC0. Without the BUSALLOC step the mux
     * cannot route the pins to the IADC and every scan entry reads a
     * floating node.
     *
     *      PA2 -> Mother ECG   (port A, even pin)
     *      PA3 -> Fetal  ECG   (port A, odd  pin)
     *      PB7 -> PVDF   kick  (port B, odd  pin)
     *--------------------------------------------------------------*/
    GPIO_PinModeSet(gpioPortA, 2, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortA, 3, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortB, 7, gpioModeDisabled, 0);

    GPIO->ABUSALLOC = GPIO_ABUSALLOC_AEVEN0_ADC0
                      | GPIO_ABUSALLOC_AODD0_ADC0;

    GPIO->BBUSALLOC = GPIO_BBUSALLOC_BODD0_ADC0;

    //----------------------------------------------------------------------
    // LETIMER — 250 Hz underflow pulse
    //----------------------------------------------------------------------
    LETIMER_Init_TypeDef letimerInit = LETIMER_INIT_DEFAULT;

    /* Free-running periodic timer, started later by analog_start(). */
    letimerInit.enable   = false;
    letimerInit.comp0Top = true;
    letimerInit.repMode  = letimerRepeatFree;

    /* Generate a PRS pulse on every underflow */
    letimerInit.ufoa0 = letimerUFOAPulse;

    /* COMP0 acts as TOP; set it through init so it is applied atomically. */
    letimerInit.topValue = (32768U / SAMPLE_RATE_HZ) - 1U;

    LETIMER_Init(LETIMER0, &letimerInit);

    //----------------------------------------------------------------------
    // PRS: LETIMER0 underflow -> IADC0 scan trigger
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

    init.timebase = IADC_calcTimebase(IADC0, 0);

    init.srcClkPrescale =
        IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

    /*
     * Keep the analog front end warm. Warming up per conversion costs
     * more time than the 4 ms scan period allows once the device is
     * sleeping between triggers.
     */
    init.warmup = iadcWarmupKeepWarm;

    IADC_AllConfigs_t allConfigs = IADC_ALLCONFIGS_DEFAULT;

    /*----------------------------------------------------------
     * Config 0 — high-speed ECG/PVDF Scan Queue
     *---------------------------------------------------------*/
    allConfigs.configs[0].reference = iadcCfgReferenceVddx;
    allConfigs.configs[0].vRef      = 3300;

    allConfigs.configs[0].adcClkPrescale =
        IADC_calcAdcClkPrescale(IADC0,
                                CLK_ADC_FREQ,
                                0,
                                iadcCfgModeNormal,
                                init.srcClkPrescale);

    /*----------------------------------------------------------
     * Config 1 — low-rate battery Single Queue
     *---------------------------------------------------------*/
    allConfigs.configs[1].reference  = iadcCfgReferenceInt1V2;
    allConfigs.configs[1].vRef       = 1210;
    allConfigs.configs[1].analogGain = iadcCfgAnalogGain1x;

    allConfigs.configs[1].adcClkPrescale =
        IADC_calcAdcClkPrescale(IADC0,
                                CLK_ADC_FREQ,
                                0,
                                iadcCfgModeNormal,
                                init.srcClkPrescale);

    /*----------------------------------------------------------
     * Scan Queue — 3 channels, PRS triggered at 250 Hz
     *---------------------------------------------------------*/
    IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;

    initScan.triggerSelect = iadcTriggerSelPrs0PosEdge;

    /* One full scan of the table per PRS pulse */
    initScan.triggerAction = iadcTriggerActionOnce;

    /* Request the DMA once all 3 channels of a scan are in the FIFO */
    initScan.dataValidLevel = iadcFifoCfgDvl3;

    /*
     * Let the FIFO wake the LDMA out of EM2. The LDMA itself does not
     * run below EM1, so without this the FIFO would fill and stall.
     */
    initScan.fifoDmaWakeup = true;

    /*
     * Arm the scan queue as part of init. Leaving this false (the
     * struct default) configures the trigger but never enables the
     * queue, so PRS pulses arrive and nothing converts.
     */
    initScan.start = true;

    IADC_ScanTable_t scanTable = IADC_SCANTABLE_DEFAULT;

    scanTable.entries[CH_MOTHER_ECG].posInput      = IADC_INPUT_MOTHER_ECG;
    scanTable.entries[CH_MOTHER_ECG].negInput      = iadcNegInputGnd;
    scanTable.entries[CH_MOTHER_ECG].configId      = 0;
    scanTable.entries[CH_MOTHER_ECG].includeInScan = true;

    scanTable.entries[CH_FETAL_ECG].posInput      = IADC_INPUT_FETAL_ECG;
    scanTable.entries[CH_FETAL_ECG].negInput      = iadcNegInputGnd;
    scanTable.entries[CH_FETAL_ECG].configId      = 0;
    scanTable.entries[CH_FETAL_ECG].includeInScan = true;

    scanTable.entries[CH_PVDF_KICK].posInput      = IADC_INPUT_PVDF_KICK;
    scanTable.entries[CH_PVDF_KICK].negInput      = iadcNegInputGnd;
    scanTable.entries[CH_PVDF_KICK].configId      = 0;
    scanTable.entries[CH_PVDF_KICK].includeInScan = true;

    /*----------------------------------------------------------
     * Single Queue — reserved for battery measurements.
     *
     * Tailgated so a battery conversion can never preempt the
     * 250 Hz ECG scan.
     *---------------------------------------------------------*/
    IADC_InitSingle_t initSingle = IADC_INITSINGLE_DEFAULT;

    initSingle.singleTailgate  = true;
    initSingle.triggerSelect   = iadcTriggerSelImmediate;
    initSingle.triggerAction   = iadcTriggerActionOnce;
    initSingle.dataValidLevel  = iadcFifoCfgDvl1;
    initSingle.start           = false;

    IADC_SingleInput_t singleInput = IADC_SINGLEINPUT_DEFAULT;

    singleInput.posInput = iadcPosInputAvdd;
    singleInput.negInput = iadcNegInputGnd;
    singleInput.configId = 1;

    IADC_init(IADC0, &init, &allConfigs);

    IADC_initScan(IADC0, &initScan, &scanTable);

    IADC_initSingle(IADC0, &initSingle, &singleInput);

    //----------------------------------------------------------------------
    // LDMA
    //
    // The DMA Manager component (dma_manager_init) has already enabled
    // the LDMA bus clocks, initialised the peripheral and taken over
    // LDMA_IRQHandler. Calling emlib LDMA_Init() here would re-run that
    // setup destructively: it writes CHDIS = all and IEN = ERROR only,
    // which disables every channel and clears the DONE interrupt enables
    // that SPIDRV installed for the IMU moments earlier.
    //----------------------------------------------------------------------
    if (sl_dma_manager_allocate_channel(&sl_dma_handle_ldma0,
                                        &s_ldma_channel) == SL_STATUS_OK)
    {
        s_ldma_channel_valid = true;

        (void)sl_dma_manager_register_channel_irq_callback(
            &sl_dma_handle_ldma0,
            s_ldma_channel,
            wombcare_ldma_callback);
    }
    else
    {
        WC_LOG("ADC: no free LDMA channel\r\n");
    }

    /*----------------------------------------------------------
     * Ping descriptor: drain SCANFIFODATA into adcBufferPing.
     *
     * 12-bit results are read as halfwords, one FIFO pop per DMA
     * request, 750 requests (250 scans x 3 channels) per buffer.
     *---------------------------------------------------------*/
    ldmaDescriptors[0].xfer.structType = ldmaCtrlStructTypeXfer;
    ldmaDescriptors[0].xfer.srcAddr    = (uint32_t)&IADC0->SCANFIFODATA;
    ldmaDescriptors[0].xfer.dstAddr    = (uint32_t)adcBufferPing;
    ldmaDescriptors[0].xfer.xferCnt    = DMA_BUFFER_SIZE - 1;
    ldmaDescriptors[0].xfer.size       = ldmaCtrlSizeHalf;
    ldmaDescriptors[0].xfer.srcInc     = ldmaCtrlSrcIncNone;
    ldmaDescriptors[0].xfer.dstInc     = ldmaCtrlDstIncOne;
    ldmaDescriptors[0].xfer.reqMode    = ldmaCtrlReqModeBlock;
    ldmaDescriptors[0].xfer.blockSize  = ldmaCtrlBlockSizeUnit1;
    ldmaDescriptors[0].xfer.doneIfs    = 1;
    ldmaDescriptors[0].xfer.link       = 1;
    ldmaDescriptors[0].xfer.linkMode   = ldmaLinkModeRel;

    /*
     * Relative link addresses are counted in 32-bit words, not in
     * descriptors: one descriptor is LDMA_DESCRIPTOR_NON_EXTEND_SIZE_WORD
     * words long. A literal 1 here points 4 bytes ahead, i.e. into the
     * middle of this descriptor, and the ping-pong chain never forms.
     */
    ldmaDescriptors[0].xfer.linkAddr = LDMA_DESCRIPTOR_NON_EXTEND_SIZE_WORD;

    /* Pong descriptor: same transfer, other buffer, links back. */
    ldmaDescriptors[1] = ldmaDescriptors[0];

    ldmaDescriptors[1].xfer.dstAddr  = (uint32_t)adcBufferPong;
    ldmaDescriptors[1].xfer.linkAddr = -LDMA_DESCRIPTOR_NON_EXTEND_SIZE_WORD;

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
    if (!s_ldma_channel_valid)
    {
        return;
    }

    ping_buffer_ready = false;
    pong_buffer_ready = false;

    /* The chain always restarts on the Ping descriptor. */
    s_next_is_ping = true;

    /*
     * Hold EM1 for as long as we are acquiring.
     *
     * The IADC and LETIMER survive EM2, but the LDMA does not, and this
     * chain moves 750 halfwords every second without pause. Letting the
     * Power Manager drop to EM2 mid-buffer relies entirely on the FIFO
     * wake-up and makes buffer timing non-deterministic.
     */
    if (!s_em1_held)
    {
        sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
        s_em1_held = true;
    }

    LETIMER_CounterSet(LETIMER0, LETIMER_CompareGet(LETIMER0, 0));

    LDMA_TransferCfg_t transferCfg =
        LDMA_TRANSFER_CFG_PERIPHERAL(
            ldmaPeripheralSignal_IADC0_IADC_SCAN);

    LDMA_StartTransfer(s_ldma_channel,
                       &transferCfg,
                       &ldmaDescriptors[0]);

    LETIMER_Enable(LETIMER0, true);

    /*
     * Re-arm the scan queue. IADC_initScan() armed it once at boot, but
     * wombcare_analog_stop() issues STOPSCAN, so a restart needs this.
     */
    IADC_command(IADC0, iadcCmdStartScan);
}

void wombcare_analog_stop(void)
{
    LETIMER_Enable(LETIMER0, false);

    IADC_command(IADC0, iadcCmdStopScan);

    if (s_ldma_channel_valid)
    {
        LDMA_StopTransfer(s_ldma_channel);
    }

    ping_buffer_ready = false;
    pong_buffer_ready = false;

    /*
     * Deliberately no IADC_reset() here. Resetting wipes CFG, the scan
     * table and the single-queue setup, and nothing re-runs
     * wombcare_hardware_init(), so the next wombcare_analog_start()
     * would drive a blank peripheral. Stopping the queue is enough.
     */

    if (s_em1_held)
    {
        sl_power_manager_remove_em_requirement(SL_POWER_MANAGER_EM1);
        s_em1_held = false;
    }
}

/*
 * DMA transfer-complete callback.
 *
 * The LDMA_IRQHandler is owned by the DMA Manager component
 * (sl_dma_manager_hal_ldma.c). It clears the interrupt flags and
 * dispatches to the per-channel callback registered above, so this
 * function must NOT touch the interrupt flags itself.
 */
static void wombcare_ldma_callback(void)
{
    if (s_next_is_ping)
    {
        ping_buffer_ready = true;
    }
    else
    {
        pong_buffer_ready = true;
    }

    s_next_is_ping = !s_next_is_ping;
}

/*----------------------------------------------------------
 * IADC Interrupt Handler
 *
 * Handles completion of tailgated Single Queue
 * battery conversions.
 *---------------------------------------------------------*/
void IADC_IRQHandler(void)
{
    uint32_t flags = IADC_getEnabledInt(IADC0);

    if (flags & IADC_IF_SINGLEDONE)
    {
        IADC_clearInt(IADC0, IADC_IF_SINGLEDONE);

        IADC_Result_t result = IADC_pullSingleFifoResult(IADC0);

        float battery_voltage =
            ((float)result.data * BATTERY_VREF_V * BATTERY_DIVIDER)
            / IADC_FULL_SCALE;

        wombcare_battery_update(battery_voltage);
    }
}
