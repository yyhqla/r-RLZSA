# Paper Experiment Data Export

This folder collects detailed experiment outputs for paper writing.

Exported datasets and source result directories:

| export dataset | source result directory |
|---|---|
| world_leaders | `measurements/results/r-RLZSA_world_leaders_m4_128_rerun_20260603` |
| SARS-Cov-2_40 | `measurements/results/r-RLZSA_SARS-Cov-2_40_m4_128_rerun_20260603` |
| DNA | `measurements/results/r-RLZSA_DNA_full_m4_128_rerun_20260603` |
| candida_auris | `measurements/results/r-RLZSA_candida_auris_full_m4_128_rerun_20260607` |
| english_200MB | `measurements/results/r-RLZSA_english_200MB_m4_128_rerun_20260607` |
| einstein | `measurements/results/r-RLZSA_einstein_full_m4_128_rerun_20260603` |
| boost | `measurements/results/r-RLZSA_boost_full_m4_128_rerun_20260603` |

Per-dataset subfolders contain:

- `final_test_summary.md`: human-readable build/space/workload/performance table.
- `final_test_performance.csv`: per method and workload final test data.
- `build_summary_selected.csv`: build time, space, and memory summary.
- `workload_occ_stats.csv`: occurrence distribution for workloads.
- `validation_theta_scan.csv`: theta validation scan for adaptive methods.
- `selected_theta.txt`: selected theta values.

Combined CSV files at this folder root merge the same metrics across all datasets.
