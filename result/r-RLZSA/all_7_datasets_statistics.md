# Seven-Dataset Experiment Statistics

This file aggregates the paper-writing statistics exported from the seven datasets.

## Source Directories

| dataset | source_result_dir |
| --- | --- |
| world_leaders | measurements/results/r-RLZSA_world_leaders_m4_128_rerun_20260603 |
| SARS-Cov-2_40 | measurements/results/r-RLZSA_SARS-Cov-2_40_m4_128_rerun_20260603 |
| DNA | measurements/results/r-RLZSA_DNA_full_m4_128_rerun_20260603 |
| candida_auris | measurements/results/r-RLZSA_candida_auris_full_m4_128_rerun_20260607 |
| english_200MB | measurements/results/r-RLZSA_english_200MB_m4_128_rerun_20260607 |
| einstein | measurements/results/r-RLZSA_einstein_full_m4_128_rerun_20260603 |
| boost | measurements/results/r-RLZSA_boost_full_m4_128_rerun_20260603 |

## Build And Space Summary

| export_dataset | method | index_MiB | build_time_ns | peak_mem_usage_reported | max_rss_kb_time_v |
| --- | --- | --- | --- | --- | --- |
| world_leaders | Move-r | 13.42 | 522664252 | 265488362 | 267620 |
| world_leaders | Move-r-RLZ | 26.26 | 925933889 | 303532861 | 309412 |
| world_leaders | Move-r-RLZEnd | 16.43 | 34789387525 | 1052920564 | 1049292 |
| world_leaders | r-RLZSA@25% | 17.50 | 914674124 | 308175087 | 318472 |
| world_leaders | r-RLZSA@12.5% | 16.50 | 891663059 | 308074075 | 316792 |
| SARS-Cov-2_40 | Move-r | 3.32 | 501489779 | 230307316 | 220964 |
| SARS-Cov-2_40 | Move-r-RLZ | 8.55 | 624954773 | 239161290 | 238292 |
| SARS-Cov-2_40 | Move-r-RLZEnd | 4.31 | 37316530988 | 933977227 | 924184 |
| SARS-Cov-2_40 | r-RLZSA@25% | 7.56 | 613932105 | 241340021 | 239492 |
| SARS-Cov-2_40 | r-RLZSA@12.5% | 6.91 | 601685506 | 241125601 | 241120 |
| DNA | Move-r | 28.91 | 7983559228 | 3192573407 | 3143356 |
| DNA | Move-r-RLZ | 192.54 | 13951341314 | 3459001816 | 3562376 |
| DNA | Move-r-RLZEnd | 55.40 | 1014133412612 | 14147897733 | 13870408 |
| DNA | r-RLZSA@25% | 94.03 | 13882239375 | 3496467458 | 3570416 |
| DNA | r-RLZSA@12.5% | 68.77 | 13907685412 | 3497413069 | 3576524 |
| candida_auris | Move-r | 1206.74 | 14772340631 | 3396593589 | 4183604 |
| candida_auris | Move-r-RLZ | 1468.92 | 75132814349 | 5819322861 | 6013160 |
| candida_auris | Move-r-RLZEnd | 1418.30 | 489847478544 | 9263632750 | 9824636 |
| candida_auris | r-RLZSA@25% | 1479.24 | 75740631719 | 6220745857 | 6739424 |
| candida_auris | r-RLZSA@12.5% | 1354.12 | 75514812694 | 6220740301 | 6621364 |
| english_200MB | Move-r | 1749.70 | 14883649655 | 3252790447 | 4480448 |
| english_200MB | Move-r-RLZ | 1627.15 | 63317656782 | 5509983896 | 6969972 |
| english_200MB | Move-r-RLZEnd | 1913.34 | 278473832807 | 8245603735 | 8730980 |
| english_200MB | r-RLZSA@25% | 1933.49 | 66336993813 | 6087796234 | 7421292 |
| english_200MB | r-RLZSA@12.5% | 1835.22 | 66397338666 | 6086076124 | 7532812 |
| einstein | Move-r | 6.76 | 7275360636 | 2362111823 | 2307024 |
| einstein | Move-r-RLZ | 21.85 | 7679291897 | 2378424804 | 2339812 |
| einstein | Move-r-RLZEnd | 8.32 | 660815019197 | 10492657505 | 10265848 |
| einstein | r-RLZSA@25% | 18.32 | 7667381504 | 2396111464 | 2349264 |
| einstein | r-RLZSA@12.5% | 14.99 | 7667183371 | 2396063586 | 2351056 |
| boost | Move-r | 1.52 | 8692455414 | 3164364470 | 3084260 |
| boost | Move-r-RLZ | 5.39 | 9014614648 | 3154282835 | 3094056 |
| boost | Move-r-RLZEnd | 1.91 | 643583041085 | 14128953681 | 13812976 |
| boost | r-RLZSA@25% | 7.01 | 9144078705 | 3172840535 | 3098028 |
| boost | r-RLZSA@12.5% | 4.60 | 8980333000 | 3173946237 | 3098860 |

## Selected Theta

| dataset | method | theta |
| --- | --- | --- |
| world_leaders | r-RLZSA25 | 1024 |
| world_leaders | r-RLZSA12 | 256 |
| SARS-Cov-2_40 | r-RLZSA25 | 64 |
| SARS-Cov-2_40 | r-RLZSA12 | 32 |
| DNA | r-RLZSA25 | 64 |
| DNA | r-RLZSA12 | 128 |
| candida_auris | r-RLZSA25 | 32 |
| candida_auris | r-RLZSA12 | 64 |
| english_200MB | r-RLZSA25 | 128 |
| english_200MB | r-RLZSA12 | 0 |
| einstein | r-RLZSA25 | 0 |
| einstein | r-RLZSA12 | 32 |
| boost | r-RLZSA25 | 256 |
| boost | r-RLZSA12 | 256 |

