# Phase 3 — Firmware DSP Fixes (handoff to Malay)

**Goal:** make `wombcare_dsp.c` compute the 8 features **exactly** as the model was
trained (see [FEATURE_SPEC.md](../FEATURE_SPEC.md)). The skeleton is good — these are
targeted fixes. Each item lists the file:line, the problem, and the fix.

Model + scaler to link against: `ml/artifacts/ctg/firmware/`
(`model_data.c/.h`, `scaler.c/.h`, `feature_order.txt`, `INTEGRATION.md`).

---

## 🐞 FIX 1 — Use the fetal signal (real bug)
[wombcare_dsp.c:63,72](../../src/wombcare_dsp.c#L63)

The LMS call is correct, but you consume the **wrong buffer**. In `arm_lms_norm_f32`:
`pOut` = the *maternal estimate*, `pErr` = **fetal ECG (what you want)**.

```c
// call is fine: pSrc=mother(ref), pRef=abdom, pOut=maternal_est, pErr=FETAL
arm_lms_norm_f32(&lms_instance, flat_mother_ecg, flat_abdom_ecg,
                 maternal_estimate, clean_fetal_ecg, RING_BUFFER_CAPACITY);
// ^ rename: put the FETAL result in clean_fetal_ecg (the 5th arg / pErr),
//   and run peak detection on clean_fetal_ecg.
```
Right now peaks are detected on the maternal estimate → everything downstream is wrong.

---

## FIX 2 — Real Pan-Tompkins R-peak detection
[wombcare_dsp.c:70-84](../../src/wombcare_dsp.c#L70)

Fixed threshold `0.5f` won't survive amplitude changes. Implement the standard chain
(all CMSIS-DSP):
1. Band-pass ~5–15 Hz (cascade `arm_biquad_cascade_df1_f32` low+high).
2. Derivative (5-point finite difference).
3. Square (`arm_mult_f32(x, x, ...)`).
4. Moving-window integrate (~150 ms window = ~37 samples @250 Hz).
5. **Adaptive threshold** = running fraction of the integrated signal's peak; refractory
   ~250 ms (fetal RR ≥ ~375 ms). Store `RR[i]` in ms.

*(The old fixed-threshold code can stay as a fallback for the demo, but adaptive is required for real signals.)*

---

## FIX 3 — Valid-beat filter (new — Feature Spec §G2)
Before computing any feature, drop bad beats:
```c
// keep beat only if 50 <= FHR <= 240 and |FHR[i]-FHR[i-1]| <= 25
float fhr = 60000.0f / rr_ms;
if (fhr < 50.0f || fhr > 240.0f) continue;
if (i>0 && fabsf(fhr - fhr_prev) > 25.0f) continue;
```
Compute an `fhr[]` array (bpm) from the valid RR intervals — most features use `fhr[]`, not `rr[]`.

---

## FIX 4 — MSTV in **bpm**, not ms
[wombcare_dsp.c:106](../../src/wombcare_dsp.c#L106)

**Why this matters:** the model was trained on variability in **bpm** (UCI MSTV ≈
0.2–7). The current code computes `mean|ΔRR|` in **milliseconds** (tens of ms).
Feeding ms into a bpm-trained model → garbage output. Compute it directly on the
`fhr[]` (bpm) series — this is exact (no conversion needed):

```c
// mean absolute successive difference of FHR (bpm), not RR (ms)
float s=0; for (n=1;n<n_beats;n++) s += fabsf(fhr[n]-fhr[n-1]);
out->mstv = s/(n_beats-1);   // bpm
```

> **Do NOT convert ms→bpm by scaling against baseline** — the RR→BPM relationship
> is a reciprocal (`FHR = 60000/RR`), not linear. If you ever must convert an
> ms value, the correct first-order factor is `× LB²/60000`
> (since `dFHR/dRR = −FHR²/60000`), e.g. LB=140 → ×0.327. But computing on `fhr[]`
> directly avoids this approximation entirely and is the recommended path.

## FIX 5 — MeanHR and Variance on FHR (bpm / bpm²)
[wombcare_dsp.c:93-99](../../src/wombcare_dsp.c#L93)

```c
arm_mean_f32(fhr, n_beats, &out->mean_hr_bpm);   // NOT equal to LB
arm_var_f32 (fhr, n_beats, &out->hr_variance);   // variance of FHR in bpm^2, NOT RR
// LB = median(fhr) (robust baseline) — sort a copy or use a running median
```

## FIX 6 — MLTV (currently hardcoded 0)
[wombcare_dsp.c:132](../../src/wombcare_dsp.c#L132)

```c
// split the window into 60 s blocks; per block range = max(fhr)-min(fhr); average
// (map beats to blocks by cumulative time from RR intervals)
out->mltv = mean_over_blocks_of(maxFHR_block - minFHR_block);  // bpm
```

## FIX 7 — Accelerations / Decelerations = sustained episodes, per-minute
[wombcare_dsp.c:111-116](../../src/wombcare_dsp.c#L111)

Count **episodes** (≥15 bpm from LB for **≥15 s**), not single beats, then normalize:
```c
// walk fhr[] with elapsed-time tracking; an episode = deviation held >=15s
uint16_t acc_ep=0, dec_ep=0;  // count sustained episodes (state machine on duration)
out->accel_rate = acc_ep / W_MIN;   // W_MIN = window seconds / 60
out->decel_rate = dec_ep / W_MIN;
```

## FIX 8 — FM per-minute
[wombcare_dsp.c:119-128](../../src/wombcare_dsp.c#L119)

Keep the RMS-threshold kick detector, but normalize and add the 3 Hz high-pass first:
```c
// high-pass ~3 Hz on flat_pvdf (remove maternal respiration) before RMS/threshold
out->fm_rate = kick_count / W_MIN;
```

---

## FIX 9 — Fill the feature vector in Spec order, then hand to ML
Feature order (must match `feature_order.txt`):
`[LB, MSTV, MLTV, AC_rate, DEC_rate, FM_rate, MeanHR, Variance]`
Standardize + quantize + invoke happens in `wombcare_ml.cc` (Phase 4) using `scaler.c`.

---

## (Recommended) FIX 10 — longer feature window without more RAM
Feature Spec §G3: keep the 60 s **raw** buffer for LMS+Pan-Tompkins, but store the
derived `fhr[]`/`RR[]` series (a few Hz, tiny) over **~240 s** for stabler AC/DEC/FM
rates. Optional but improves parity.

---

## Checklist for Malay
- [ ] FIX 1: peak-detect on the LMS **error** output (fetal), not the estimate
- [ ] FIX 2: Pan-Tompkins adaptive peak detection
- [ ] FIX 3: valid-beat filter → build `fhr[]` (bpm)
- [ ] FIX 4–5: MSTV/Mean/Variance in bpm; LB = median
- [ ] FIX 6: implement MLTV
- [ ] FIX 7: episode-based AC/DEC, per-minute
- [ ] FIX 8: FM per-minute (+3 Hz HPF)
- [ ] FIX 9: fill struct in Spec order
- [ ] FIX 10 (opt): 240 s derived-FHR window
- [ ] Then Phase 4: `wombcare_ml.cc` + parity validation

Reference implementation for every feature formula: `ml/features_v2.py`.
