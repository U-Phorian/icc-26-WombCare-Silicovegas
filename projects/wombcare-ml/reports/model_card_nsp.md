# WombCare NSP Model Card (UCI CTG, on-device)

> **Not a diagnosis.** This model performs **automated CTG interpretation** — it
> reproduces an expert's FIGO-style reading (Normal / Suspect / Pathologic) from
> heart-rate features. It is a wellness-awareness aid, not a diagnostic device and
> not a prediction of the baby's actual outcome.

## Task
- **Input:** 8 features computed on-device from the fetal FHR + PVDF (per
  [FEATURE_SPEC.md](../../../resources/docs/FEATURE_SPEC.md)): `LB, MSTV, MLTV, AC_rate, DEC_rate,
  FM_rate, MeanHR, Variance`.
- **Output:** 3-class **NSP** — Normal / Suspect / Pathologic — + confidence.

## Data
- **UCI Cardiotocography** (`cardiotocography.zip`, "Raw Data").
- **2,126 segments from 352 recordings.** Class balance: Normal 77.8%, Suspect
  13.9%, Pathologic 8.3%.
- Count features normalized to **per-minute**; variability in **bpm** (Feature Spec).

## Model
- **Class-weighted multinomial logistic regression** on 8 standardized features,
  transplanted into a Keras `Dense(3, softmax)` for int8 TFLite export.
- **Why logistic (not the higher-accuracy trees):** it gives the **best Pathologic
  recall** and is interpretable + deployable. For a screening aid, catching the
  sick baby outweighs raw accuracy.

## Performance — honest, recording-level repeated CV (5-fold × 6)
> **Split by `FileName`** so the same fetus never appears in train + test. This is
> why the numbers are ~78%, not the ~99% seen online (which is segment-level
> leakage).

| Metric | Value |
|---|---|
| Accuracy | **0.784** |
| Macro-F1 | 0.662 |
| **Pathologic recall** | **0.69** |
| Per-class recall | Normal 0.79 · Suspect 0.80 · Pathologic 0.74 |

**Confusion (summed over folds), rows = true:**
| | →Normal | →Suspect | →Pathologic |
|---|---|---|---|
| **Normal** | 7805 | 1486 | 639 |
| **Suspect** | 126 | 1421 | 223 |
| **Pathologic** | 83 | 191 | 782 |

**Key safety number:** of all Pathologic cases, only **8% are missed as Normal** —
**~92% receive at least a "Suspect" alert.** The model errs toward over-flagging,
the safe direction for a baby monitor.

## Quantization (int8, deployed on MG26 MVP)
| | Value |
|---|---|
| Model size | **1.5 KB** |
| Float vs int8 agreement | **99.0%** |
| Train acc float / int8 | 0.794 / 0.797 |

Effectively lossless. Artifacts: `ml/artifacts/ctg/firmware/`.

## Limitations & honesty
- **Interpretation, not outcome.** NSP was assigned by clinicians from the same
  trace; this automates that reading — it does not predict pH/Apgar outcomes.
- **Feature parity is the deployment risk.** On-device features must match the
  training features exactly (Feature Spec); `MLTV`/`FM` are the least-faithful and
  are validated in Phase 4 (fallback: 6-feature set, small recall cost).
- **~78% accuracy is honest** under a leakage-free split; treat "99%" claims with
  suspicion.

## Reproduce
`python ctg_baseline.py` (honest CV) → `python ctg_train_export.py` (deploy + int8).
Seeded. Numbers regenerate into `reports/ctg_baseline.json` + `reports/ctg_parity.json`.
