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
// Auto-generated golden vectors (Feature Spec v1). wombcare_ml.cc must match.
#ifndef WOMBCARE_GOLDEN_H
#define WOMBCARE_GOLDEN_H
#define GOLDEN_N 6
#define GOLDEN_FEATS 8
static const float golden_features[GOLDEN_N][GOLDEN_FEATS] = {
  { 132.0000f, 2.1000f, 10.4000f, 1.5311f, 0.7656f, 0.0000f, 136.0000f, 12.0000f },
  { 122.0000f, 1.7000f, 9.7000f, 1.6198f, 0.8099f, 0.0000f, 125.0000f, 18.0000f },
  { 120.0000f, 0.5000f, 2.4000f, 0.0000f, 0.0000f, 0.0000f, 137.0000f, 73.0000f },
  { 144.0000f, 0.6000f, 8.5000f, 0.0000f, 0.2002f, 0.8007f, 149.0000f, 10.0000f },
  { 134.0000f, 5.9000f, 0.0000f, 0.2518f, 2.2665f, 0.0000f, 107.0000f, 170.0000f },
  { 132.0000f, 1.3000f, 14.2000f, 0.0000f, 0.3380f, 0.0000f, 117.0000f, 61.0000f },
};
static const int golden_expected_class[GOLDEN_N] = { 0, 0, 1, 1, 2, 2 };
static const float golden_expected_prob[GOLDEN_N][3] = {
  { 0.9961f, 0.0000f, 0.0000f },
  { 0.9961f, 0.0000f, 0.0000f },
  { 0.0078f, 0.9688f, 0.0234f },
  { 0.1055f, 0.8477f, 0.0469f },
  { 0.1367f, 0.1250f, 0.7383f },
  { 0.0312f, 0.1797f, 0.7891f },
};
#endif
