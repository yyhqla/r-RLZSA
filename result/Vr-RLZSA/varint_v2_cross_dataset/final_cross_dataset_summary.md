# Varint-v2 Cross-Dataset Summary

## Raw vs Varint-v2 Space

| dataset | pair | raw MiB | v2 MiB | saving MiB | saving % |
|---|---|---:|---:|---:|---:|
| DNA | Adaptive-Mixed@25%-raw -> Adaptive-Mixed@25%-varint-v2 | 94.139 | 75.902 | 18.237 | 19.37 |
| DNA | Adaptive-Mixed@12.5%-raw -> Adaptive-Mixed@12.5%-varint-v2 | 69.176 | 59.917 | 9.259 | 13.38 |
| SARS-Cov-2_40 | Adaptive-Mixed@25%-raw -> Adaptive-Mixed@25%-varint-v2 | 7.562 | 6.925 | 0.637 | 8.43 |
| SARS-Cov-2_40 | Adaptive-Mixed@12.5%-raw -> Adaptive-Mixed@12.5%-varint-v2 | 6.812 | 6.450 | 0.362 | 5.32 |
| boost | Adaptive-Mixed@25%-raw -> Adaptive-Mixed@25%-varint-v2 | 7.055 | 4.522 | 2.533 | 35.90 |
| boost | Adaptive-Mixed@12.5%-raw -> Adaptive-Mixed@12.5%-varint-v2 | 4.748 | 3.499 | 1.248 | 26.29 |
| einstein.en.txt | Adaptive-Mixed@25%-raw -> Adaptive-Mixed@25%-varint-v2 | 18.869 | 16.147 | 2.722 | 14.43 |
| einstein.en.txt | Adaptive-Mixed@12.5%-raw -> Adaptive-Mixed@12.5%-varint-v2 | 15.238 | 13.919 | 1.319 | 8.66 |
| world_leaders | Adaptive-Mixed@25%-raw -> Adaptive-Mixed@25%-varint-v2 | 16.630 | 16.187 | 0.442 | 2.66 |
| world_leaders | Adaptive-Mixed@12.5%-raw -> Adaptive-Mixed@12.5%-varint-v2 | 15.965 | 15.749 | 0.216 | 1.36 |

## Correctness

All query checksums PASS.
