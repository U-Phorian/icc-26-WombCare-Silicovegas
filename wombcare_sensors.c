#include "wombcare_sensors.h"
#include "em_cmu.h"
#include "em_letimer.h"
#include "em_prs.h"
#include "em_iadc.h"
#include "em_ldma.h"
#include "em_core.h"

// ---------------------------------------------------------
// GLOBAL MEMORY ALLOCATIONS
// ---------------------------------------------------------
uint16_t adcBufferPing[DMA_BUFFER_SIZE];
uint16_t adcBufferPong[DMA_BUFFER_SIZE];

volatile bool ping_buffer_ready = false;
volatile bool pong_buffer_ready = false;

LDMA_Descriptor_t ldmaDescriptors[2];

#define LDMA_CHANNEL 0

void wombcare_hardware_init(void) {
    // ---------------------------------------------------------
    // 1. ENABLE CLOCKS
    // ---------------------------------------------------------
    CMU_ClockEnable(cmuClock_IADC0, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_LDMA, true); 
    
    CMU_ClockSelectSet(cmuClock_EM23GRPACLK, cmuSelect_LFRCO);
    CMU_ClockEnable(cmuClock_LETIMER0, true);

    // ---------------------------------------------------------
    // 2. CONFIGURE LETIMER0 (250 Hz)
    // ---------------------------------------------------------
    LETIMER_Init_TypeDef letimerInit = LETIMER_INIT_DEFAULT;
    letimerInit.comp0Top = true; 
    letimerInit.enable = false; 
    LETIMER_Init(LETIMER0, &letimerInit);

    uint32_t topValue = (CMU_ClockFreqGet(cmuClock_LETIMER0) / 250) - 1;
    LETIMER_CompareSet(LETIMER0, 0, topValue);

    // ---------------------------------------------------------
    // 3. CONFIGURE PRS
    // ---------------------------------------------------------
    PRS_SourceAsyncSignalSet(0, PRS_ASYNC_CH_CTRL_SOURCESEL_LETIMER0, PRS_ASYNC_CH_CTRL_SIGSEL_LETIMER0CH0);
    PRS_ConnectConsumer(0, prsTypeAsync, prsConsumerIADC0_SCANTRIGGER);

    // ---------------------------------------------------------
    // 4. CONFIGURE IADC
    // ---------------------------------------------------------
    IADC_Init_t init = IADC_INIT_DEFAULT;
    IADC_AllConfigs_t allConfigs = IADC_ALLCONFIGS_DEFAULT;
    IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
    IADC_ScanTable_t scanTable = IADC_SCANTABLE_DEFAULT;

    allConfigs.configs[0].reference = iadcCfgReferenceVddx;
    initScan.triggerAction = iadcTriggerActionOnce;
    initScan.dataValidLevel = iadcFifoCfgDvl3; 

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

    // ---------------------------------------------------------
    // 5. CONFIGURE THE LDMA PING-PONG ENGINE (Manual Structs)
    // ---------------------------------------------------------
    LDMA_Init_t ldmaInit = LDMA_INIT_DEFAULT;
    LDMA_Init(&ldmaInit);

    LDMA_TransferCfg_t transferCfg = LDMA_TRANSFER_CFG_PERIPHERAL(ldmaPeripheralSignal_IADC0_IADC_SCAN);

    // Setup Ping Descriptor (Index 0) Manually
    ldmaDescriptors[0].xfer.srcAddr = (uint32_t)&(IADC0->SCANFIFODATA);
    ldmaDescriptors[0].xfer.dstAddr = (uint32_t)adcBufferPing;
    ldmaDescriptors[0].xfer.xferCnt = (DMA_BUFFER_SIZE - 1);
    ldmaDescriptors[0].xfer.size = ldmaCtrlSizeHalf;       // 16-bit halfword
    ldmaDescriptors[0].xfer.srcInc = ldmaCtrlSrcIncNone;   // Do not increment ADC register
    ldmaDescriptors[0].xfer.dstInc = ldmaCtrlDstIncOne;    // Increment RAM array
    ldmaDescriptors[0].xfer.reqMode = ldmaCtrlReqModeBlock;
    ldmaDescriptors[0].xfer.blockSize = ldmaCtrlBlockSizeUnit1;
    ldmaDescriptors[0].xfer.doneIfs = 1;                   // Fire interrupt when Ping is full
    ldmaDescriptors[0].xfer.link = 1;                      // Enable Linking
    ldmaDescriptors[0].xfer.linkMode = ldmaLinkModeRel;
    ldmaDescriptors[0].xfer.linkAddr = 1;                  // Jump FORWARD 1 to Pong

    // Setup Pong Descriptor (Index 1) Manually
    ldmaDescriptors[1].xfer.srcAddr = (uint32_t)&(IADC0->SCANFIFODATA);
    ldmaDescriptors[1].xfer.dstAddr = (uint32_t)adcBufferPong;
    ldmaDescriptors[1].xfer.xferCnt = (DMA_BUFFER_SIZE - 1);
    ldmaDescriptors[1].xfer.size = ldmaCtrlSizeHalf;       
    ldmaDescriptors[1].xfer.srcInc = ldmaCtrlSrcIncNone;   
    ldmaDescriptors[1].xfer.dstInc = ldmaCtrlDstIncOne;    
    ldmaDescriptors[1].xfer.reqMode = ldmaCtrlReqModeBlock;
    ldmaDescriptors[1].xfer.blockSize = ldmaCtrlBlockSizeUnit1;
    ldmaDescriptors[1].xfer.doneIfs = 1;                   // Fire interrupt when Pong is full
    ldmaDescriptors[1].xfer.link = 1;                      // Enable Linking
    ldmaDescriptors[1].xfer.linkMode = ldmaLinkModeRel;
    ldmaDescriptors[1].xfer.linkAddr = -1;                 // Jump BACKWARD 1 to Ping

    // Open the NVIC Gate for LDMA
    NVIC_ClearPendingIRQ(LDMA_IRQn);
    NVIC_EnableIRQ(LDMA_IRQn);

    // Start the endless background loop
    LDMA_StartTransfer(LDMA_CHANNEL, &transferCfg, &ldmaDescriptors[0]);

    // ---------------------------------------------------------
    // 6. START THE METRONOME
    // ---------------------------------------------------------
    LETIMER_Enable(LETIMER0, true);
}

// ---------------------------------------------------------
// PHASE 2: THE LDMA WAKE-UP INTERRUPT
// ---------------------------------------------------------
void LDMA_IRQHandler(void) {
    // SiSDK v2024.x and newer uses IF (Interrupt Flag) nomenclature
    uint32_t pending = LDMA_IntGet();

    // Clear the interrupt flag immediately
    LDMA_IntClear(pending);

    if (pending & (1 << LDMA_CHANNEL)) {
        static bool isPing = true;
        
        if (isPing) {
            ping_buffer_ready = true;
        } else {
            pong_buffer_ready = true;
        }
        
        isPing = !isPing; // Toggle for the next second
    }
}