## Workload Occurrence Statistics

| export_dataset | workload | occ_avg | occ_min | occ_median | occ_max |
| --- | --- | --- | --- | --- | --- |
| world_leaders | m4 | 10946592.37 | 5 | 11110727.5 | 21848294 |
| world_leaders | m8 | 8877140.07 | 1 | 19840.5 | 20611048 |
| world_leaders | m16 | 7163126.30 | 1 | 8884.5 | 18150219 |
| world_leaders | m32 | 3688975.73 | 1 | 763.5 | 13326037 |
| world_leaders | m64 | 526074.85 | 1 | 56.0 | 4912598 |
| world_leaders | m128 | 861.08 | 1 | 28.0 | 9412 |
| SARS-Cov-2_40 | m4 | 236303.19 | 2401 | 222928.0 | 882741 |
| SARS-Cov-2_40 | m8 | 21389.57 | 2 | 2655.0 | 855407 |
| SARS-Cov-2_40 | m16 | 15847.56 | 1 | 1378.0 | 808299 |
| SARS-Cov-2_40 | m32 | 10000.46 | 1 | 1367.5 | 728532 |
| SARS-Cov-2_40 | m64 | 14375.31 | 1 | 1338.0 | 601336 |
| SARS-Cov-2_40 | m128 | 2725.74 | 1 | 1289.5 | 423349 |
| DNA | m4 | 23446229.87 | 9826 | 15671490.0 | 48300668 |
| DNA | m8 | 14292074.51 | 596 | 4370988.0 | 40570280 |
| DNA | m16 | 9848782.32 | 596 | 3096656.0 | 29101121 |
| DNA | m32 | 2658144.68 | 563 | 609592.0 | 15842143 |
| DNA | m64 | 958363.93 | 1 | 590229.0 | 4719165 |
| DNA | m128 | 508268.65 | 1 | 553601.0 | 1106582 |
| candida_auris | m4 | 1631683.62 | 515951 | 1539685.5 | 3700398 |
| candida_auris | m8 | 7866.29 | 1 | 6797.0 | 48864 |
| candida_auris | m16 | 42.17 | 1 | 13.0 | 6584 |
| candida_auris | m32 | 17.35 | 1 | 10.0 | 483 |
| candida_auris | m64 | 10.23 | 1 | 5.0 | 371 |
| candida_auris | m128 | 1.56 | 1 | 1.0 | 60 |
| english_200MB | m4 | 118047.71 | 4 | 17617.5 | 2641644 |
| english_200MB | m8 | 3260.17 | 1 | 158.5 | 273351 |
| english_200MB | m16 | 12.56 | 1 | 2.0 | 1872 |
| english_200MB | m32 | 5.43 | 1 | 1.0 | 1476 |
| english_200MB | m64 | 11.12 | 1 | 1.0 | 4756 |
| english_200MB | m128 | 1.47 | 1 | 1.0 | 4 |
| einstein | m4 | 290651.11 | 384 | 56063.5 | 3397531 |
| einstein | m8 | 38157.17 | 30 | 6697.0 | 2475825 |
| einstein | m16 | 8258.01 | 29 | 4897.0 | 369952 |
| einstein | m32 | 4350.85 | 6 | 3524.5 | 69416 |
| einstein | m64 | 3322.29 | 1 | 3056.5 | 67948 |
| einstein | m128 | 2318.15 | 1 | 1773.5 | 7292 |
| boost | m4 | 8791312.73 | 1760 | 134535.0 | 81173858 |
| boost | m8 | 3564104.19 | 1760 | 29920.0 | 44101707 |
| boost | m16 | 341983.76 | 1416 | 5870.0 | 14554913 |
| boost | m32 | 19673.90 | 425 | 2185.0 | 2619288 |
| boost | m64 | 5500.20 | 425 | 2185.0 | 375680 |
| boost | m128 | 2688.78 | 425 | 1760.0 | 39312 |

## Final Performance

