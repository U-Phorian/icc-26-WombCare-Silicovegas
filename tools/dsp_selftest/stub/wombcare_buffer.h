#ifndef TEST_BUFFER_H
#define TEST_BUFFER_H
#include <stdint.h>
#include <stdbool.h>
#include "wombcare_sensors.h"
#define RING_BUFFER_CAPACITY (SAMPLE_RATE_HZ * 60U)
typedef struct { uint32_t head; uint32_t valid_samples; bool is_primed; } RingBufferTracker_t;
extern RingBufferTracker_t tracker_mother, tracker_fetal, tracker_pvdf;
extern volatile bool minute_window_ready;
extern uint16_t ring_mother_ecg[RING_BUFFER_CAPACITY];
extern uint16_t ring_fetal_ecg[RING_BUFFER_CAPACITY];
extern uint16_t ring_pvdf_kick[RING_BUFFER_CAPACITY];
#endif
