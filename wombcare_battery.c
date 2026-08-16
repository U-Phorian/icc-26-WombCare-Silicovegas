#include "wombcare_battery.h"

#include <stdint.h>
#include <stdbool.h>

/*--------------------------------------------------------------------
 * Private Driver State
 *-------------------------------------------------------------------*/

/*
 * Cached battery percentage.
 *
 * This value is updated whenever a new battery conversion completes.
 * The application always reads this cached value rather than directly
 * accessing the ADC.
 */
static uint8_t s_battery_percent = 100U;

/*
 * Cached battery voltage (Volts).
 */
static float s_battery_voltage = 3.0f;

/*
 * Set when the application requests a battery measurement.
 *
 * The IADC driver (tailgated Single Queue) will clear this after
 * completing the conversion.
 */
static volatile bool s_measurement_requested = false;

static volatile bool s_conversion_active = false;

/*--------------------------------------------------------------------
 * Private Helpers
 *-------------------------------------------------------------------*/

/*
 * Convert battery voltage into percentage.
 *
 * Current assumptions:
 *      Full Battery  : 3.0 V
 *      Empty Battery : 2.2 V
 *
 * These values can later be tuned based on the actual battery chemistry.
 */
static uint8_t battery_voltage_to_percent(float voltage)
{
    const float FULL_VOLTAGE  = 3.0f;
    const float EMPTY_VOLTAGE = 2.2f;

    if (voltage >= FULL_VOLTAGE)
    {
        return 100U;
    }

    if (voltage <= EMPTY_VOLTAGE)
    {
        return 0U;
    }

    float percentage =
        ((voltage - EMPTY_VOLTAGE) /
        (FULL_VOLTAGE - EMPTY_VOLTAGE)) * 100.0f;

    return (uint8_t)(percentage + 0.5f);
}

/*--------------------------------------------------------------------
 * Public API
 *-------------------------------------------------------------------*/

void wombcare_battery_init(void)
{
    s_battery_voltage = 3.0f;
s_battery_percent = 100U;
s_measurement_requested = false;
s_conversion_active = false;
}

// void wombcare_battery_init(void)
// {
//     s_battery_voltage = 2.8f;
//     s_battery_percent = 77U;

//     s_measurement_requested = false;
//     s_conversion_active = false;
// }

void wombcare_battery_request_measurement(void)
{
    s_measurement_requested = true;
}

bool wombcare_battery_measurement_pending(void)
{
    return s_measurement_requested;
}


bool wombcare_battery_start_conversion(void)
{
    /*
     * No measurement requested.
     */
    if (!s_measurement_requested)
    {
        return false;
    }

    /*
     * Conversion already running.
     */
    if (s_conversion_active)
    {
        return false;
    }

    s_conversion_active = true;

    return true;
}

void wombcare_battery_update(float measured_voltage)
{
    s_battery_voltage = measured_voltage;
    s_battery_percent = battery_voltage_to_percent(measured_voltage);

    s_measurement_requested = false;
s_conversion_active = false;
}

float wombcare_battery_get_voltage(void)
{
    return s_battery_voltage;
}

uint8_t wombcare_battery_get_percentage(void)
{
    return s_battery_percent;
}