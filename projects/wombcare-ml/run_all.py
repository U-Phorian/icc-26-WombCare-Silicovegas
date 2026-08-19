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
"""Run the full WombCare ML pipeline end to end (FR-ML-1 .. FR-ML-6).

    python run_all.py

Reproduces: train -> evaluate -> quantize/export. Seeded; safe to re-run.
"""
import runpy
import sys

STAGES = ["train", "evaluate", "quantize_export"]

if __name__ == "__main__":
    for stage in STAGES:
        print(f"\n{'='*60}\n  {stage}\n{'='*60}")
        sys.argv = [stage + ".py"]
        runpy.run_module(stage, run_name="__main__")
    print("\nDone. See artifacts/ and reports/.")
