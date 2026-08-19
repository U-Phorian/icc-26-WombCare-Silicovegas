# Resources

This folder contains resources that are not directly related to the software. Examples of such resources include images, documentation, and other supplementary files.

## Contents

- **[docs/](docs/)**: Project documentation.
  - [FEATURE_SPEC.md](docs/FEATURE_SPEC.md) — the parity contract binding the firmware's
    feature extraction to the model's training. **Read this before changing either side.**
  - [repository-guidelines.md](docs/repository-guidelines.md) — branching, commits, review,
    and what must never be committed.
  - [handoff/](docs/handoff/) — phase handover notes between the ML and firmware owners.
- **Images**: Store any images used in documentation or the project here.
- **Supplementary Files**: Any other files that are useful for the project but not part of the source code.

Documentation is Markdown and [Mermaid](https://mermaid.js.org/) only. `pdf`, `xlsx`, `docx`
and `ppt` do not diff, do not review and do not merge — the root
[.gitignore](../.gitignore) blocks them.

## Usage

Place any non-software-related resources in this folder to keep the project organized and maintain a clear separation between code and supplementary materials.
