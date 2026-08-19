# code/ -- Data & Signal Pipeline (CTU-UHB CTG)

Upstream data layer shared by the project. The **raw dataset and the regenerable
`processed_dat/` are not stored in git** (they're large and reproducible); fetch
and build them locally.

## Contents (tracked)
| File | Role |
|---|---|
| `download_data.py` | fetch the raw CTU-UHB records from PhysioNet -> `dat/` |
| `simple_denoise.py` | FHR cleaning recipe (stable-start, spike/gap/large-change handling) |
| `waveform_processing.py` | batch-clean all records -> `processed_dat/` |
| `generate_outcomes.py` | parse `.hea` outcome blocks -> `outcomes.csv` (pH, Apgar, ...) |
| `outcomes.csv` | per-record clinical labels (small; committed for convenience) |

## Not tracked (gitignored -- regenerate locally)
| Path | How to (re)create |
|---|---|
| `dat/` (~42 MB, raw FHR+UC @ 4 Hz) | `python code/download_data.py` |
| `processed_dat/` (~49 MB, cleaned FHR) | `python code/waveform_processing.py` |

## Quick start (fresh clone)
```bash
pip install wfdb numpy pandas scipy
python code/download_data.py        # -> code/dat/
# outcomes.csv is committed; to rebuild it: python code/generate_outcomes.py
# the ML pipeline (../ml) reads the RAW records directly:
python ml/run_all.py
```

> Note: the v2 ML pipeline in [`../ml`](../ml) reads the **raw** records in `dat/`
> (FHR + UC, time-aligned). `processed_dat/` is only needed by the v1 FHR-only
> reference extractor.

Dataset: PhysioNet `ctu-uhb-ctgdb` -- https://physionet.org/content/ctu-uhb-ctgdb/
