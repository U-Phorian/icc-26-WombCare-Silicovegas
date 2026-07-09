# WombCare NSP model — firmware integration note

The MG26 runs the int8 TFLite-Micro model `g_wombcare_nsp_model` (model_data.c),
1.5 KB, on the MVP accelerator. Governed by [FEATURE_SPEC.md](../../../../FEATURE_SPEC.md).

## Inference flow (in wombcare_ml.cc)
```c
// 1. wombcare_dsp_run_pipeline() fills WombCareFeatures_t in the Feature-Spec order:
//    [LB, MSTV, MLTV, AC_rate, DEC_rate, FM_rate, MeanHR, Variance]
//    (per-minute rates, bpm units — see FEATURE_SPEC.md, and fix the DSP per its §3)
// 2. Standardize:  xs[i] = (feat[i] - g_feat_mean[i]) / g_feat_std[i]     // scaler.c
// 3. Quantize:     q[i]  = round(xs[i] / g_in_scale + g_in_zp)            // int8
// 4. interpreter->Invoke();   // MVP-accelerated
// 5. Dequantize 3 outputs: p[k] = (out[k] - g_out_zp) * g_out_scale       // softmax probs
// 6. nsp = argmax(p);  confidence = p[nsp];
//    0 -> Normal (HEALTHY), 1 -> Suspect (WATCH), 2 -> Pathologic (ALERT)
```

## Tensors
- Input:  int8[1, 8]   (see scaler.c for g_in_scale / g_in_zp)
- Output: int8[1, 3]   softmax over {Normal, Suspect, Pathologic}

## Feature order (MUST match training)
See `feature_order.txt`. Any reorder = wrong predictions.

## Parity gate (Phase 4)
Before trusting on-device output, validate device-computed feature distributions
overlap the UCI training distributions (FEATURE_SPEC.md §5). `MLTV`/`FM` are the
watch-items; if they can't be matched, retrain on the 6-feature set.

## Files
`model_data.c/.h` (model), `scaler.c/.h` (mean/std + quant params), `feature_order.txt`.
