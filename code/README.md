# Code Package

This directory keeps the relevant algorithm code and experiment scripts with
their original relative paths.

## Core Files

- `include/move_r/move_r.hpp`
- `include/move_r/queries.cpp`
- `include/move_r/construction/*.cpp`
- `include/lzendsa/lzendsa_encoding.hpp`
- `cli/move-r/move-r-build.cpp`
- `cli/move-r/move-r-locate.cpp`
- `cli/move-r/move-r-partial-codec.cpp`
- `cli/move-r/move-r-dump-fields.cpp`
- `CMakeLists.txt`

## Experiment Scripts

- `measurements/run_selected_m4_128.py`: r-RLZSA cross-dataset experiments.
- `measurements/run_innovation2_varint_v2_cross_dataset.py`: Vr-RLZSA
  cross-dataset experiments.
- `measurements/run_vrrlzsa_ablation.py`: Vr-RLZSA ablation experiments.
- `measurements/dump_vrrlzsa_field_frequency.py`: field-frequency export.
- `dataset-calculate/dataset-cal.py`: dataset statistics helper.

This is a curated code subset, not a standalone replacement for the full
Move-r repository. To reproduce from a clean clone, copy these files into the
same relative paths in the full project.
