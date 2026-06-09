# 当前算法实现说明

本文档整理当前代码中用于论文实验的算法实现。当前实现是在原始 Move-r 的基础上，围绕高 occurrence workload 下 Move-r 定位慢、完整 Move-r-RLZ 又可能占空间的问题，加入了“部分 RLZSA + Phi 回退”的自适应混合定位方案。实验中使用的五个方法分别是：

- `Move-r`：原始 Move-r locate 结构，使用 `locate_move`。
- `Move-r-RLZ`：完整 RLZSA locate 结构，使用 `locate_rlzsa`，也就是实验表中的 rlz-only。
- `Move-r-RLZEnd`：LZ-End 版本，使用 `locate_lzendsa`。
- `r-RLZSA@25%`：构建 RLZSA + Phi 混合索引，只为训练集中高 occ 覆盖最多的 25% SA block 保留局部 RLZSA 数据。
- `r-RLZSA@12.5%`：同上，但只保留 12.5% SA block 的局部 RLZSA 数据，空间更小。

主要代码位置：

- 构建入口：[cli/move-r/move-r-build.cpp](/home/bixlku/Move-r/cli/move-r/move-r-build.cpp)
- 查询入口：[cli/move-r/move-r-locate.cpp](/home/bixlku/Move-r/cli/move-r/move-r-locate.cpp)
- 核心索引结构与序列化：[include/move_r/move_r.hpp](/home/bixlku/Move-r/include/move_r/move_r.hpp)
- 查询、混合策略和部分 RLZSA 构建：[include/move_r/queries.cpp](/home/bixlku/Move-r/include/move_r/queries.cpp)
- 统一实验脚本：[measurements/run_selected_m4_128.py](/home/bixlku/Move-r/measurements/run_selected_m4_128.py)

## 一、原始 Move-r 基线

原始 Move-r 的核心思想是用 run-length BWT 上的 move data structure 表示 LF 映射，并用 Phi 相关结构恢复 suffix array 值。

当前 `Move-r` 方法构建参数为：

```bash
move-r-build -s locate_move -a 8 -p <threads> ...
```

主要结构包括：

- `_M_LF`：Move-r 的 LF move data structure，同时保存 `L'`。
- `_L_prev` / `_L_next`：byte alphabet 下用于在 `L'` block 中加速 rank/select 的辅助结构。
- `_M_Phi_m1`：`Phi^{-1}` 的 move data structure。
- `_SA_Phi_m1`：定位 `Phi^{-1}` 输出区间所需的 suffix array sample。

查询流程：

1. 对 pattern 执行 backward search，得到 SA 区间 `[b,e]`。
2. 通过 `init_phi_m1` 得到 `SA[b]`。
3. 从 `b+1` 到 `e` 反复执行 `_M_Phi_m1.move(s, s_)`，依次输出所有 occurrence。

这个方法空间相对紧，但当 `occ=e-b+1` 很大时，需要逐个 occurrence 做 Phi move，速度会随 occ 线性增长，因此在 m4/m8 这类高 occurrence workload 上会慢。

## 二、Move-r-RLZ

`Move-r-RLZ` 使用 `locate_rlzsa`，在 Move-r 的 backward search 基础上，对 differential suffix array `SA^d` 构建 RLZSA 编码。

构建参数为：

```bash
move-r-build -s locate_rlzsa -a 8 -p <threads> ...
```

主要结构包括：

- `_M_LF`、`_L_prev`、`_L_next`：仍负责 backward search。
- `_SA_s`、`_SA_s_`：用于从 BWT 区间边界恢复 SA 值。
- `_R`：RLZSA 的参考串，存储 differential SA 的参考序列。
- `_PT`：phrase type bit vector，区分 literal phrase 和 copy phrase。
- `_SCP_S`：copy phrase 起点 sample。
- `_CPL`：copy phrase length。
- `_SR`：copy phrase 在 `_R` 中的 source position。
- `_LP`：literal phrase 值。

查询流程：

1. backward search 得到 `[b,e]`。
2. 计算 `SA[b]`。
3. 如果 `b<e`，调用 `init_rlzsa` 初始化 RLZSA phrase 上下文。
4. 调用 `report_rlzsa_right` 顺序解码 `[b+1,e]` 的 differential SA，累加得到 occurrence。

完整 RLZSA 在高 occ 下通常很快，因为它可以顺序解码 differential SA；但它需要保存完整 `_R`、phrase type、source、length、literal 等结构，空间可能显著增加。

