# WombCare model -- firmware integration note (FR-ML-6)

The MG26 runs the int8 TFLite-Micro model `g_wombcare_model` (model_data.c).

## Inference entry point (pseudocode)
```c
// 1. Compute the 22 FHR features for the current 20-min window (see feature_order.txt).
// 2. Standardise: x_std[i] = (feat[i] - mean[i]) / std[i]   // scaler.npz
// 3. Quantize:    q[i] = round(x_std[i] / 0.0404099 + (-17))   // int8
// 4. Invoke the interpreter on q[] -> int8 output o.
// 5. Dequantize:  p_abnormal = (o - (-128)) * 0.00390625
// 6. wellness_score = 1.0 - p_abnormal     // dashboard number, 0..1
```

## Tensors
- Input : int8[1, 11]  scale=0.0404099  zero_point=-17
- Output: int8[1, 1]                      scale=0.00390625  zero_point=-128

## Feature vector order (must match exactly)
   0. baseline_fhr
   1. overall_std
   2. stv
   3. ltv
   4. accel_per_min
   5. decel_per_min
   6. pct_brady
   7. pct_tachy
   8. contractions_per_min
   9. late_decel_per_min
  10. late_decel_ratio

## Quantization parity (float vs int8, per-record test set)
- Float ROC-AUC : 0.7298
- Int8  ROC-AUC : 0.729   (delta -0.0008)
- Mean |score delta|: 0.0019   Max: 0.00819
- Model size: 1456 bytes
