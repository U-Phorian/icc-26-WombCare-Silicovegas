# Phase 4 — ML Integration (`wombcare_ml.cc`)

The TinyML glue is written and ready to drop into the Simplicity Studio project.
It takes the 8 features from `wombcare_dsp.c`, runs the int8 model on the MVP, and
returns Normal / Suspect / Pathologic + confidence.

## Files involved
| File | Purpose |
|---|---|
| `inc/wombcare_ml.h` / `src/wombcare_ml.cc` | inference API + TFLite-Micro implementation |
| `ml/artifacts/ctg/firmware/model_data.c` / `.h` | the int8 model as a C array (1.5 KB) |
| `ml/artifacts/ctg/firmware/scaler.c` / `.h` | trained mean/std + quant params (standardize step) |
| `ml/artifacts/ctg/firmware/golden_vectors.h` | 6 known feature→result cases to verify the C inference |

The four generator outputs live **only** under `ml/artifacts/ctg/firmware/`, written
by `ml/ctg_train_export.py`. They used to be duplicated at the repo root; the copies
are gone and `Wombcare_PreFinal2.slcp` compiles the originals in place, so retraining
cannot leave the firmware building a stale model.

## Wire-up (Malay)
1. **TensorFlow Lite Micro component** — already selected in
   `Wombcare_PreFinal2.slcp` as `tensorflow_lite_micro_optimized_kernels`,
   backed by the `aiml` SDK extension 3.0.0.
2. **Build sources** — `src/wombcare_ml.cc` and the two generator outputs are
   already listed under `source:` in `Wombcare_PreFinal2.slcp`.
3. In `app_init()`: call `wombcare_ml_init();` once.
4. In the state machine, after `wombcare_dsp_run_pipeline(&features)` succeeds:
```c
WombCareResult_t r = wombcare_ml_run(&features);
if (r.ok) {
    // r.nsp -> 0/1/2 ; r.confidence ; r.probs[3]
    const char *status = wombcare_ml_status_str(r.nsp);   // "HEALTHY"/"WATCH"/"ALERT"
    // hand status + r.confidence + features.lb_bpm + kick_count to BLE (Nikolaos)
}
```

## API
```c
void wombcare_ml_init(void);
WombCareResult_t wombcare_ml_run(const WombCareFeatures_t *features);
const char *wombcare_ml_status_str(WombCareNSP_t nsp);
```
`WombCareResult_t = { nsp, confidence, probs[3], ok }`.

## ✅ Golden self-test (run this to prove the model works before the DSP is done)
This verifies the *inference path* (scaler + quant + model) independently of the DSP.
Drop into a test build:
```c
#include "golden_vectors.h"
// after wombcare_ml_init():
for (int i = 0; i < GOLDEN_N; i++) {
    WombCareFeatures_t f;
    float *g = (float*)golden_features[i];
    f.lb_bpm=g[0]; f.mstv_ms=g[1]; f.mltv_ms=g[2]; f.accel_count=g[3];
    f.decel_count=g[4]; f.fetal_movements=g[5]; f.mean_hr_bpm=g[6]; f.hr_variance=g[7];
    WombCareResult_t r = wombcare_ml_run(&f);
    // PASS if r.nsp == golden_expected_class[i]
    //      and each r.probs[k] ~= golden_expected_prob[i][k] (tolerance ~0.02)
}
```
If all 6 pass, the C inference matches the trained model exactly. If they fail, the
problem is the ML glue (op resolver / arena / scaler), NOT the DSP — isolate fast.

## Parity gate (the real Phase 4 goal — needs the Phase 3 DSP fixes first)
The golden test proves *the model runs correctly*. Separately we must prove *the DSP
feeds it in-distribution features* (FEATURE_SPEC.md §5):
1. Log the 8 features the DSP computes on real/replay signals.
2. Compare their distributions to the UCI training ranges (I'll provide the plot/check).
3. Pass = overlapping ranges. If `MLTV`/`FM` don't overlap, retrain on the 6-feature
   set (I have it ready) — small recall cost, big robustness gain.

## Status
- ML inference glue: **done** (this file + `wombcare_ml.*`).
- On-device compile + golden test: **Malay** (needs TFLite-Micro component added).
- Feature-parity validation: **after** Phase 3 DSP fixes land.
