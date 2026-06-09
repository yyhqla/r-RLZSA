#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import random
import re
import shlex
import statistics
import subprocess
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TEXT_DIR = ROOT / "measurements" / "texts"
PATTERN_ROOT = ROOT / "measurements" / "patterns"
RESULT_ROOT = ROOT / "measurements" / "results"
INDEX_ROOT = ROOT / "measurements" / "indexes"
BUILD = ROOT / "build" / "cli" / "move-r-build"
LOCATE = ROOT / "build" / "cli" / "move-r-locate"
GEN_PATTERNS = ROOT / "build" / "cli" / "gen-patterns"
WORKLOADS = (8, 100, 200)
THETAS = (0, 32, 64, 128, 256, 512, 1024)

DATASETS = {
    "boost": "boost",
    "einstein.en": "einstein.en.txt",
    "SARS-Cov-2_40": "SARS-Cov-2_40",
    "world_leaders": "world_leaders",
    "DNA": "DNA",
}


def run(cmd: list[object], log: Path) -> float:
    log.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with log.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(shlex.quote(str(x)) for x in cmd) + "\n\n")
        out.flush()
        proc = subprocess.run([str(x) for x in cmd], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, text=True)
    elapsed = time.perf_counter() - start
    if proc.returncode:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(str(x) for x in cmd)}\nlog: {log}")
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


def read_patterns(path: Path) -> tuple[int, int, list[bytes]]:
    data = path.read_bytes()
    header, body = data.split(b"\n", 1)
    text = header.decode("utf-8", errors="replace")
    count = int(re.search(r"number=(\d+)", text).group(1))
    length = int(re.search(r"length=(\d+)", text).group(1))
    return count, length, [body[i * length:(i + 1) * length] for i in range(count)]


