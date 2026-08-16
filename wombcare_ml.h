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

// This is the type app.c expects back from wombcare_ml_run().
// IMPORTANT: app.h must NOT also define a `WombCareResult_t` (an older app.h did,
// holding {features, imu_confidence, ml_prediction}) -- that collides with this.
// The current app.c uses THIS struct (.nsp / .confidence), so keep only this one.
typedef struct {
    WombCareNSP_t nsp;      // winning class (0=Normal,1=Suspect,2=Pathologic)
    float confidence;      // probability of the winning class, 0..1
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

/*--------------------------------------------------------------------
 * Golden-vector self-test
 *
 * Runs the 6 known feature vectors in golden_vectors.h through the full
 * scaler -> quantize -> inference -> dequantize path and logs each result.
 * Returns the number that matched (6 == the C path reproduces the trained
 * model exactly).
 *
 * This isolates blame: if the golden test PASSES but live output still
 * looks wrong, the fault is in the DSP feature extraction, not the ML
 * glue. Off by default -- it costs flash and a few ms at boot.
 *-------------------------------------------------------------------*/
#ifndef WOMBCARE_ML_SELFTEST
#define WOMBCARE_ML_SELFTEST  0
#endif

// Tolerance per Phase 4 integration note.
#define WOMBCARE_GOLDEN_PROB_TOL  0.02f

int wombcare_ml_selftest(void);

#ifdef __cplusplus
}
#endif
#endif // WOMBCARE_ML_H
