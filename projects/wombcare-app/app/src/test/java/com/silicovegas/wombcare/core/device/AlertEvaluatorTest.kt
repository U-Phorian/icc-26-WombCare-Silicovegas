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

import com.silicovegas.wombcare.core.ble.ClinicalUpdateParseResult
import com.silicovegas.wombcare.core.ble.ClinicalUpdateParser
import com.silicovegas.wombcare.core.ble.WellnessStatus
import org.junit.Assert.assertEquals
import org.junit.Test

class AlertEvaluatorTest {

    private fun reading(
        nsp: WellnessStatus,
        confidence: Int = 85,
        signalLow: Boolean = false,
        minute: Int = 1,
    ) = (ClinicalUpdateParser.parse(
        ClinicalUpdateFrame.v1(nsp, confidence, fhrBpm = 150, kickCount = 0, deviceMinute = minute, signalLow = signalLow),
        receivedAtEpochMillis = 0,
    ) as ClinicalUpdateParseResult.Success).reading

    @Test
    fun `a single pathologic window does not alert — it needs two in a row`() {
        val e = AlertEvaluator()
        assertEquals(AlertDecision.ADVISORY, e.decisionFor(reading(WellnessStatus.PATHOLOGIC, minute = 1)))
        assertEquals(AlertDecision.ALERT, e.decisionFor(reading(WellnessStatus.PATHOLOGIC, minute = 2)))
    }

    @Test
    fun `a normal window between two pathologics resets the streak`() {
        val e = AlertEvaluator()
        e.decisionFor(reading(WellnessStatus.PATHOLOGIC, minute = 1))     // ADVISORY
        assertEquals(AlertDecision.NONE, e.decisionFor(reading(WellnessStatus.NORMAL, minute = 2)))
        // Next pathologic is again only the first — must not alert.
        assertEquals(AlertDecision.ADVISORY, e.decisionFor(reading(WellnessStatus.PATHOLOGIC, minute = 3)))
    }

    @Test
    fun `low signal turns a pathologic reading into a re-check, not an alarm`() {
        val e = AlertEvaluator()
        assertEquals(
            AlertDecision.RECHECK,
            e.decisionFor(reading(WellnessStatus.PATHOLOGIC, signalLow = true, minute = 1)),
        )
        // And even a second low-signal pathologic stays RECHECK, never ALERT.
        assertEquals(
            AlertDecision.RECHECK,
            e.decisionFor(reading(WellnessStatus.PATHOLOGIC, signalLow = true, minute = 2)),
        )
    }

    @Test
    fun `low confidence also downgrades a pathologic to re-check`() {
        val e = AlertEvaluator()
        assertEquals(
            AlertDecision.RECHECK,
            e.decisionFor(reading(WellnessStatus.PATHOLOGIC, confidence = 20, minute = 1)),
        )
    }

    @Test
    fun `suspect is advisory, normal is none`() {
        val e = AlertEvaluator()
        assertEquals(AlertDecision.ADVISORY, e.decisionFor(reading(WellnessStatus.SUSPECT)))
        assertEquals(AlertDecision.NONE, e.decisionFor(reading(WellnessStatus.NORMAL)))
    }
}
