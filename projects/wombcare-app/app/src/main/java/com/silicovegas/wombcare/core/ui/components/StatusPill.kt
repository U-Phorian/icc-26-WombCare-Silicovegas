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
package com.silicovegas.wombcare.core.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import com.silicovegas.wombcare.R
import com.silicovegas.wombcare.core.ble.WellnessStatus
import com.silicovegas.wombcare.core.ui.format.visual
import com.silicovegas.wombcare.core.ui.theme.Radii
import com.silicovegas.wombcare.core.ui.theme.Sizes
import com.silicovegas.wombcare.core.ui.theme.Spacing
import com.silicovegas.wombcare.core.ui.theme.WombCareTheme

enum class PillSize { SMALL, LARGE }

/**
 * Clinical status as colour + icon + word, never fewer than all three.
 *
 * TalkBack reads "Status: Suspect" rather than announcing the icon and the text separately.
 */
@Composable
fun StatusPill(
    status: WellnessStatus,
    modifier: Modifier = Modifier,
    size: PillSize = PillSize.SMALL,
) {
    val v = status.visual()
    val description = stringResource(R.string.a11y_status, v.label)

    Surface(
        modifier = modifier.clearAndSetSemantics { contentDescription = description },
        shape = RoundedCornerShape(Radii.pill),
        color = v.container,
        contentColor = v.accent,
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(
                if (size == PillSize.LARGE) Spacing.sm else Spacing.xs,
            ),
            modifier = Modifier.padding(
                horizontal = if (size == PillSize.LARGE) Spacing.lg else Spacing.md,
                vertical = if (size == PillSize.LARGE) Spacing.sm else 5.dp,
            ),
        ) {
            Icon(
                imageVector = v.icon,
                contentDescription = null,
                modifier = Modifier.size(
                    if (size == PillSize.LARGE) Sizes.iconMd else Sizes.iconSm,
                ),
            )
            Text(
                text = v.label,
                style = if (size == PillSize.LARGE) {
                    MaterialTheme.typography.titleMedium
                } else {
                    MaterialTheme.typography.labelLarge
                },
            )
        }
    }
}

@Preview
@Composable
private fun StatusPillPreview() {
    WombCareTheme {
        Row(
            horizontalArrangement = Arrangement.spacedBy(Spacing.sm),
            modifier = Modifier.padding(Spacing.lg),
        ) {
            WellnessStatus.entries.forEach { StatusPill(it) }
        }
    }
}
