// Inside app.c (Application State Machine)
#include "wombcare_sensors.h"
#include "wombcare_buffer.h"

void app_process_action(void) {
    // Check if the hardware LDMA filled a 1-second buffer
    if (ping_buffer_ready) {
        ping_buffer_ready = false; 
        wombcare_buffer_ingest(adcBufferPing); // Ingest Ping array
        
        // Optional: Trigger IMU gating or DSP pipeline if buffers are primed
    }

    if (pong_buffer_ready) {
        pong_buffer_ready = false; 
        wombcare_buffer_ingest(adcBufferPong); // Ingest Pong array
        
        // Optional: Trigger IMU gating or DSP pipeline if buffers are primed
    }
}