### world_leaders

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 13.42 |  |  | 118986187.2 | 1.000 | 0.087 | PASS |
| m8 | Move-r | 13.42 |  |  | 96822277.6 | 1.000 | 0.087 | PASS |
| m16 | Move-r | 13.42 |  |  | 77915525.1 | 1.000 | 0.087 | PASS |
| m32 | Move-r | 13.42 |  |  | 39970780.5 | 1.000 | 0.089 | PASS |
| m64 | Move-r | 13.42 |  |  | 5668146.0 | 1.000 | 0.094 | PASS |
| m128 | Move-r | 13.42 |  |  | 17755.2 | 1.000 | 0.446 | PASS |
| m4 | Move-r-RLZ | 26.26 |  |  | 10383456.9 | 11.459 | 1.000 | PASS |
| m8 | Move-r-RLZ | 26.26 |  |  | 8449807.4 | 11.459 | 1.000 | PASS |
| m16 | Move-r-RLZ | 26.26 |  |  | 6811202.9 | 11.439 | 1.000 | PASS |
| m32 | Move-r-RLZ | 26.26 |  |  | 3570652.3 | 11.194 | 1.000 | PASS |
| m64 | Move-r-RLZ | 26.26 |  |  | 531140.4 | 10.672 | 1.000 | PASS |
| m128 | Move-r-RLZ | 26.26 |  |  | 7914.4 | 2.243 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 16.43 |  |  | 97335298.4 | 1.222 | 0.107 | PASS |
| m8 | Move-r-RLZEnd | 16.43 |  |  | 79135770.0 | 1.223 | 0.107 | PASS |
| m16 | Move-r-RLZEnd | 16.43 |  |  | 63718587.6 | 1.223 | 0.107 | PASS |
| m32 | Move-r-RLZEnd | 16.43 |  |  | 32998889.4 | 1.211 | 0.108 | PASS |
| m64 | Move-r-RLZEnd | 16.43 |  |  | 4717564.3 | 1.201 | 0.113 | PASS |
| m128 | Move-r-RLZEnd | 16.43 |  |  | 15324.8 | 1.159 | 0.516 | PASS |
| m4 | r-RLZSA@25% | 17.50 | 1024 | 53.6 | 62819137.6 | 1.894 | 0.165 | PASS |
| m8 | r-RLZSA@25% | 17.50 | 1024 | 56.9 | 47826382.6 | 2.024 | 0.177 | PASS |
| m16 | r-RLZSA@25% | 17.50 | 1024 | 64.6 | 32778664.4 | 2.377 | 0.208 | PASS |
| m32 | r-RLZSA@25% | 17.50 | 1024 | 87.9 | 7578687.4 | 5.274 | 0.471 | PASS |
| m64 | r-RLZSA@25% | 17.50 | 1024 | 99.3 | 451675.6 | 12.549 | 1.176 | PASS |
| m128 | r-RLZSA@25% | 17.50 | 1024 | 42.4 | 13694.4 | 1.297 | 0.578 | PASS |
| m4 | r-RLZSA@12.5% | 16.50 | 256 | 26.8 | 94326152.3 | 1.261 | 0.110 | PASS |
| m8 | r-RLZSA@12.5% | 16.50 | 256 | 28.4 | 74863433.0 | 1.293 | 0.113 | PASS |
| m16 | r-RLZSA@12.5% | 16.50 | 256 | 32.3 | 57279974.6 | 1.360 | 0.119 | PASS |
| m32 | r-RLZSA@12.5% | 16.50 | 256 | 43.9 | 24959484.8 | 1.601 | 0.143 | PASS |
| m64 | r-RLZSA@12.5% | 16.50 | 256 | 99.2 | 469113.1 | 12.083 | 1.132 | PASS |
| m128 | r-RLZSA@12.5% | 16.50 | 256 | 23.8 | 16198.6 | 1.096 | 0.489 | PASS |

### SARS-Cov-2_40

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 3.32 |  |  | 1825495.4 | 1.000 | 0.129 | PASS |
| m8 | Move-r | 3.32 |  |  | 115126.5 | 1.000 | 0.215 | PASS |
| m16 | Move-r | 3.32 |  |  | 82237.0 | 1.000 | 0.224 | PASS |
| m32 | Move-r | 3.32 |  |  | 51675.1 | 1.000 | 0.261 | PASS |
| m64 | Move-r | 3.32 |  |  | 69812.8 | 1.000 | 0.272 | PASS |
| m128 | Move-r | 3.32 |  |  | 20579.1 | 1.000 | 0.359 | PASS |
| m4 | Move-r-RLZ | 8.55 |  |  | 236287.8 | 7.726 | 1.000 | PASS |
| m8 | Move-r-RLZ | 8.55 |  |  | 24786.5 | 4.645 | 1.000 | PASS |
| m16 | Move-r-RLZ | 8.55 |  |  | 18458.2 | 4.455 | 1.000 | PASS |
| m32 | Move-r-RLZ | 8.55 |  |  | 13495.5 | 3.829 | 1.000 | PASS |
| m64 | Move-r-RLZ | 8.55 |  |  | 18962.6 | 3.682 | 1.000 | PASS |
| m128 | Move-r-RLZ | 8.55 |  |  | 7397.7 | 2.782 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 4.31 |  |  | 1668597.2 | 1.094 | 0.142 | PASS |
| m8 | Move-r-RLZEnd | 4.31 |  |  | 131786.9 | 0.874 | 0.188 | PASS |
| m16 | Move-r-RLZEnd | 4.31 |  |  | 96446.8 | 0.853 | 0.191 | PASS |
| m32 | Move-r-RLZEnd | 4.31 |  |  | 63502.4 | 0.814 | 0.213 | PASS |
| m64 | Move-r-RLZEnd | 4.31 |  |  | 86832.8 | 0.804 | 0.218 | PASS |
| m128 | Move-r-RLZEnd | 4.31 |  |  | 22785.7 | 0.903 | 0.325 | PASS |
| m4 | r-RLZSA@25% | 7.56 | 64 | 35.8 | 1341310.7 | 1.361 | 0.176 | PASS |
| m8 | r-RLZSA@25% | 7.56 | 64 | 91.9 | 34177.5 | 3.368 | 0.725 | PASS |
| m16 | r-RLZSA@25% | 7.56 | 64 | 93.8 | 23822.0 | 3.452 | 0.775 | PASS |
| m32 | r-RLZSA@25% | 7.56 | 64 | 90.1 | 19399.3 | 2.664 | 0.696 | PASS |
| m64 | r-RLZSA@25% | 7.56 | 64 | 93.7 | 23342.8 | 2.991 | 0.812 | PASS |
| m128 | r-RLZSA@25% | 7.56 | 64 | 71.6 | 13923.2 | 1.478 | 0.531 | PASS |
| m4 | r-RLZSA@12.5% | 6.91 | 32 | 21.0 | 1586489.7 | 1.151 | 0.149 | PASS |
| m8 | r-RLZSA@12.5% | 6.91 | 32 | 89.9 | 39673.2 | 2.902 | 0.625 | PASS |
| m16 | r-RLZSA@12.5% | 6.91 | 32 | 92.7 | 25869.8 | 3.179 | 0.714 | PASS |
| m32 | r-RLZSA@12.5% | 6.91 | 32 | 88.7 | 20676.5 | 2.499 | 0.653 | PASS |
| m64 | r-RLZSA@12.5% | 6.91 | 32 | 93.0 | 24070.9 | 2.900 | 0.788 | PASS |
| m128 | r-RLZSA@12.5% | 6.91 | 32 | 65.9 | 16911.9 | 1.217 | 0.437 | PASS |

