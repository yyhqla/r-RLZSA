# GitHub Release Package

This directory is a compact package for publishing the current research code
and experiment results.

## Layout

- `code/`: source files and experiment scripts related to the current
  algorithms.
- `result/r-RLZSA/`: experiment data for innovation 1. In this package the
  method is consistently named `r-RLZSA`.
- `result/Vr-RLZSA/`: experiment data for innovation 2, including varint-v2
  cross-dataset results, ablation data, and compact field-frequency data.

## Naming

- `r-RLZSA`: innovation 1.
- `Vr-RLZSA`: innovation 2.
- `Move-r-RLZ`: full RLZSA baseline, also called rlz-only in older notes.
- `Move-r-RLZEnd`: LZ-End baseline.

Large local datasets, generated indexes, raw pattern files, full logs, build
directories, and private notes are intentionally not included.
