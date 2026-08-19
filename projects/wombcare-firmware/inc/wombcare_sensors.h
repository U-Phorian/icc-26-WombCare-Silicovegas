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
#ifndef WOMBCARE_SENSORS_H
#define WOMBCARE_SENSORS_H

#include <em_device.h>
#include <em_iadc.h>
#include <em_ldma.h>
#include <stdint.h>
#include <stdbool.h>

/*--------------------------------------------------------------------
 * IADC Analog Input Mapping
 *-------------------------------------------------------------------*/
#define IADC_INPUT_MOTHER_ECG    iadcPosInputPortBPin7
#define IADC_INPUT_FETAL_ECG     iadcPosInputPortBPin8
#define IADC_INPUT_PVDF_KICK     iadcPosInputPortDPin8

/*--------------------------------------------------------------------
 * Logical Channel Order
 *
 * This order MUST remain identical throughout:
 *      ADC Scan Table
 *      DMA Buffer
 *      DSP
 *      Feature Extraction
 *-------------------------------------------------------------------*/
#define CH_MOTHER_ECG            0U
#define CH_FETAL_ECG             1U
#define CH_PVDF_KICK             2U

/*--------------------------------------------------------------------
 * Canonical Sampling Configuration
 *-------------------------------------------------------------------*/
#define SAMPLE_RATE_HZ           250U

/* Backward compatibility */
#define SAMPLES_PER_SECOND       SAMPLE_RATE_HZ

#define NUM_CHANNELS             3U

/* 250 Scan Cycles × 3 Channels = 750 Samples */
#define DMA_BUFFER_SIZE          (SAMPLE_RATE_HZ * NUM_CHANNELS)

/*--------------------------------------------------------------------
 * DMA Ping-Pong Buffers
 *-------------------------------------------------------------------*/
extern uint16_t adcBufferPing[DMA_BUFFER_SIZE];
extern uint16_t adcBufferPong[DMA_BUFFER_SIZE];

/*--------------------------------------------------------------------
 * Buffer Ready Flags
 *-------------------------------------------------------------------*/
extern volatile bool ping_buffer_ready;
extern volatile bool pong_buffer_ready;

/*--------------------------------------------------------------------
 * Public Driver API
 *-------------------------------------------------------------------*/
void wombcare_hardware_init(void);

void wombcare_analog_start(void);

void wombcare_analog_stop(void);

#endif /* WOMBCARE_SENSORS_H */