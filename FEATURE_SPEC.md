# WombCare — Feature Definition Spec (the parity contract)

**Purpose:** This is the single source of truth for the 8 features the TinyML model
consumes. **The training pipeline (`ml/`) and the firmware (`wombcare_dsp.c`) MUST
compute these features identically** — same order, same units, same window, same
normalization. If the device computes them differently from how the model was
trained, the model receives out-of-distribution inputs and fails silently.

Owners: Nitish (ML/training) + Malay (firmware). Change this file only by mutual
agreement, and bump `spec_version`.

`spec_version: 1`

---

## 1. Canonical feature vector (order is fixed = TFLite input order)

The model input is a length-8 `float32` vector in **exactly** this order (matches
`WombCareFeatures_t` in [wombcare_dsp.h](wombcare_dsp.h)):

| # | Name | Unit | Source signal |
|---|------|------|---------------|
| 0 | `LB`        | bpm      | fetal FHR |
| 1 | `MSTV`      | bpm      | fetal FHR |
| 2 | `MLTV`      | bpm      | fetal FHR |
| 3 | `AC_rate`   | per min  | fetal FHR |
| 4 | `DEC_rate`  | per min  | fetal FHR (DL+DS combined) |
| 5 | `FM_rate`   | per min  | PVDF |
| 6 | `MeanHR`    | bpm      | fetal FHR |
| 7 | `Variance`  | bpm²     | fetal FHR |

> **Key change from the current firmware:** all variability features are in **bpm**
> (not ms), and all count features are **per-minute rates** (not raw counts). This
> is what removes the scale mismatch with the UCI training data.

---

## 2. Global rules (apply to every feature)

**G1 — Inputs.** Features are derived from the **fetal R-peak series** produced by
Pan-Tompkins on the LMS *error* output (the fetal ECG), plus the PVDF array.
- `RR[i]` = interval between consecutive fetal R-peaks, in **ms**.
- `FHR[i] = 60000 / RR[i]` = instantaneous fetal heart rate, in **bpm**.

**G2 — Valid-beat filter (do this before any feature).** Discard a beat if
`FHR[i] < 50` or `FHR[i] > 240` bpm, or if `|FHR[i] − FHR[i−1]| > 25` bpm
(artifact jump). Compute all features on the **valid** FHR series only.

