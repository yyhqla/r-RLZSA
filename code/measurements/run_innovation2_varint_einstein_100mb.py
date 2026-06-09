#!/usr/bin/env python3
from __future__ import annotations

import csv
import shlex
import statistics
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEXT = ROOT / "measurements" / "texts" / "einstein_100MB"
TEXT_NAME = "einstein_100MB"
PATTERN_DIR = ROOT / "measurements" / "patterns" / "r_rlzsa_einstein_100MB"
RESULT_DIR = ROOT / "measurements" / "results" / "varint_v2_einstein_100MB"
INDEX_DIR = ROOT / "measurements" / "indexes" / "varint_v2_einstein_100MB"
BUILD = ROOT / "build" / "cli" / "move-r-build"
LOCATE = ROOT / "build" / "cli" / "move-r-locate"


METHODS = [
    {
        "method": "Move-r",
        "slug": "move-r",
        "support": "locate_move",
        "kind": "move",
        "gap": 0,
        "budget": 0,
        "theta": 0,
        "codec": "raw",
    },
    {
        "method": "r-RLZSA@25%",
        "slug": "adaptive25-raw",
        "support": "locate_rlzsa",
        "kind": "adaptive",
        "gap": 4,
        "budget": 25601,
        "theta": 128,
        "codec": "raw",
    },
    {
        "method": "Vr-RLZSA-v1@25%",
        "slug": "adaptive25-varint-v1",
        "support": "locate_rlzsa",
        "kind": "adaptive",
        "gap": 4,
        "budget": 25601,
        "theta": 128,
        "codec": "varint-v1",
    },
    {
        "method": "Vr-RLZSA@25%",
        "slug": "adaptive25-varint-v2",
        "support": "locate_rlzsa",
        "kind": "adaptive",
        "gap": 4,
        "budget": 25601,
        "theta": 128,
        "codec": "varint-v2",
    },
    {
        "method": "r-RLZSA@12.5%",
        "slug": "adaptive12-raw",
        "support": "locate_rlzsa",
        "kind": "adaptive",
        "gap": 8,
        "budget": 12801,
        "theta": 64,
        "codec": "raw",
    },
    {
        "method": "Vr-RLZSA-v1@12.5%",
        "slug": "adaptive12-varint-v1",
        "support": "locate_rlzsa",
        "kind": "adaptive",
        "gap": 8,
        "budget": 12801,
        "theta": 64,
        "codec": "varint-v1",
    },
    {
        "method": "Vr-RLZSA@12.5%",
        "slug": "adaptive12-varint-v2",
        "support": "locate_rlzsa",
        "kind": "adaptive",
        "gap": 8,
        "budget": 12801,
        "theta": 64,
        "codec": "varint-v2",
    },
]


def run(cmd: list[str], log: Path) -> float:
    log.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with log.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(shlex.quote(x) for x in cmd) + "\n\n")
        out.flush()
        proc = subprocess.run(cmd, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, text=True)
    elapsed = time.perf_counter() - start
    if proc.returncode != 0:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(cmd)}\nlog: {log}")
    return elapsed


