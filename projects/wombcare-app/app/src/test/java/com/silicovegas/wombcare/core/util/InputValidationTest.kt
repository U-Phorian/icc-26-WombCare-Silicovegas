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
package com.silicovegas.wombcare.core.util

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class InputValidationTest {

    @Test
    fun `accepts a well-formed email`() {
        assertNull(InputValidation.emailError("mom@example.com"))
        assertNull(InputValidation.emailError("  mom@example.com  "))
    }

    @Test
    fun `rejects malformed or empty emails`() {
        assertNotNull(InputValidation.emailError(""))
        assertNotNull(InputValidation.emailError("mom@"))
        assertNotNull(InputValidation.emailError("mom.example.com"))
        assertNotNull(InputValidation.emailError("a b@c.com"))
    }

    @Test
    fun `password needs 8 chars with a letter and a digit`() {
        assertNull(InputValidation.passwordError("kick2025"))
        assertNotNull(InputValidation.passwordError("short1")) // too short
        assertNotNull(InputValidation.passwordError("12345678")) // no letter
        assertNotNull(InputValidation.passwordError("password")) // no digit
    }

    @Test
    fun `confirm must match`() {
        assertNull(InputValidation.confirmPasswordError("kick2025", "kick2025"))
        assertNotNull(InputValidation.confirmPasswordError("kick2025", "kick2026"))
    }

    @Test
    fun `name and registration have sensible floors`() {
        assertNull(InputValidation.nameError("Asha"))
        assertNotNull(InputValidation.nameError(" "))
        assertNotNull(InputValidation.nameError("A"))

        assertNull(InputValidation.registrationError("MH-12345"))
        assertNotNull(InputValidation.registrationError(""))
        assertNotNull(InputValidation.registrationError("12"))
    }
}
