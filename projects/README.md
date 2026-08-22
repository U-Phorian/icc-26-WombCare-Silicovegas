# Projects

Software projects live here, one folder each, per the
[ICC-26 team repo template](https://github.com/IoT-Challenge-2026/icc-26-team-repo-template).

| Project | What it is | Build and clean |
| --- | --- | --- |
| [wombcare-firmware/](wombcare-firmware/) | EFR32MG26 wearable firmware — sensing, DSP, on-device TinyML, BLE GATT server | [README](wombcare-firmware/README.md) |
| [wombcare-ml/](wombcare-ml/) | CTG training pipeline and the exported int8 model the firmware links against | [README](wombcare-ml/README.md) |
| [wombcare-app/](wombcare-app/) | Android companion app — patient and doctor roles, BLE client, Firebase sync | [README](wombcare-app/README.md) |

Each project documents its own build and clean steps in its README.

## How they connect

`wombcare-ml` exports the model, and `wombcare-firmware` compiles it **in place** —
`Wombcare_PreFinal2.slcp` lists `../wombcare-ml/artifacts/ctg/firmware/model_data.c` and
`scaler.c` in its `source:` block. There is no copy step, so retraining cannot leave the
firmware linked against a model it was not exported for.

`wombcare-app` is the third side of the same contract: the device↔app wire format is specified
once, in [wombcare-app/docs/BLE_CONTRACT.md](wombcare-app/docs/BLE_CONTRACT.md). Change that
document before touching BLE code in either the firmware or the app.

The 8-feature vector the firmware and the model both compute is pinned by
[resources/docs/FEATURE_SPEC.md](../resources/docs/FEATURE_SPEC.md). If the firmware and the
training pipeline disagree, the model receives out-of-distribution inputs and still returns a
confident answer — the failure is silent. Change the spec first, by agreement, and bump
`spec_version`.

## Adding a project

Create a folder here and give it a README covering what it is, its prerequisites, how to
build it and how to clean it — template rule 4. Add any generated or vendored output to the root
[.gitignore](../.gitignore) — see
[resources/docs/repository-guidelines.md](../resources/docs/repository-guidelines.md).