def write_patterns(path: Path, length: int, patterns: list[bytes], text_name: str, note: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    header = f"# number={len(patterns)} length={length} file={text_name} {note} forbidden=\n".encode()
    path.write_bytes(header + b"".join(patterns))


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


def generate_patterns_fallback(text: Path, text_name: str, length: int, count: int, output: Path, log: Path) -> None:
    size = text.stat().st_size
    rng = random.Random(20260608 + length * 1000003 + count)
    with text.open("rb") as inp, output.open("wb") as out:
        out.write(f"# number={count} length={length} file={text_name} forbidden=\n".encode())
        for _ in range(count):
            pos = rng.randrange(0, max(1, size - length))
            inp.seek(pos)
            out.write(inp.read(length))
    with log.open("a", encoding="utf-8", errors="replace") as out:
        out.write("\n[python-fallback] generated patterns\n")


def generate_patterns(text: Path, text_name: str, length: int, count: int, output: Path, log: Path) -> None:
    try:
        run([GEN_PATTERNS, text, length, count, output], log)
    except RuntimeError:
        generate_patterns_fallback(text, text_name, length, count, output, log)


def patterns_ready(text_name: str, pattern_dir: Path, patterns: int) -> bool:
    for m in WORKLOADS:
        expected = [
            pattern_dir / f"{text_name}-m{m}-n{patterns}.patterns",
            pattern_dir / f"{text_name}-m{m}-train-adaptive.patterns",
            pattern_dir / f"{text_name}-m{m}-valid.patterns",
            pattern_dir / f"{text_name}-m{m}-test-n{patterns // 2}.patterns",
        ]
        if any(not path.exists() or path.stat().st_size == 0 for path in expected):
            return False
    return (pattern_dir / "mixed_train.manifest").exists() and (pattern_dir / "mixed_valid.manifest").exists()


def prepare_patterns(text: Path, text_name: str, pattern_dir: Path, result_dir: Path, patterns: int, reuse: bool) -> None:
    pattern_dir.mkdir(parents=True, exist_ok=True)
    if reuse and patterns_ready(text_name, pattern_dir, patterns):
        print(f"[patterns] reuse {pattern_dir}", flush=True)
        return
    for m in WORKLOADS:
        raw = pattern_dir / f"{text_name}-m{m}-n{patterns}.patterns"
        generate_patterns(text, text_name, m, patterns, raw, result_dir / f"gen-patterns-m{m}.log")
        _, length, pats = read_patterns(raw)
        ids = list(range(len(pats)))
        random.Random(20260530).shuffle(ids)
        half = len(ids) // 2
        train_ids = sorted(ids[:half])
        test_ids = sorted(ids[half:])
        train = [pats[i] for i in train_ids]
        write_patterns(pattern_dir / f"{text_name}-m{m}-test-n{len(test_ids)}.patterns", length, [pats[i] for i in test_ids], text_name, "split=test seed=20260530")
        ids2 = list(range(len(train)))
        random.Random(20260531).shuffle(ids2)
        valid_count = max(1, len(train) // 5)
        valid_ids = sorted(ids2[:valid_count])
        adaptive_ids = sorted(ids2[valid_count:])
        write_patterns(pattern_dir / f"{text_name}-m{m}-train-adaptive.patterns", length, [train[i] for i in adaptive_ids], text_name, "split=train-adaptive seed=20260531")
        write_patterns(pattern_dir / f"{text_name}-m{m}-valid.patterns", length, [train[i] for i in valid_ids], text_name, "split=valid seed=20260531")
    for split, suffix in (("train", "train-adaptive"), ("valid", "valid")):
        lines = ["# mixed_patterns_manifest=1\n"]
        for m in WORKLOADS:
            lines.append(str((pattern_dir / f"{text_name}-m{m}-{suffix}.patterns").resolve()) + "\n")
        (pattern_dir / f"mixed_{split}.manifest").write_text("".join(lines), encoding="utf-8")


def methods(num_blocks: int) -> list[dict[str, object]]:
    budget25 = (num_blocks + 3) // 4
    budget12 = (num_blocks + 7) // 8
    return [
        {"method": "Move-r", "slug": "move-r", "support": "locate_move", "kind": "move", "gap": 0, "budget": 0, "codec": "raw", "build": True},
        {"method": "Move-r-RLZ", "slug": "move-r-rlz", "support": "locate_rlzsa", "kind": "rlz", "gap": 0, "budget": 0, "codec": "raw", "build": True},
        {"method": "Move-r-LZEnd", "slug": "move-r-lzend", "support": "locate_lzendsa", "kind": "lzend", "gap": 0, "budget": 0, "codec": "raw", "build": True},
        {"method": "Adaptive-Mixed@25%-raw", "slug": "adaptive25-raw", "support": "locate_rlzsa", "kind": "adaptive", "gap": 4, "budget": budget25, "codec": "raw", "build": True},
        {"method": "Adaptive-Mixed@25%-varint-v2", "slug": "adaptive25-varint-v2", "support": "locate_rlzsa", "kind": "adaptive", "gap": 4, "budget": budget25, "codec": "varint-v2", "build": False, "source_slug": "adaptive25-raw"},
        {"method": "Adaptive-Mixed@12.5%-raw", "slug": "adaptive12-raw", "support": "locate_rlzsa", "kind": "adaptive", "gap": 8, "budget": budget12, "codec": "raw", "build": True},
        {"method": "Adaptive-Mixed@12.5%-varint-v2", "slug": "adaptive12-varint-v2", "support": "locate_rlzsa", "kind": "adaptive", "gap": 8, "budget": budget12, "codec": "varint-v2", "build": False, "source_slug": "adaptive12-raw"},
    ]


def index_path(index_dir: Path, text_name: str, slug: str, support: str) -> Path:
    suffix = ".move-r-rlzsa" if support == "locate_rlzsa" else ".move-r"
    return Path(str(index_dir / f"{text_name}.a8.{slug}") + suffix)


def parse_rss(log: Path) -> int:
    if not log.exists():
        return 0
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        if "Maximum resident set size" in line:
            return int(line.rsplit(":", 1)[1].strip())
    return 0


def build_indexes(text: Path, text_name: str, pattern_dir: Path, index_dir: Path, result_dir: Path, build_threads: int, all_methods: list[dict[str, object]], reuse: bool) -> tuple[dict[str, Path], list[dict[str, object]]]:
    index_dir.mkdir(parents=True, exist_ok=True)
    built: dict[str, Path] = {}
    build_rows: list[dict[str, object]] = []
    for method in all_methods:
        slug = str(method["slug"])
        idx = index_path(index_dir, text_name, slug, str(method["support"]))
        built[slug] = idx
    for method in all_methods:
        if not method["build"]:
            slug = str(method["slug"])
            idx = built[slug]
            build_rows.append({
                "dataset": text_name,
                "method": method["method"],
                "codec": method["codec"],
                "index_MiB": f"{mib(idx.stat().st_size):.3f}" if idx.exists() else "0.000",
                "build_wall_seconds": "0.000",
                "note": "written as varint-v2 copy from raw adaptive build",
            })
            continue
        slug = str(method["slug"])
        support = str(method["support"])
        idx = built[slug]
        if reuse and idx.exists() and idx.stat().st_size > 0:
            elapsed = 0.0
        else:
            result = result_dir / f"build-{slug}.result"
            mds = result_dir / f"build-{slug}.mds"
            result.unlink(missing_ok=True)
            mds.unlink(missing_ok=True)
            cmd: list[object] = [
                BUILD, "-s", support, "-p", build_threads, "-a", "8",
                "-o", index_dir / f"{text_name}.a8.{slug}", "-m_idx", result, "-m_mds", mds,
            ]
            if method["kind"] == "adaptive":
                copy_slug = slug.replace("-raw", "-varint-v2")
                cmd += [
                    "-hybrid", "-hybrid-thr", "32", "-hybrid-min-occ", "2", "-hybrid-max-pattern", "64",
                    "-partial-rlzsa-adaptive", "1024", method["budget"], pattern_dir / "mixed_train.manifest",
                    "-partial-train-thr", "100",
                    "-partial-codec", "raw",
                    "-partial-field-stats", result_dir / "field_distribution.csv", result_dir / "estimated_space_saving.csv", method["method"],
                    "-partial-varint-v2-copy", index_dir / f"{text_name}.a8.{copy_slug}",
                ]
            print(f"[build] {text_name} {method['method']}", flush=True)
            elapsed = run(["/usr/bin/time", "-v"] + cmd + [text], result_dir / f"build-{slug}.log")
        rows = parse_result(result_dir / f"build-{slug}.result")
        row = rows[-1] if rows else {}
        build_rows.append({
            "dataset": text_name,
            "method": method["method"],
            "codec": method["codec"],
            "index_MiB": f"{mib(idx.stat().st_size):.3f}" if idx.exists() else "0.000",
            "build_wall_seconds": f"{elapsed:.3f}",
            "build_time_ns": intval(row, "time_construction"),
            "max_rss_kb_time_v": parse_rss(result_dir / f"build-{slug}.log"),
        })
    write_csv(result_dir / "build_summary.csv", build_rows)
    return built, build_rows


def locate_one(text_name: str, idx: Path, patterns: Path, result: Path, log: Path, kind: str, gap: int, theta: int) -> None:
    cmd: list[object] = [LOCATE]
    if kind == "adaptive":
        cmd += ["-block-hybrid", "1024", gap, "100", "-block-hybrid-rlz-thr", theta]
    cmd += ["-m", result, text_name, idx, patterns]
    run(cmd, log)


def select_theta(text_name: str, pattern_dir: Path, result_dir: Path, built: dict[str, Path], repeats: int) -> dict[str, int]:
    selected: dict[str, int] = {}
    for m in WORKLOADS:
        result = result_dir / f"valid-move-r-m{m}.result"
        result.unlink(missing_ok=True)
        for rep in range(repeats):
            locate_one(text_name, built["move-r"], pattern_dir / f"{text_name}-m{m}-valid.patterns", result, result_dir / f"valid-move-r-m{m}-rep{rep + 1}.log", "", 0, 0)
    move_rows = {m: parse_result(result_dir / f"valid-move-r-m{m}.result") for m in WORKLOADS}
    move_total = median([sum(intval(move_rows[m][rep], "time_locate") for m in WORKLOADS) for rep in range(repeats)])
    scan_rows: list[dict[str, object]] = []
    for slug, gap in (("adaptive25-raw", 4), ("adaptive12-raw", 8)):
        totals: list[tuple[int, float]] = []
        for theta in THETAS:
            for m in WORKLOADS:
                result = result_dir / f"valid-{slug}-m{m}-theta{theta}.result"
                result.unlink(missing_ok=True)
                for rep in range(repeats):
                    locate_one(text_name, built[slug], pattern_dir / f"{text_name}-m{m}-valid.patterns", result, result_dir / f"valid-{slug}-m{m}-theta{theta}-rep{rep + 1}.log", "adaptive", gap, theta)
            total = median([
                sum(intval(parse_result(result_dir / f"valid-{slug}-m{m}-theta{theta}.result")[rep], "time_locate") for m in WORKLOADS)
                for rep in range(repeats)
            ])
            totals.append((theta, total))
        best_theta, best_total = min(totals, key=lambda item: item[1])
        selected[slug] = best_theta
        selected[slug.replace("-raw", "-varint-v2")] = best_theta
        for theta, total in totals:
            scan_rows.append({
                "dataset": text_name,
                "method": slug,
                "theta_rlz": theta,
                "selected": int(theta == best_theta),
                "mixed_total_time_median_ns": int(total),
                "mixed_total_speedup_vs_move_r": f"{move_total / total:.3f}" if total else "0.000",
            })
        print(f"[theta] {text_name} {slug}={best_theta} total={int(best_total)}", flush=True)
    write_csv(result_dir / "validation_theta_scan.csv", scan_rows)
    (result_dir / "selected_theta.txt").write_text("\n".join(f"{k} {v}" for k, v in selected.items()) + "\n", encoding="utf-8")
    return selected


def final_test(text_name: str, pattern_dir: Path, result_dir: Path, built: dict[str, Path], selected: dict[str, int], repeats: int, all_methods: list[dict[str, object]]) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
    for method in all_methods:
        slug = str(method["slug"])
        print(f"[test] {text_name} {method['method']}", flush=True)
        for m in WORKLOADS:
            result = result_dir / f"test-{slug}-m{m}.result"
            result.unlink(missing_ok=True)
            for rep in range(repeats):
                locate_one(
                    text_name,
                    built[slug],
                    pattern_dir / f"{text_name}-m{m}-test-n500.patterns",
                    result,
                    result_dir / f"test-{slug}-m{m}-rep{rep + 1}.log",
                    str(method["kind"]),
                    int(method["gap"]),
                    selected.get(slug, 0),
                )
    ref = {m: parse_result(result_dir / f"test-move-r-m{m}.result")[-1] for m in WORKLOADS}
    move_time: dict[int, float] = {}
    for m in WORKLOADS:
        rows = parse_result(result_dir / f"test-move-r-m{m}.result")
        move_time[m] = median([intval(r, "time_locate") / max(1, intval(r, "num_patterns")) for r in rows])
    perf_rows: list[dict[str, object]] = []
    space_rows: list[dict[str, object]] = []
    for method in all_methods:
        slug = str(method["slug"])
        idx = built[slug]
        first_last: dict[str, str] | None = None
        for m in WORKLOADS:
            rows = parse_result(result_dir / f"test-{slug}-m{m}.result")
            ts = [intval(r, "time_locate") / max(1, intval(r, "num_patterns")) for r in rows]
            last = rows[-1]
            if first_last is None:
                first_last = last
            phi = intval(last, "block_hybrid_phi_occurrences")
            rlz = intval(last, "block_hybrid_rlzsa_occurrences")
            correct = (
                intval(last, "num_patterns") == intval(ref[m], "num_patterns") and
                intval(last, "num_occurrences") == intval(ref[m], "num_occurrences") and
                intval(last, "occurrence_checksum") == intval(ref[m], "occurrence_checksum")
            )
            perf_rows.append({
                "dataset": text_name,
                "method": method["method"],
                "codec": method["codec"],
                "workload": f"m{m}",
                "index_MiB": f"{mib(idx.stat().st_size):.3f}",
                "theta_rlz": selected.get(slug, ""),
                "ns_per_pattern_min": f"{min(ts):.1f}",
                "ns_per_pattern_median": f"{median(ts):.1f}",
                "ns_per_pattern_max": f"{max(ts):.1f}",
                "speedup_vs_Move_r": f"{move_time[m] / median(ts):.3f}" if median(ts) else "0.000",
                "num_patterns": intval(last, "num_patterns"),
                "num_occurrences": intval(last, "num_occurrences"),
                "occurrence_checksum": intval(last, "occurrence_checksum"),
                "RLZSA_occ_percent": f"{100 * rlz / (phi + rlz):.2f}" if phi + rlz else "",
                "correctness": "PASS" if correct else "FAIL",
            })
        if first_last is not None:
            keys = [
                "size_index", "size_r", "size_partial_offsets", "size_partial_offsets_varint",
                "size_partial_copy_offsets", "size_partial_copy_offsets_varint",
                "size_partial_literal_offsets", "size_partial_literal_offsets_varint",
                "size_partial_block_ids", "size_partial_block_ids_varint",
                "size_partial_pt", "size_partial_pt_bitvector",
                "size_partial_cpl", "size_partial_cpl_varint",
                "size_partial_sr", "size_partial_sr_varint",
                "size_partial_lp", "size_hybrid_m_phim1",
            ]
            for key in keys:
                if key in first_last:
                    space_rows.append({
                        "dataset": text_name,
                        "method": method["method"],
                        "codec": method["codec"],
                        "component": key,
                        "bytes": intval(first_last, key),
                        "MiB": f"{mib(intval(first_last, key)):.3f}",
                    })
            space_rows.append({
                "dataset": text_name,
                "method": method["method"],
                "codec": method["codec"],
                "component": "serialized_index_file",
                "bytes": idx.stat().st_size,
                "MiB": f"{mib(idx.stat().st_size):.3f}",
            })
    write_csv(result_dir / "final_test_performance.csv", perf_rows)
    write_csv(result_dir / "space_breakdown.csv", space_rows)
    return perf_rows, space_rows


def raw_v2_savings(space_rows: list[dict[str, object]]) -> list[dict[str, object]]:
    by = {(r["dataset"], r["method"], r["component"]): int(r["bytes"]) for r in space_rows}
    rows: list[dict[str, object]] = []
    for dataset in sorted({str(r["dataset"]) for r in space_rows}):
        for base, v2 in (
            ("Adaptive-Mixed@25%-raw", "Adaptive-Mixed@25%-varint-v2"),
            ("Adaptive-Mixed@12.5%-raw", "Adaptive-Mixed@12.5%-varint-v2"),
        ):
            raw = by.get((dataset, base, "serialized_index_file"), 0)
            enc = by.get((dataset, v2, "serialized_index_file"), 0)
            rows.append({
                "dataset": dataset,
                "method_pair": f"{base} -> {v2}",
                "raw_MiB": f"{mib(raw):.3f}",
                "varint_v2_MiB": f"{mib(enc):.3f}",
                "saving_MiB": f"{mib(raw - enc):.3f}",
                "saving_percent": f"{100 * (raw - enc) / raw:.2f}" if raw else "0.00",
            })
    return rows


def write_dataset_summary(result_dir: Path, text_name: str, perf_rows: list[dict[str, object]], space_rows: list[dict[str, object]], build_rows: list[dict[str, object]]) -> None:
    savings = raw_v2_savings(space_rows)
    md = [f"# Varint-v2 Adaptive-Mixed Experiment: {text_name}\n\n", "## Space Saving\n\n| pair | raw MiB | v2 MiB | saving MiB | saving % |\n|---|---:|---:|---:|---:|\n"]
    for row in savings:
        if row["dataset"] == text_name:
            md.append(f"| {row['method_pair']} | {row['raw_MiB']} | {row['varint_v2_MiB']} | {row['saving_MiB']} | {row['saving_percent']} |\n")
    md.append("\n## Build\n\n| method | index MiB | build wall seconds | max RSS KB |\n|---|---:|---:|---:|\n")
    for row in build_rows:
        md.append(f"| {row['method']} | {row['index_MiB']} | {row['build_wall_seconds']} | {row.get('max_rss_kb_time_v', '')} |\n")
    md.append("\n## Query\n\n| workload | method | ns/pattern median | speedup vs Move-r | checksum | correctness |\n|---|---|---:|---:|---:|---|\n")
    for m in WORKLOADS:
        for row in [r for r in perf_rows if r["workload"] == f"m{m}"]:
            md.append(f"| m{m} | {row['method']} | {row['ns_per_pattern_median']} | {row['speedup_vs_Move_r']} | {row['occurrence_checksum']} | {row['correctness']} |\n")
    (result_dir / "final_summary.md").write_text("".join(md), encoding="utf-8")


def run_dataset(dataset_key: str, text_file: str, args: argparse.Namespace) -> tuple[list[dict[str, object]], list[dict[str, object]], list[dict[str, object]]]:
    text = TEXT_DIR / text_file
    if not text.exists():
        raise FileNotFoundError(text)
    text_name = text_file
    out_name = f"varint_v2_{dataset_key}"
    result_dir = RESULT_ROOT / args.out_root / out_name
    pattern_dir = PATTERN_ROOT / args.out_root / out_name
    index_dir = INDEX_ROOT / args.out_root / out_name
    result_dir.mkdir(parents=True, exist_ok=True)
    pattern_dir.mkdir(parents=True, exist_ok=True)
    index_dir.mkdir(parents=True, exist_ok=True)
    num_blocks = ((text.stat().st_size + 1) + 1023) // 1024
    all_methods = methods(num_blocks)
    print(f"[dataset] {dataset_key}: {text} ({text.stat().st_size} bytes), blocks={num_blocks}", flush=True)
    prepare_patterns(text, text_name, pattern_dir, result_dir, args.patterns, args.reuse_patterns)
    built, build_rows = build_indexes(text, text_name, pattern_dir, index_dir, result_dir, args.build_threads, all_methods, args.reuse_indexes)
    selected = select_theta(text_name, pattern_dir, result_dir, built, args.repeats)
    perf_rows, space_rows = final_test(text_name, pattern_dir, result_dir, built, selected, args.repeats, all_methods)
    write_dataset_summary(result_dir, text_name, perf_rows, space_rows, build_rows)
    print(f"[done] {result_dir / 'final_summary.md'}", flush=True)
    return build_rows, perf_rows, space_rows


def write_combined(out_root: Path, build_rows: list[dict[str, object]], perf_rows: list[dict[str, object]], space_rows: list[dict[str, object]]) -> None:
    out_root.mkdir(parents=True, exist_ok=True)
    write_csv(out_root / "all_build_summary.csv", build_rows)
    write_csv(out_root / "all_query_summary.csv", perf_rows)
    write_csv(out_root / "all_space_breakdown.csv", space_rows)
    savings = raw_v2_savings(space_rows)
    write_csv(out_root / "all_raw_vs_varint_v2_space_saving.csv", savings)
    md = ["# Varint-v2 Cross-Dataset Summary\n\n", "## Raw vs Varint-v2 Space\n\n| dataset | pair | raw MiB | v2 MiB | saving MiB | saving % |\n|---|---|---:|---:|---:|---:|\n"]
    for row in savings:
        md.append(f"| {row['dataset']} | {row['method_pair']} | {row['raw_MiB']} | {row['varint_v2_MiB']} | {row['saving_MiB']} | {row['saving_percent']} |\n")
    md.append("\n## Correctness\n\n")
    bad = [r for r in perf_rows if r.get("correctness") != "PASS"]
    md.append("All query checksums PASS.\n" if not bad else f"Failures: {len(bad)} rows.\n")
    (out_root / "final_cross_dataset_summary.md").write_text("".join(md), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--datasets", nargs="*", default=list(DATASETS.keys()), choices=list(DATASETS.keys()))
    parser.add_argument("--out-root", default="varint_v2_cross_dataset")
    parser.add_argument("--patterns", type=int, default=1000)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--build-threads", type=int, default=8)
    parser.add_argument("--reuse-patterns", action="store_true")
    parser.add_argument("--reuse-indexes", action="store_true")
    args = parser.parse_args()
    all_build: list[dict[str, object]] = []
    all_perf: list[dict[str, object]] = []
    all_space: list[dict[str, object]] = []
    for dataset_key in args.datasets:
        build_rows, perf_rows, space_rows = run_dataset(dataset_key, DATASETS[dataset_key], args)
        all_build.extend(build_rows)
        all_perf.extend(perf_rows)
        all_space.extend(space_rows)
        write_combined(RESULT_ROOT / args.out_root, all_build, all_perf, all_space)
    print(RESULT_ROOT / args.out_root / "final_cross_dataset_summary.md", flush=True)


if __name__ == "__main__":
    main()