注意：实验中的 `Move-r-RLZ` 就是 rlz-only，不是“Move-r + RLZSA 全量叠加然后重复计算空间”的另一个算法名。代码层面它仍然保留 backward search 需要的 Move-r 结构，同时 locate 使用完整 RLZSA 解码。

## 三、Move-r-RLZEnd

`Move-r-RLZEnd` 使用 `locate_lzendsa`，对 differential suffix array 使用 LZ-End 编码。

构建参数为：

```bash
move-r-build -s locate_lzendsa -a 8 -p <threads> ...
```

主要结构包括：

- `_M_LF`、`_L_prev`、`_L_next`：backward search。
- `_SA_s`、`_SA_s_`：区间边界 SA 值。
- `_lzendsa`：LZ-End 编码的 differential SA。

查询流程：

1. backward search 得到 `[b,e]`。
2. 从区间右端恢复 `SA[e]`。
3. 调用 `_lzendsa.extract_deltas(b+1, e, ...)` 反向解码 differential SA，并输出 occurrence。

从实验看，RLZEnd 通常空间介于 Move-r 与完整 RLZSA 之间，但构建时间常常明显更高，尤其在较大的数据集上。

## 四、当前创新算法：Vr-RLZSA

当前论文创新点对应的主要算法是 `r-RLZSA@25%` 和 `r-RLZSA@12.5%`。核心思想是：

高 occ 查询慢的根源是某些 SA 区间非常大。完整 RLZSA 能加速这些区间，但完整保存 RLZSA 空间代价高。因此只为“训练 workload 中高 occ 查询经常覆盖的 SA block”保留局部 RLZSA 解码数据；其他 block 不保存 RLZSA，查询时回退到 Phi 枚举。

### 1. 构建入口

实验脚本中 Adaptive 方法调用：

```bash
move-r-build \
  -s locate_rlzsa \
  -hybrid \
  -hybrid-thr 32 \
  -hybrid-min-occ 2 \
  -hybrid-max-pattern 64 \
  -partial-rlzsa-adaptive 1024 <budget_blocks> <mixed_train.manifest> \
  -partial-train-thr 100 \
  ...
```

其中：

- `-hybrid`：额外构建 `_M_Phi_m1`，让 RLZSA 索引也能用 Phi 回退。
- `-partial-rlzsa-adaptive 1024 budget train_manifest`：以 1024 个 SA 位置为一个 block，根据训练 pattern 选择最多 `budget` 个增强 block。
- `budget` 对应数据集 SA block 数量的比例：
  - `r-RLZSA@25%`：`budget = ceil(num_blocks / 4)`
  - `r-RLZSA@12.5%`：`budget = ceil(num_blocks / 8)`
- `-partial-train-thr 100`：只有训练集中 occurrence 数量至少 100 的 pattern 才用于给 block 打分。

### 2. 训练 pattern 如何产生

实验脚本 `run_selected_m4_128.py` 对每个 m 值生成 1000 个 pattern：

1. 每个 `m in {4,8,16,32,64,128}` 生成 1000 个 pattern。
2. 固定随机种子 `20260530` 拆成 500 个 train 和 500 个 test。
3. train 再用种子 `20260531` 拆成：
   - `train-adaptive`：用于选择局部 RLZSA block。
   - `valid`：用于选择查询时的 theta。
4. `mixed_train.manifest` 中包含所有 m 的 `train-adaptive` 文件，使 block 选择能看到混合 workload。

大文件上如果原始 `gen-patterns` 工具失败，脚本会使用 Python fallback，从文本中随机 seek 并写 Pizza&Chili pattern 格式，保证大于 2GB 的文本也能生成 pattern。

### 3. 如何选择需要 RLZSA 的 block

核心函数是 `build_partial_rlzsa_adaptive`。

输入：

- `block_size`：当前实验固定为 1024。
- `budget_blocks`：最多增强多少个 block。
- `training_patterns`：训练 pattern manifest。
- `train_occ_threshold`：当前为 100。

算法步骤：

1. 计算 `num_blocks = ceil(n / block_size)`。
2. 初始化 `score[num_blocks] = 0`。
3. 遍历训练 pattern：
   - backward search 得到 SA 区间 `[b,e]`。
   - 若 pattern 不出现，跳过。
   - 若 `occ = e-b+1 < train_occ_threshold`，跳过。
   - 将 `[b,e]` 覆盖到的 SA block 找出来。
   - 对每个覆盖 block，将该 block 中被 `[b,e]` 覆盖的 occurrence 数加入 `score[block]`。
4. 按 `score` 从大到小排序所有 block，取前 `budget_blocks`。
5. 将选中的 block 排序后进入局部 RLZSA 序列化构建。

