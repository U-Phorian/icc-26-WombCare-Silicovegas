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
package com.silicovegas.wombcare.feature.patient

import android.annotation.SuppressLint
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.silicovegas.wombcare.core.device.BleScanner
import com.silicovegas.wombcare.core.device.DiscoveredDevice
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.launchIn
import kotlinx.coroutines.flow.onCompletion
import kotlinx.coroutines.flow.onEach
import javax.inject.Inject

/**
 * Backs the "choose your device" screen. Streams the live scan list from [BleScanner].
 * The BLUETOOTH_SCAN permission is requested by the dashboard *before* navigating here, so
 * by the time this runs it's granted.
 */
@HiltViewModel
class DeviceScanViewModel @Inject constructor(
    private val scanner: BleScanner,
) : ViewModel() {

    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()

    private val _scanning = MutableStateFlow(false)
    val scanning: StateFlow<Boolean> = _scanning.asStateFlow()

    private var job: Job? = null

    fun isBluetoothOn(): Boolean = scanner.isBluetoothOn()

    @SuppressLint("MissingPermission") // dashboard requests BLUETOOTH_SCAN before navigating here
    fun startScan() {
        if (!scanner.isBluetoothOn()) return
        job?.cancel()
        _devices.value = emptyList()
        _scanning.value = true
        job = scanner.scan()
            .onEach { _devices.value = it }
            .onCompletion { _scanning.value = false }
            .launchIn(viewModelScope)
    }

    fun stopScan() {
        job?.cancel()
        _scanning.value = false
    }

    override fun onCleared() {
        job?.cancel()
    }
}