### DNA

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 28.91 |  |  | 319373300.3 | 1.000 | 0.103 | PASS |
| m8 | Move-r | 28.91 |  |  | 194748263.9 | 1.000 | 0.096 | PASS |
| m16 | Move-r | 28.91 |  |  | 135166092.0 | 1.000 | 0.105 | PASS |
| m32 | Move-r | 28.91 |  |  | 36498856.2 | 1.000 | 0.098 | PASS |
| m64 | Move-r | 28.91 |  |  | 13229722.5 | 1.000 | 0.107 | PASS |
| m128 | Move-r | 28.91 |  |  | 6903647.5 | 1.000 | 0.112 | PASS |
| m4 | Move-r-RLZ | 192.54 |  |  | 32883074.9 | 9.712 | 1.000 | PASS |
| m8 | Move-r-RLZ | 192.54 |  |  | 18769994.2 | 10.376 | 1.000 | PASS |
| m16 | Move-r-RLZ | 192.54 |  |  | 14133795.2 | 9.563 | 1.000 | PASS |
| m32 | Move-r-RLZ | 192.54 |  |  | 3586015.9 | 10.178 | 1.000 | PASS |
| m64 | Move-r-RLZ | 192.54 |  |  | 1417394.3 | 9.334 | 1.000 | PASS |
| m128 | Move-r-RLZ | 192.54 |  |  | 771477.0 | 8.949 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 55.40 |  |  | 238932835.1 | 1.337 | 0.138 | PASS |
| m8 | Move-r-RLZEnd | 55.40 |  |  | 136023965.8 | 1.432 | 0.138 | PASS |
| m16 | Move-r-RLZEnd | 55.40 |  |  | 87182449.7 | 1.550 | 0.162 | PASS |
| m32 | Move-r-RLZEnd | 55.40 |  |  | 25991804.6 | 1.404 | 0.138 | PASS |
| m64 | Move-r-RLZEnd | 55.40 |  |  | 9960811.5 | 1.328 | 0.142 | PASS |
| m128 | Move-r-RLZEnd | 55.40 |  |  | 6284228.0 | 1.099 | 0.123 | PASS |
| m4 | r-RLZSA@25% | 94.03 | 64 | 44.6 | 201473205.8 | 1.585 | 0.163 | PASS |
| m8 | r-RLZSA@25% | 94.03 | 64 | 60.1 | 107362442.3 | 1.814 | 0.175 | PASS |
| m16 | r-RLZSA@25% | 94.03 | 64 | 80.1 | 39716125.1 | 3.403 | 0.356 | PASS |
| m32 | r-RLZSA@25% | 94.03 | 64 | 65.7 | 15986681.5 | 2.283 | 0.224 | PASS |
| m64 | r-RLZSA@25% | 94.03 | 64 | 41.2 | 9493293.6 | 1.394 | 0.149 | PASS |
| m128 | r-RLZSA@25% | 94.03 | 64 | 22.2 | 6185598.5 | 1.116 | 0.125 | PASS |
| m4 | r-RLZSA@12.5% | 68.77 | 128 | 22.2 | 266018980.9 | 1.201 | 0.124 | PASS |
| m8 | r-RLZSA@12.5% | 68.77 | 128 | 33.4 | 142944114.3 | 1.362 | 0.131 | PASS |
| m16 | r-RLZSA@12.5% | 68.77 | 128 | 42.6 | 86637456.7 | 1.560 | 0.163 | PASS |
| m32 | r-RLZSA@12.5% | 68.77 | 128 | 41.0 | 23783340.0 | 1.535 | 0.151 | PASS |
| m64 | r-RLZSA@12.5% | 68.77 | 128 | 27.9 | 10026411.3 | 1.319 | 0.141 | PASS |
| m128 | r-RLZSA@12.5% | 68.77 | 128 | 9.6 | 7774080.1 | 0.888 | 0.099 | PASS |

