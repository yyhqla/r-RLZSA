#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import shlex
import statistics
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "measurements" / "results" / "varint_v2_cross_dataset"
SOURCE_INDEX_ROOT = ROOT / "measurements" / "indexes" / "varint_v2_cross_dataset"
SOURCE_PATTERN_ROOT = ROOT / "measurements" / "patterns" / "varint_v2_cross_dataset"
OUT_ROOT = ROOT / "measurements" / "results" / "vrrlzsa_ablation"
LOCATE = ROOT / "build" / "cli" / "move-r-locate"
CODEC = ROOT / "build" / "cli" / "move-r-partial-codec"
WORKLOADS = (8, 100, 200)
DATASET_DIRS = {
    "boost": ("varint_v2_boost", "boost"),
    "einstein.en.txt": ("varint_v2_einstein.en", "einstein.en.txt"),
    "SARS-Cov-2_40": ("varint_v2_SARS-Cov-2_40", "SARS-Cov-2_40"),
    "world_leaders": ("varint_v2_world_leaders", "world_leaders"),
    "DNA": ("varint_v2_DNA", "DNA"),
}


def run(cmd: list[object], log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(shlex.quote(str(x)) for x in cmd) + "\n\n")
        out.flush()
        proc = subprocess.run([str(x) for x in cmd], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, text=True)
    if proc.returncode:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(str(x) for x in cmd)}\nlog: {log}")


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


def index_path(index_dir: Path, text_name: str, slug: str) -> Path:
    return index_dir / f"{text_name}.a8.{slug}.move-r-rlzsa"


def ensure_v1_index(dataset: str, source_subdir: str, text_name: str) -> Path:
    index_dir = SOURCE_INDEX_ROOT / source_subdir
    raw25 = index_path(index_dir, text_name, "adaptive25-raw")
    raw12 = index_path(index_dir, text_name, "adaptive12-raw")
    out25 = index_path(index_dir, text_name, "adaptive25-varint-v1")
    out12 = index_path(index_dir, text_name, "adaptive12-varint-v1")
    for raw, out in ((raw25, out25), (raw12, out12)):
        if not raw.exists():
            raise FileNotFoundError(raw)
        if not out.exists() or out.stat().st_mtime < raw.stat().st_mtime:
            run([CODEC, "v1", raw, out], OUT_ROOT / dataset / f"convert-{out.stem}.log")
    return index_dir


def read_selected_theta(result_dir: Path) -> dict[str, int]:
    selected: dict[str, int] = {}
    for line in (result_dir / "selected_theta.txt").read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        key, value = line.split()
        selected[key] = int(value)
    selected["adaptive25-varint-v1"] = selected["adaptive25-raw"]
    selected["adaptive12-varint-v1"] = selected["adaptive12-raw"]
    return selected


def source_test_result(result_dir: Path, slug: str, m: int) -> Path:
    return result_dir / f"test-{slug}-m{m}.result"


def copy_or_run_query(dataset: str, result_dir: Path, pattern_dir: Path, index_dir: Path, text_name: str, slug: str, budget: str, theta: int, m: int, repeats: int) -> Path:
    out_dir = OUT_ROOT / dataset
    out_dir.mkdir(parents=True, exist_ok=True)
    result = out_dir / f"test-{slug}-m{m}.result"
    if result.exists() and len(parse_result(result)) >= repeats:
        return result
    result.unlink(missing_ok=True)
    idx = index_path(index_dir, text_name, slug)
    patterns = pattern_dir / f"{text_name}-m{m}-test-n500.patterns"
    gap = 4 if budget == "25" else 8
    for rep in range(repeats):
        run(
            [
                LOCATE,
                "-block-hybrid", "1024", gap, "100",
                "-block-hybrid-rlz-thr", theta,
                "-m", result,
                text_name,
                idx,
                patterns,
            ],
            out_dir / f"test-{slug}-m{m}-rep{rep + 1}.log",
        )
    return result


def component_bytes(row: dict[str, str]) -> dict[str, int]:
    offsets = sum(intval(row, key) for key in (
        "size_partial_offsets",
        "size_partial_copy_offsets",
        "size_partial_literal_offsets",
    ))
    offsets_v = sum(intval(row, key) for key in (
        "size_partial_offsets_varint",
        "size_partial_copy_offsets_varint",
        "size_partial_literal_offsets_varint",
    ))
    return {
        "block_ids": intval(row, "size_partial_block_ids"),
        "block_ids_varint": intval(row, "size_partial_block_ids_varint"),
        "SR": intval(row, "size_partial_sr"),
        "SR_varint": intval(row, "size_partial_sr_varint"),
        "offsets": offsets,
        "offsets_varint": offsets_v,
        "PT": intval(row, "size_partial_pt"),
        "PT_bitvector": intval(row, "size_partial_pt_bitvector"),
        "CPL": intval(row, "size_partial_cpl"),
        "CPL_varint": intval(row, "size_partial_cpl_varint"),
        "LP": intval(row, "size_partial_lp"),
        "reference": intval(row, "size_r"),
        "Phi_fallback": intval(row, "size_hybrid_m_phim1"),
    }


