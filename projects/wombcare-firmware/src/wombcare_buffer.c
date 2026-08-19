/***************************************************************************//**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 ******************************************************************************/
#include "wombcare_buffer.h"
#include "wombcare_sensors.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define ADC_CENTER        2048.0f
#define ADC_TO_UV_SCALE   805.8f

//------------------------------------------------------------------
// Ring Buffer Storage
//------------------------------------------------------------------

RingBufferTracker_t tracker_mother;
RingBufferTracker_t tracker_fetal;
RingBufferTracker_t tracker_pvdf;

/*
 * Set true whenever a complete 60-second window
 * has been accumulated.
 */
volatile bool minute_window_ready = false;

float ring_mother_ecg[RING_BUFFER_CAPACITY];
float ring_fetal_ecg[RING_BUFFER_CAPACITY];
float ring_pvdf_kick[RING_BUFFER_CAPACITY];

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

    /* One DMA buffer contains one second of data:
     * 750 ADC values = 250 scans × 3 channels
     */
    uint32_t num_samples = DMA_BUFFER_SIZE / NUM_CHANNELS;

    for (uint32_t i = 0U; i < num_samples; i++)
    {
        uint32_t dma_idx = i * NUM_CHANNELS;

        /*----------------------------------------------------------
         * Convert ADC Counts -> Physical Units
         *---------------------------------------------------------*/

        ring_mother_ecg[tracker_mother.head] =
            ((float)dma_source_buffer[dma_idx + CH_MOTHER_ECG] -
             ADC_CENTER) * ADC_TO_UV_SCALE;

        ring_fetal_ecg[tracker_fetal.head] =
            ((float)dma_source_buffer[dma_idx + CH_FETAL_ECG] -
             ADC_CENTER) * ADC_TO_UV_SCALE;

        /* PVDF remains raw for later DSP normalization */
        ring_pvdf_kick[tracker_pvdf.head] =
            (float)dma_source_buffer[dma_idx + CH_PVDF_KICK];

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
    }

    /*----------------------------------------------------------
     * One complete minute is now available.
     *
     * DSP + Feature Extraction + ML may now execute.
     *---------------------------------------------------------*/
    if (tracker_mother.is_primed &&
        tracker_fetal.is_primed &&
        tracker_pvdf.is_primed)
    {
        minute_window_ready = true;
    }
}