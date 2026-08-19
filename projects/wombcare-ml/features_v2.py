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
"""FHR + UC feature extraction (PRD v2, Section 3).

v2 upgrades v1 from FHR-only to **FHR + uterine-contraction (UC)** features and
adds **late-deceleration** detection -- a deceleration whose nadir falls 10-45 s
*after* a contraction peak, the strongest clinical sign of fetal distress.

Unlike v1 (which read the segment-concatenated `processed_dat/`), v2 reads the
**raw** WFDB record so FHR and UC stay time-aligned, and cleans FHR by in-place
interpolation (keeps the time axis intact -- required to relate decels to
contractions).

v2 feature vector (11):
  baseline_fhr, overall_std, stv, ltv, accel_per_min, decel_per_min,
  pct_brady, pct_tachy, contractions_per_min, late_decel_per_min, late_decel_ratio
"""
from __future__ import annotations

import numpy as np
from scipy.signal import find_peaks

from config import FS

FEATURE_NAMES_V2: list[str] = [
    "baseline_fhr",
    "overall_std",
    "stv",                  # short-term variability (beat-to-beat proxy)
    "ltv",                  # long-term variability (per-minute range)
    "accel_per_min",
    "decel_per_min",
    "pct_brady",            # fraction < 110 bpm
    "pct_tachy",            # fraction > 160 bpm
    "contractions_per_min",
    "late_decel_per_min",   # late decels per minute
    "late_decel_ratio",     # late decels / contractions
]
N_FEATURES_V2 = len(FEATURE_NAMES_V2)

_HR_MIN, _HR_MAX = 50.0, 200.0


def clean_fhr_inplace(fhr: np.ndarray) -> tuple[np.ndarray | None, np.ndarray]:
    """Interpolate gaps/spikes while preserving the time axis.

    Returns (clean_fhr, valid_mask) or (None, mask) when too little signal.
    """
    fhr = np.asarray(fhr, dtype=np.float64).copy()
    valid = (fhr >= _HR_MIN) & (fhr <= _HR_MAX)
    if valid.sum() < max(8 * 60 * FS, int(0.2 * fhr.size)):
        return None, valid
    x = np.arange(fhr.size)
    fhr = np.interp(x, x[valid], fhr[valid])
    return fhr, valid


def _baseline(sig: np.ndarray) -> float:
    if sig.size < 5:
        return float(np.median(sig)) if sig.size else 0.0
    local = np.abs(np.diff(sig, prepend=sig[0]))
    stable = sig[local < 5.0]
    return float(np.median(stable) if stable.size > sig.size * 0.2 else np.median(sig))


def _episodes(sig: np.ndarray, baseline: float, *, sign: int,
              amp: float = 15.0, min_dur_s: float = 15.0):
    """Return list of (start, nadir/peak_idx, end) for accel(+1)/decel(-1)."""
    min_len = int(min_dur_s * FS)
    mask = (sig >= baseline + amp) if sign > 0 else (sig <= baseline - amp)
    out, run_start = [], None
    for i, m in enumerate(mask):
        if m and run_start is None:
            run_start = i
        elif not m and run_start is not None:
            if i - run_start >= min_len:
                seg = sig[run_start:i]
                ext = run_start + (int(np.argmax(seg)) if sign > 0 else int(np.argmin(seg)))
                out.append((run_start, ext, i))
            run_start = None
    if run_start is not None and sig.size - run_start >= min_len:
        seg = sig[run_start:]
        ext = run_start + (int(np.argmax(seg)) if sign > 0 else int(np.argmin(seg)))
        out.append((run_start, ext, sig.size))
    return out


def detect_contractions(uc: np.ndarray) -> np.ndarray:
    """Contraction peak indices from the UC (tocograph) channel.

    Smooth, then find prominent peaks >= ~1 min apart (physiological spacing).
    """
    uc = np.asarray(uc, dtype=np.float64)
    if uc.size < FS * 60:
        return np.array([], dtype=int)
    win = FS * 15                                  # 15 s moving average
    kern = np.ones(win) / win
    smooth = np.convolve(uc, kern, mode="same")
    peaks, _ = find_peaks(smooth, prominence=15.0, distance=FS * 60)
    return peaks


def extract_features_v2(fhr: np.ndarray, uc: np.ndarray) -> np.ndarray:
    """Map aligned (FHR, UC) arrays -> v2 feature vector (already cleaned FHR)."""
    sig = np.asarray(fhr, dtype=np.float64).ravel()
    if sig.size == 0:
        return np.zeros(N_FEATURES_V2, dtype=np.float32)
    minutes = sig.size / (FS * 60)
    baseline = _baseline(sig)

    stv = float(np.mean(np.abs(np.diff(sig)))) if sig.size > 1 else 0.0
    per_min = FS * 60
    ranges = [np.ptp(sig[i:i + per_min]) for i in range(0, sig.size - per_min + 1, per_min)]
    ltv = float(np.mean(ranges)) if ranges else float(np.ptp(sig))

    accels = _episodes(sig, baseline, sign=+1)
    decels = _episodes(sig, baseline, sign=-1)

    contractions = detect_contractions(uc)
    # A decel is "late" if its nadir falls 10-45 s after some contraction peak.
    lo, hi = int(10 * FS), int(45 * FS)
    n_late = 0
    for _, nadir, _ in decels:
        if np.any((nadir - contractions >= lo) & (nadir - contractions <= hi)):
            n_late += 1

    feats = {
        "baseline_fhr": baseline,
        "overall_std": float(np.std(sig)),
        "stv": stv,
        "ltv": ltv,
        "accel_per_min": len(accels) / minutes if minutes else 0.0,
        "decel_per_min": len(decels) / minutes if minutes else 0.0,
        "pct_brady": float(np.mean(sig < 110.0)),
        "pct_tachy": float(np.mean(sig > 160.0)),
        "contractions_per_min": len(contractions) / minutes if minutes else 0.0,
        "late_decel_per_min": n_late / minutes if minutes else 0.0,
        "late_decel_ratio": n_late / max(1, len(contractions)),
    }
    return np.array([feats[n] for n in FEATURE_NAMES_V2], dtype=np.float32)


if __name__ == "__main__":
    import wfdb
    from config import TOOLS_DIR
    sig, _ = wfdb.rdsamp(str(TOOLS_DIR / "dat" / "1001"))
    fhr, valid = clean_fhr_inplace(sig[:, 0])
    v = extract_features_v2(fhr, sig[:, 1])
    for n, x in zip(FEATURE_NAMES_V2, v):
        print(f"  {n:22s} {x:8.3f}")
    print("OK", v.shape)