### candida_auris

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 1206.74 |  |  | 282528797.2 | 1.000 | 0.028 | PASS |
| m8 | Move-r | 1206.74 |  |  | 1413705.7 | 1.000 | 0.035 | PASS |
| m16 | Move-r | 1206.74 |  |  | 13800.4 | 1.000 | 0.353 | PASS |
| m32 | Move-r | 1206.74 |  |  | 12317.3 | 1.000 | 0.686 | PASS |
| m64 | Move-r | 1206.74 |  |  | 16687.3 | 1.000 | 0.884 | PASS |
| m128 | Move-r | 1206.74 |  |  | 26713.7 | 1.000 | 0.942 | PASS |
| m4 | Move-r-RLZ | 1468.92 |  |  | 7953899.2 | 35.521 | 1.000 | PASS |
| m8 | Move-r-RLZ | 1468.92 |  |  | 49215.8 | 28.725 | 1.000 | PASS |
| m16 | Move-r-RLZ | 1468.92 |  |  | 4871.7 | 2.833 | 1.000 | PASS |
| m32 | Move-r-RLZ | 1468.92 |  |  | 8443.6 | 1.459 | 1.000 | PASS |
| m64 | Move-r-RLZ | 1468.92 |  |  | 14759.9 | 1.131 | 1.000 | PASS |
| m128 | Move-r-RLZ | 1468.92 |  |  | 25173.5 | 1.061 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 1418.30 |  |  | 75529187.0 | 3.741 | 0.105 | PASS |
| m8 | Move-r-RLZEnd | 1418.30 |  |  | 403350.3 | 3.505 | 0.122 | PASS |
| m16 | Move-r-RLZEnd | 1418.30 |  |  | 7454.3 | 1.851 | 0.654 | PASS |
| m32 | Move-r-RLZEnd | 1418.30 |  |  | 10272.7 | 1.199 | 0.822 | PASS |
| m64 | Move-r-RLZEnd | 1418.30 |  |  | 15911.7 | 1.049 | 0.928 | PASS |
| m128 | Move-r-RLZEnd | 1418.30 |  |  | 26922.9 | 0.992 | 0.935 | PASS |
| m4 | r-RLZSA@25% | 1479.24 | 32 | 27.3 | 212255083.8 | 1.331 | 0.037 | PASS |
| m8 | r-RLZSA@25% | 1479.24 | 32 | 29.4 | 994781.1 | 1.421 | 0.049 | PASS |
| m16 | r-RLZSA@25% | 1479.24 | 32 | 11.8 | 14364.9 | 0.961 | 0.339 | PASS |
| m32 | r-RLZSA@25% | 1479.24 | 32 | 9.9 | 14510.7 | 0.849 | 0.582 | PASS |
| m64 | r-RLZSA@25% | 1479.24 | 32 | 5.5 | 18090.4 | 0.922 | 0.816 | PASS |
| m128 | r-RLZSA@25% | 1479.24 | 32 | 0.0 | 29803.8 | 0.896 | 0.845 | PASS |
| m4 | r-RLZSA@12.5% | 1354.12 | 64 | 18.2 | 235139286.8 | 1.202 | 0.034 | PASS |
| m8 | r-RLZSA@12.5% | 1354.12 | 64 | 15.2 | 1172045.3 | 1.206 | 0.042 | PASS |
| m16 | r-RLZSA@12.5% | 1354.12 | 64 | 9.5 | 13900.2 | 0.993 | 0.350 | PASS |
| m32 | r-RLZSA@12.5% | 1354.12 | 64 | 1.3 | 13548.9 | 0.909 | 0.623 | PASS |
| m64 | r-RLZSA@12.5% | 1354.12 | 64 | 0.0 | 19504.9 | 0.856 | 0.757 | PASS |
| m128 | r-RLZSA@12.5% | 1354.12 | 64 | 0.0 | 30653.2 | 0.871 | 0.821 | PASS |

### english_200MB

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 1749.70 |  |  | 20569545.4 | 1.000 | 0.015 | PASS |
| m8 | Move-r | 1749.70 |  |  | 577426.6 | 1.000 | 0.024 | PASS |
| m16 | Move-r | 1749.70 |  |  | 8293.1 | 1.000 | 0.654 | PASS |
| m32 | Move-r | 1749.70 |  |  | 10750.3 | 1.000 | 0.915 | PASS |
| m64 | Move-r | 1749.70 |  |  | 17279.7 | 1.000 | 0.965 | PASS |
| m128 | Move-r | 1749.70 |  |  | 31463.9 | 1.000 | 0.993 | PASS |
| m4 | Move-r-RLZ | 1627.15 |  |  | 314969.1 | 65.307 | 1.000 | PASS |
| m8 | Move-r-RLZ | 1627.15 |  |  | 13841.3 | 41.718 | 1.000 | PASS |
| m16 | Move-r-RLZ | 1627.15 |  |  | 5420.3 | 1.530 | 1.000 | PASS |
| m32 | Move-r-RLZ | 1627.15 |  |  | 9834.0 | 1.093 | 1.000 | PASS |
| m64 | Move-r-RLZ | 1627.15 |  |  | 16668.8 | 1.037 | 1.000 | PASS |
| m128 | Move-r-RLZ | 1627.15 |  |  | 31245.2 | 1.007 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 1913.34 |  |  | 3078728.0 | 6.681 | 0.102 | PASS |
| m8 | Move-r-RLZEnd | 1913.34 |  |  | 96363.1 | 5.992 | 0.144 | PASS |
| m16 | Move-r-RLZEnd | 1913.34 |  |  | 6726.0 | 1.233 | 0.806 | PASS |
| m32 | Move-r-RLZEnd | 1913.34 |  |  | 11760.3 | 0.914 | 0.836 | PASS |
| m64 | Move-r-RLZEnd | 1913.34 |  |  | 18279.7 | 0.945 | 0.912 | PASS |
| m128 | Move-r-RLZEnd | 1913.34 |  |  | 31973.9 | 0.984 | 0.977 | PASS |
| m4 | r-RLZSA@25% | 1933.49 | 128 | 73.4 | 5205550.2 | 3.951 | 0.061 | PASS |
| m8 | r-RLZSA@25% | 1933.49 | 128 | 66.4 | 196907.1 | 2.932 | 0.070 | PASS |
| m16 | r-RLZSA@25% | 1933.49 | 128 | 22.2 | 10146.9 | 0.817 | 0.534 | PASS |
| m32 | r-RLZSA@25% | 1933.49 | 128 | 0.0 | 11563.7 | 0.930 | 0.850 | PASS |
| m64 | r-RLZSA@25% | 1933.49 | 128 | 85.5 | 19118.5 | 0.904 | 0.872 | PASS |
| m128 | r-RLZSA@25% | 1933.49 | 128 | 0.0 | 33164.5 | 0.949 | 0.942 | PASS |
| m4 | r-RLZSA@12.5% | 1835.22 | 0 | 69.2 | 5998638.2 | 3.429 | 0.053 | PASS |
| m8 | r-RLZSA@12.5% | 1835.22 | 0 | 58.5 | 239518.6 | 2.411 | 0.058 | PASS |
| m16 | r-RLZSA@12.5% | 1835.22 | 0 | 14.9 | 9830.3 | 0.844 | 0.551 | PASS |
| m32 | r-RLZSA@12.5% | 1835.22 | 0 | 0.0 | 12376.9 | 0.869 | 0.795 | PASS |
| m64 | r-RLZSA@12.5% | 1835.22 | 0 | 85.5 | 20337.0 | 0.850 | 0.820 | PASS |
| m128 | r-RLZSA@12.5% | 1835.22 | 0 | 0.0 | 35582.4 | 0.884 | 0.878 | PASS |

