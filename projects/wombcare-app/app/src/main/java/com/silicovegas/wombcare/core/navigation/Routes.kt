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
package com.silicovegas.wombcare.core.navigation

/**
 * Every navigation destination as a typed object. Route strings live only here so a typo
 * can't silently break navigation — screens reference [Routes] members, never literals.
 */
object Routes {
    const val SPLASH = "splash"
    const val ONBOARDING = "onboarding"
    const val ROLE_SELECT = "role_select"

    // Auth. `role` is carried as an argument into signup so one screen serves both.
    const val LOGIN = "login"
    const val SIGNUP = "signup/{role}"
    const val FORGOT_PASSWORD = "forgot_password"
    fun signup(role: String) = "signup/$role"

    // Legal. `doc` is "terms" or "privacy"; opened from signup and from settings.
    const val LEGAL = "legal/{doc}"
    fun legal(doc: String) = "legal/$doc"

    // Landing zones after auth — role decides which.
    const val PATIENT_HOME = "patient_home"
    const val DOCTOR_HOME = "doctor_home"

    // Patient subtree.
    const val PATIENT_DASHBOARD = "patient_dashboard"
    const val PATIENT_SETTINGS = "patient_settings"
    const val PATIENT_SHARE = "patient_share"
    const val PATIENT_SCAN = "patient_scan"

    // Doctor subtree.
    const val DOCTOR_LIST = "doctor_list"
    const val DOCTOR_ADD = "doctor_add"
    const val DOCTOR_DETAIL = "doctor_detail/{patientUid}"
    fun doctorDetail(patientUid: String) = "doctor_detail/$patientUid"
    const val DOCTOR_ALERTS = "doctor_alerts"

    // Debug-only design gallery (kept reachable during development).
    const val GALLERY = "gallery"
}
