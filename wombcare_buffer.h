#ifndef WOMBCARE_BUFFER_H
#define WOMBCARE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "wombcare_sensors.h" // Inherit SAMPLES_PER_SECOND and NUM_CHANNELS

// ---------------------------------------------------------
// RING BUFFER CONFIGURATION
// ---------------------------------------------------------
#define HISTORY_WINDOW_SECONDS  60
#define RING_BUFFER_CAPACITY    (SAMPLES_PER_SECOND * HISTORY_WINDOW_SECONDS) // 15,000

// ---------------------------------------------------------
// TRACKING STRUCTURE
// ---------------------------------------------------------
typedef struct {
    uint32_t head;      // Write pointer for incoming samples
    uint32_t tail;      // Read pointer (useful for Phase 5 DSP extraction)
    bool is_primed;     // Transitions to true once the buffer hits 60s for the first time
} wombcare_ring_t;

// ---------------------------------------------------------
// GLOBAL STATIC ARRAYS (180 KB Total Footprint)
// ---------------------------------------------------------
extern float ring_mother_ecg[RING_BUFFER_CAPACITY];
extern float ring_fetal_ecg[RING_BUFFER_CAPACITY];
extern float ring_pvdf_kick[RING_BUFFER_CAPACITY];

// State trackers for each channel
extern wombcare_ring_t tracker_mother;
extern wombcare_ring_t tracker_fetal;
extern wombcare_ring_t tracker_pvdf;

// ---------------------------------------------------------
// PROTOTYPES
// ---------------------------------------------------------
void wombcare_buffer_init(void);
void wombcare_buffer_ingest(uint16_t *dma_source_buffer);

#endif // WOMBCARE_BUFFER_H