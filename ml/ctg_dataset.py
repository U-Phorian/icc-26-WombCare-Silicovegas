"""UCI CTG dataset loader -- builds features per feature_spec.json (Phase 1).

Reads CTG.xls from cardiotocography.zip, constructs the 8 features EXACTLY as the
Feature Definition Spec prescribes (per-minute normalized counts, bpm units), and
exposes recording-level groups (FileName) so evaluation can split without leakage.
"""
from __future__ import annotations

import io
import json
import zipfile
from dataclasses import dataclass

import numpy as np
import pandas as pd

import config as C

SPEC_PATH = C.ML_DIR / "feature_spec.json"
ZIP_PATH = C.REPO_DIR / "cardiotocography.zip"


def load_spec() -> dict:
    return json.load(open(SPEC_PATH))


@dataclass
class CTGData:
    X: np.ndarray            # (n_segments, n_features) float32
    y: np.ndarray            # (n_segments,) int in {0,1,2}  (NSP-1)
    groups: np.ndarray       # (n_segments,) recording id (FileName)
    feature_names: list


def _raw_table() -> pd.DataFrame:
    z = zipfile.ZipFile(ZIP_PATH)
    xls = [n for n in z.namelist() if n.lower().endswith(".xls")][0]
    df = pd.read_excel(io.BytesIO(z.read(xls)), sheet_name="Raw Data")
    return df[df["NSP"].isin([1, 2, 3])].copy()


def build(feature_set: str = "A_all8") -> CTGData:
    spec = load_spec()
    df = _raw_table()
    fs = spec["duration"]["fs_hz"]
    dur_min = (df["e"] - df["b"]) / fs / 60.0

    # construct every spec feature, then select the requested set
    col = {
        "LB": df["LB"], "MSTV": df["MSTV"], "MLTV": df["MLTV"],
        "AC_rate": df["AC"] / dur_min,
        "DEC_rate": (df["DL"] + df["DS"]) / dur_min,
        "FM_rate": df["FM"] / dur_min,
        "MeanHR": df["Mean"], "Variance": df["Variance"],
    }
    names = spec["feature_sets"][feature_set]
    X = np.column_stack([pd.to_numeric(col[n], errors="coerce").to_numpy() for n in names])
    X = np.nan_to_num(X, nan=0.0).astype(np.float32)
    y = (df["NSP"].astype(int) - 1).to_numpy()          # 0=Normal,1=Suspect,2=Pathologic
    groups = df["FileName"].astype(str).to_numpy()
    return CTGData(X=X, y=y, groups=groups, feature_names=names)


if __name__ == "__main__":
    for fs_name in ("A_all8", "B_high_parity6"):
        d = build(fs_name)
        cls, cnt = np.unique(d.y, return_counts=True)
        print(f"[{fs_name}] X={d.X.shape}  recordings={len(set(d.groups))}  "
              f"class counts={dict(zip(cls.tolist(), cnt.tolist()))}")
        print("  features:", d.feature_names)
