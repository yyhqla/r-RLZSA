# Vr-RLZSA: Vr-RLZSA Experiments

## Algorithms

- `Move-r`: original Phi-based locate.
- `Move-r-RLZ`: full RLZSA locate, also called rlz-only in earlier notes.
- `Move-r-RLZEnd`: LZ-End encoding for differential suffix array locate.
- `r-RLZSA@25%`: partial RLZSA keeps the highest-scored 25% of SA blocks.
- `r-RLZSA@12.5%`: partial RLZSA keeps the highest-scored 12.5% of SA blocks.

## Current Implementation

The adaptive index is implemented as a workload-aware hybrid:

1. Generate train/validation/test patterns for each workload.
2. Run backward search on training patterns.
3. Score SA blocks by how many high-occurrence query positions they cover.
4. Keep only the top-scored blocks within the configured budget.
5. Serialize RLZSA data only for those blocks.
6. Use local RLZSA decoding when a query interval falls in an enhanced block;
   otherwise fall back to Phi locate.

The current serializer also supports compressed partial RLZSA fields:

- `raw`: legacy partial fields.
- `varint-v1`: varbyte block id gaps and SR delta coding.
- `varint-v2`: v1 plus offset delta coding, PT bit packing, and CPL varbyte
  coding.

The recommended public experimental variant is `varint-v2`.

## Main Code Paths

- `include/move_r/move_r.hpp`
  - partial RLZSA metadata
  - raw/v1/v2 serialization
  - compressed field size accounting
  - field-frequency dump helpers
- `include/move_r/queries.cpp`
  - adaptive block scoring
  - partial RLZSA construction
  - local reference construction
  - hybrid query fallback
- `cli/move-r/move-r-build.cpp`
  - CLI flags for Vr-RLZSA and partial codec selection
- `cli/move-r/move-r-partial-codec.cpp`
  - offline conversion among raw, varint-v1, and varint-v2
- `cli/move-r/move-r-dump-fields.cpp`
  - SR/CPL field-frequency export for plots

## Important CLI Flags

```text
-hybrid
-hybrid-thr <occ_threshold>
-hybrid-min-occ <min_occ>
-hybrid-max-pattern <length>
-partial-rlzsa-adaptive <block_size> <budget_blocks> <train_manifest>
-partial-train-thr <occ_threshold>
-partial-codec raw|varint|varint-v1|varint-v2
-partial-varint
-partial-varint-v2
-partial-field-stats <distribution.csv> <saving.csv> <label>
```

Typical r-RLZSA@25% build:

```bash
build/cli/move-r/move-r-build \
  -s locate_rlzsa \
  -hybrid \
  -hybrid-thr 32 \
  -hybrid-min-occ 2 \
  -hybrid-max-pattern 64 \
  -partial-rlzsa-adaptive 1024 <budget_blocks> <mixed_train.manifest> \
  -partial-train-thr 100 \
  -partial-codec varint-v2 \
  -p <threads> \
  <text> <index>
```

## Result Files

Compact public outputs:

```text
paper_results/innovation2/
```

Full local outputs:

```text
measurements/results/
```

The full local output directory is intentionally ignored by git.
