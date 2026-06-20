# WombCare Fetal-Wellness Model -- Model Card (v2: FHR + UC)

> **Not a diagnosis.** WombCare is a home wellness-awareness and early-warning
> aid. It flags *patterns associated with* reduced fetal wellbeing so a user can
> seek professional care sooner. It does not diagnose and is not a clinical
> decision device.

## Intended use
- **Input:** aligned fetal heart-rate (FHR) + uterine-contraction (UC) signal,
  4 Hz, windowed to 20 min.
- **Output:** a calibrated **Wellness Score** in [0,1] (= P(well) = 1 - P(abnormal))
  and a tier (Good / Watch / Alert) for the dashboard and alert logic.

## Data
- **Source:** CTU-UHB Intrapartum Cardiotocography Database (PhysioNet `ctu-uhb-ctgdb`).
- **Records:** 545 usable (raw WFDB, FHR + UC channels, time-aligned).
- **FHR cleaning:** in-place interpolation of spikes (<50 / >200 bpm) and gaps,
  preserving the time axis so decelerations can be related to contractions.
- **Windowing:** overlapping 20-min windows from the last 40 min before delivery
  (`WINDOW_MODE="tail"`) -- acidemia develops near birth. 1635 windows.

## Label
- **Primary:** `abnormal = umbilical-artery pH < 7.15` -> 18.7% positive.
- **Severe head:** pH < 7.05 (drives the Alert tier).
- **Cross-check:** Apgar5 <= 7.

## Features (11, PRD v2 Section 3)
`baseline_fhr, overall_std, stv, ltv, accel_per_min, decel_per_min, pct_brady,
pct_tachy, contractions_per_min, late_decel_per_min, late_decel_ratio`.

The v2 additions are **UC-derived**: contraction detection (UC peak-finding) and
**late decelerations** (decel nadir 10-45 s after a contraction peak -- the
strongest clinical distress sign). UC features are the single biggest accuracy
lever (FHR-only CV AUC 0.56 -> FHR+UC 0.67; +tail-windowing -> ~0.70). All
features are cheap enough to recompute on the MG26.

## Model
- **Architecture:** logistic regression on the 11 standardised features,
  class-weighted. Fit with scikit-learn (reliable convex solver), then its
  weights are transplanted into a Keras `Dense(1, sigmoid)` for TFLite export.
- **Model choice is proven, not asserted.** Logistic, a small MLP and gradient
  boosting were compared under one identical repeated patient-level CV (5-fold x
  10 repeats, 50 estimates each):

  | Model | Per-record ROC-AUC (95% CI) |
  |---|---|
  | logistic | **0.691 +/- 0.015** |
  | small MLP | 0.684 +/- 0.016 |
  | gradient boosting | 0.696 +/- 0.017 |

  The three **tie within their 95% CIs** -- no model meaningfully beats the
  others. We pick logistic for simplicity, interpretability, and a 1.4 KB
  deployment. (A deep Keras MLP *can* overfit this small data if trained without
  care -- another reason to keep the model linear.)
- **Split:** patient-level (by record), stratified. **No window leaks across folds.**

## Metrics -- no single "lucky split"
Headline = repeated patient-level CV (above). For curves/operating points we use
**out-of-fold (OOF) predictions**: every one of the 545 records is scored by a
model that never saw it, so these use ALL records with zero split variance.

| Metric (OOF, per record) | Value |
|---|---|
| ROC-AUC | **0.694** |
| PR-AUC | **0.368** (baseline = prevalence 0.19) |
| Brier (calibration) | 0.219 |
| Balanced operating point | sensitivity 0.62, specificity 0.72 |
| Sensitivity-first point | **sensitivity 0.80, specificity 0.43** (catches 82/102) |

Most-weighted features: `pct_brady (+0.48)`, `baseline_fhr`, `accel_per_min`,
**`stv (-0.21)`** (less variability -> more abnormal, clinically correct),
`pct_tachy` (see `reports/feature_weights.json`).

## Quantization parity (FR-ML-G)
Float vs int8 on the same data (a **fidelity** check, not a performance claim):
ROC-AUC delta **-0.0008**, max |score delta| 0.015 -> **lossless in practice.**
Model size **1.4 KB**. The deployed model is trained on all 545 records.

## Limitations & honest caveats
- **Hard task.** FHR+UC prediction of cord-blood pH is inherently modest;
  AUC ~0.70 is consistent with the CTU-UHB literature. **Anyone reporting 0.90+
  on this data is almost certainly leaking** (segment-level splits). We report
  sensitivity and PR-AUC, never headline accuracy.
- **Single dataset, retrospective intrapartum labels.** Generalisation to home
  antepartum monitoring is an assumption, not yet validated.
- **STV is a beat-to-beat proxy**, not the Dawes-Redman epoch definition (future
  work). UC must be available on-device for the v2 features; if only FHR is
  available, the model degrades toward the ~0.62 FHR-only baseline.
- **Awareness, not diagnosis.** A low score means "seek professional care," never
  a clinical instruction.

## Reproducibility
Seeded (`SEED=1337`). `python run_all.py` regenerates `reports/{cv,metrics,parity,
feature_weights}.json` and `eval_curves.png`.
