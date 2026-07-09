"""Generate golden test vectors for the on-device ML glue (Phase 4).

Picks representative CTG examples (Normal/Suspect/Pathologic), runs them through the
exact exported int8 model, and records: the 8 input features + the expected class +
probabilities. Malay's wombcare_ml.cc, given the same 8 features, MUST reproduce
these outputs. This unit-tests the C inference independently of the DSP.

Outputs: artifacts/ctg/firmware/golden_vectors.h (+ prints a readable table).
"""
from __future__ import annotations

import numpy as np
import tensorflow as tf

import config as C
from ctg_dataset import build

OUT = C.ML_DIR / "artifacts" / "ctg" / "firmware" / "golden_vectors.h"
CLASSES = ["Normal", "Suspect", "Pathologic"]


def main():
    d = build("A_all8")
    sc = np.load(C.ML_DIR / "artifacts" / "ctg" / "scaler.npz", allow_pickle=True)
    mean, std = sc["mean"], sc["std"]
    interp = tf.lite.Interpreter(
        model_path=str(C.ML_DIR / "artifacts" / "ctg" / "wombcare_nsp_int8.tflite"))
    interp.allocate_tensors()
    inp, outp = interp.get_input_details()[0], interp.get_output_details()[0]
    isc, izp = inp["quantization"]; osc, ozp = outp["quantization"]

    rng = np.random.default_rng(C.SEED)
    picks = []
    for cls in (0, 1, 2):                       # 2 examples per class
        idxs = np.where(d.y == cls)[0]
        picks += list(idxs[[0, len(idxs) // 2]])

    rows = []
    for i in picks:
        xs = (d.X[i] - mean) / std
        q = np.round(xs / isc + izp).astype(np.int8).reshape(inp["shape"])
        interp.set_tensor(inp["index"], q); interp.invoke()
        probs = (interp.get_tensor(outp["index"])[0].astype(np.float32) - ozp) * osc
        rows.append((d.X[i], int(np.argmax(probs)), probs, int(d.y[i])))

    # readable table
    print(f"{'features (LB,MSTV,MLTV,AC,DEC,FM,Mean,Var)':52s} pred  conf   true")
    for feats, k, probs, true in rows:
        fs = ",".join(f"{v:.1f}" for v in feats)
        print(f"  [{fs:48s}] {CLASSES[k][:4]:5s} {probs[k]*100:4.0f}%  {CLASSES[true][:4]}")

    # C header
    n = len(rows[0][0])
    lines = ["// Auto-generated golden vectors (Feature Spec v1). wombcare_ml.cc must match.",
             "#ifndef WOMBCARE_GOLDEN_H", "#define WOMBCARE_GOLDEN_H",
             f"#define GOLDEN_N {len(rows)}", f"#define GOLDEN_FEATS {n}",
             f"static const float golden_features[GOLDEN_N][GOLDEN_FEATS] = {{"]
    for feats, k, probs, true in rows:
        lines.append("  { " + ", ".join(f"{v:.4f}f" for v in feats) + " },")
    lines.append("};")
    lines.append("static const int golden_expected_class[GOLDEN_N] = { " +
                 ", ".join(str(k) for _, k, _, _ in rows) + " };")
    lines.append("static const float golden_expected_prob[GOLDEN_N][3] = {")
    for feats, k, probs, true in rows:
        lines.append("  { " + ", ".join(f"{p:.4f}f" for p in probs) + " },")
    lines.append("};")
    lines.append("#endif")
    OUT.write_text("\n".join(lines) + "\n")
    print(f"\nsaved -> {OUT}")


if __name__ == "__main__":
    main()