因此，Adaptive 的 block 不是均匀抽样，而是 workload-aware selection：训练集中越常被高 occ 查询覆盖的 SA block，越可能保留 RLZSA 解码数据。

### 4. 部分 RLZSA 如何裁剪

局部序列化逻辑在 `build_partial_rlzsa_from_blocks` 和 `build_partial_rlzsa_adaptive` 后半段中。

完整 RLZSA 本来有全局：

- `_R`
- `_PT`
- `_CPL`
- `_SR`
- `_LP`
- `_SCP_S`

部分 RLZSA 不再保留完整 phrase 序列，而是只保留被选中 SA block 里的 phrase 片段：

- `_partial_rlzsa_block_ids`：哪些 block 是增强 block。
- `_partial_rlzsa_offsets`：每个增强 block 对应的 partial phrase 起止位置。
- `_partial_rlzsa_copy_offsets`：每个增强 block 的 copy phrase 起止位置。
- `_partial_rlzsa_literal_offsets`：每个增强 block 的 literal 起止位置。
- `_partial_rlzsa_pt`：局部 phrase type。
- `_partial_rlzsa_cpl`：局部 copy length。
- `_partial_rlzsa_sr`：局部 copy source。
- `_partial_rlzsa_lp`：局部 literal。

更重要的是：当前实现已经不再保存全局 `_R`，而是构造局部参考串。

具体做法：

1. 对每个增强 block，找到其 SA 区间内对应的 differential SA 范围。
2. 扫描完整 RLZSA phrase，把和该 block 相交的 phrase 切成局部 chunk。
3. 对 copy phrase，记录其引用的 `_R` 区间 `[src, src+len)`。
4. 收集所有引用区间后排序、合并重叠区间。
5. 生成 `local_R`，只复制这些被局部 copy phrase 用到的 `_R` 片段。
6. 将 `_partial_rlzsa_sr` 从全局 `_R` 坐标改写为 `local_R` 坐标。
7. 用 `local_R` 替换 `_R`。

这一步是真正的“索引序列化层裁剪”：Adaptive 索引文件中只保存增强 block 所需的局部 reference，而不是完整 RLZSA reference。

### 5. 序列化格式

序列化位于 `move_r::serialize`。

对于 `locate_rlzsa`：

1. 写入基础元信息：`n, sigma, r, r_, a, ...`。
2. 写入 `_M_LF` 和 `L'` 辅助结构。
3. 写入 `z, z_l, z_c`。
4. 写入 `_SA_s, _SA_s_`。
5. 写入 `_R`。
6. 写入 `partial_rlzsa` 标志。
7. 如果 `partial_rlzsa == true`：
   - 写入 `partial_rlzsa_block_size`、`partial_rlzsa_gap`。
   - 写入 partial 数组长度。
   - 写入 `_partial_rlzsa_offsets`、`_partial_rlzsa_copy_offsets`、`_partial_rlzsa_literal_offsets`、`_partial_rlzsa_block_ids`、`_partial_rlzsa_pt`、`_partial_rlzsa_cpl`、`_partial_rlzsa_sr`、`_partial_rlzsa_lp`。
8. 如果 `partial_rlzsa == false`：
   - 写入完整 `_SCP_S, _CPL, _SR, _LP, _PT`。
9. 如果支持 backward search，还会写入 hybrid Phi 信息：
   - `has_hybrid_phi`
   - `r__`
   - `_M_Phi_m1`
   - hybrid cost model 参数
10. 写入 adaptive sample 信息。

加载 `load` 会按同样顺序读回；为了兼容旧索引，读 hybrid/adaptive 扩展字段前用 `offs_end` 判断是否还有剩余字段。

## 五、查询时如何分流

当前有两层查询分流逻辑：

1. 普通 hybrid cost model：`locate()` 内部调用 `prefer_phi_locate`。
2. 实验用 block hybrid：`locate_block_hybrid()`，这是当前 Adaptive 实验主要使用的查询路径。

### 1. 普通 hybrid cost model

`prefer_phi_locate(b,e,pattern_length)` 的逻辑：

1. 如果没有启用 hybrid 或没有 `_M_Phi_m1`，不能用 Phi。
2. 计算 `occ=e-b+1`。
3. 如果 `occ < hybrid_phi_min_occ`，不用 Phi。
4. 如果 `pattern_length > hybrid_phi_max_pattern`，不用 Phi。
5. 如果 `occ > hybrid_phi_threshold`，不用 Phi。
6. 估算：
   - `cost_phi = c_phi * occ`
   - `cost_rlz = c_rlz_init + c_rlz_phrase * crossed_phrases + c_rlz_decode * occ`
