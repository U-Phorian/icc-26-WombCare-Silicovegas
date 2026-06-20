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
