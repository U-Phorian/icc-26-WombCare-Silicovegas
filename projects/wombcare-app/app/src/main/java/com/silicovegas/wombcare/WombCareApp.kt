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
package com.silicovegas.wombcare

import android.app.Application
import dagger.hilt.android.HiltAndroidApp

/**
 * App Check attests that requests come from a genuine build of this app, not a scraped
 * config talking straight to the REST API. It's the backstop that makes the "a doctor who
 * knows a code can resolve it" rule safe.
 *
 * The provider differs per build type (Play Integrity for release, the debug provider for
 * dev), and the debug provider isn't on the release classpath — so the actual install lives
 * in [installAppCheck], which has a separate implementation in the `debug` and `release`
 * source sets. This class stays variant-agnostic.
 */
@HiltAndroidApp
class WombCareApp : Application() {
    override fun onCreate() {
        super.onCreate()
        installAppCheck(this)
    }
}