7. 如果 `cost_phi <= cost_rlz`，走 Phi；否则走 RLZSA。

当前实验构建 Adaptive 时保留了这套参数，但 final test 中 Adaptive 主要通过 `-block-hybrid` 走 block-level 分流。

### 2. Block Hybrid 查询

实验脚本对 Adaptive 方法调用 locate 时使用：

```bash
move-r-locate \
  -block-hybrid 1024 <gap> 100 \
  -block-hybrid-rlz-thr <theta> \
  -m <result> <text_name> <index> <patterns>
```

其中：

- `block_size=1024`
- `gap=4` 对应 r-RLZSA@25%
- `gap=8` 对应 r-RLZSA@12.5%
- `occ_threshold=100`
- `theta` 由 validation 自动选择，候选集合为 `{0,32,64,128,256,512,1024}`

`locate_block_hybrid` 流程：

1. backward search 得到 `[b,e]`。
2. 计算 `occ=e-b+1`。
3. 如果 `occ < occ_threshold`，整段走 Phi。
4. 如果设置了 `theta_rlz`：
   - 先扫描 `[b,e]` 覆盖的 block。
   - 统计其中落在增强 block 的 occurrence 数 `occ_rlz`。
   - 若 `occ_rlz < theta_rlz`，整段走 Phi。
5. 否则开始逐 block 输出：
   - 若当前 block 是增强 block，走 RLZSA block 解码。
   - 若不是增强 block，走 Phi 枚举。
6. 统计：
   - `block_hybrid_phi_blocks`
   - `block_hybrid_rlzsa_blocks`
   - `block_hybrid_phi_occurrences`
   - `block_hybrid_rlzsa_occurrences`

对于 partial RLZSA 索引，判断增强 block 的方法不是 `block % gap == 0`，而是在 `_partial_rlzsa_block_ids` 中二分查找该 block 是否被训练选择。

### 3. 增强 block 如何解码

如果当前 block 是增强 block：

1. 当前 block 第一个 occurrence 的 SA 值仍由 Phi 初始化得到。
2. 对 block 内后续位置：
   - 从 `_partial_rlzsa_offsets[enhanced_id]` 找到该 block 的局部 phrase 范围。
   - 对 literal phrase：`s += _partial_rlzsa_lp[lp]; s -= n`。
   - 对 copy phrase：从局部 `_R` 的 `_partial_rlzsa_sr[cp]` 开始读 differential 值，逐个累加。
3. 输出每个 SA 值。

如果当前 block 不是增强 block：

1. 使用 `_M_Phi_m1` 初始化当前 SA 值所在的 Phi 区间。
2. 在 block 内反复 `M_Phi_m1().move(s, s_)`。

这就实现了“高价值 block 用 RLZSA 快速解码，其余 block 用 Phi 节省空间”的 tradeoff。

## 六、theta 的作用

`theta` 是查询时是否值得启动 block-level RLZSA 的二级门槛。

原因是：即使总 occ 很高，如果命中的 occurrence 大多落在非增强 block，那么 block hybrid 会频繁在 Phi 和 block 之间切换，收益不明显。因此代码提供 `-block-hybrid-rlz-thr theta`：

- `theta=0`：只要 `occ>=100`，就执行 block hybrid。
- `theta>0`：只有当前 query 在增强 block 中至少覆盖 `theta` 个 occurrence，才执行 block hybrid；否则直接 Phi。

实验脚本在 validation 集上扫描：

```python
THETAS = (0, 32, 64, 128, 256, 512, 1024)
```

对 r-RLZSA@25% 和 r-RLZSA@12.5% 分别选一个全局 theta，使 6 个 workload 的 validation 总时间中位数最小。

## 七、实验脚本如何组织五方法对比

统一脚本是 `measurements/run_selected_m4_128.py`。

### 1. 构建五个索引

脚本中的 `methods()` 定义：

- `Move-r` -> `locate_move`
- `Move-r-RLZ` -> `locate_rlzsa`
- `Move-r-RLZEnd` -> `locate_lzendsa`
- `r-RLZSA@25%` -> `locate_rlzsa + hybrid + partial_rlzsa_adaptive`
- `r-RLZSA@12.5%` -> `locate_rlzsa + hybrid + partial_rlzsa_adaptive`

Adaptive 的 budget 根据文本大小动态计算：

```text
num_blocks = ceil((text_size + 1) / 1024)
r-RLZSA@25% budget = ceil(num_blocks / 4)
r-RLZSA@12.5% budget = ceil(num_blocks / 8)
```

### 2. Validation

