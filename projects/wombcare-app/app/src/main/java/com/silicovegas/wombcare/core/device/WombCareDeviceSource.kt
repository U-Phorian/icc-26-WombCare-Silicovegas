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
package com.silicovegas.wombcare.core.device

import com.silicovegas.wombcare.core.ble.ClinicalReading
import com.silicovegas.wombcare.core.ble.ConnectionState
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.StateFlow

/**
 * The seam between "where readings come from" and everything above it.
 *
 * Both the real Bluetooth path ([BleDeviceSource]) and the demo path
 * ([SimulatedDeviceSource]) implement this, and — crucially — both emit
 * [ClinicalReading]s produced by the SAME `ClinicalUpdateParser`. Demo mode therefore
 * exercises the production decode path, not a parallel fake one, and swapping real hardware
 * in is a one-line change in the DI module. The UI never learns which it's talking to.
 */
interface WombCareDeviceSource {
    val connectionState: StateFlow<ConnectionState>
    val readings: Flow<ClinicalReading>

    /** Begin scanning/connecting (or, for the simulator, start emitting). */
    fun connect(deviceId: String? = null)

    fun disconnect()

    /** Human label for settings/debug ("WombCare device" vs "Demo mode"). */
    val sourceLabel: String
}
