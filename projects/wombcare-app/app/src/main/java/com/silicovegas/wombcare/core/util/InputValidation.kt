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

/**
 * Client-side form validation. Pure functions, unit-tested — the point is instant,
 * offline feedback in the form, not security. Firebase Auth remains the real authority on
 * whether an email is taken or a password is acceptable.
 */
object InputValidation {

    private val EMAIL = Regex("^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$")

    fun emailError(email: String): String? = when {
        email.isBlank() -> "Enter your email"
        !EMAIL.matches(email.trim()) -> "That doesn't look like an email address"
        else -> null
    }

    /**
     * Firebase enforces a 6-char minimum. We ask for 8 plus a letter and a digit — a
     * clinical app holding a pregnancy's data warrants better than the platform floor,
     * and saying so up front beats a rejected signup round trip.
     */
    fun passwordError(password: String): String? = when {
        password.length < 8 -> "Use at least 8 characters"
        !password.any { it.isLetter() } -> "Include at least one letter"
        !password.any { it.isDigit() } -> "Include at least one number"
        else -> null
    }

    fun confirmPasswordError(password: String, confirm: String): String? =
        if (password != confirm) "Passwords don't match" else null

    fun nameError(name: String): String? = when {
        name.isBlank() -> "Enter your name"
        name.trim().length < 2 -> "Enter your full name"
        else -> null
    }

    /** Doctors self-declare a registration number; we only check it's plausibly present. */
    fun registrationError(reg: String): String? = when {
        reg.isBlank() -> "Enter your medical registration number"
        reg.trim().length < 4 -> "That registration number looks too short"
        else -> null
    }
}