脚本先跑 Move-r baseline validation，然后对两个 Adaptive 索引扫描 theta：

- 每个 `m` 的 valid pattern 数为 100。
- 每个 theta、每个 m、重复 `repeats` 次。
- 记录每个 theta 下：
  - `ns_per_pattern_median`
  - `speedup_vs_move_r`
  - `rlzsa_occ_coverage`
  - `mixed_total_time_median_ns`
  - `selected_global_theta`

输出文件：

- `validation_theta_scan.csv`
- `selected_theta.txt`

### 3. Final Test

固定 validation 选出的 theta 后，脚本对五个方法跑 test set：

- 每个 `m` 的 test pattern 数为 500。
- 默认每个 workload 重复 5 次。
- 用中位数作为最终 `ns_per_pattern_median`。
- 正确性通过和 Move-r baseline 比较：
  - `num_patterns`
  - `num_occurrences`
  - `occurrence_checksum`

输出：

- `final_test_performance.csv`
- `build_summary_selected.csv`
- `workload_occ_stats.csv`
- `final_test_summary.md`

## 八、输出字段含义

`final_test_performance.csv` 中关键字段：

- `index_MiB`：索引文件大小。
- `move_r_base_MiB`：基础 Move-r 结构大小估算。
- `reference_MiB`：`_R` reference 大小。Adaptive 中这里是局部 reference。
- `partial_data_MiB`：partial RLZSA 附加数组大小。
- `enhanced_blocks`：增强 block 数量。
- `theta_rlz`：validation 选出的 `theta`。
- `RLZSA_occ_coverage`：final test 中由 RLZSA block 处理的 occurrence 占比。
- `ns_per_pattern_median`：每个 pattern 的 locate 时间中位数。
- `speedup_vs_Move_r`：相对 Move-r 的速度比。
- `speed_relative_to_Move_r_RLZ`：相对 Move-r-RLZ 的速度比。
- `correctness`：是否与 Move-r 的 occurrence 数和 checksum 一致。

`workload_occ_stats.csv` 中关键字段：

- `occ_avg`
- `occ_min`
- `occ_median`
- `occ_max`

这些用于解释为什么 m4/m8 通常是高 occ，为什么 Move-r 在这些 workload 上慢，以及 Adaptive/RLZSA 的收益主要来自哪里。

## 九、当前算法的设计取舍

当前 Adaptive 算法的目标不是完全替代 Move-r-RLZ，而是在空间和速度之间做折中。

优点：

- 相比完整 RLZSA，可以通过裁剪 `_R` 和 phrase 序列降低空间。
- 对训练 workload 中高 occ 查询覆盖的 block，保留 RLZSA 解码能力。
- 对未增强 block 使用 Phi，不需要保存完整 RLZSA。
- 通过 validation 自动选择 `theta`，避免在 RLZSA 覆盖很低的查询上强行走 block hybrid。
- 输出了 Phi/RLZSA occurrence 覆盖率，可直接用于论文分析。

局限：

- 如果高 occ 查询覆盖范围很分散，而 budget 不够，RLZSA 覆盖率会低，速度收益有限。
- 如果完整 Move-r-RLZ 在某些数据集上本身比 Move-r 更小，Adaptive 空间优势不一定成立，因为 Adaptive 还保留了 hybrid Phi 结构。
- 当前 block size 固定为 1024，没有自动调参。
- block 选择基于训练 pattern，若训练和测试 workload 分布不一致，效果会下降。
- 对低 occ workload，Adaptive 可能比 Move-r 或 Move-r-RLZ 更慢，因为分流判断和额外结构带来开销。

## 十、论文中可表述的算法流程

可以将当前方法描述为：

1. 构建 Move-r-RLZ 索引，并额外保留 Phi locate 所需的 `_M_Phi_m1`。
2. 将 suffix array 区间按固定 block size 划分。
3. 使用训练查询集合，对高 occurrence 查询覆盖的 SA block 进行打分。
4. 在给定空间预算下，选择得分最高的一部分 block。
5. 对选中 block，从完整 RLZSA phrase 中截取局部 phrase，并只保留这些 phrase 需要的局部 reference string。
6. 查询时先 backward search 得到 SA 区间：
   - 低 occ 查询直接使用 Phi。
   - 高 occ 查询先判断增强 block 覆盖是否达到 theta。
   - 达到 theta 后按 block 混合定位：增强 block 用局部 RLZSA，普通 block 用 Phi。
7. 通过 validation 自动选择 theta，并在 test set 上报告速度、空间、正确性和 RLZSA 覆盖率。

这就是当前代码中实现的自适应部分 RLZSA 混合定位算法。