**G3 — Analysis window `W`.** All features are computed over the most recent `W`
seconds of FHR. **`W` is a shared constant.**
- Firmware today: `W = 60 s` ([wombcare_buffer.h:11](wombcare_buffer.h#L11)).
- **Recommended: `W = 240 s`** (store the derived FHR/RR series — a few Hz, tiny —
  in a longer buffer; you do NOT need 4 min of raw 250 Hz ECG). Longer `W` makes
  the rate features (AC/DEC/FM) far more stable and closer to the UCI segment
  statistics (median UCI segment ≈ 3.3 min).
- `W_min = W / 60` (minutes) is used for per-minute normalization.

**G4 — Normalization = per minute.** Every count feature is divided by `W_min`.
This makes the device (any `W`) and the training data (variable-length segments)
directly comparable.

**G5 — Standardization (scaler).** After the 8 features are computed, apply the
**z-score scaler saved during training**: `z[k] = (x[k] − mean[k]) / std[k]`.
`mean[]` and `std[]` are shipped as C constants alongside the model
(`ml/artifacts/firmware/`). The device applies this right before inference.

**G6 — Calibration (MSTV/MLTV only).** These two are SisPorto-specific and can't be
reproduced exactly. Phase-1 validation compares device-formula values to the UCI
distribution and, if needed, fits a per-feature linear map `x' = a·x + b` so the
device values land in the training range. `a,b` (default 1,0) are shipped with the
scaler. See §5.

---

## 3. Per-feature definitions

### 0. `LB` — Baseline Fetal Heart Rate (bpm)
The steady-state heart rate, excluding accel/decel excursions.
- **Formula:** `LB = median(FHR_valid)` over `W` (median is robust to excursions;
  a trimmed mean is acceptable if identical on both sides).
- **Training source:** UCI column `LB` (already bpm). No normalization.
- **Parity confidence:** High.

### 1. `MSTV` — Mean Short-Term Variability (bpm)
Beat-to-beat variability.
- **Formula:** `MSTV = mean( |FHR[i+1] − FHR[i]| )` over valid beats, in **bpm**.
- **Training source:** UCI `MSTV`.
- **Firmware fix:** currently computed in ms on RR ([dsp.c:106](wombcare_dsp.c#L106)) →
  change to bpm on FHR.
- **Parity confidence:** Medium → **requires §6 calibration + §5 validation.**

### 2. `MLTV` — Mean Long-Term Variability (bpm)
Minute-scale swing of the heart rate.
- **Formula:** split `W` into consecutive **60 s** sub-blocks; per block compute
  `max(FHR) − min(FHR)`; `MLTV = mean` of those block ranges, in **bpm**.
- **Training source:** UCI `MLTV`.
- **Firmware fix:** currently hardcoded `0` ([dsp.c:132](wombcare_dsp.c#L132)) → implement.
- **Parity confidence:** Medium → **requires §6 calibration + §5 validation.**

### 3. `AC_rate` — Accelerations per minute
- **Episode definition:** FHR rises `≥ 15 bpm` above `LB` for a sustained
  `≥ 15 s`. Count episodes, then `AC_rate = episodes / W_min`.
- **Training source:** `AC_rate = UCI.AC / dur_min`, where
  `dur_min = (e − b) / 4 / 60` (UCI CTG is 4 Hz).
- **Firmware fix:** currently counts single beats, not ≥15 s episodes
  ([dsp.c:111](wombcare_dsp.c#L111)) → add duration state-tracking.
- **Parity confidence:** High (once episode-based + per-minute).

### 4. `DEC_rate` — Decelerations per minute (DL + DS combined)
- **Episode definition:** FHR drops `≥ 15 bpm` below `LB` for `≥ 15 s`.
  `DEC_rate = episodes / W_min`.
- **Training source:** `DEC_rate = (UCI.DL + UCI.DS) / dur_min`. (`DS ≈ 0` in this
  dataset, so effectively DL.)
- **Firmware fix:** same as AC — episode-based, not per-beat.
- **Parity confidence:** High.

### 5. `FM_rate` — Fetal Movements per minute (from PVDF)
- **Formula:** on the PVDF array over `W`: high-pass ~3 Hz (remove respiration),
  `rms = arm_rms_f32(pvdf)`, count threshold-crossings where `sample > 4·rms` with a
  **1 s refractory**; `FM_rate = kicks / W_min`.
- **Training source:** `FM_rate = UCI.FM / dur_min`.
- **Note:** FM **is a model input**, so PVDF feeds the ML path (supersedes the
  earlier "PVDF bypasses ML" wording). The same kick count is also sent to BLE.
- **Parity confidence:** Low (PVDF kicks ≠ CTG movement sensor). **Candidate to
  drop if Phase-1 shows it doesn't help — see §4.**

### 6. `MeanHR` — Mean Heart Rate (bpm)
- **Formula:** `mean(FHR_valid)` over `W`, in **bpm**.
- **Training source:** UCI `Mean`.
- **Firmware fix:** currently set equal to `LB` ([dsp.c:94](wombcare_dsp.c#L94)) →
  compute the true mean of instantaneous FHR.
- **Parity confidence:** High.

### 7. `Variance` — Heart Rate Variance (bpm²)
- **Formula:** `var(FHR_valid)` over `W`, in **bpm²** (variance of instantaneous
  **FHR**, not RR intervals).
- **Training source:** UCI `Variance`.
- **Firmware fix:** currently RR-interval variance
  ([dsp.c:99](wombcare_dsp.c#L99)) → change to FHR variance in bpm².
- **Parity confidence:** Medium-High.

---

## 4. Feature-set decision (finalized in Phase 1)
Phase 1 trains and reports accuracy with:
- **Set A (all 8)** — as above.
- **Set B (high-parity 6)** — drop `MLTV` and `FM_rate` (lowest parity confidence).

**Rule: fewer features that the device reproduces faithfully beat more features it
can't.** We keep whichever set gives comparable Pathologic recall with lower parity
risk. The chosen set + `spec_version` is frozen after Phase 1.

---

## 5. Validation procedure (Phase 4 gate)
1. Compute device-formula features on a set of signals (e.g. reconstructed/synthetic
   FHR, or CTU-UHB records processed with the reference code).
2. Plot device-feature histograms vs UCI training histograms per feature.
3. **Pass criterion:** overlapping ranges and similar mean/std (after §6 calibration).
   If a feature can't be made to overlap, drop it (update the set + retrain).

---

## 6. Reference implementation & artifacts
- **Reference feature code:** `ml/features_v2.py` already computes LB, variability,
  accel/decel episodes, mean, variance from an FHR series — Malay mirrors it in C.
- **Shipped with the model** (`ml/artifacts/firmware/`): `model_data.c/.h`,
  `scaler` (`mean[]`, `std[]`), optional `calib` (`a[]`, `b[]`), and
  `feature_order.txt` (this order).
- **Machine-readable copy of this spec:** `ml/feature_spec.json`.

---

## 7. Open items to confirm with the team
- **O-A:** Adopt `W = 240 s` (store derived FHR, not raw ECG) for stable rates? *(recommended)*
- **O-B:** Final feature set A (8) vs B (6) — decided by Phase-1 numbers.
- **O-C:** `LB` = median vs trimmed-mean (pick one, both sides identical).
