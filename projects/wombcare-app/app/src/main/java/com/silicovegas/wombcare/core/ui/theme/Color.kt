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
package com.silicovegas.wombcare.core.ui.theme

import androidx.compose.ui.graphics.Color

/**
 * Design tokens. Phase 1 builds the component library on top of these; nothing else in the
 * app should declare a raw colour.
 *
 * Two role palettes off one neutral set: the mother sees rose, the doctor sees clinical
 * sky-blue. Same components, same spacing — so a demo never confuses the two apps.
 */

// Patient — rose
val RosePrimary = Color(0xFFD96A85)
val RoseOnPrimary = Color(0xFFFFFFFF)
val RoseContainer = Color(0xFFFFE4EA)
val RoseOnContainer = Color(0xFF5C2331)

// Doctor — clinical sky
val SkyPrimary = Color(0xFF4A90D9)
val SkyOnPrimary = Color(0xFFFFFFFF)
val SkyContainer = Color(0xFFDFEDFB)
val SkyOnContainer = Color(0xFF102A44)

// Neutrals
val Ink = Color(0xFF1F2430)
val InkMuted = Color(0xFF6B7280)
val Hairline = Color(0xFFECEEF2)
val Background = Color(0xFFFDFBFC)
val SurfaceLight = Color(0xFFFFFFFF)

val BackgroundDark = Color(0xFF14161C)
val SurfaceDark = Color(0xFF1C1F27)
val HairlineDark = Color(0xFF2C313C)
val InkDark = Color(0xFFECEEF2)
val InkMutedDark = Color(0xFF9AA3B2)

/**
 * Clinical status. Never used alone — every surface pairs the colour with an icon and the
 * word, so the status survives colour-blindness and greyscale printing.
 */
val StatusNormal = Color(0xFF2E9E7B)
val StatusSuspect = Color(0xFFE0A03A)
val StatusPathologic = Color(0xFFD64545)
val StatusUnknown = Color(0xFF6B7280)

val StatusNormalContainer = Color(0xFFDFF3EC)
val StatusSuspectContainer = Color(0xFFFCF0DA)
val StatusPathologicContainer = Color(0xFFFBE3E3)
val StatusUnknownContainer = Color(0xFFEDEFF2)
