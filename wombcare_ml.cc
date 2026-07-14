// WombCare TinyML inference glue (Phase 4).
// Feeds the 8 spec features -> standardize -> int8 quantize -> TFLite-Micro
// (MVP-accelerated) -> dequantize -> NSP class + confidence.
//
// Depends on generated artifacts (from ml/ctg_train_export.py):
//   model_data.c/.h  : const uint8 g_wombcare_nsp_model[]
//   scaler.c/.h      : g_feat_mean[], g_feat_std[], g_in_scale/zp, g_out_scale/zp
// Governed by FEATURE_SPEC.md. Verify against golden_vectors.h.

#include <math.h>

#include "wombcare_ml.h"
#include "model_data.h"
#include "scaler.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {
const tflite::Model *g_model = nullptr;
tflite::MicroInterpreter *g_interpreter = nullptr;
TfLiteTensor *g_input = nullptr;
TfLiteTensor *g_output = nullptr;

// Logistic softmax model is tiny; 4 KB arena is ample.
constexpr int kArenaSize = 4 * 1024;
alignas(16) uint8_t g_arena[kArenaSize];
}  // namespace

extern "C" void wombcare_ml_init(void) {
    g_model = tflite::GetModel(g_wombcare_nsp_model);
    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        return;  // schema mismatch
    }
    // Only two ops in this model: FullyConnected + Softmax.
    static tflite::MicroMutableOpResolver<2> resolver;
    resolver.AddFullyConnected();
    resolver.AddSoftmax();

    static tflite::MicroInterpreter interpreter(g_model, resolver, g_arena, kArenaSize);
    g_interpreter = &interpreter;
    if (g_interpreter->AllocateTensors() != kTfLiteOk) {
        g_interpreter = nullptr;
        return;
    }
    g_input = g_interpreter->input(0);
    g_output = g_interpreter->output(0);
}

extern "C" WombCareResult_t wombcare_ml_run(const WombCareFeatures_t *f) {
    WombCareResult_t res = {WOMBCARE_NORMAL, 0.0f, {0, 0, 0}, false};
    if (!g_interpreter || !g_input || !g_output) return res;

    // Feature vector in the FIXED spec order (== feature_order.txt).
    const float x[WOMBCARE_N_FEATURES] = {
        f->lb_bpm,          // LB
        f->mstv_ms,         // MSTV  (value must be bpm per spec)
        f->mltv_ms,         // MLTV  (value must be bpm per spec)
        f->accel_count,     // AC_rate  (per-minute per spec)
        f->decel_count,     // DEC_rate (per-minute per spec)
        f->fetal_movements, // FM_rate  (per-minute per spec)
        f->mean_hr_bpm,     // MeanHR
        f->hr_variance,     // Variance (bpm^2 per spec)
    };

    // Standardize with the trained scaler, then int8-quantize into the input tensor.
    for (int i = 0; i < WOMBCARE_N_FEATURES; i++) {
        float xs = (x[i] - g_feat_mean[i]) / g_feat_std[i];
        int32_t q = (int32_t)lroundf(xs / g_in_scale) + g_in_zp;
        if (q < -128) q = -128;
        if (q > 127) q = 127;
        g_input->data.int8[i] = (int8_t)q;
    }

    if (g_interpreter->Invoke() != kTfLiteOk) return res;

    // Dequantize the 3 outputs -> probabilities; argmax.
    int best = 0;
    float best_p = -1.0f;
    for (int k = 0; k < 3; k++) {
        float p = (g_output->data.int8[k] - g_out_zp) * g_out_scale;
        res.probs[k] = p;
        if (p > best_p) { best_p = p; best = k; }
    }
    res.nsp = (WombCareNSP_t)best;
    res.confidence = best_p;
    res.ok = true;
    return res;
}

extern "C" const char *wombcare_ml_status_str(WombCareNSP_t nsp) {
    switch (nsp) {
        case WOMBCARE_NORMAL:     return "HEALTHY";
        case WOMBCARE_SUSPECT:    return "WATCH";
        case WOMBCARE_PATHOLOGIC: return "ALERT";
        default:                  return "UNKNOWN";
    }
}
