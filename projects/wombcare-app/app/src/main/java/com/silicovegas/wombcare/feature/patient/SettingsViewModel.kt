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
package com.silicovegas.wombcare.feature.patient

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.silicovegas.wombcare.core.data.AccountRepository
import com.silicovegas.wombcare.core.data.AuthRepository
import com.silicovegas.wombcare.core.data.DeviceModePreference
import com.silicovegas.wombcare.core.data.model.UserRole
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

/** State of the delete-my-data action, so the UI can show progress and errors. */
sealed interface DeleteState {
    data object Idle : DeleteState
    data object Deleting : DeleteState
    data class Error(val message: String) : DeleteState
}

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val deviceMode: DeviceModePreference,
    private val auth: AuthRepository,
    private val account: AccountRepository,
) : ViewModel() {

    private val _demoMode = MutableStateFlow(deviceMode.isDemoMode())
    val demoMode: StateFlow<Boolean> = _demoMode.asStateFlow()

    private val _deleteState = MutableStateFlow<DeleteState>(DeleteState.Idle)
    val deleteState: StateFlow<DeleteState> = _deleteState.asStateFlow()

    fun setDemoMode(enabled: Boolean) {
        deviceMode.setDemoMode(enabled)
        _demoMode.value = enabled
    }

    /**
     * Erase everything and delete the account. On success the auth state flips, which the
     * reactive host observes to route back to sign-in — so there's no explicit navigation.
     */
    fun deleteMyData() {
        val uid = auth.currentUid ?: return
        _deleteState.value = DeleteState.Deleting
        viewModelScope.launch {
            val role = auth.loadRole(uid)
            account.deleteMyData(uid, role)
                .onFailure {
                    _deleteState.value = DeleteState.Error(
                        "Couldn't finish deleting. You may need to log in again, then retry.",
                    )
                }
            // On success, do nothing: authState → SignedOut re-routes automatically.
        }
    }
}
