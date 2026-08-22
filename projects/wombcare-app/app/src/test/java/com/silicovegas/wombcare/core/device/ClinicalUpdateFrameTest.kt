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
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Encode (simulator) and decode (production parser) are mirror images of the wire contract.
 * If either drifts, this round trip breaks — which is the point of testing them together.
 */
class ClinicalUpdateFrameTest {

    private fun roundTrip(bytes: ByteArray) =
        (ClinicalUpdateParser.parse(bytes, 0) as ClinicalUpdateParseResult.Success).reading

    @Test
    fun `a normal window round-trips field for field`() {
        val r = roundTrip(
            ClinicalUpdateFrame.v1(
                nsp = WellnessStatus.NORMAL, confidence = 87, fhrBpm = 142,
                kickCount = 8, deviceMinute = 513,
            ),
        )
        assertEquals(WellnessStatus.NORMAL, r.status)
        assertEquals(87, r.confidencePercent)
        assertEquals(142, r.fhrBpm)
        assertEquals(8, r.kickCountInWindow)
        assertEquals(513, r.deviceMinute) // proves the LE encode matches the LE decode
    }

    @Test
    fun `pathologic sets the alert flag on the wire`() {
        val r = roundTrip(
            ClinicalUpdateFrame.v1(WellnessStatus.PATHOLOGIC, 80, 170, 0, deviceMinute = 5),
        )
        assertEquals(WellnessStatus.PATHOLOGIC, r.status)
        assertTrue(r.alert)
    }

    @Test
    fun `zero fhr encodes and decodes as no lock`() {
        val r = roundTrip(ClinicalUpdateFrame.v1(WellnessStatus.NORMAL, 0, 0, 0, deviceMinute = 0))
        assertNull(r.fhrBpm)
        assertTrue(r.isPlaceholder)
    }

    @Test
    fun `signal-low and active flags survive the round trip independently`() {
        val r = roundTrip(
            ClinicalUpdateFrame.v1(
                WellnessStatus.NORMAL, 40, 150, 1, deviceMinute = 4,
                signalLow = true, motherActive = true,
            ),
        )
        assertTrue(r.signalLow)
        assertTrue(r.motherActive)
        assertFalse(r.confidenceIsTrustworthy)
    }
}
