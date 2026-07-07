#ifndef WOMBCARE_BUFFER_H
#define WOMBCARE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "wombcare_dsp.h"

// ---------------------------------------------------------
// BUFFER CONFIGURATION
// ---------------------------------------------------------
#define RING_BUFFER_CAPACITY  1024  // 60 seconds at 17.07 Hz effective
#define SAMPLES_PER_SECOND    1000  // Expected samples per DMA transfer

// ---------------------------------------------------------
// RING BUFFER TRACKER STRUCTURE
// ---------------------------------------------------------
typedef struct {
    bool is_primed;
    uint32_t head;
} RingBufferTracker_t;

// ---------------------------------------------------------
// EXTERNAL BUFFER TRACKERS (for cross-module access)
// ---------------------------------------------------------
extern RingBufferTracker_t tracker_mother;
extern RingBufferTracker_t tracker_fetal;
extern RingBufferTracker_t tracker_pvdf;

extern float ring_mother_ecg[RING_BUFFER_CAPACITY];
extern float ring_fetal_ecg[RING_BUFFER_CAPACITY];
extern float ring_pvdf_kick[RING_BUFFER_CAPACITY];

// ---------------------------------------------------------
// FUNCTION PROTOTYPES
// ---------------------------------------------------------
void wombcare_buffer_init(void);
void wombcare_buffer_reset(void);
void wombcare_buffer_ingest(uint16_t *dma_source_buffer);

#endif  // WOMBCARE_BUFFER_H