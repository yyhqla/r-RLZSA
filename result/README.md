# Result Package

This directory contains compact experiment results for public release.

## r-RLZSA

`r-RLZSA/` contains the innovation 1 result tables on seven datasets:

- `combined_build_summary.csv`
- `combined_final_test_performance.csv`
- `combined_validation_theta_scan.csv`
- `combined_workload_occ_stats.csv`
- per-dataset summaries under each dataset directory
- `all_7_datasets_statistics.md`

## Vr-RLZSA

`Vr-RLZSA/` contains the innovation 2 result tables:

- `varint_v2_cross_dataset/`: Move-r, Move-r-RLZ, Move-r-RLZEnd, and
  Vr-RLZSA comparison.
- `vrrlzsa_ablation/`: r-RLZSA raw fields vs Vr-RLZSA-v1 vs Vr-RLZSA-v2.
- `field_frequency_top100/`: compact SR/CPL top-100 rank-frequency data for
  plotting and paper analysis.

The full local logs and generated indexes are not included.
