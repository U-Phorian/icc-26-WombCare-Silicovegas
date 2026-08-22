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
import com.silicovegas.wombcare.core.ble.WellnessStatus

/**
 * A running monitoring session, rebuilt as each window arrives.
 *
 * Kept immutable and derived so the UI just renders it, and so the same snapshot can be
 * written to Room / synced to the doctor without a second computation. Aggregates follow
 * the contract's rules:
 *  - [averageFhr] ignores invalid (null) FHR windows — a "no lock" minute must not drag the
 *    average toward zero.
 *  - [totalKicks] is the app's running SUM; the firmware only reports per-window counts.
 *  - [worstStatus] is the most severe class seen this session, for the summary card.
 */
data class SessionSnapshot(
    val sessionId: String,
    val startedAtEpochMillis: Long,
    val readings: List<ClinicalReading> = emptyList(),
    val latest: ClinicalReading? = null,
    val totalKicks: Int = 0,
    val averageFhr: Int? = null,
    val worstStatus: WellnessStatus = WellnessStatus.NORMAL,
    val lastDecision: AlertDecision = AlertDecision.NONE,
) {
    val windowCount: Int get() = readings.size
}

/** Emitted when a window drives an alert-worthy transition, so the app can notify once. */
data class AlertEvent(
    val sessionId: String,
    val reading: ClinicalReading,
    val decision: AlertDecision,
)
