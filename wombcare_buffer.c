#include "wombcare_buffer.h"

// ---------------------------------------------------------
// STATIC MEMORY ALLOCATION
// ---------------------------------------------------------
// This locks in exactly 180 KB of RAM at compile time, guaranteeing
// memory safety and preventing runtime heap fragmentation.
float ring_mother_ecg[RING_BUFFER_CAPACITY];
float ring_fetal_ecg[RING_BUFFER_CAPACITY];
float ring_pvdf_kick[RING_BUFFER_CAPACITY];

// Initialize trackers to 0
wombcare_ring_t tracker_mother = { .head = 0, .tail = 0, .is_primed = false };
wombcare_ring_t tracker_fetal  = { .head = 0, .tail = 0, .is_primed = false };
wombcare_ring_t tracker_pvdf   = { .head = 0, .tail = 0, .is_primed = false };

// ---------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------
void wombcare_buffer_init(void) {
    // Arrays in .bss are zero-initialized by the linker, 
    // but explicit tracker resets ensure safe soft-reboots.
    tracker_mother.head = 0; tracker_mother.is_primed = false;
    tracker_fetal.head = 0;  tracker_fetal.is_primed = false;
    tracker_pvdf.head = 0;   tracker_pvdf.is_primed = false;
}

// ---------------------------------------------------------
// ZERO-COPY INGESTION WORKER
// ---------------------------------------------------------
void wombcare_buffer_ingest(uint16_t *dma_source_buffer) {
    // The DMA buffer contains exactly 1 second of data (750 halfwords).
    // It is physically interleaved by the hardware IADC Scan Table:
    // [Mother0, Fetal0, PVDF0, Mother1, Fetal1, PVDF1 ... Mother249, Fetal249, PVDF249]

    for (uint32_t i = 0; i < SAMPLES_PER_SECOND; i++) {
        // Calculate the physical base index in the flat DMA array
        uint32_t dma_idx = i * NUM_CHANNELS; 

        // 1. Demultiplex, Cast to Float, and Store at the Current Head
        ring_mother_ecg[tracker_mother.head] = (float)dma_source_buffer[dma_idx + CH_MOTHER_ECG];
        ring_fetal_ecg[tracker_fetal.head]   = (float)dma_source_buffer[dma_idx + CH_FETAL_ECG];
        ring_pvdf_kick[tracker_pvdf.head]    = (float)dma_source_buffer[dma_idx + CH_PVDF_KICK];

        // 2. Increment Write Pointers
        tracker_mother.head++;
        tracker_fetal.head++;
        tracker_pvdf.head++;

        // 3. Fast Wrap-Around Mechanics [ O(1) Time Complexity ]
        if (tracker_mother.head >= RING_BUFFER_CAPACITY) {
            tracker_mother.head = 0;
            tracker_mother.is_primed = true; // We now have a full 60 seconds of history
        }
        
        if (tracker_fetal.head >= RING_BUFFER_CAPACITY) {
            tracker_fetal.head = 0;
            tracker_fetal.is_primed = true;
        }
        
        if (tracker_pvdf.head >= RING_BUFFER_CAPACITY) {
            tracker_pvdf.head = 0;
            tracker_pvdf.is_primed = true;
        }
    }
}