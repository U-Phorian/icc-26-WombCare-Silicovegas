"""Phase 1 -- honest recording-level baseline on UCI CTG (NSP 3-class).

Compares models x feature-sets under **recording-level (FileName) repeated CV** so
the same fetus never appears in train and test (the leakage trap that produces the
fake "99%" numbers online). Reports overall accuracy, macro-F1, and -- the metric
that matters -- **per-class recall, especially Pathologic (missing a sick baby)**.
"""
from __future__ import annotations

import json
import warnings

import numpy as np
from sklearn.ensemble import GradientBoostingClassifier, RandomForestClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import balanced_accuracy_score, confusion_matrix, f1_score, recall_score
from sklearn.model_selection import StratifiedGroupKFold
from sklearn.neural_network import MLPClassifier
from sklearn.preprocessing import StandardScaler

import config as C
from ctg_dataset import build

warnings.filterwarnings("ignore")
N_SPLITS, N_REPEATS = 5, 6
CLASSES = ["Normal", "Suspect", "Pathologic"]


def _model(kind):
    if kind == "logistic":
        return LogisticRegression(max_iter=3000, class_weight="balanced", multi_class="multinomial")
    if kind == "mlp":
        return MLPClassifier(hidden_layer_sizes=(16, 8), alpha=1e-2, max_iter=800, random_state=C.SEED)
    if kind == "rf":
        return RandomForestClassifier(n_estimators=300, class_weight="balanced_subsample", random_state=C.SEED)
    if kind == "gboost":
        return GradientBoostingClassifier(random_state=C.SEED)
    raise ValueError(kind)


def evaluate(kind, feature_set):
    d = build(feature_set)
    accs, macrof1, path_recall, cms = [], [], [], []
    for r in range(N_REPEATS):
        sgkf = StratifiedGroupKFold(n_splits=N_SPLITS, shuffle=True, random_state=C.SEED + r)
        for tr, te in sgkf.split(d.X, d.y, groups=d.groups):
            sc = StandardScaler().fit(d.X[tr])
            m = _model(kind).fit(sc.transform(d.X[tr]), d.y[tr])
            pred = m.predict(sc.transform(d.X[te]))
            accs.append((pred == d.y[te]).mean())
            macrof1.append(f1_score(d.y[te], pred, average="macro"))
            path_recall.append(recall_score(d.y[te], pred, labels=[2], average="macro"))
            cms.append(confusion_matrix(d.y[te], pred, labels=[0, 1, 2]))
    cm = np.sum(cms, axis=0)
    per_class_recall = cm.diagonal() / cm.sum(axis=1)
    return {
        "accuracy": round(float(np.mean(accs)), 4),
        "macro_f1": round(float(np.mean(macrof1)), 4),
        "pathologic_recall": round(float(np.mean(path_recall)), 4),
        "per_class_recall": {CLASSES[i]: round(float(per_class_recall[i]), 3) for i in range(3)},
        "confusion_total": cm.tolist(),
    }


def main():
    results = {}
    print(f"Recording-level repeated CV ({N_SPLITS}-fold x {N_REPEATS}); label=NSP\n")
    print(f"{'model':10s} {'features':14s} {'acc':>6s} {'macroF1':>8s} {'PATH-recall':>12s}  per-class recall")
    for feature_set in ("A_all8", "B_high_parity6"):
        for kind in ("logistic", "mlp", "rf", "gboost"):
            r = evaluate(kind, feature_set)
            results[f"{kind}|{feature_set}"] = r
            pcr = r["per_class_recall"]
            print(f"{kind:10s} {feature_set:14s} {r['accuracy']:6.3f} {r['macro_f1']:8.3f} "
                  f"{r['pathologic_recall']:12.3f}  N={pcr['Normal']} S={pcr['Suspect']} P={pcr['Pathologic']}")
    json.dump(results, open(C.REPORTS_DIR / "ctg_baseline.json", "w"), indent=2)
    print("\nsaved -> reports/ctg_baseline.json")
    # show confusion of the best-by-pathologic-recall (deployable = logistic/mlp)
    best = max([k for k in results if k.startswith(("logistic", "mlp"))],
               key=lambda k: results[k]["pathologic_recall"])
    print(f"\nbest deployable by Pathologic recall: {best}")
    cm = np.array(results[best]["confusion_total"])
    print("confusion (rows=true N/S/P, cols=pred):")
    for i, row in enumerate(cm):
        print(f"  {CLASSES[i]:11s} {row.tolist()}")


if __name__ == "__main__":
    main()
