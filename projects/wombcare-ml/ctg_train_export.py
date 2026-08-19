# *Licensed to the Apache Software Foundation (ASF) under one
# *or more contributor license agreements.  See the NOTICE file
# *distributed with this work for additional information
# *regarding copyright ownership.  The ASF licenses this file
# *to you under the Apache License, Version 2.0 (the
# *"License"); you may not use this file except in compliance
# *with the License.  You may obtain a copy of the License at
#
# *  http://www.apache.org/licenses/LICENSE-2.0
#
# *Unless required by applicable law or agreed to in writing,
# *software distributed under the License is distributed on an
# *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# *KIND, either express or implied.  See the License for the
# *specific language governing permissions and limitations
# *under the License.
"""Phase 2 -- train the deployed NSP model and export it for the MG26.

Model: class-weighted multinomial logistic regression on the 8 spec features
(chosen in Phase 1 for the best Pathologic recall + tiny/interpretable/deployable).
Fit with scikit-learn, transplanted into a Keras Dense(3, softmax) for int8 TFLite
export -- same trick as the CTU-UHB model, now 3-class.

Outputs (ml/artifacts/ctg/):
  scaler.npz, wombcare_nsp_int8.tflite, firmware/{model_data.c,.h,scaler.c,
  feature_order.txt, INTEGRATION.md}; reports/ctg_parity.json
"""
from __future__ import annotations

import json
import warnings

import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler

import config as C
from ctg_dataset import build, load_spec

warnings.filterwarnings("ignore")
CLASSES = ["Normal", "Suspect", "Pathologic"]
OUT = C.ML_DIR / "artifacts" / "ctg"
FW = OUT / "firmware"
FW.mkdir(parents=True, exist_ok=True)


def keras_softmax_from_sklearn(clf, n_features):
    from tensorflow import keras
    from tensorflow.keras import layers
    model = keras.Sequential([
        layers.Input(shape=(n_features,)),
        layers.Dense(3, activation="softmax"),
    ], name="wombcare_nsp")
    # sklearn coef_ is (3, n_features); Keras kernel is (n_features, 3)
    model.layers[0].set_weights([clf.coef_.T.astype("float32"),
                                 clf.intercept_.astype("float32")])
    return model


# Template rule 8: every source file carries the ASF Apache 2.0 header. These
# files are generated, so the generator has to emit it -- otherwise a re-export
# silently strips the header from the tracked artifacts.
ASF_C_HEADER = """/***************************************************************************//**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 ******************************************************************************/
"""


def to_c_array(tfl, var="g_wombcare_nsp_model"):
    hexs = ", ".join(f"0x{b:02x}" for b in tfl)
    h = (ASF_C_HEADER +
         f"#ifndef WOMBCARE_MODEL_DATA_H\n#define WOMBCARE_MODEL_DATA_H\n#include <stdint.h>\n"
         f"extern const unsigned char {var}[];\nextern const unsigned int {var}_len;\n"
         f"#endif\n")
    c = (ASF_C_HEADER +
         f'#include "model_data.h"\nconst unsigned char {var}[] = {{ {hexs} }};\n'
         f"const unsigned int {var}_len = {len(tfl)};\n")
    return h, c


def main():
    import tensorflow as tf

    d = build("A_all8")
    mean = d.X.mean(axis=0); std = d.X.std(axis=0) + 1e-8
    Xs = (d.X - mean) / std
    clf = LogisticRegression(max_iter=3000, class_weight="balanced",
                             multi_class="multinomial").fit(Xs, d.y)

    model = keras_softmax_from_sklearn(clf, d.X.shape[1])
    np.savez(OUT / "scaler.npz", mean=mean, std=std,
             feature_names=np.array(d.feature_names), classes=np.array(CLASSES))

    # int8 quantization
    def rep():
        for row in Xs.astype(np.float32):
            yield [row.reshape(1, -1)]
    conv = tf.lite.TFLiteConverter.from_keras_model(model)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = rep
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    tfl = conv.convert()
    (OUT / "wombcare_nsp_int8.tflite").write_bytes(tfl)

    # parity: float vs int8 argmax agreement on all data
    interp = tf.lite.Interpreter(model_content=tfl); interp.allocate_tensors()
    inp, outp = interp.get_input_details()[0], interp.get_output_details()[0]
    isc, izp = inp["quantization"]; osc, ozp = outp["quantization"]
    q_pred = []
    for row in Xs.astype(np.float32):
        q = np.round(row / isc + izp).astype(np.int8).reshape(inp["shape"])
        interp.set_tensor(inp["index"], q); interp.invoke()
        o = (interp.get_tensor(outp["index"]).astype(np.float32) - ozp) * osc
        q_pred.append(int(np.argmax(o)))
    q_pred = np.array(q_pred)
    f_pred = np.argmax(model.predict(Xs, verbose=0), axis=1)
    parity = {
        "float_vs_int8_argmax_agreement": round(float((q_pred == f_pred).mean()), 4),
        "float_train_acc": round(float((f_pred == d.y).mean()), 4),
        "int8_train_acc": round(float((q_pred == d.y).mean()), 4),
        "tflite_bytes": len(tfl),
    }
    json.dump(parity, open(C.REPORTS_DIR / "ctg_parity.json", "w"), indent=2)

    # firmware artifacts
    h, c = to_c_array(tfl)
    (FW / "model_data.h").write_text(h)
    (FW / "model_data.c").write_text(c)
    (FW / "feature_order.txt").write_text("\n".join(d.feature_names) + "\n")
    _write_scaler_c(FW, mean, std, d.feature_names, isc, izp, osc, ozp)

    print(json.dumps(parity, indent=2))
    print(f"\nint8 NSP model: {len(tfl)} bytes ({len(tfl)/1024:.1f} KB)")
    print("class order:", CLASSES)
    print("saved -> artifacts/ctg/{scaler.npz, wombcare_nsp_int8.tflite, firmware/*}")


def _write_scaler_c(fw, mean, std, names, isc, izp, osc, ozp):
    lines = [ASF_C_HEADER.rstrip("\n"),
             "// Auto-generated scaler + quant params (Feature Spec v1). Apply before inference.",
             "#include \"scaler.h\"",
             f"const float g_feat_mean[{len(mean)}] = {{ {', '.join(f'{m:.6f}f' for m in mean)} }};",
             f"const float g_feat_std[{len(std)}]  = {{ {', '.join(f'{s:.6f}f' for s in std)} }};",
             f"const float g_in_scale = {isc:.8f}f;  const int g_in_zp = {int(izp)};",
             f"const float g_out_scale = {osc:.8f}f; const int g_out_zp = {int(ozp)};"]
    (fw / "scaler.c").write_text("\n".join(lines) + "\n")
    (fw / "scaler.h").write_text(
        ASF_C_HEADER +
        "#ifndef WOMBCARE_SCALER_H\n#define WOMBCARE_SCALER_H\n"
        f"#define WOMBCARE_N_FEATURES {len(mean)}\n"
        "extern const float g_feat_mean[WOMBCARE_N_FEATURES];\n"
        "extern const float g_feat_std[WOMBCARE_N_FEATURES];\n"
        "extern const float g_in_scale; extern const int g_in_zp;\n"
        "extern const float g_out_scale; extern const int g_out_zp;\n#endif\n")


if __name__ == "__main__":
    main()
