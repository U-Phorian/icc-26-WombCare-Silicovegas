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
#ifndef WOMBCARE_SCALER_H
#define WOMBCARE_SCALER_H
#define WOMBCARE_N_FEATURES 8
extern const float g_feat_mean[WOMBCARE_N_FEATURES];
extern const float g_feat_std[WOMBCARE_N_FEATURES];
extern const float g_in_scale; extern const int g_in_zp;
extern const float g_out_scale; extern const int g_out_zp;
#endif
