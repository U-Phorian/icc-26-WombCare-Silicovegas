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
package com.silicovegas.wombcare.core.data

import android.content.Context
import android.content.SharedPreferences
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton

/**
 * The demo-mode toggle (Settings → Demo mode). A plain [SharedPreferences] boolean rather
 * than DataStore, because the device layer needs it *synchronously* when deciding which
 * source to open on connect, and a single boolean doesn't justify a suspend read.
 *
 * Defaults to **demo ON**: the app is fully demoable out of the box, and the real firmware
 * BLE stack isn't finished yet, so a fresh install should show live-looking data rather
 * than an empty scan.
 */
@Singleton
class DeviceModePreference @Inject constructor(
    @ApplicationContext context: Context,
) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("wombcare_device", Context.MODE_PRIVATE)

    fun isDemoMode(): Boolean = prefs.getBoolean(KEY_DEMO, true)

    fun setDemoMode(enabled: Boolean) {
        prefs.edit().putBoolean(KEY_DEMO, enabled).apply()
    }

    private companion object {
        const val KEY_DEMO = "demo_mode"
    }
}