def parse_result(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    if not path.exists():
        return rows
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith("RESULT"):
            continue
        row: dict[str, str] = {}
        for token in line.split()[1:]:
            if "=" in token:
                key, value = token.split("=", 1)
                row[key] = value
        rows.append(row)
    return rows


def intval(row: dict[str, str], key: str) -> int:
    try:
        return int(row.get(key, "0"))
    except ValueError:
        return 0


def median(values: list[float]) -> float:
    return statistics.median(values) if values else 0.0


def mib(value: int) -> float:
    return value / 1024 / 1024


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    fields: list[str] = []
    for row in rows:
        for key in row:
            if key not in fields:
                fields.append(key)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def index_path(slug: str, support: str) -> Path:
    suffix = ".move-r-rlzsa" if support == "locate_rlzsa" else ".move-r"
    return Path(str(INDEX_DIR / f"{TEXT_NAME}.a8.{slug}") + suffix)


def prepare_manifests() -> tuple[Path, Path]:
    RESULT_DIR.mkdir(parents=True, exist_ok=True)
    INDEX_DIR.mkdir(parents=True, exist_ok=True)
    train = RESULT_DIR / "mixed_train.local.manifest"
    valid = RESULT_DIR / "mixed_valid.local.manifest"
    train.write_text(
        "# mixed_patterns_manifest=1 file=einstein_100MB local_paths=1\n"
        + str((PATTERN_DIR / f"{TEXT_NAME}-m8-train400.patterns").resolve()) + "\n"
        + str((PATTERN_DIR / f"{TEXT_NAME}-m100-train400.patterns").resolve()) + "\n"
        + str((PATTERN_DIR / f"{TEXT_NAME}-m200-train400.patterns").resolve()) + "\n",
        encoding="utf-8",
    )
    valid.write_text(
        "# mixed_patterns_manifest=1 file=einstein_100MB local_paths=1\n"
        + str((PATTERN_DIR / f"{TEXT_NAME}-m8-valid100.patterns").resolve()) + "\n"
        + str((PATTERN_DIR / f"{TEXT_NAME}-m100-valid100.patterns").resolve()) + "\n"
        + str((PATTERN_DIR / f"{TEXT_NAME}-m200-valid100.patterns").resolve()) + "\n",
        encoding="utf-8",
    )
    return train, valid


def build_indexes(threads: int, rebuild: bool) -> list[dict[str, object]]:
    train_manifest, _ = prepare_manifests()
    dist_csv = RESULT_DIR / "field_distribution.csv"
    saving_csv = RESULT_DIR / "estimated_space_saving.csv"
    if rebuild:
        dist_csv.unlink(missing_ok=True)
        saving_csv.unlink(missing_ok=True)

    rows: list[dict[str, object]] = []
    for method in METHODS:
        idx = index_path(method["slug"], method["support"])
        if method["codec"] in ("varint-v1", "varint-v2") and method["kind"] == "adaptive":
            rows.append({
                "method": method["method"],
                "codec": method["codec"],
                "index_file": str(idx),
                "index_file_bytes": idx.stat().st_size if idx.exists() else 0,
                "index_file_mib": f"{mib(idx.stat().st_size):.3f}" if idx.exists() else "0.000",
                "build_wall_seconds": "0.000",
                "note": f"written as {method['codec']} copy from the matching raw build",
            })
            continue
        if rebuild or not idx.exists():
            prefix = INDEX_DIR / f"{TEXT_NAME}.a8.{method['slug']}"
            m_idx = RESULT_DIR / f"build-{method['slug']}.result"
            m_mds = RESULT_DIR / f"build-{method['slug']}.mds"
            m_idx.unlink(missing_ok=True)
            cmd = [
                str(BUILD),
                "-s", str(method["support"]),
                "-p", str(threads),
                "-a", "8",
                "-o", str(prefix),
                "-m_idx", str(m_idx),
                "-m_mds", str(m_mds),
            ]
            if method["kind"] == "adaptive":
                cmd += [
                    "-hybrid",
                    "-hybrid-thr", "32",
                    "-hybrid-min-occ", "2",
                    "-hybrid-max-pattern", "64",
                    "-partial-rlzsa-adaptive", "1024", str(method["budget"]), str(train_manifest),
                    "-partial-train-thr", "100",
                    "-partial-codec", str(method["codec"]),
                    "-partial-field-stats", str(dist_csv), str(saving_csv), str(method["method"]),
                ]
                if method["codec"] == "raw":
                    copy_slug_v1 = method["slug"].replace("-raw", "-varint-v1")
                    copy_slug_v2 = method["slug"].replace("-raw", "-varint-v2")
                    cmd += [
                        "-partial-varint-copy", str(INDEX_DIR / f"{TEXT_NAME}.a8.{copy_slug_v1}"),
                        "-partial-varint-v2-copy", str(INDEX_DIR / f"{TEXT_NAME}.a8.{copy_slug_v2}"),
                    ]
            elapsed = run(["/usr/bin/time", "-v"] + cmd + [str(TEXT)], RESULT_DIR / f"build-{method['slug']}.log")
        else:
            elapsed = 0.0
        rows.append({
            "method": method["method"],
            "codec": method["codec"],
            "index_file": str(idx),
            "index_file_bytes": idx.stat().st_size if idx.exists() else 0,
            "index_file_mib": f"{mib(idx.stat().st_size):.3f}" if idx.exists() else "0.000",
            "build_wall_seconds": f"{elapsed:.3f}",
        })
    write_csv(RESULT_DIR / "build_summary.csv", rows)
    return rows


def locate_all(repeats: int) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    query_rows: list[dict[str, object]] = []
    space_rows: list[dict[str, object]] = []
    move_median_by_m: dict[int, float] = {}
    component_seen: set[str] = set()

    for method in METHODS:
        idx = index_path(method["slug"], method["support"])
        for m in (8, 100, 200):
            result = RESULT_DIR / f"test-{method['slug']}-m{m}.result"
            result.unlink(missing_ok=True)
            patterns = PATTERN_DIR / f"{TEXT_NAME}-m{m}-test-n500.patterns"
            for rep in range(repeats):
                cmd = [str(LOCATE)]
                if method["kind"] == "adaptive":
                    cmd += [
                        "-block-hybrid", "1024", str(method["gap"]), "100",
                        "-block-hybrid-rlz-thr", str(method["theta"]),
                    ]
                cmd += ["-m", str(result), TEXT_NAME, str(idx), str(patterns)]
                run(cmd, RESULT_DIR / f"test-{method['slug']}-m{m}-rep{rep + 1}.log")
            rows = parse_result(result)
            ns_per_pattern = [intval(row, "time_locate") / max(1, intval(row, "num_patterns")) for row in rows]
            last = rows[-1]
            if method["slug"] == "move-r":
                move_median_by_m[m] = median(ns_per_pattern)
            base = move_median_by_m.get(m, 0.0)
            current = median(ns_per_pattern)
            phi_occ = intval(last, "block_hybrid_phi_occurrences")
            rlz_occ = intval(last, "block_hybrid_rlzsa_occurrences")
            query_rows.append({
                "method": method["method"],
                "codec": method["codec"],
                "m": m,
                "theta_rlz": method["theta"],
                "ns_per_pattern_min": f"{min(ns_per_pattern):.1f}",
                "ns_per_pattern_median": f"{current:.1f}",
                "ns_per_pattern_max": f"{max(ns_per_pattern):.1f}",
                "speedup_vs_move_r": f"{base / current:.3f}" if base and current else "1.000",
                "num_patterns": intval(last, "num_patterns"),
                "num_occurrences": intval(last, "num_occurrences"),
                "occurrence_checksum": intval(last, "occurrence_checksum"),
                "phi_occurrences": phi_occ,
                "rlzsa_occurrences": rlz_occ,
                "rlzsa_occ_percent": f"{100.0 * rlz_occ / (phi_occ + rlz_occ):.2f}" if phi_occ + rlz_occ else "0.00",
            })

            if method["slug"] not in component_seen:
                component_seen.add(method["slug"])
                size_keys = [
                    "size_index",
                    "size_r",
                    "size_partial_offsets",
                    "size_partial_copy_offsets",
                    "size_partial_literal_offsets",
                    "size_partial_block_ids",
                    "size_partial_block_ids_varint",
                    "size_partial_offsets_varint",
                    "size_partial_copy_offsets_varint",
                    "size_partial_literal_offsets_varint",
                    "size_partial_pt",
                    "size_partial_pt_bitvector",
                    "size_partial_cpl",
                    "size_partial_cpl_varint",
                    "size_partial_sr",
                    "size_partial_sr_varint",
                    "size_partial_lp",
                    "size_hybrid_m_phim1",
                    "size_m_phim1",
                    "size_sa_phim1",
                ]
                for key in size_keys:
                    if key in last:
                        space_rows.append({
                            "method": method["method"],
                            "codec": method["codec"],
                            "component": key,
                            "bytes": intval(last, key),
                            "MiB": f"{mib(intval(last, key)):.3f}",
                        })
                space_rows.append({
                    "method": method["method"],
                    "codec": method["codec"],
                    "component": "serialized_index_file",
                    "bytes": idx.stat().st_size,
                    "MiB": f"{mib(idx.stat().st_size):.3f}",
                })

    write_csv(RESULT_DIR / "query_summary.csv", query_rows)
    write_csv(RESULT_DIR / "space_breakdown.csv", space_rows)
    return query_rows, space_rows


def write_summary(build_rows: list[dict[str, object]], query_rows: list[dict[str, object]], space_rows: list[dict[str, object]]) -> None:
    by_component = {(row["method"], row["component"]): row for row in space_rows}
    raw25 = by_component.get(("r-RLZSA@25%", "serialized_index_file"), {})
    v1_25 = by_component.get(("Vr-RLZSA-v1@25%", "serialized_index_file"), {})
    v2_25 = by_component.get(("Vr-RLZSA@25%", "serialized_index_file"), {})
    raw12 = by_component.get(("r-RLZSA@12.5%", "serialized_index_file"), {})
    v1_12 = by_component.get(("Vr-RLZSA-v1@12.5%", "serialized_index_file"), {})
    v2_12 = by_component.get(("Vr-RLZSA@12.5%", "serialized_index_file"), {})

    def saving(raw: dict[str, object], var: dict[str, object]) -> str:
        rb = int(raw.get("bytes", 0) or 0)
        vb = int(var.get("bytes", 0) or 0)
        return f"{mib(rb):.3f} -> {mib(vb):.3f} MiB, save {mib(rb - vb):.3f} MiB ({100 * (rb - vb) / rb:.2f}%)" if rb else "n/a"

    lines = [
        "# Vr-RLZSA Varint-v2 Field Experiment: Einstein 100MB",
        "",
        f"- Text: `{TEXT}` ({TEXT.stat().st_size} bytes)",
        "- Methods: Move-r, r-RLZSA@25% raw/varint-v1/varint-v2, r-RLZSA@12.5% raw/varint-v1/varint-v2",
        "- Workloads: m=8,100,200, 500 test patterns each, repeated locate runs are summarized by min/median/max ns/pattern.",
        "- Varint-v1 compresses block-id gaps and SR zigzag deltas.",
        "- Varint-v2 additionally compresses phrase/copy/literal offsets with delta-varbyte, PT with bit packing, and CPL with value-varbyte.",
        "- Locate loads compressed fields back into the existing arrays, so routing semantics are unchanged.",
        "",
        "## Serialized Space",
        "",
        f"- r-RLZSA@25% v1: {saving(raw25, v1_25)}",
        f"- r-RLZSA@25% v2: {saving(raw25, v2_25)}",
        f"- r-RLZSA@12.5% v1: {saving(raw12, v1_12)}",
        f"- r-RLZSA@12.5% v2: {saving(raw12, v2_12)}",
        "",
        "## Output Files",
        "",
        "- `field_distribution.csv`: field value/gap distributions.",
        "- `estimated_space_saving.csv`: per-field varint/bitvector saving estimates.",
        "- `build_summary.csv`: build wall time and serialized index file sizes.",
        "- `space_breakdown.csv`: component-level space table from locate metadata.",
        "- `query_summary.csv`: speed, checksum, and Phi/RLZSA split per workload.",
        "",
    ]
    RESULT_DIR.joinpath("final_summary.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--threads", type=int, default=8)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--reuse", action="store_true")
    args = parser.parse_args()

    build_rows = build_indexes(args.threads, rebuild=not args.reuse)
    query_rows, space_rows = locate_all(args.repeats)
    write_summary(build_rows, query_rows, space_rows)
    print(f"results: {RESULT_DIR}")


if __name__ == "__main__":
    main()
