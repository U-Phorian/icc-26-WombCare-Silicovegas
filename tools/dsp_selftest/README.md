# DSP host self-test

Runs the **real** `wombcare_dsp.c` on a PC against synthetic signals, so
the detector can be changed without having to flash a board to find out
whether it still behaves.

```sh
sh tools/dsp_selftest/run.sh          # uses cc/gcc/clang
CC=/c/MinGW/bin/gcc sh tools/dsp_selftest/run.sh
```

Exit status is 0 when every safety check passes.

Nothing here mocks the DSP. `run.sh` copies `wombcare_dsp.c` next to a
set of small stand-in headers (`stub/`) that provide only the CMSIS-DSP
routines the file calls — `arm_fir_f32`, `arm_lms_norm_f32`, `arm_mean_f32`
and so on — as plain reference implementations. The code under test is
byte-for-byte the code that ships.

## What this bench can tell you

The checks under **SAFETY PROPERTIES** and **MATERNAL HEART RATE** are
real pass/fail assertions:

- an unplugged electrode produces no reading, rather than the confident
  ~147 bpm the previous detector reported;
- a flatline is rejected rather than reported as a row of zeros;
- the maternal rate is recovered to within a couple of bpm across
  55–120 bpm;
- **the mother's pulse is never published in the fetal field**, which is
  the most dangerous thing this device could do — it looks like a healthy
  reading at exactly the moment the fetal signal has been lost.

## What it cannot tell you

Whether the fetal rate is actually recovered from a real abdominal
recording. That is the hard problem of this product, and a synthetic
signal cannot settle it:

- the maternal/fetal amplitude ratio and the QRS shapes here are
  plausible guesses, not measurements;
- the harness runs each window from a cold LMS filter, whereas on the
  device the canceller runs continuously and stays converged, so real
  maternal cancellation should be considerably better than what is
  modelled here.

The fetal figures are therefore printed as **diagnostics, not
assertions**. On this synthetic input the pipeline reports "not detected"
rather than guessing — the safe outcome, but not evidence that it will
find a real fetus.

## Calibrating against real signals

Two thresholds in `wombcare_dsp.c` are set from this bench and want
confirming against real recordings:

| Constant | Now | Notes |
|---|---|---|
| `QRS_QUALITY_MIN_MATERNAL` | 3.0 | Bench: noise ≈ 1.2, real trace 4.3–5.3. Wide margin. This gate is the only thing between noise and a published maternal rate. |
| `QRS_QUALITY_MIN_FETAL` | 1.5 | Deliberately low. Measurement showed the other gates reject noise on this path without it, so the real risk is setting it too high and silencing the device on a weak but genuine trace. |

To calibrate: capture real windows, feed them in place of the synthetic
generators in `main.c`, and print `fetal_quality` (already on the
`WombCareVitals_t` struct and byte-visible in the `WC_LOG` line as `q=`).
Raise the fetal figure only with real traces in hand, and treat any
increase as a change that can stop the device reporting at all.
