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
package com.silicovegas.wombcare.core.ble

/**
 * Link state as the UI must present it — deliberately more granular than "connected".
 *
 * Two distinctions carry real weight:
 *  - [Connected] vs [Monitoring]: the GATT link can be up while the device sits asleep
 *    waiting for BTN0. Reporting "Connected" as if data were flowing would leave the mother
 *    staring at an empty screen with no idea she must press the button.
 *  - [Monitoring] vs [SignalLost]: one notification per 60 s means silence is normal.
 *    Only [WombCareGatt.DROPOUT_MILLIS] of it is a fault.
 */
sealed interface ConnectionState {
    data object Idle : ConnectionState
    data object Scanning : ConnectionState
    data object Connecting : ConnectionState

    /** Bonding/encryption in progress — the phone is asking for the device passkey. */
    data object Pairing : ConnectionState

    /** Link up and subscribed, but no clinical window received yet. */
    data object Connected : ConnectionState

    /** Receiving windows on schedule. */
    data object Monitoring : ConnectionState

    /** Was monitoring, then went quiet past the dropout threshold. */
    data object SignalLost : ConnectionState

    data object BluetoothOff : ConnectionState
    data object PermissionRequired : ConnectionState
    data class Failed(val reason: String) : ConnectionState

    val isLive: Boolean get() = this is Connected || this is Monitoring
    val needsUserAction: Boolean
        get() = this is BluetoothOff || this is PermissionRequired || this is Failed
}
