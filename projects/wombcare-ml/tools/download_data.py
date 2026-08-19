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
"""Fetch the CTU-UHB Intrapartum Cardiotocography Database from PhysioNet.

The raw dataset (~42 MB, 552 records of FHR + UC at 4 Hz) is public data and is
NOT stored in git -- run this once after cloning to populate `tools/dat/`.

    python code/download_data.py

Then (optionally) regenerate the cleaned signals and labels:
    python tools/waveform_processing.py   # -> tools/processed_dat/
    python code/generate_outcomes.py      # -> code/outcomes.csv

Reference: PhysioNet `ctu-uhb-ctgdb` (https://physionet.org/content/ctu-uhb-ctgdb/).
"""
from __future__ import annotations

import sys
from pathlib import Path

DB = "ctu-uhb-ctgdb"
DEST = Path(__file__).resolve().parent / "dat"


def main() -> int:
    try:
        import wfdb
    except ImportError:
        print("wfdb not installed. Run: pip install wfdb")
        return 1

    DEST.mkdir(parents=True, exist_ok=True)
    # Skip if it already looks populated (RECORDS + at least one record present).
    if (DEST / "RECORDS").exists() and any(DEST.glob("*.dat")):
        n = len(list(DEST.glob("*.dat")))
        print(f"Dataset already present in {DEST} ({n} .dat files). Nothing to do.")
        return 0

    print(f"Downloading '{DB}' from PhysioNet into {DEST} ...")
    wfdb.dl_database(DB, dl_dir=str(DEST))
    n = len(list(DEST.glob("*.dat")))
    print(f"Done. {n} records in {DEST}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
