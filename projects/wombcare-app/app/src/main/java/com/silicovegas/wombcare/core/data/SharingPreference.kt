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
 * The master sharing switch, mirrored locally so the monitoring loop can decide
 * synchronously whether to push a window to the cloud. The authoritative copy also lives on
 * the patient's RTDB node (for visibility), but sync must not block on a network read every
 * minute — hence a local boolean.
 *
 * **Defaults to OFF.** Nothing about a session leaves the phone until the mother explicitly
 * turns sharing on — the privacy-by-default posture the whole product rests on.
 */
@Singleton
class SharingPreference @Inject constructor(
    @ApplicationContext context: Context,
) {
    private val prefs: SharedPreferences =
        context.getSharedPreferences("wombcare_sharing", Context.MODE_PRIVATE)

    fun isSharingEnabled(): Boolean = prefs.getBoolean(KEY, false)

    fun setSharingEnabled(enabled: Boolean) {
        prefs.edit().putBoolean(KEY, enabled).apply()
    }

    private companion object {
        const val KEY = "sharing_enabled"
    }
}
