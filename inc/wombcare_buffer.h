#ifndef WOMBCARE_BUFFER_H
#define WOMBCARE_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "wombcare_sensors.h"

/*----------------------------------------------------------
 * BUFFER CONFIGURATION
 *---------------------------------------------------------*/

/*
 * 60-second rolling window
 * 250 samples/sec × 60 sec = 15000 samples/channel
 */
#define RING_BUFFER_CAPACITY   (SAMPLE_RATE_HZ * 60U)

/*----------------------------------------------------------
 * RING BUFFER TRACKER
 *---------------------------------------------------------*/

typedef struct
{
    /* Next write position */
    uint32_t head;

    /* Number of valid samples currently stored */
    uint32_t valid_samples;

    /* True once the entire ring has been filled at least once */
    bool is_primed;

} RingBufferTracker_t;

/*----------------------------------------------------------
 * GLOBAL TRACKERS
 *---------------------------------------------------------*/

extern RingBufferTracker_t tracker_mother;
extern RingBufferTracker_t tracker_fetal;
extern RingBufferTracker_t tracker_pvdf;
/*----------------------------------------------------------
 * One Minute Window Flag
 *---------------------------------------------------------*/
extern volatile bool minute_window_ready;

/*----------------------------------------------------------
 * RING BUFFERS
 *---------------------------------------------------------*/

extern float ring_mother_ecg[RING_BUFFER_CAPACITY];
extern float ring_fetal_ecg[RING_BUFFER_CAPACITY];
extern float ring_pvdf_kick[RING_BUFFER_CAPACITY];

/*----------------------------------------------------------
 * API
 *---------------------------------------------------------*/

void wombcare_buffer_init(void);

void wombcare_buffer_reset(void);

void wombcare_buffer_ingest(const uint16_t *dma_source_buffer);

#endif /* WOMBCARE_BUFFER_H */