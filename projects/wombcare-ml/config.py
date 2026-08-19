# *Licensed to the Apache Software Foundation (ASF) under one
# *or more contributor license agreements.  See the NOTICE file
# *distributed with this work for additional information
# *regarding copyright ownership.  The ASF licenses this file
# *to you under the Apache License, Version 2.0 (the
# *"License"); you may not use this file except in compliance
# *with the License.  You may obtain a copy of the License at
#
# *  http://www.apache.org/licenses/LICENSE-2.0
#
# *Unless required by applicable law or agreed to in writing,
# *software distributed under the License is distributed on an
# *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# *KIND, either express or implied.  See the License for the
# *specific language governing permissions and limitations
# *under the License.
"""WombCare ML pipeline -- central configuration.

Single source of truth for paths, label thresholds, windowing and the random
seed. All other scripts import from here so the team can tune one constant and
have it propagate (FR-ML-2: "Make the threshold a single config constant").

Paths are resolved relative to this file so scripts run from any working dir.
"""
from __future__ import annotations

from pathlib import Path

# --- Paths -------------------------------------------------------------------
ML_DIR = Path(__file__).resolve().parent            # projects/wombcare-ml/
REPO_DIR = ML_DIR.parents[1]                         # repo root
TOOLS_DIR = ML_DIR / "tools"                         # data scripts + their output
PROCESSED_DIR = TOOLS_DIR / "processed_dat"          # cleaned FHR (waveform_processing.py)
OUTCOMES_CSV = TOOLS_DIR / "outcomes.csv"            # pH/Apgar labels (generate_outcomes.py)

ARTIFACTS_DIR = ML_DIR / "artifacts"                 # models, scalers, splits
REPORTS_DIR = ML_DIR / "reports"                     # metrics json, figures, model card
for _d in (ARTIFACTS_DIR, REPORTS_DIR):
    _d.mkdir(parents=True, exist_ok=True)

# --- Signal ------------------------------------------------------------------
FS = 4                                               # Hz, CTU-UHB sampling rate

# --- Windowing (FR-ML-1) -----------------------------------------------------
# 20-min windows (N=4800 @ 4 Hz) match the published preprocessing target and
# WombCare's "30-min resting cycle". Set WINDOW_MIN=10 for the low-latency variant.
WINDOW_MIN = 20
WINDOW_OVERLAP = 0.5                                 # fraction overlap between windows
MIN_WINDOW_MIN = 8                                   # shortest record we still accept as 1 window

# Fetal acidemia develops near delivery, so the predictive FHR pattern lives in
# the *end* of each recording. WINDOW_MODE controls which windows we keep:
#   "tail" -> overlapping windows from the last TAIL_MIN minutes (default; most data)
#   "last" -> a single final window per record (cleanest, fewest samples)
#   "all"  -> every window across the recording (noisiest labels)
WINDOW_MODE = "tail"
TAIL_MIN = 40                                        # span sampled for "tail" mode

# --- Labels (FR-ML-2) --------------------------------------------------------
PH_ABNORMAL = 7.15                                   # primary cut-off -> ~18.7% positive
PH_SEVERE = 7.05                                     # "severe" tier head for the Alert logic
APGAR5_LOW = 7                                        # cross-check label: Apgar5 <= 7

# --- Reproducibility ---------------------------------------------------------
SEED = 1337
TEST_SIZE = 0.20                                      # patient-level holdout fraction
VAL_SIZE = 0.20                                       # of the train remainder, for early stopping

# --- Wellness score / tiers (FR-ML-7) ----------------------------------------
# Score = calibrated P(well) in [0,1]. Tiers map the score for UI + alert logic.
TIER_WATCH = 0.66                                     # score >= Good; below -> Watch
TIER_ALERT = 0.40                                     # score < Alert (aligns with severe head)


def window_samples(window_min: int | None = None) -> int:
    return int((window_min or WINDOW_MIN) * 60 * FS)


if __name__ == "__main__":
    print("ML_DIR        :", ML_DIR)
    print("PROCESSED_DIR :", PROCESSED_DIR, "exists:", PROCESSED_DIR.exists())
    print("OUTCOMES_CSV  :", OUTCOMES_CSV, "exists:", OUTCOMES_CSV.exists())
    print("window samples:", window_samples(), f"({WINDOW_MIN} min @ {FS} Hz)")
