#ifndef WOMBCARE_SENSORS_H
#define WOMBCARE_SENSORS_H

#include "em_device.h"
#include "em_iadc.h"
#include "em_ldma.h"
#include <stdint.h>
#include <stdbool.h>

// Hardware Pin Definitions (SiSDK 6.1.0)
#define IADC_INPUT_MOTHER_ECG   iadcPosInputPortBPin7
#define IADC_INPUT_FETAL_ECG    iadcPosInputPortBPin8
#define IADC_INPUT_PVDF_KICK    iadcPosInputPortDPin8

#define CH_MOTHER_ECG           0
#define CH_FETAL_ECG            1
#define CH_PVDF_KICK            2

// Phase 2: LDMA Ping-Pong Buffer Definitions
// 250 Hz * 3 channels = 750 samples per 1-second interrupt
#define SAMPLES_PER_SECOND      250
#define NUM_CHANNELS            3
#define DMA_BUFFER_SIZE         (SAMPLES_PER_SECOND * NUM_CHANNELS)

// Expose these buffers and flags so app.c can process the data
extern uint16_t adcBufferPing[DMA_BUFFER_SIZE];
extern uint16_t adcBufferPong[DMA_BUFFER_SIZE];
extern volatile bool ping_buffer_ready;
extern volatile bool pong_buffer_ready;

// Hardware Initialization Prototype
void wombcare_hardware_init(void);

#endif // WOMBCARE_SENSORS_H