### einstein

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 6.76 |  |  | 1702114.1 | 1.000 | 0.159 | PASS |
| m8 | Move-r | 6.76 |  |  | 224529.2 | 1.000 | 0.171 | PASS |
| m16 | Move-r | 6.76 |  |  | 52419.9 | 1.000 | 0.225 | PASS |
| m32 | Move-r | 6.76 |  |  | 31509.4 | 1.000 | 0.255 | PASS |
| m64 | Move-r | 6.76 |  |  | 26140.6 | 1.000 | 0.340 | PASS |
| m128 | Move-r | 6.76 |  |  | 25404.1 | 1.000 | 0.375 | PASS |
| m4 | Move-r-RLZ | 21.85 |  |  | 270807.3 | 6.285 | 1.000 | PASS |
| m8 | Move-r-RLZ | 21.85 |  |  | 38439.4 | 5.841 | 1.000 | PASS |
| m16 | Move-r-RLZ | 21.85 |  |  | 11794.8 | 4.444 | 1.000 | PASS |
| m32 | Move-r-RLZ | 21.85 |  |  | 8032.7 | 3.923 | 1.000 | PASS |
| m64 | Move-r-RLZ | 21.85 |  |  | 8899.0 | 2.937 | 1.000 | PASS |
| m128 | Move-r-RLZ | 21.85 |  |  | 9536.9 | 2.664 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 8.32 |  |  | 1785034.5 | 0.954 | 0.152 | PASS |
| m8 | Move-r-RLZEnd | 8.32 |  |  | 224178.1 | 1.002 | 0.171 | PASS |
| m16 | Move-r-RLZEnd | 8.32 |  |  | 52250.3 | 1.003 | 0.226 | PASS |
| m32 | Move-r-RLZEnd | 8.32 |  |  | 32229.3 | 0.978 | 0.249 | PASS |
| m64 | Move-r-RLZEnd | 8.32 |  |  | 27462.5 | 0.952 | 0.324 | PASS |
| m128 | Move-r-RLZEnd | 8.32 |  |  | 24668.3 | 1.030 | 0.387 | PASS |
| m4 | r-RLZSA@25% | 18.32 | 0 | 72.4 | 665631.1 | 2.557 | 0.407 | PASS |
| m8 | r-RLZSA@25% | 18.32 | 0 | 74.1 | 89136.2 | 2.519 | 0.431 | PASS |
| m16 | r-RLZSA@25% | 18.32 | 0 | 36.8 | 40586.1 | 1.292 | 0.291 | PASS |
| m32 | r-RLZSA@25% | 18.32 | 0 | 25.9 | 28545.7 | 1.104 | 0.281 | PASS |
| m64 | r-RLZSA@25% | 18.32 | 0 | 28.1 | 25087.3 | 1.042 | 0.355 | PASS |
| m128 | r-RLZSA@25% | 18.32 | 0 | 26.9 | 22018.6 | 1.154 | 0.433 | PASS |
| m4 | r-RLZSA@12.5% | 14.99 | 32 | 62.8 | 822710.9 | 2.069 | 0.329 | PASS |
| m8 | r-RLZSA@12.5% | 14.99 | 32 | 58.9 | 113399.2 | 1.980 | 0.339 | PASS |
| m16 | r-RLZSA@12.5% | 14.99 | 32 | 26.7 | 43860.4 | 1.195 | 0.269 | PASS |
| m32 | r-RLZSA@12.5% | 14.99 | 32 | 11.8 | 30220.9 | 1.043 | 0.266 | PASS |
| m64 | r-RLZSA@12.5% | 14.99 | 32 | 14.5 | 26129.0 | 1.000 | 0.341 | PASS |
| m128 | r-RLZSA@12.5% | 14.99 | 32 | 13.2 | 20949.7 | 1.213 | 0.455 | PASS |

### boost