def method_component(component: str, raw_value: int, row: dict[str, str], codec: str) -> int:
    if codec == "raw":
        return raw_value
    if codec == "v1":
        if component == "block_ids":
            return intval(row, "size_partial_block_ids_varint")
        if component == "SR":
            return intval(row, "size_partial_sr_varint")
        return raw_value
    if codec == "v2":
        mapping = {
            "block_ids": "size_partial_block_ids_varint",
            "SR": "size_partial_sr_varint",
            "offsets": None,
            "PT": "size_partial_pt_bitvector",
            "CPL": "size_partial_cpl_varint",
        }
        if component == "offsets":
            return sum(intval(row, key) for key in (
                "size_partial_offsets_varint",
                "size_partial_copy_offsets_varint",
                "size_partial_literal_offsets_varint",
            ))
        key = mapping.get(component)
        if key:
            return intval(row, key)
    return raw_value


def summarize_dataset(dataset: str, source_subdir: str, text_name: str, repeats: int) -> tuple[list[dict[str, object]], list[dict[str, object]], list[dict[str, object]]]:
    source_result_dir = SOURCE_ROOT / source_subdir
    source_pattern_dir = SOURCE_PATTERN_ROOT / source_subdir
    index_dir = ensure_v1_index(dataset, source_subdir, text_name)
    selected = read_selected_theta(source_result_dir)

    rows_query: list[dict[str, object]] = []
    rows_space: list[dict[str, object]] = []
    rows_components: list[dict[str, object]] = []

    move_ref = {m: parse_result(source_result_dir / f"test-move-r-m{m}.result")[-1] for m in (8, 100, 200)}

    for budget, raw_slug in (("25", "adaptive25-raw"), ("12.5", "adaptive12-raw")):
        method_defs = [
            ("r-RLZSA", raw_slug, "raw"),
            ("Vr-RLZSA-v1", raw_slug.replace("-raw", "-varint-v1"), "v1"),
            ("Vr-RLZSA-v2", raw_slug.replace("-raw", "-varint-v2"), "v2"),
        ]
        result_by_method: dict[str, dict[int, Path]] = {}
        for method, slug, codec in method_defs:
            result_by_method[method] = {}
            for m in WORKLOADS:
                if codec in ("raw", "v2"):
                    src = source_test_result(source_result_dir, slug, m)
                    if not src.exists():
                        raise FileNotFoundError(src)
                    result_by_method[method][m] = src
                else:
                    result_by_method[method][m] = copy_or_run_query(
                        dataset, source_result_dir, source_pattern_dir, index_dir,
                        text_name, slug, budget.replace(".5", ""), selected[slug], m, repeats)

        raw_idx = index_path(index_dir, text_name, raw_slug).stat().st_size
        raw_row = parse_result(result_by_method["r-RLZSA"][8])[-1]
        raw_components = component_bytes(raw_row)

        raw_times = {
            m: median([
                intval(r, "time_locate") / max(1, intval(r, "num_patterns"))
                for r in parse_result(result_by_method["r-RLZSA"][m])
            ])
            for m in WORKLOADS
        }

        for method, slug, codec in method_defs:
            idx_size = index_path(index_dir, text_name, slug).stat().st_size
            saving = raw_idx - idx_size
            rows_space.append({
                "dataset": dataset,
                "budget": budget,
                "method": method,
                "codec": codec,
                "index_mib": f"{mib(idx_size):.3f}",
                "relative_to_r_rlzsa_percent": f"{100 * idx_size / raw_idx:.2f}",
                "saving_mib": f"{mib(saving):.3f}",
                "saving_percent": f"{100 * saving / raw_idx:.2f}",
            })

            sample_row = parse_result(result_by_method[method][8])[-1]
            for comp in ("block_ids", "SR", "offsets", "PT", "CPL", "LP", "reference", "Phi_fallback"):
                raw_value = raw_components[comp]
                value = method_component(comp, raw_value, sample_row, codec)
                rows_components.append({
                    "dataset": dataset,
                    "budget": budget,
                    "method": method,
                    "codec": codec,
                    "component": comp,
                    "bytes": value,
                    "MiB": f"{mib(value):.3f}",
                    "relative_to_r_rlzsa_percent": f"{100 * value / raw_value:.2f}" if raw_value else "0.00",
                })
            other = max(0, idx_size - sum(int(r["bytes"]) for r in rows_components if r["dataset"] == dataset and r["budget"] == budget and r["method"] == method))
            rows_components.append({
                "dataset": dataset,
                "budget": budget,
                "method": method,
                "codec": codec,
                "component": "Other(serialized)",
                "bytes": other,
                "MiB": f"{mib(other):.3f}",
                "relative_to_r_rlzsa_percent": "",
            })

            for m in WORKLOADS:
                result = result_by_method[method][m]
                qr = parse_result(result)
                times = [intval(r, "time_locate") / max(1, intval(r, "num_patterns")) for r in qr]
                last = qr[-1]
                correct = (
                    intval(last, "num_occurrences") == intval(move_ref[m], "num_occurrences") and
                    intval(last, "occurrence_checksum") == intval(move_ref[m], "occurrence_checksum")
                )
                rows_query.append({
                    "dataset": dataset,
                    "budget": budget,
                    "method": method,
                    "codec": codec,
                    "workload": f"m{m}",
                    "ns_per_pattern_median": f"{median(times):.1f}",
                    "ns_per_pattern_min": f"{min(times):.1f}",
                    "ns_per_pattern_max": f"{max(times):.1f}",
                    "speed_relative_to_r_rlzsa": f"{raw_times[m] / median(times):.3f}" if median(times) else "0.000",
                    "num_occurrences": intval(last, "num_occurrences"),
                    "occurrence_checksum": intval(last, "occurrence_checksum"),
                    "correctness": "PASS" if correct else "FAIL",
                })
    return rows_space, rows_query, rows_components


