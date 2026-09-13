#ifndef TEST_SENSORS_H
#define TEST_SENSORS_H
#include <stdint.h>
#include <stdbool.h>

#define CH_MOTHER_ECG 0U
#define CH_FETAL_ECG  1U
#define CH_PVDF_KICK  2U
#define SAMPLE_RATE_HZ 250U
#define SAMPLES_PER_SECOND SAMPLE_RATE_HZ
#define NUM_CHANNELS 3U

/*
 * ADC scaling. Mirrors wombcare_sensors.h, which derives these from the
 * IADC analog-gain setting -- the real header cannot be used here because
 * it pulls in the emlib IADC types. Values are the WOMBCARE_ADC_EXTENDED_RANGE
 * (0.5x gain) defaults, which is what the firmware builds with.
 *
 * If the real header's gain or scaling changes, change it here too or the
 * bench stops testing what ships.
 */
#define ADC_FULL_SCALE_MV       6600.0f
#define ADC_UV_PER_COUNT        ((ADC_FULL_SCALE_MV * 1000.0f) / 4096.0f)
#define ADC_MIDSCALE_COUNT      2048.0f
#define ADC_SUPPLY_RAIL_COUNT   ((3300.0f / ADC_FULL_SCALE_MV) * 4096.0f)
#define ADC_RAIL_FRACTION       0.95f

#define DMA_BUFFER_SIZE         (SAMPLE_RATE_HZ * NUM_CHANNELS)

#endif
