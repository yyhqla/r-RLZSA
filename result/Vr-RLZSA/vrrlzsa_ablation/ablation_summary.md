# Vr-RLZSA Ablation Summary

## Space

| dataset | budget | method | index MiB | saving MiB | saving % |
|---|---:|---|---:|---:|---:|
| boost | 25 | r-RLZSA | 7.055 | 0.000 | 0.00 |
| boost | 25 | Vr-RLZSA-v1 | 6.150 | 0.905 | 12.83 |
| boost | 25 | Vr-RLZSA-v2 | 4.522 | 2.533 | 35.90 |
| boost | 12.5 | r-RLZSA | 4.748 | 0.000 | 0.00 |
| boost | 12.5 | Vr-RLZSA-v1 | 4.305 | 0.442 | 9.32 |
| boost | 12.5 | Vr-RLZSA-v2 | 3.499 | 1.248 | 26.29 |
| einstein.en.txt | 25 | r-RLZSA | 18.869 | 0.000 | 0.00 |
| einstein.en.txt | 25 | Vr-RLZSA-v1 | 17.997 | 0.872 | 4.62 |
| einstein.en.txt | 25 | Vr-RLZSA-v2 | 16.147 | 2.722 | 14.43 |
| einstein.en.txt | 12.5 | r-RLZSA | 15.238 | 0.000 | 0.00 |
| einstein.en.txt | 12.5 | Vr-RLZSA-v1 | 14.803 | 0.435 | 2.85 |
| einstein.en.txt | 12.5 | Vr-RLZSA-v2 | 13.919 | 1.319 | 8.66 |
| SARS-Cov-2_40 | 25 | r-RLZSA | 7.562 | 0.000 | 0.00 |
| SARS-Cov-2_40 | 25 | Vr-RLZSA-v1 | 7.331 | 0.231 | 3.06 |
| SARS-Cov-2_40 | 25 | Vr-RLZSA-v2 | 6.925 | 0.637 | 8.43 |
| SARS-Cov-2_40 | 12.5 | r-RLZSA | 6.812 | 0.000 | 0.00 |
| SARS-Cov-2_40 | 12.5 | Vr-RLZSA-v1 | 6.673 | 0.139 | 2.03 |
| SARS-Cov-2_40 | 12.5 | Vr-RLZSA-v2 | 6.450 | 0.362 | 5.32 |
| world_leaders | 25 | r-RLZSA | 16.630 | 0.000 | 0.00 |
| world_leaders | 25 | Vr-RLZSA-v1 | 16.472 | 0.157 | 0.95 |
| world_leaders | 25 | Vr-RLZSA-v2 | 16.187 | 0.442 | 2.66 |
| world_leaders | 12.5 | r-RLZSA | 15.965 | 0.000 | 0.00 |
| world_leaders | 12.5 | Vr-RLZSA-v1 | 15.889 | 0.076 | 0.48 |
| world_leaders | 12.5 | Vr-RLZSA-v2 | 15.749 | 0.216 | 1.36 |
| DNA | 25 | r-RLZSA | 94.139 | 0.000 | 0.00 |
| DNA | 25 | Vr-RLZSA-v1 | 89.304 | 4.835 | 5.14 |
| DNA | 25 | Vr-RLZSA-v2 | 75.902 | 18.237 | 19.37 |
| DNA | 12.5 | r-RLZSA | 69.176 | 0.000 | 0.00 |
| DNA | 12.5 | Vr-RLZSA-v1 | 66.714 | 2.461 | 3.56 |
| DNA | 12.5 | Vr-RLZSA-v2 | 59.917 | 9.259 | 13.38 |

## Correctness

All ablation query checksums PASS.

## Field Contribution Hint

- DNA @25%: v1 fields save 4.835 MiB; v2 extra fields save 13.402 MiB.
- DNA @12.5%: v1 fields save 2.461 MiB; v2 extra fields save 6.797 MiB.
- SARS-Cov-2_40 @25%: v1 fields save 0.231 MiB; v2 extra fields save 0.406 MiB.
- SARS-Cov-2_40 @12.5%: v1 fields save 0.139 MiB; v2 extra fields save 0.224 MiB.
- boost @25%: v1 fields save 0.905 MiB; v2 extra fields save 1.627 MiB.
- boost @12.5%: v1 fields save 0.442 MiB; v2 extra fields save 0.806 MiB.
- einstein.en.txt @25%: v1 fields save 0.872 MiB; v2 extra fields save 1.850 MiB.
- einstein.en.txt @12.5%: v1 fields save 0.435 MiB; v2 extra fields save 0.884 MiB.
- world_leaders @25%: v1 fields save 0.158 MiB; v2 extra fields save 0.285 MiB.
- world_leaders @12.5%: v1 fields save 0.076 MiB; v2 extra fields save 0.140 MiB.