| workload | method | index_MiB | theta_rlz | RLZSA_occ_coverage | ns_per_pattern_median | speedup_vs_Move_r | speed_relative_to_Move_r_RLZ | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| m4 | Move-r | 1.52 |  |  | 16424669.7 | 1.000 | 0.462 | PASS |
| m8 | Move-r | 1.52 |  |  | 6644372.9 | 1.000 | 0.460 | PASS |
| m16 | Move-r | 1.52 |  |  | 640858.9 | 1.000 | 0.459 | PASS |
| m32 | Move-r | 1.52 |  |  | 38662.5 | 1.000 | 0.474 | PASS |
| m64 | Move-r | 1.52 |  |  | 12768.5 | 1.000 | 0.517 | PASS |
| m128 | Move-r | 1.52 |  |  | 8017.8 | 1.000 | 0.659 | PASS |
| m4 | Move-r-RLZ | 5.39 |  |  | 7585136.0 | 2.165 | 1.000 | PASS |
| m8 | Move-r-RLZ | 5.39 |  |  | 3054530.1 | 2.175 | 1.000 | PASS |
| m16 | Move-r-RLZ | 5.39 |  |  | 294198.3 | 2.178 | 1.000 | PASS |
| m32 | Move-r-RLZ | 5.39 |  |  | 18315.1 | 2.111 | 1.000 | PASS |
| m64 | Move-r-RLZ | 5.39 |  |  | 6601.2 | 1.934 | 1.000 | PASS |
| m128 | Move-r-RLZ | 5.39 |  |  | 5281.2 | 1.518 | 1.000 | PASS |
| m4 | Move-r-RLZEnd | 1.91 |  |  | 30630643.0 | 0.536 | 0.248 | PASS |
| m8 | Move-r-RLZEnd | 1.91 |  |  | 12488158.1 | 0.532 | 0.245 | PASS |
| m16 | Move-r-RLZEnd | 1.91 |  |  | 1208841.8 | 0.530 | 0.243 | PASS |
| m32 | Move-r-RLZEnd | 1.91 |  |  | 75527.0 | 0.512 | 0.242 | PASS |
| m64 | Move-r-RLZEnd | 1.91 |  |  | 22270.5 | 0.573 | 0.296 | PASS |
| m128 | Move-r-RLZEnd | 1.91 |  |  | 13055.7 | 0.614 | 0.405 | PASS |
| m4 | r-RLZSA@25% | 7.01 | 256 | 98.1 | 4898820.3 | 3.353 | 1.548 | PASS |
| m8 | r-RLZSA@25% | 7.01 | 256 | 98.2 | 1979537.2 | 3.357 | 1.543 | PASS |
| m16 | r-RLZSA@25% | 7.01 | 256 | 95.4 | 207459.3 | 3.089 | 1.418 | PASS |
| m32 | r-RLZSA@25% | 7.01 | 256 | 62.0 | 23410.5 | 1.652 | 0.782 | PASS |
| m64 | r-RLZSA@25% | 7.01 | 256 | 26.1 | 11302.7 | 1.130 | 0.584 | PASS |
| m128 | r-RLZSA@25% | 7.01 | 256 | 23.9 | 8503.6 | 0.943 | 0.621 | PASS |
| m4 | r-RLZSA@12.5% | 4.60 | 256 | 89.5 | 6005624.0 | 2.735 | 1.263 | PASS |
| m8 | r-RLZSA@12.5% | 4.60 | 256 | 94.5 | 2146262.1 | 3.096 | 1.423 | PASS |
| m16 | r-RLZSA@12.5% | 4.60 | 256 | 87.1 | 243865.5 | 2.628 | 1.206 | PASS |
| m32 | r-RLZSA@12.5% | 4.60 | 256 | 24.0 | 32802.5 | 1.179 | 0.558 | PASS |
| m64 | r-RLZSA@12.5% | 4.60 | 256 | 10.3 | 12648.0 | 1.010 | 0.522 | PASS |
| m128 | r-RLZSA@12.5% | 4.60 | 256 | 9.4 | 8260.8 | 0.971 | 0.639 | PASS |

## High-Occurrence Workload Compact View

