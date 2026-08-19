# Firmware fixes — status

## ✅ Applied to the repo
- **`wombcare_dsp.c`** — MSTV / MLTV / Variance / MeanHR now computed on an
  `fhr_bpm[]` array (bpm / bpm²), matching the model's training units
  (`../FEATURE_SPEC.md`). Search `PARITY FIX` in the file. (LMS, peak detection,
  accel/decel episodes, PVDF, counts-per-minute — unchanged.)

## ⏳ Pending on Malay's copies (these files are on his branch, not in this tree)
Malay's latest `app.c` (with `wombcare_ble` + the ML call) and `wombcare_ble.*`
are not merged here yet, so apply these two when they land:

**A) `app.c` — confidence fusion (one line).** `ml_output.confidence` is 0..1,
`physical_motion_trust` is 0..100 → dividing by 100 makes it ≈ 0.
```c
// BEFORE:  (uint8_t)((ml_output.confidence * (float)physical_motion_trust) / 100.0f)
// AFTER:
uint8_t final_dashboard_confidence =
    (uint8_t)(ml_output.confidence * (float)physical_motion_trust);   // 0..100
```

**B) `app.h` — no duplicate result struct.** The ML `WombCareResult_t`
(`.nsp`/`.confidence`) comes from `wombcare_ml.h`. If an older `app.h` still
defines its own `WombCareResult_t {features, imu_confidence, ml_prediction}`,
delete that — otherwise it collides.

## Verify (after merge)
Build on SDK 2026.6.0 → run the golden self-test (`ml/artifacts/ctg/firmware/golden_vectors.h`, snippet
in `phase4-ml-integration.md`). All 6 cases must match.

> Note: `wombcare_dsp.c` was reviewed, not compiled here (no ARM toolchain).
