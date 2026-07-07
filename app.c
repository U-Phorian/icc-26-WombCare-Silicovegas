#include "app.h"
#include "wombcare_sensors.h"
#include "wombcare_buffer.h"
#include "wombcare_imu.h"
#include "wombcare_dsp.h"  

// Global structure to hold the 8 TinyML inputs
WombCareFeatures_t current_patient_features;

// ---------------------------------------------------------
// FUTURE INCLUDES (Phase 6)
// ---------------------------------------------------------
// #include "wombcare_dsp.h" 
// #include "wombcare_ml.h"  

// ---------------------------------------------------------
// SYSTEM BOOT SEQUENCE
// ---------------------------------------------------------
void app_init(void) {
    // 1. Initialize static RAM buffers (Phase 3)
    wombcare_buffer_init();
    
    // 2. Initialize IMU GPIO pins (Phase 4)
    wombcare_imu_init();
    
    // 3. Initialize DSP Pipeline (Phase 5)
    wombcare_dsp_init();
    
    // 4. Initialize hardware metronome & DMA (Phases 1 & 2)
    wombcare_hardware_init(); 
}

// ---------------------------------------------------------
// MAIN APPLICATION STATE MACHINE (Infinite Loop)
// ---------------------------------------------------------
void app_process_action(void) {
    
    switch (current_system_state) {
        
        case SYSTEM_STATE_IMU_EVAL:
            // Phase 4: Mother is evaluated for movement (30-second window)
            wombcare_evaluate_rest_state();
            break;

        case SYSTEM_STATE_DATA_ACQ:
            // Phase 3: Only ingest hardware data if the mother is confirmed at rest
            if (ping_buffer_ready) {
                ping_buffer_ready = false; 
                wombcare_buffer_ingest(adcBufferPing); 
            }

            if (pong_buffer_ready) {
                pong_buffer_ready = false; 
                wombcare_buffer_ingest(adcBufferPong); 
            }

            // ---------------------------------------------------------
            // THE DSP HANDOFF
            // ---------------------------------------------------------
            // We check if the static ring buffer has accumulated a full 60 seconds.
            if (tracker_mother.is_primed && tracker_fetal.is_primed && tracker_pvdf.is_primed) {
                
                // Reset flags so we don't trigger this continuously
                tracker_mother.is_primed = false;
                tracker_fetal.is_primed = false;
                tracker_pvdf.is_primed = false;

                // Move the state machine forward to Phase 5
                current_system_state = SYSTEM_STATE_DSP_PROCESSING;
            }
            break;
            
        case SYSTEM_STATE_DEEP_SLEEP:
            // Phase 4 Cool-down: The MCU drops into EM2.
            break;

        // ---------------------------------------------------------
        // PHASE 5: DIGITAL SIGNAL PROCESSING
        // ---------------------------------------------------------
        case SYSTEM_STATE_DSP_PROCESSING: {
            // Run the CMSIS-DSP pipeline to extract the 8 features
            bool success = wombcare_dsp_run_pipeline(&current_patient_features);
            
            if (success) {
                // Successfully extracted 8 features! Move to Phase 6 (AI)
                current_system_state = SYSTEM_STATE_ML_INFERENCE;
            } else {
                // Not enough valid heartbeats detected (e.g., too much noise).
                // Abort ML inference and go back to collecting data.
                current_system_state = SYSTEM_STATE_IMU_EVAL; 
            }
            break;
        }

        // ---------------------------------------------------------
        // PHASE 6: TINYML & BLUETOOTH (Pending)
        // ---------------------------------------------------------
        case SYSTEM_STATE_ML_INFERENCE:
            // TODO: Pass 'current_patient_features' into TFLite MVP accelerator
            
            // current_system_state = SYSTEM_STATE_BLE_BROADCAST;
            break;

        case SYSTEM_STATE_BLE_BROADCAST:
            // TODO: Update GATT server characteristics and notify local network
            
            // current_system_state = SYSTEM_STATE_IMU_EVAL; // Restart the cycle
            break;
    }
}