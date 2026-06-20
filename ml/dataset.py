"""Dataset assembly (FR-ML-1, FR-ML-2, FR-ML-5) -- v2 (FHR + UC).

Reads the **raw** WFDB records so FHR and UC stay time-aligned (required for the
late-deceleration features), cleans FHR in place (keeps the time axis), windows
both channels together, extracts v2 features, joins clinical labels, and builds a
**patient-level** train/val/test split so windows from one mother never leak
across folds (the mandated anti-leakage rule).

The unit of feature extraction is a window; the unit of *splitting* is a record.
"""
from __future__ import annotations

import warnings
from dataclasses import dataclass

import numpy as np
import pandas as pd
from sklearn.model_selection import train_test_split

import config as C
from features_v2 import (extract_features_v2, clean_fhr_inplace,
                         N_FEATURES_V2 as N_FEATURES)

warnings.filterwarnings("ignore")


@dataclass
class Dataset:
    X: np.ndarray            # (n_windows, N_FEATURES) float32
    y: np.ndarray            # (n_windows,) int  -- abnormal = pH < threshold
    y_apgar: np.ndarray      # (n_windows,) int  -- cross-check Apgar5 <= 7
    groups: np.ndarray       # (n_windows,) record id (str) for patient-level split
    record_ph: np.ndarray    # (n_windows,) float pH (for severe-tier / analysis)


def _record_ids() -> list[str]:
    recs = np.loadtxt(C.CODE_DIR / "dat" / "RECORDS")
    return [str(int(x)) for x in np.atleast_1d(recs)]


def _read_raw(rec: str):
    """Raw record -> (clean_fhr, uc) aligned, or (None, None) if too little signal."""
    import wfdb
    sig, _ = wfdb.rdsamp(str(C.CODE_DIR / "dat" / rec))
    fhr, valid = clean_fhr_inplace(sig[:, 0])
    if fhr is None:
        return None, None
    return fhr, sig[:, 1].astype(np.float64)


def _window_pairs(fhr: np.ndarray, uc: np.ndarray, window_min: int,
                  mode: str | None = None):
    """Slice an (FHR, UC) pair into aligned windows per WINDOW_MODE."""
    mode = mode or C.WINDOW_MODE
    w = C.window_samples(window_min)
    floor = C.window_samples(C.MIN_WINDOW_MIN)
    if fhr.size < floor:
        return []
    if fhr.size < w:
        return [(fhr, uc)]
    if mode == "last":
        return [(fhr[-w:], uc[-w:])]
    if mode == "tail":
        span = C.window_samples(C.TAIL_MIN)
        if fhr.size > span:
            fhr, uc = fhr[-span:], uc[-span:]
    step = max(1, int(w * (1 - C.WINDOW_OVERLAP)))
    return [(fhr[i:i + w], uc[i:i + w]) for i in range(0, fhr.size - w + 1, step)]


def build(window_min: int | None = None, verbose: bool = True) -> Dataset:
    window_min = window_min or C.WINDOW_MIN
    out = pd.read_csv(C.OUTCOMES_CSV)
    out["rec"] = out["filename"].str.replace(".png", "", regex=False)
    for col in ("pH", "Apgar5"):
        out[col] = pd.to_numeric(out[col], errors="coerce")
    labels = out.set_index("rec")

    X, y, ya, groups, phs = [], [], [], [], []
    n_records = n_dropped = n_skipped_label = 0

    for rec in _record_ids():
        if rec not in labels.index or pd.isna(labels.loc[rec, "pH"]):
            n_skipped_label += 1
            continue
        fhr, uc = _read_raw(rec)
        if fhr is None:
            n_dropped += 1
            continue
        wins = _window_pairs(fhr, uc, window_min)
        if not wins:
            n_dropped += 1
            continue

        ph = float(labels.loc[rec, "pH"])
        apgar5 = labels.loc[rec, "Apgar5"]
        abn = int(ph < C.PH_ABNORMAL)
        abn_apgar = int(apgar5 <= C.APGAR5_LOW) if pd.notna(apgar5) else -1
        for win_fhr, win_uc in wins:
            X.append(extract_features_v2(win_fhr, win_uc))
            y.append(abn)
            ya.append(abn_apgar)
            groups.append(rec)
            phs.append(ph)
        n_records += 1

    ds = Dataset(
        X=np.asarray(X, dtype=np.float32),
        y=np.asarray(y, dtype=np.int64),
        y_apgar=np.asarray(ya, dtype=np.int64),
        groups=np.asarray(groups),
        record_ph=np.asarray(phs, dtype=np.float64),
    )
    if verbose:
        pos = ds.y.sum()
        print(f"records used      : {n_records}  (dropped {n_dropped} empty/short, "
              f"{n_skipped_label} unlabeled)")
        print(f"windows           : {len(ds.y)}  ({window_min}-min, "
              f"{int(C.WINDOW_OVERLAP*100)}% overlap)")
        print(f"feature dim       : {ds.X.shape[1]} (expected {N_FEATURES})")
        print(f"abnormal windows  : {pos} ({100*pos/len(ds.y):.1f}%)  "
              f"[pH < {C.PH_ABNORMAL}]")
    return ds


def split_by_record(ds: Dataset, seed: int | None = None):
    """Patient-level train/val/test split, stratified by each record's label.

    Returns dict of (X, y) per fold plus the index arrays. Stratification is on
    the *record* label so class balance is preserved without window leakage.
    """
    seed = C.SEED if seed is None else seed
    rec_ids = np.array(sorted(set(ds.groups)))
    # one label per record (all windows of a record share it)
    first_idx = {r: np.where(ds.groups == r)[0][0] for r in rec_ids}
    rec_y = np.array([ds.y[first_idx[r]] for r in rec_ids])

    train_recs, test_recs = train_test_split(
        rec_ids, test_size=C.TEST_SIZE, random_state=seed, stratify=rec_y)
    tr_y = np.array([ds.y[first_idx[r]] for r in train_recs])
    train_recs, val_recs = train_test_split(
        train_recs, test_size=C.VAL_SIZE, random_state=seed, stratify=tr_y)

    def mask(recs):
        return np.isin(ds.groups, recs)

    folds = {}
    for name, recs in (("train", train_recs), ("val", val_recs), ("test", test_recs)):
        m = mask(recs)
        folds[name] = {
            "X": ds.X[m], "y": ds.y[m], "groups": ds.groups[m],
            "ph": ds.record_ph[m], "y_apgar": ds.y_apgar[m], "recs": recs,
        }
    return folds


if __name__ == "__main__":
    ds = build()
    folds = split_by_record(ds)
    for name, f in folds.items():
        pos = f["y"].sum()
        print(f"  {name:5s}: {len(f['recs']):3d} records  {len(f['y']):4d} windows  "
              f"{pos:3d} abnormal ({100*pos/len(f['y']):.1f}%)")
    # leakage assertion: no record appears in more than one fold
    s = {n: set(f["recs"]) for n, f in folds.items()}
    assert not (s["train"] & s["test"]) and not (s["train"] & s["val"]) \
        and not (s["val"] & s["test"]), "PATIENT LEAKAGE!"
    print("OK no patient leakage across folds")
