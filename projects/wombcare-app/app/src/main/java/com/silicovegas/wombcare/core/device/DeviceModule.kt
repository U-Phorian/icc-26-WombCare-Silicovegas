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

import android.content.Context
import com.silicovegas.wombcare.core.data.DeviceModePreference
import dagger.Module
import dagger.Provides
import dagger.hilt.InstallIn
import dagger.hilt.android.qualifiers.ApplicationContext
import dagger.hilt.components.SingletonComponent
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.SupervisorJob
import javax.inject.Qualifier
import javax.inject.Singleton

/**
 * Provides the two device sources. The active one isn't chosen here — [DeviceSourceProvider]
 * resolves it per-connect from the user's demo-mode toggle, so the choice is a runtime
 * setting, not a build flavour. That keeps the demo switch one tap away in the app rather
 * than a rebuild.
 */
@Qualifier annotation class Simulated
@Qualifier annotation class Ble

@Module
@InstallIn(SingletonComponent::class)
object DeviceModule {

    @Provides @Singleton
    fun appScope(): CoroutineScope = CoroutineScope(SupervisorJob())

    @Provides @Singleton @Simulated
    fun simulated(scope: CoroutineScope): WombCareDeviceSource =
        SimulatedDeviceSource(scope)

    @Provides @Singleton @Ble
    fun ble(@ApplicationContext ctx: Context): WombCareDeviceSource =
        BleDeviceSource(ctx)
}

/**
 * Chooses which source to hand callers, based on the persisted demo-mode preference.
 * The ViewModels depend on this, not on a concrete source.
 */
@Singleton
class DeviceSourceProvider @javax.inject.Inject constructor(
    @Simulated private val simulated: WombCareDeviceSource,
    @Ble private val ble: WombCareDeviceSource,
    private val prefs: DeviceModePreference,
) {
    fun current(): WombCareDeviceSource = if (prefs.isDemoMode()) simulated else ble

    /** True when the next connect will use real Bluetooth (so BLE permissions are needed). */
    fun isRealDevice(): Boolean = !prefs.isDemoMode()
}
