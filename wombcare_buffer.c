#include "wombcare_buffer.h"
#include "wombcare_sensors.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------
// RING BUFFER STORAGE (exported for DSP module)
// ---------------------------------------------------------
RingBufferTracker_t tracker_mother;
RingBufferTracker_t tracker_fetal;
RingBufferTracker_t tracker_pvdf;

float ring_mother_ecg[RING_BUFFER_CAPACITY];
float ring_fetal_ecg[RING_BUFFER_CAPACITY];
float ring_pvdf_kick[RING_BUFFER_CAPACITY];

// ---------------------------------------------------------
// BUFFER MANAGEMENT
// ---------------------------------------------------------
void wombcare_buffer_init(void) {
    wombcare_buffer_reset();
}

void wombcare_buffer_reset(void) {
    tracker_mother.head = 0; 
    tracker_mother.is_primed = false;
    tracker_fetal.head = 0;  
    tracker_fetal.is_primed = false;
    tracker_pvdf.head = 0;   
    tracker_pvdf.is_primed = false;
}

void wombcare_buffer_ingest(uint16_t *dma_source_buffer) {
    // Process DMA_BUFFER_SIZE samples (250 samples/sec * 3 channels = 750 total values)
    uint32_t num_samples = (DMA_BUFFER_SIZE / NUM_CHANNELS);  // 250 samples
    
    for (uint32_t i = 0; i < num_samples; i++) {
        uint32_t dma_idx = i * NUM_CHANNELS; 
        
        // Center the 12-bit ADC value (0-4095) around 0 and convert to approx uV scale
        ring_mother_ecg[tracker_mother.head] = ((float)dma_source_buffer[dma_idx + CH_MOTHER_ECG] - 2048.0f) * 805.8f;
        ring_fetal_ecg[tracker_fetal.head]   = ((float)dma_source_buffer[dma_idx + CH_FETAL_ECG] - 2048.0f) * 805.8f;
        ring_pvdf_kick[tracker_pvdf.head]    = (float)dma_source_buffer[dma_idx + CH_PVDF_KICK];

        tracker_mother.head++; 
        tracker_fetal.head++; 
        tracker_pvdf.head++;

        if (tracker_mother.head >= RING_BUFFER_CAPACITY) { 
            tracker_mother.head = 0; 
            tracker_mother.is_primed = true; 
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