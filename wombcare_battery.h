#ifndef WOMBCARE_BATTERY_H
#define WOMBCARE_BATTERY_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Request a battery measurement.
 *
 * The actual conversion is performed later by the IADC
 * tailgated Single Queue.
 */
void wombcare_battery_request_measurement(void);

/*
 * Returns true if a battery measurement is pending.
 */
bool wombcare_battery_measurement_pending(void);

/*
 * Called by the battery acquisition logic after a successful
 * AVDD conversion.
 */
void wombcare_battery_update(float measured_voltage);

/*
 * Starts a pending battery conversion.
 *
 * Returns:
 *      true  -> caller should trigger the IADC Single Queue
 *      false -> nothing to do (already running or no request)
 */
bool wombcare_battery_start_conversion(void);

/*--------------------------------------------------------------------
 * Battery Monitoring Driver
 *
 * Uses the internal IADC single-conversion path to periodically
 * measure the device supply voltage (AVDD). This driver is completely
 * independent of the continuous ECG scan engine so that battery
 * measurements never interfere with deterministic sensor sampling.
 *-------------------------------------------------------------------*/

/*
 * Initialize battery monitoring resources.
 *
 * Safe to call once during application startup.
 */
void wombcare_battery_init(void);

/*
 * Measure the current supply voltage.
 *
 * Returns:
 *     Supply voltage in Volts.
 */
float wombcare_battery_get_voltage(void);

/*
 * Convert the measured supply voltage into an approximate
 * battery percentage.
 *
 * Returns:
 *     0–100 %
 */
uint8_t wombcare_battery_get_percentage(void);

#endif /* WOMBCARE_BATTERY_H */