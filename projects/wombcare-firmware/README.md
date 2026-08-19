# WombCare firmware — EFR32MG26

Wearable fetal-wellness firmware. Samples maternal ECG, fetal ECG and a PVDF kick sensor,
runs the full DSP and an int8 TinyML classifier **on-device**, and notifies one 15-byte
clinical summary per minute over Bluetooth LE.

Raw ECG never leaves the device. See the [root README](../../README.md) for the system
architecture and the [feature spec](../../resources/docs/FEATURE_SPEC.md) for the parity
contract this firmware must honour.

## Layout

| Path | What it is |
| --- | --- |
| `Wombcare_PreFinal2.slcp` | The SLC project definition — the source of truth for components and sources |
| `src/` | Implementation |
| `inc/` | Headers |
| `config/` | SLC component configuration, hand-tuned after generation |
| `vscode.conf` | Pinned tool versions |
| `autogen/`, `cmake_gcc/` | **Generated.** Not tracked; recreated by `slc generate` |
| `simplicity_sdk_2026.6.0/`, `aiml_3.0.0/` | **Vendored SDKs.** Not tracked; supplied by Simplicity Studio |

The int8 model and scaler are **not** duplicated here. `Wombcare_PreFinal2.slcp` compiles
`model_data.c` and `scaler.c` in place from
[`../wombcare-ml/artifacts/ctg/firmware/`](../wombcare-ml/artifacts/ctg/firmware/), so
retraining cannot leave the firmware linked against a model it was not exported for.

## Prerequisites

| Tool | Version |
| --- | --- |
| Simplicity Studio | 6.0.0 |
| Simplicity SDK | 2026.6.0 (Gecko / 32-bit MCU) |
| AI/ML SDK extension | 3.0.0 — provides `tensorflow_lite_micro` |
| Arm GNU toolchain | 14.2.rel1 |
| CMake | ≥ 3.25 |
| Ninja | any recent |
| Simplicity Commander | 1.24.1 |

Target part: **EFR32MG26B510F3200IM68**.

## Build

A fresh clone contains no `autogen/` and no `cmake_gcc/` — both are generated. Create them
first:

```bash
cd projects/wombcare-firmware
slc generate Wombcare_PreFinal2.slcp -d . --with EFR32MG26B510F3200IM68
```

Then build:

```bash
cd cmake_gcc
cmake --workflow --preset project      # configure + build
```

The image lands in `cmake_gcc/build/base/`.

In Simplicity Studio or VS Code with the Silicon Labs extension, opening
`Wombcare_PreFinal2.slcp` performs the generate step for you.

> In VS Code the folder containing the `.slcp` must itself be a workspace root, or the
> Silicon Labs extension does not activate.

## Clean

```bash
cd cmake_gcc
cmake --build build --config base --target clean    # object files only
```

Full clean — delete the generated trees and regenerate:

```bash
cd projects/wombcare-firmware
rm -rf cmake_gcc build autogen
slc generate Wombcare_PreFinal2.slcp -d . --with EFR32MG26B510F3200IM68
```

Nothing in `autogen/` or `cmake_gcc/` is tracked, so deleting them is always safe.

## Flash

```bash
commander flash cmake_gcc/build/base/Wombcare_PreFinal2.hex --device EFR32MG26B510F3200
```

## Known issues

- **`wombcare_buffer` type mismatch.** `src/wombcare_dsp.c` expects `uint16_t` ring buffers
  of raw ADC counts and performs the microvolt conversion itself, while
  `inc/wombcare_buffer.h` still declares them `float` and converts on ingest. This is three
  incompatible-pointer errors at compile time, and the scaling would otherwise be applied
  twice. Fix before flashing.
- **`src/wombcare_ble.c` includes `app_log.h`** but the `app_log` component is not selected
  in the `.slcp`. Either select the component or drop the include.
- **`wombcare_battery.c/.h` are absent.** They exist in the `Wombcare_8PreFinal` working
  tree but were never pushed to this repository.
