"""FHR feature extraction (FR-ML-3).

Turns one cleaned FHR window -> a fixed-length feature vector of clinically
standard cardiotocography descriptors (FIGO-inspired): baseline, short- and
long-term variability, accelerations/decelerations, and signal-quality proxies.

All features are cheap, streaming-friendly arithmetic so the same logic can be
ported to fixed-point on the EFR32MG26 later (FR-ML-3, FR-ML-6).
"""
from __future__ import annotations

import numpy as np

from config import FS

# Physiological FHR bounds (bpm). Cleaning already enforces 50..200; we keep
# the same bounds here for the validity proxy.
_HR_MIN, _HR_MAX = 50.0, 200.0

# Ordered feature names -> guarantees a stable vector layout across the pipeline
# and the on-device port. Do not reorder without bumping the model.
FEATURE_NAMES: list[str] = [
    "baseline_fhr",       # robust baseline (median of stable samples)
    "mean_fhr",
    "std_fhr",
    "min_fhr",
    "max_fhr",
    "p10_fhr",
    "p90_fhr",
    "stv",                # short-term variability (mean |Δ| between successive samples)
    "ltv",                # long-term variability (mean per-minute range)
    "delta_line_var",     # mean abs deviation from baseline
    "accel_count",        # accelerations per window
    "accel_rate_per_min",
    "decel_count",        # decelerations per window
    "decel_rate_per_min",
    "pct_time_accel",     # fraction of window in acceleration
    "pct_time_decel",     # fraction of window in deceleration
    "pct_valid",          # signal-quality proxy: fraction within physiological range
    "abnormal_low_frac",  # fraction of samples < 110 bpm (bradycardia tendency)
    "abnormal_high_frac", # fraction of samples > 160 bpm (tachycardia tendency)
    "decel_area",         # mean depth of dips > 15 bpm below baseline (deceleration burden)
    "decel_depth",        # deepest single dip below baseline (bpm)
    "stv_last5",          # short-term variability over the final 5 min (acidemia marker)
]
N_FEATURES = len(FEATURE_NAMES)


def _baseline(sig: np.ndarray) -> float:
    """Robust baseline: median of samples whose local variability is low.

    Approximates the clinical 'baseline' as the stable heart-rate level,
    excluding accel/decel excursions.
    """
    if sig.size < 5:
        return float(np.median(sig)) if sig.size else 0.0
    local = np.abs(np.diff(sig, prepend=sig[0]))
    stable = sig[local < 5.0]                 # < 5 bpm change = stable
    base = np.median(stable) if stable.size > sig.size * 0.2 else np.median(sig)
    return float(base)


def _variability(sig: np.ndarray) -> tuple[float, float]:
    """Short-term (sample-to-sample) and long-term (per-minute range) variability."""
    stv = float(np.mean(np.abs(np.diff(sig)))) if sig.size > 1 else 0.0
    per_min = FS * 60
    ranges = [
        np.ptp(sig[i:i + per_min])
        for i in range(0, sig.size - per_min + 1, per_min)
    ]
    ltv = float(np.mean(ranges)) if ranges else float(np.ptp(sig))
    return stv, ltv


def _episodes(sig: np.ndarray, baseline: float, *, sign: int,
              amp: float = 15.0, min_dur_s: float = 15.0) -> tuple[int, float]:
    """Count accelerations (sign=+1) / decelerations (sign=-1).

    Clinical definition: a deviation of >= `amp` bpm from baseline lasting
    >= `min_dur_s` seconds. Returns (count, fraction-of-time-in-episode).
    """
    min_len = int(min_dur_s * FS)
    if sign > 0:
        mask = sig >= baseline + amp
    else:
        mask = sig <= baseline - amp

    count, in_episode = 0, 0
    run = 0
    for m in mask:
        if m:
            run += 1
        else:
            if run >= min_len:
                count += 1
                in_episode += run
            run = 0
    if run >= min_len:                         # trailing run
        count += 1
        in_episode += run
    frac = in_episode / sig.size if sig.size else 0.0
    return count, float(frac)


def extract_features(window: np.ndarray) -> np.ndarray:
    """Map a 1-D FHR window (bpm, 4 Hz) -> feature vector (len = N_FEATURES)."""
    sig = np.asarray(window, dtype=np.float64).ravel()
    sig = sig[np.isfinite(sig)]
    if sig.size == 0:
        return np.zeros(N_FEATURES, dtype=np.float32)

    baseline = _baseline(sig)
    stv, ltv = _variability(sig)
    n_acc, t_acc = _episodes(sig, baseline, sign=+1)
    n_dec, t_dec = _episodes(sig, baseline, sign=-1)
    minutes = sig.size / (FS * 60)

    in_range = np.logical_and(sig >= _HR_MIN, sig <= _HR_MAX)

    # Deceleration burden relative to baseline; STV over the final 5 minutes
    # (variability collapses near acidemia, so the late-window value is salient).
    dip = np.clip((baseline - 15.0) - sig, 0.0, None)
    last5 = sig[-FS * 60 * 5:] if sig.size >= FS * 60 * 5 else sig
    stv_last5 = float(np.mean(np.abs(np.diff(last5)))) if last5.size > 1 else 0.0

    feats = {
        "baseline_fhr": baseline,
        "mean_fhr": float(np.mean(sig)),
        "std_fhr": float(np.std(sig)),
        "min_fhr": float(np.min(sig)),
        "max_fhr": float(np.max(sig)),
        "p10_fhr": float(np.percentile(sig, 10)),
        "p90_fhr": float(np.percentile(sig, 90)),
        "stv": stv,
        "ltv": ltv,
        "delta_line_var": float(np.mean(np.abs(sig - baseline))),
        "accel_count": float(n_acc),
        "accel_rate_per_min": n_acc / minutes if minutes else 0.0,
        "decel_count": float(n_dec),
        "decel_rate_per_min": n_dec / minutes if minutes else 0.0,
        "pct_time_accel": t_acc,
        "pct_time_decel": t_dec,
        "pct_valid": float(np.mean(in_range)),
        "abnormal_low_frac": float(np.mean(sig < 110.0)),
        "abnormal_high_frac": float(np.mean(sig > 160.0)),
        "decel_area": float(dip.sum() / sig.size),
        "decel_depth": float(dip.max()),
        "stv_last5": stv_last5,
    }
    return np.array([feats[name] for name in FEATURE_NAMES], dtype=np.float32)


if __name__ == "__main__":
    # Smoke test on a synthetic window: baseline 140 with one acceleration.
    rng = np.random.default_rng(0)
    s = 140 + rng.normal(0, 3, FS * 60 * 20)
    s[FS * 60 * 2: FS * 60 * 2 + FS * 20] += 25      # 20-s acceleration
    v = extract_features(s)
    for name, val in zip(FEATURE_NAMES, v):
        print(f"  {name:20s} {val:8.3f}")
    assert v.shape == (N_FEATURES,)
    print("OK", v.shape)
