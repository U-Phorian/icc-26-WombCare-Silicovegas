#ifndef WOMBCARE_ML_H
#define WOMBCARE_ML_H

#include "wombcare_dsp.h"   // WombCareFeatures_t

#ifdef __cplusplus
extern "C" {
#endif

// NSP classes (order fixed = model output order = Feature Spec)
typedef enum {
    WOMBCARE_NORMAL     = 0,   // HEALTHY
    WOMBCARE_SUSPECT    = 1,   // WATCH
    WOMBCARE_PATHOLOGIC = 2    // ALERT
} WombCareNSP_t;

typedef struct {
    WombCareNSP_t nsp;      // winning class
    float confidence;      // probability of the winning class (0..1)
    float probs[3];        // {Normal, Suspect, Pathologic}, sums to ~1
    bool  ok;              // false if inference failed
} WombCareResult_t;

// Call once at startup (sets up TFLite-Micro interpreter on the MVP).
void wombcare_ml_init(void);

// Run inference on one feature vector. Applies the trained scaler + int8 quant
// internally (see scaler.c). Feature values MUST follow FEATURE_SPEC.md
// (bpm units, per-minute rates), regardless of the legacy struct field names.
WombCareResult_t wombcare_ml_run(const WombCareFeatures_t *features);

// Convenience: "HEALTHY" / "WATCH" / "ALERT" string for BLE/UI.
const char *wombcare_ml_status_str(WombCareNSP_t nsp);

#ifdef __cplusplus
}
#endif
#endif // WOMBCARE_ML_H
