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

import androidx.compose.ui.unit.dp

/** 8 dp grid. 4 dp is allowed *inside* a tile and nowhere else. */
object Spacing {
    val xs = 4.dp
    val sm = 8.dp
    val md = 12.dp
    val lg = 16.dp
    val xl = 20.dp
    val xxl = 28.dp
    val screen = 20.dp
}

object Radii {
    val card = 20.dp
    val tile = 16.dp
    val pill = 999.dp
    val chip = 12.dp
}

object Sizes {
    /** Minimum interactive target. Material says 48; 44 is our floor for dense doctor rows. */
    val minTouch = 44.dp
    val hairline = 1.dp
    val statusDot = 10.dp
    val iconSm = 16.dp
    val iconMd = 20.dp
    val iconLg = 28.dp
}