| dataset | workload | method | index_MiB | ns_per_pattern_median | speedup_vs_Move_r | RLZSA_occ_coverage | correctness |
| --- | --- | --- | --- | --- | --- | --- | --- |
| world_leaders | m4 | Move-r | 13.42 | 118986187.2 | 1.000 |  | PASS |
| world_leaders | m8 | Move-r | 13.42 | 96822277.6 | 1.000 |  | PASS |
| world_leaders | m4 | Move-r-RLZ | 26.26 | 10383456.9 | 11.459 |  | PASS |
| world_leaders | m8 | Move-r-RLZ | 26.26 | 8449807.4 | 11.459 |  | PASS |
| world_leaders | m4 | Move-r-RLZEnd | 16.43 | 97335298.4 | 1.222 |  | PASS |
| world_leaders | m8 | Move-r-RLZEnd | 16.43 | 79135770.0 | 1.223 |  | PASS |
| world_leaders | m4 | r-RLZSA@25% | 17.50 | 62819137.6 | 1.894 | 53.6 | PASS |
| world_leaders | m8 | r-RLZSA@25% | 17.50 | 47826382.6 | 2.024 | 56.9 | PASS |
| world_leaders | m4 | r-RLZSA@12.5% | 16.50 | 94326152.3 | 1.261 | 26.8 | PASS |
| world_leaders | m8 | r-RLZSA@12.5% | 16.50 | 74863433.0 | 1.293 | 28.4 | PASS |
| SARS-Cov-2_40 | m4 | Move-r | 3.32 | 1825495.4 | 1.000 |  | PASS |
| SARS-Cov-2_40 | m8 | Move-r | 3.32 | 115126.5 | 1.000 |  | PASS |
| SARS-Cov-2_40 | m4 | Move-r-RLZ | 8.55 | 236287.8 | 7.726 |  | PASS |
| SARS-Cov-2_40 | m8 | Move-r-RLZ | 8.55 | 24786.5 | 4.645 |  | PASS |
| SARS-Cov-2_40 | m4 | Move-r-RLZEnd | 4.31 | 1668597.2 | 1.094 |  | PASS |
| SARS-Cov-2_40 | m8 | Move-r-RLZEnd | 4.31 | 131786.9 | 0.874 |  | PASS |
| SARS-Cov-2_40 | m4 | r-RLZSA@25% | 7.56 | 1341310.7 | 1.361 | 35.8 | PASS |
| SARS-Cov-2_40 | m8 | r-RLZSA@25% | 7.56 | 34177.5 | 3.368 | 91.9 | PASS |
| SARS-Cov-2_40 | m4 | r-RLZSA@12.5% | 6.91 | 1586489.7 | 1.151 | 21.0 | PASS |
| SARS-Cov-2_40 | m8 | r-RLZSA@12.5% | 6.91 | 39673.2 | 2.902 | 89.9 | PASS |
| DNA | m4 | Move-r | 28.91 | 319373300.3 | 1.000 |  | PASS |
| DNA | m8 | Move-r | 28.91 | 194748263.9 | 1.000 |  | PASS |
| DNA | m4 | Move-r-RLZ | 192.54 | 32883074.9 | 9.712 |  | PASS |
| DNA | m8 | Move-r-RLZ | 192.54 | 18769994.2 | 10.376 |  | PASS |
| DNA | m4 | Move-r-RLZEnd | 55.40 | 238932835.1 | 1.337 |  | PASS |
| DNA | m8 | Move-r-RLZEnd | 55.40 | 136023965.8 | 1.432 |  | PASS |
| DNA | m4 | r-RLZSA@25% | 94.03 | 201473205.8 | 1.585 | 44.6 | PASS |
| DNA | m8 | r-RLZSA@25% | 94.03 | 107362442.3 | 1.814 | 60.1 | PASS |
| DNA | m4 | r-RLZSA@12.5% | 68.77 | 266018980.9 | 1.201 | 22.2 | PASS |
| DNA | m8 | r-RLZSA@12.5% | 68.77 | 142944114.3 | 1.362 | 33.4 | PASS |
| candida_auris | m4 | Move-r | 1206.74 | 282528797.2 | 1.000 |  | PASS |
| candida_auris | m8 | Move-r | 1206.74 | 1413705.7 | 1.000 |  | PASS |
| candida_auris | m4 | Move-r-RLZ | 1468.92 | 7953899.2 | 35.521 |  | PASS |
| candida_auris | m8 | Move-r-RLZ | 1468.92 | 49215.8 | 28.725 |  | PASS |
| candida_auris | m4 | Move-r-RLZEnd | 1418.30 | 75529187.0 | 3.741 |  | PASS |
| candida_auris | m8 | Move-r-RLZEnd | 1418.30 | 403350.3 | 3.505 |  | PASS |
| candida_auris | m4 | r-RLZSA@25% | 1479.24 | 212255083.8 | 1.331 | 27.3 | PASS |
| candida_auris | m8 | r-RLZSA@25% | 1479.24 | 994781.1 | 1.421 | 29.4 | PASS |
| candida_auris | m4 | r-RLZSA@12.5% | 1354.12 | 235139286.8 | 1.202 | 18.2 | PASS |
| candida_auris | m8 | r-RLZSA@12.5% | 1354.12 | 1172045.3 | 1.206 | 15.2 | PASS |
| english_200MB | m4 | Move-r | 1749.70 | 20569545.4 | 1.000 |  | PASS |
| english_200MB | m8 | Move-r | 1749.70 | 577426.6 | 1.000 |  | PASS |
| english_200MB | m4 | Move-r-RLZ | 1627.15 | 314969.1 | 65.307 |  | PASS |
| english_200MB | m8 | Move-r-RLZ | 1627.15 | 13841.3 | 41.718 |  | PASS |
| english_200MB | m4 | Move-r-RLZEnd | 1913.34 | 3078728.0 | 6.681 |  | PASS |
| english_200MB | m8 | Move-r-RLZEnd | 1913.34 | 96363.1 | 5.992 |  | PASS |
| english_200MB | m4 | r-RLZSA@25% | 1933.49 | 5205550.2 | 3.951 | 73.4 | PASS |
| english_200MB | m8 | r-RLZSA@25% | 1933.49 | 196907.1 | 2.932 | 66.4 | PASS |
| english_200MB | m4 | r-RLZSA@12.5% | 1835.22 | 5998638.2 | 3.429 | 69.2 | PASS |
| english_200MB | m8 | r-RLZSA@12.5% | 1835.22 | 239518.6 | 2.411 | 58.5 | PASS |
| einstein | m4 | Move-r | 6.76 | 1702114.1 | 1.000 |  | PASS |
| einstein | m8 | Move-r | 6.76 | 224529.2 | 1.000 |  | PASS |
| einstein | m4 | Move-r-RLZ | 21.85 | 270807.3 | 6.285 |  | PASS |
| einstein | m8 | Move-r-RLZ | 21.85 | 38439.4 | 5.841 |  | PASS |
| einstein | m4 | Move-r-RLZEnd | 8.32 | 1785034.5 | 0.954 |  | PASS |
| einstein | m8 | Move-r-RLZEnd | 8.32 | 224178.1 | 1.002 |  | PASS |
| einstein | m4 | r-RLZSA@25% | 18.32 | 665631.1 | 2.557 | 72.4 | PASS |
| einstein | m8 | r-RLZSA@25% | 18.32 | 89136.2 | 2.519 | 74.1 | PASS |
| einstein | m4 | r-RLZSA@12.5% | 14.99 | 822710.9 | 2.069 | 62.8 | PASS |
| einstein | m8 | r-RLZSA@12.5% | 14.99 | 113399.2 | 1.980 | 58.9 | PASS |
| boost | m4 | Move-r | 1.52 | 16424669.7 | 1.000 |  | PASS |
| boost | m8 | Move-r | 1.52 | 6644372.9 | 1.000 |  | PASS |
| boost | m4 | Move-r-RLZ | 5.39 | 7585136.0 | 2.165 |  | PASS |
| boost | m8 | Move-r-RLZ | 5.39 | 3054530.1 | 2.175 |  | PASS |
| boost | m4 | Move-r-RLZEnd | 1.91 | 30630643.0 | 0.536 |  | PASS |
| boost | m8 | Move-r-RLZEnd | 1.91 | 12488158.1 | 0.532 |  | PASS |
| boost | m4 | r-RLZSA@25% | 7.01 | 4898820.3 | 3.353 | 98.1 | PASS |
| boost | m8 | r-RLZSA@25% | 7.01 | 1979537.2 | 3.357 | 98.2 | PASS |
| boost | m4 | r-RLZSA@12.5% | 4.60 | 6005624.0 | 2.735 | 89.5 | PASS |
| boost | m8 | r-RLZSA@12.5% | 4.60 | 2146262.1 | 3.096 | 94.5 | PASS |
