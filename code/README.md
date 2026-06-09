# 代码包

本目录保留相关算法代码和实验脚本，并维持它们原始的相对路径。

## 核心文件

- `include/move_r/move_r.hpp`
- `include/move_r/queries.cpp`
- `include/move_r/construction/*.cpp`
- `include/lzendsa/lzendsa_encoding.hpp`
- `cli/move-r/move-r-build.cpp`
- `cli/move-r/move-r-locate.cpp`
- `cli/move-r/move-r-partial-codec.cpp`
- `cli/move-r/move-r-dump-fields.cpp`
- `CMakeLists.txt`

## 实验脚本

- `measurements/run_selected_m4_128.py`：r-RLZSA 跨数据集实验。
- `measurements/run_innovation2_varint_v2_cross_dataset.py`：Vr-RLZSA
  跨数据集实验。
- `measurements/run_vrrlzsa_ablation.py`：Vr-RLZSA 消融实验。
- `measurements/dump_vrrlzsa_field_frequency.py`：字段频率导出。
- `dataset-calculate/dataset-cal.py`：数据集统计辅助脚本。

这是筛选后的代码子集，不能作为完整 Move-r 仓库的独立替代版本。若要从干净克隆的仓库中复现实验，请将这些文件复制到完整项目中相同的相对路径下。