def write_summary(space_rows: list[dict[str, object]], query_rows: list[dict[str, object]], component_rows: list[dict[str, object]]) -> None:
    md = ["# Vr-RLZSA Ablation Summary\n\n"]
    md.append("## Space\n\n| dataset | budget | method | index MiB | saving MiB | saving % |\n|---|---:|---|---:|---:|---:|\n")
    for row in space_rows:
        md.append(f"| {row['dataset']} | {row['budget']} | {row['method']} | {row['index_mib']} | {row['saving_mib']} | {row['saving_percent']} |\n")
    md.append("\n## Correctness\n\n")
    bad = [row for row in query_rows if row["correctness"] != "PASS"]
    md.append("All ablation query checksums PASS.\n" if not bad else f"Failures: {len(bad)} rows.\n")

    md.append("\n## Field Contribution Hint\n\n")
    for dataset in sorted({str(r["dataset"]) for r in component_rows}):
        for budget in ("25", "12.5"):
            raw = {r["component"]: int(r["bytes"]) for r in component_rows if r["dataset"] == dataset and r["budget"] == budget and r["method"] == "r-RLZSA"}
            v1 = {r["component"]: int(r["bytes"]) for r in component_rows if r["dataset"] == dataset and r["budget"] == budget and r["method"] == "Vr-RLZSA-v1"}
            v2 = {r["component"]: int(r["bytes"]) for r in component_rows if r["dataset"] == dataset and r["budget"] == budget and r["method"] == "Vr-RLZSA-v2"}
            if not raw or not v1 or not v2:
                continue
            v1_gain = (raw.get("block_ids", 0) - v1.get("block_ids", 0)) + (raw.get("SR", 0) - v1.get("SR", 0))
            v2_extra = sum(raw.get(c, 0) - v2.get(c, 0) for c in ("offsets", "PT", "CPL"))
            md.append(f"- {dataset} @{budget}%: v1 fields save {mib(v1_gain):.3f} MiB; v2 extra fields save {mib(v2_extra):.3f} MiB.\n")
    (OUT_ROOT / "ablation_summary.md").write_text("".join(md), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--datasets", nargs="*", default=list(DATASET_DIRS.keys()), choices=list(DATASET_DIRS.keys()))
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()

    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    all_space: list[dict[str, object]] = []
    all_query: list[dict[str, object]] = []
    all_components: list[dict[str, object]] = []
    for dataset in args.datasets:
        source_subdir, text_name = DATASET_DIRS[dataset]
        print(f"[ablation] {dataset}", flush=True)
        space, query, components = summarize_dataset(dataset, source_subdir, text_name, args.repeats)
        all_space.extend(space)
        all_query.extend(query)
        all_components.extend(components)
        write_csv(OUT_ROOT / "ablation_space_summary.csv", all_space)
        write_csv(OUT_ROOT / "ablation_query_summary.csv", all_query)
        write_csv(OUT_ROOT / "ablation_component_space.csv", all_components)
        write_summary(all_space, all_query, all_components)
    print(OUT_ROOT / "ablation_summary.md", flush=True)


if __name__ == "__main__":
    main()
