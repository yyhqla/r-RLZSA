# 论文实验数据导出

本文件夹汇总了用于论文写作的详细实验输出。

导出的数据集及其源结果目录如下：

| 导出数据集 | 源结果目录 |
|---|---|
| world_leaders | `measurements/results/r-RLZSA_world_leaders_m4_128_rerun_20260603` |
| SARS-Cov-2_40 | `measurements/results/r-RLZSA_SARS-Cov-2_40_m4_128_rerun_20260603` |
| DNA | `measurements/results/r-RLZSA_DNA_full_m4_128_rerun_20260603` |
| candida_auris | `measurements/results/r-RLZSA_candida_auris_full_m4_128_rerun_20260607` |
| english_200MB | `measurements/results/r-RLZSA_english_200MB_m4_128_rerun_20260607` |
| einstein | `measurements/results/r-RLZSA_einstein_full_m4_128_rerun_20260603` |
| boost | `measurements/results/r-RLZSA_boost_full_m4_128_rerun_20260603` |

每个数据集子文件夹包含：

- `final_test_summary.md`：便于阅读的构建、空间、负载和性能表。
- `final_test_performance.csv`：按方法和负载划分的最终测试数据。
- `build_summary_selected.csv`：构建时间、空间和内存摘要。
- `workload_occ_stats.csv`：负载的 occurrence 分布。
- `validation_theta_scan.csv`：自适应方法的 theta 验证扫描结果。
- `selected_theta.txt`：选定的 theta 值。

本文件夹根目录下的合并 CSV 文件汇总了所有数据集上的相同指标。
