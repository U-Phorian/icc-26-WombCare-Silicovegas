#include "wombcare_buffer.h"
#include "wombcare_sensors.h"
#include "wombcare_debug.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

//------------------------------------------------------------------
// Ring Buffer Storage
//------------------------------------------------------------------

RingBufferTracker_t tracker_mother;
RingBufferTracker_t tracker_fetal;
RingBufferTracker_t tracker_pvdf;

/*
 * Set when a fresh slice of data has landed on top of a full
 * 60-second window. The DSP stage consumes the flag and clears it
 * (wombcare_dsp_run_pipeline).
 */
volatile bool minute_window_ready = false;

/*----------------------------------------------------------
 * Re-trigger interval, in samples per channel.
 *
 * The ring is a rolling 60-second window, so is_primed stays true
 * forever after the first minute and cannot be used as the trigger
 * on its own. Instead we count newly ingested samples and fire every
 * time this many have arrived.
 *
 * SAMPLE_RATE_HZ == one trigger per second, i.e. the DSP re-runs on
 * the latest rolling minute every second. That is the most responsive
 * setting and also the heaviest: each run walks 15000 samples x 3
 * channels. Raise this (e.g. 5 * SAMPLE_RATE_HZ) if the super-loop
 * cannot keep up and DMA buffers start being ingested late.
 *---------------------------------------------------------*/
#define DSP_TRIGGER_INTERVAL_SAMPLES   SAMPLE_RATE_HZ

static uint32_t samples_since_trigger = 0U;

/*
 * Raw ADC counts (uint16). Conversion to physical units is done by
 * the DSP unwrap stage to keep this storage compact.
 */
uint16_t ring_mother_ecg[RING_BUFFER_CAPACITY];
uint16_t ring_fetal_ecg[RING_BUFFER_CAPACITY];
uint16_t ring_pvdf_kick[RING_BUFFER_CAPACITY];

//------------------------------------------------------------------
// Buffer Management
//------------------------------------------------------------------

void wombcare_buffer_init(void)
{
    wombcare_buffer_reset();
}

void wombcare_buffer_reset(void)
{
    tracker_mother.head = 0U;
    tracker_mother.valid_samples = 0U;
    tracker_mother.is_primed = false;

    tracker_fetal.head = 0U;
    tracker_fetal.valid_samples = 0U;
    tracker_fetal.is_primed = false;

    tracker_pvdf.head = 0U;
    tracker_pvdf.valid_samples = 0U;
    tracker_pvdf.is_primed = false;

    minute_window_ready = false;
    samples_since_trigger = 0U;

    memset(ring_mother_ecg, 0, sizeof(ring_mother_ecg));
    memset(ring_fetal_ecg, 0, sizeof(ring_fetal_ecg));
    memset(ring_pvdf_kick, 0, sizeof(ring_pvdf_kick));
}

void wombcare_buffer_ingest(const uint16_t *dma_source_buffer)
{
    /* Defensive programming */
    if (dma_source_buffer == NULL)
    {
        return;
    }

#if WOMBCARE_DEBUG_LOGGING
    {
        static uint32_t ingest_counter = 0U;

        ingest_counter++;

        WC_LOG("DMA 0: M=%u F=%u P=%u | "
       "DMA 1: M=%u F=%u P=%u | "
       "DMA 2: M=%u F=%u P=%u\r\n",

       (unsigned)dma_source_buffer[0],
       (unsigned)dma_source_buffer[1],
       (unsigned)dma_source_buffer[2],

       (unsigned)dma_source_buffer[3],
       (unsigned)dma_source_buffer[4],
       (unsigned)dma_source_buffer[5],

       (unsigned)dma_source_buffer[6],
       (unsigned)dma_source_buffer[7],
       (unsigned)dma_source_buffer[8]);
    }
#endif

    /* One DMA buffer contains one second of data:
     * 750 ADC values = 250 scans × 3 channels
     */
    uint32_t num_samples = DMA_BUFFER_SIZE / NUM_CHANNELS;

    for (uint32_t i = 0U; i < num_samples; i++)
    {
        uint32_t dma_idx = i * NUM_CHANNELS;

        /*----------------------------------------------------------
         * Store raw ADC counts.
         *
         * Physical-unit conversion (ECG) and normalization (PVDF)
         * are performed downstream in the DSP unwrap stage.
         *---------------------------------------------------------*/

        ring_mother_ecg[tracker_mother.head] =
            dma_source_buffer[dma_idx + CH_MOTHER_ECG];

        ring_fetal_ecg[tracker_fetal.head] =
            dma_source_buffer[dma_idx + CH_FETAL_ECG];

        ring_pvdf_kick[tracker_pvdf.head] =
            dma_source_buffer[dma_idx + CH_PVDF_KICK];

        /*----------------------------------------------------------
         * Advance Write Pointers
         *---------------------------------------------------------*/

        tracker_mother.head++;
        tracker_fetal.head++;
        tracker_pvdf.head++;

        /*----------------------------------------------------------
         * Track Valid Samples
         *---------------------------------------------------------*/

        if (tracker_mother.valid_samples < RING_BUFFER_CAPACITY)
        {
            tracker_mother.valid_samples++;
        }

        if (tracker_fetal.valid_samples < RING_BUFFER_CAPACITY)
        {
            tracker_fetal.valid_samples++;
        }

        if (tracker_pvdf.valid_samples < RING_BUFFER_CAPACITY)
        {
            tracker_pvdf.valid_samples++;
        }

        /*----------------------------------------------------------
         * Ring Buffer Wrap
         *---------------------------------------------------------*/

        if (tracker_mother.head >= RING_BUFFER_CAPACITY)
        {
            tracker_mother.head = 0U;
            tracker_mother.is_primed = true;
        }

        if (tracker_fetal.head >= RING_BUFFER_CAPACITY)
        {
            tracker_fetal.head = 0U;
            tracker_fetal.is_primed = true;
        }

        if (tracker_pvdf.head >= RING_BUFFER_CAPACITY)
        {
            tracker_pvdf.head = 0U;
            tracker_pvdf.is_primed = true;
        }

        /*----------------------------------------------------------
         * Trigger Logic
         *
         * First trigger:
         *     the ring becomes fully primed (60 seconds collected).
         *
         * Subsequent triggers:
         *     every DSP_TRIGGER_INTERVAL_SAMPLES of new data, always
         *     against the latest rolling 60-second window.
         *---------------------------------------------------------*/

        if (tracker_mother.is_primed
            && tracker_fetal.is_primed
            && tracker_pvdf.is_primed)
        {
            samples_since_trigger++;

            if (samples_since_trigger >= DSP_TRIGGER_INTERVAL_SAMPLES)
            {
                samples_since_trigger = 0U;
                minute_window_ready = true;
            }
        }
    }
}
