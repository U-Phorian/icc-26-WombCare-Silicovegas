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
"""Evaluate on out-of-fold (OOF) predictions -- no single split (FR-ML-D).

Every record is scored by a model that never saw it (train.py CV pass), so the
curves and operating points below use ALL 545 records with zero single-split
variance -- the fix for "a lucky 109-record split could read anywhere from 0.48
to 0.66". The cross-validated mean +/- 95% CI (reports/cv.json) is the headline.

Reports the metrics that matter on imbalanced data:
  * ROC-AUC / PR-AUC (never headline plain accuracy)
  * sensitivity / specificity at a balanced and a sensitivity-first point
  * confusion matrix, calibration (is the 0-1 score meaningful?)
Both per-window and **per-record** (a mother's windows averaged to one score).
"""
from __future__ import annotations

import json

import numpy as np

import config as C


def _agg_by_record(groups, y, p):
    """Collapse per-window predictions to one score per record (mean)."""
    recs = np.array(sorted(set(groups)))
    yr = np.array([y[groups == r][0] for r in recs])
    pr = np.array([p[groups == r].mean() for r in recs])
    return recs, yr, pr


def _op_point(y, p):
    """Operating point by Youden's J (maximises sensitivity + specificity - 1).

    Sensitivity (catching distress) is the priority for this use case, so among
    near-optimal thresholds we break ties toward the higher-sensitivity one.
    """
    from sklearn.metrics import confusion_matrix
    best = None
    for t in np.unique(p)[::-1]:
        pred = (p >= t).astype(int)
        tn, fp, fn, tp = confusion_matrix(y, pred, labels=[0, 1]).ravel()
        sens = tp / (tp + fn) if (tp + fn) else 0.0
        spec = tn / (tn + fp) if (tn + fp) else 0.0
        j = sens + spec - 1
        if best is None or j > best[0] or (j == best[0] and sens > best[2]):
            best = (j, float(t), sens, spec, (tn, fp, fn, tp))
    _, t, sens, spec, cm = best
    return t, sens, spec, cm


def _sens_first(y, p, target_sens=0.80):
    """Highest threshold that still catches >= target_sens of the distress cases.

    The PRD prioritises sensitivity (catching distress), so we also report the
    specificity achievable at a recall-first operating point.
    """
    from sklearn.metrics import confusion_matrix
    # Scan thresholds high -> low; the first to reach target recall is the
    # highest threshold achieving it (i.e. the best specificity at that recall).
    for t in np.unique(p)[::-1]:
        pred = (p >= t).astype(int)
        tn, fp, fn, tp = confusion_matrix(y, pred, labels=[0, 1]).ravel()
        sens = tp / (tp + fn) if (tp + fn) else 0.0
        spec = tn / (tn + fp) if (tn + fp) else 0.0
        if sens >= target_sens:
            return (float(t), sens, spec, (tn, fp, fn, tp))
    return None


def main() -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from sklearn.metrics import (roc_auc_score, average_precision_score,
                                 roc_curve, precision_recall_curve,
                                 brier_score_loss)

    oof = np.load(C.ARTIFACTS_DIR / "oof.npz", allow_pickle=True)
    p_win, yte, gte = oof["p"], oof["y"], oof["groups"]
    recs, yrec, prec = _agg_by_record(gte, yte, p_win)

    metrics = {"label": f"pH < {C.PH_ABNORMAL}", "eval": "out-of-fold (all records)",
               "n_records": int(len(recs)),
               "n_windows": int(len(yte)), "prevalence": float(yrec.mean())}
    for tag, (y, p) in {"per_window": (yte, p_win), "per_record": (yrec, prec)}.items():
        auc = roc_auc_score(y, p)
        prauc = average_precision_score(y, p)
        t, sens, spec, (tn, fp, fn, tp) = _op_point(y, p)
        entry = {
            "roc_auc": round(float(auc), 4),
            "pr_auc": round(float(prauc), 4),
            "brier": round(float(brier_score_loss(y, p)), 4),
            "balanced_operating_point": {
                "threshold": round(t, 4),
                "sensitivity": round(float(sens), 4),
                "specificity": round(float(spec), 4),
                "confusion": {"tn": int(tn), "fp": int(fp), "fn": int(fn), "tp": int(tp)},
            },
        }
        sf = _sens_first(y, p)
        if sf is not None:
            t2, s2, sp2, (tn2, fp2, fn2, tp2) = sf
            entry["sensitivity_first_point"] = {
                "threshold": round(t2, 4),
                "sensitivity": round(float(s2), 4),
                "specificity": round(float(sp2), 4),
                "confusion": {"tn": int(tn2), "fp": int(fp2), "fn": int(fn2), "tp": int(tp2)},
            }
        metrics[tag] = entry

    with open(C.REPORTS_DIR / "metrics.json", "w") as fh:
        json.dump(metrics, fh, indent=2)

    # --- figures ----------------------------------------------------------
    y, p = yrec, prec
    fpr, tpr, _ = roc_curve(y, p)
    pr_p, pr_r, _ = precision_recall_curve(y, p)
    fig, ax = plt.subplots(1, 3, figsize=(15, 4.2))
    ax[0].plot(fpr, tpr, lw=2, label=f"AUC={metrics['per_record']['roc_auc']:.3f}")
    ax[0].plot([0, 1], [0, 1], "k--", alpha=.4); ax[0].set_title("ROC (per record)")
    ax[0].set_xlabel("1 - specificity"); ax[0].set_ylabel("sensitivity"); ax[0].legend()
    ax[1].plot(pr_r, pr_p, lw=2, label=f"PR-AUC={metrics['per_record']['pr_auc']:.3f}")
    ax[1].axhline(y.mean(), ls="--", color="k", alpha=.4, label=f"baseline={y.mean():.2f}")
    ax[1].set_title("Precision-Recall"); ax[1].set_xlabel("recall")
    ax[1].set_ylabel("precision"); ax[1].legend()
    # calibration
    bins = np.linspace(0, 1, 6)
    idx = np.digitize(p, bins) - 1
    xs, ys = [], []
    for b in range(len(bins) - 1):
        sel = idx == b
        if sel.sum() > 0:
            xs.append(p[sel].mean()); ys.append(y[sel].mean())
    ax[2].plot([0, 1], [0, 1], "k--", alpha=.4)
    ax[2].plot(xs, ys, "o-", lw=2)
    ax[2].set_title(f"Calibration (Brier={metrics['per_record']['brier']:.3f})")
    ax[2].set_xlabel("predicted P(abnormal)"); ax[2].set_ylabel("observed")
    fig.tight_layout(); fig.savefig(C.REPORTS_DIR / "eval_curves.png", dpi=120)

    print(json.dumps(metrics, indent=2))
    print("\nsaved -> reports/metrics.json, reports/eval_curves.png")


if __name__ == "__main__":
    main()
