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
WORKLOADS = (4, 8, 16, 32, 64, 128)
THETAS = (0, 32, 64, 128, 256, 512, 1024)


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


def read_patterns(path: Path) -> tuple[int, int, list[bytes]]:
    data = path.read_bytes()
    header, body = data.split(b"\n", 1)
    header_text = header.decode("utf-8", errors="replace")
    count = int(re.search(r"number=(\d+)", header_text).group(1))
    length = int(re.search(r"length=(\d+)", header_text).group(1))
    return count, length, [body[i * length:(i + 1) * length] for i in range(count)]


def write_patterns(path: Path, length: int, patterns: list[bytes], text_name: str, note: str) -> None:
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
    with path.open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def generate_patterns_fallback(text: Path, text_name: str, length: int, count: int, output: Path, log: Path) -> None:
    start = time.perf_counter()
    size = text.stat().st_size
    rng = random.Random(20260607 + length * 1000003 + count)
    output.parent.mkdir(parents=True, exist_ok=True)
    with text.open("rb") as inp, output.open("wb") as out:
        out.write(f"# number={count} length={length} file={text_name} forbidden=\n".encode())
        for _ in range(count):
            pos = rng.randrange(0, size - length)
            inp.seek(pos)
            out.write(inp.read(length))
    elapsed_ms = (time.perf_counter() - start) * 1000
    with log.open("a", encoding="utf-8", errors="replace") as out:
        out.write(f"\n[python-fallback] generated {count} patterns of length {length}, in ~ {elapsed_ms:.1f} ms\n")


def generate_patterns(text: Path, text_name: str, length: int, count: int, output: Path, log: Path) -> None:
    try:
        run([GEN_PATTERNS, text, length, count, output], log)
    except RuntimeError:
        generate_patterns_fallback(text, text_name, length, count, output, log)


def parse_rss(log: Path) -> int:
    if not log.exists():
        return 0
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        if "Maximum resident set size" in line:
            return int(line.rsplit(":", 1)[1].strip())
    return 0


def methods(num_blocks: int) -> list[tuple[str, str, str, str, int, int | None]]:
    budget25 = (num_blocks + 3) // 4
    budget12 = (num_blocks + 7) // 8
    return [
        ("Move-r", "move-r", "locate_move", "", 0, None),
        ("Move-r-RLZ", "move-r-rlz", "locate_rlzsa", "", 0, None),
        ("Move-r-RLZEnd", "move-r-rlzend", "locate_lzendsa", "", 0, None),
        ("r-RLZSA@25%", "r-rlzsa25", "locate_rlzsa", "adaptive", 4, budget25),
        ("r-RLZSA@12.5%", "r-rlzsa12", "locate_rlzsa", "adaptive", 8, budget12),
    ]


def index_path(index_dir: Path, text_name: str, slug: str, support: str) -> Path:
    suffix = ".move-r-rlzsa" if support == "locate_rlzsa" else ".move-r"
    return Path(str(index_dir / f"{text_name}.a8.{slug}") + suffix)


def patterns_ready(text_name: str, pattern_dir: Path, patterns: int) -> bool:
    for m in WORKLOADS:
        expected = [
            pattern_dir / f"{text_name}-m{m}-n{patterns}.patterns",
            pattern_dir / f"{text_name}-m{m}-train-n{patterns // 2}.patterns",
            pattern_dir / f"{text_name}-m{m}-test-n{patterns - patterns // 2}.patterns",
            pattern_dir / f"{text_name}-m{m}-train-adaptive.patterns",
            pattern_dir / f"{text_name}-m{m}-valid.patterns",
        ]
        if any(not path.exists() or path.stat().st_size == 0 for path in expected):
            return False
    return (
        (pattern_dir / "mixed_train.manifest").exists() and
        (pattern_dir / "mixed_valid.manifest").exists()
    )


def prepare_patterns(text: Path, text_name: str, pattern_dir: Path, result_dir: Path, patterns: int, reuse: bool = False) -> None:
    pattern_dir.mkdir(parents=True, exist_ok=True)
    if reuse and patterns_ready(text_name, pattern_dir, patterns):
        print("[patterns] reusing existing pattern files", flush=True)
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
        write_patterns(pattern_dir / f"{text_name}-m{m}-train-n{half}.patterns", length, [pats[i] for i in train_ids], text_name, "split=train seed=20260530")
        write_patterns(pattern_dir / f"{text_name}-m{m}-test-n{len(test_ids)}.patterns", length, [pats[i] for i in test_ids], text_name, "split=test seed=20260530")
        train = [pats[i] for i in train_ids]
        ids2 = list(range(len(train)))
        random.Random(20260531).shuffle(ids2)
        valid_count = max(1, len(train) // 5)
        valid_ids = sorted(ids2[:valid_count])
        train2_ids = sorted(ids2[valid_count:])
        write_patterns(pattern_dir / f"{text_name}-m{m}-train-adaptive.patterns", length, [train[i] for i in train2_ids], text_name, "split=train-adaptive seed=20260531")
        write_patterns(pattern_dir / f"{text_name}-m{m}-valid.patterns", length, [train[i] for i in valid_ids], text_name, "split=valid seed=20260531")
    for split, suffix in (("train", "train-adaptive"), ("valid", "valid")):
        lines = ["# mixed_patterns_manifest=1\n"]
        for m in WORKLOADS:
            lines.append(str((pattern_dir / f"{text_name}-m{m}-{suffix}.patterns").resolve()) + "\n")
        (pattern_dir / f"mixed_{split}.manifest").write_text("".join(lines), encoding="utf-8")


def build_indexes(text: Path, text_name: str, pattern_dir: Path, index_dir: Path, result_dir: Path, build_threads: int, all_methods) -> dict[str, Path]:
    built: dict[str, Path] = {}
    index_dir.mkdir(parents=True, exist_ok=True)
    for display, slug, support, kind, gap, budget in all_methods:
        idx = index_path(index_dir, text_name, slug, support)
        built[slug] = idx
        for old in index_dir.glob(f"{text_name}.a8.{slug}*"):
            old.unlink()
        result = result_dir / f"build-{slug}.result"
        mds = result_dir / f"build-{slug}.mds"
        result.unlink(missing_ok=True)
        mds.unlink(missing_ok=True)
        cmd: list[object] = [
            BUILD, "-s", support, "-p", build_threads, "-a", "8",
            "-o", index_dir / f"{text_name}.a8.{slug}", "-m_idx", result, "-m_mds", mds,
        ]
        if kind == "adaptive":
            cmd += [
                "-hybrid", "-hybrid-thr", "32", "-hybrid-min-occ", "2", "-hybrid-max-pattern", "64",
                "-partial-rlzsa-adaptive", "1024", budget, pattern_dir / "mixed_train.manifest",
                "-partial-train-thr", "100",
            ]
        cmd.append(text)
        print(f"[build] {display}", flush=True)
        run(["/usr/bin/time", "-v"] + cmd, result_dir / f"build-{slug}.log")
    return built


def existing_indexes(text_name: str, index_dir: Path, all_methods) -> dict[str, Path]:
    built: dict[str, Path] = {}
    missing: list[Path] = []
    for _display, slug, support, _kind, _gap, _budget in all_methods:
        idx = index_path(index_dir, text_name, slug, support)
        if not idx.exists() or idx.stat().st_size == 0:
            missing.append(idx)
        built[slug] = idx
    if missing:
        raise FileNotFoundError("missing indexes for --reuse-indexes: " + ", ".join(str(path) for path in missing))
    print("[build] reusing existing indexes", flush=True)
    return built


def locate_one(text_name: str, idx: Path, patterns: Path, result: Path, log: Path, kind: str = "", gap: int = 0, theta: int = 0) -> None:
    cmd: list[object] = [LOCATE]
    if kind:
        cmd += ["-block-hybrid", "1024", gap, "100", "-block-hybrid-rlz-thr", theta]
    cmd += ["-m", result, text_name, idx, patterns]
    run(cmd, log)


def select_theta(text_name: str, pattern_dir: Path, result_dir: Path, built: dict[str, Path], repeats: int) -> dict[str, int]:
    print("[validation] Move-r baseline", flush=True)
    for m in WORKLOADS:
        result = result_dir / f"valid-move-r-m{m}.result"
        result.unlink(missing_ok=True)
        for rep in range(repeats):
            locate_one(text_name, built["move-r"], pattern_dir / f"{text_name}-m{m}-valid.patterns", result, result_dir / f"valid-move-r-m{m}-rep{rep + 1}.log")
    move_rows = {m: parse_result(result_dir / f"valid-move-r-m{m}.result") for m in WORKLOADS}
    move_total = median([sum(intval(move_rows[m][rep], "time_locate") for m in WORKLOADS) for rep in range(repeats)])
    selected: dict[str, int] = {}
    scan_rows: list[dict[str, object]] = []
    for slug, gap in (("r-rlzsa25", 4), ("r-rlzsa12", 8)):
        print(f"[validation] {slug}", flush=True)
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
        best_theta, _ = min(totals, key=lambda item: item[1])
        selected[slug] = best_theta
        for theta, total in totals:
            for m in WORKLOADS:
                rows = parse_result(result_dir / f"valid-{slug}-m{m}-theta{theta}.result")
                ts = [intval(r, "time_locate") / intval(r, "num_patterns") for r in rows]
                last = rows[-1]
                phi = intval(last, "block_hybrid_phi_occurrences")
                rlz = intval(last, "block_hybrid_rlzsa_occurrences")
                base = [intval(r, "time_locate") / intval(r, "num_patterns") for r in move_rows[m]]
                scan_rows.append({
                    "method": slug,
                    "theta_rlz": theta,
                    "validation_workload": f"m{m}",
                    "ns_per_pattern_median": f"{median(ts):.1f}",
                    "speedup_vs_move_r": f"{median(base) / median(ts):.3f}",
                    "rlzsa_occ_coverage": f"{100 * rlz / (phi + rlz):.1f}" if phi + rlz else "0.0",
                    "selected_global_theta": int(theta == best_theta),
                    "mixed_total_time_median_ns": int(total),
                    "mixed_total_speedup_vs_move_r": f"{move_total / total:.3f}",
                })
    write_csv(result_dir / "validation_theta_scan.csv", scan_rows)
    (result_dir / "selected_theta.txt").write_text("\n".join(f"{k} {v}" for k, v in selected.items()) + "\n", encoding="utf-8")
    print(f"[theta] {selected}", flush=True)
    return selected


def final_test(text_name: str, pattern_dir: Path, result_dir: Path, built: dict[str, Path], selected: dict[str, int], repeats: int, all_methods) -> None:
    for display, slug, _support, kind, gap, _budget in all_methods:
        print(f"[test] {display}", flush=True)
        for m in WORKLOADS:
            result = result_dir / f"test-{slug}-m{m}.result"
            result.unlink(missing_ok=True)
            theta = selected.get(slug, 0)
            for rep in range(repeats):
                locate_one(text_name, built[slug], pattern_dir / f"{text_name}-m{m}-test-n500.patterns", result, result_dir / f"test-{slug}-m{m}-rep{rep + 1}.log", kind, gap, theta)

    ref = {m: parse_result(result_dir / f"test-move-r-m{m}.result")[-1] for m in WORKLOADS}
    move_time = {}
    rlz_time = {}
    for m in WORKLOADS:
        rows = parse_result(result_dir / f"test-move-r-m{m}.result")
        move_time[m] = median([intval(r, "time_locate") / intval(r, "num_patterns") for r in rows])
        rows = parse_result(result_dir / f"test-move-r-rlz-m{m}.result")
        rlz_time[m] = median([intval(r, "time_locate") / intval(r, "num_patterns") for r in rows])

    perf_rows: list[dict[str, object]] = []
    for display, slug, _support, kind, _gap, _budget in all_methods:
        idx = built[slug]
        for m in WORKLOADS:
            rows = parse_result(result_dir / f"test-{slug}-m{m}.result")
            ts = [intval(r, "time_locate") / intval(r, "num_patterns") for r in rows]
            last = rows[-1]
            phi = intval(last, "block_hybrid_phi_occurrences")
            rlz = intval(last, "block_hybrid_rlzsa_occurrences")
            partial = sum(intval(last, k) for k in [
                "size_partial_offsets", "size_partial_copy_offsets", "size_partial_literal_offsets",
                "size_partial_block_ids", "size_partial_pt", "size_partial_cpl", "size_partial_sr", "size_partial_lp",
            ])
            correct = (
                intval(last, "num_patterns") == intval(ref[m], "num_patterns") and
                intval(last, "num_occurrences") == intval(ref[m], "num_occurrences") and
                intval(last, "occurrence_checksum") == intval(ref[m], "occurrence_checksum")
            )
            perf_rows.append({
                "dataset": text_name,
                "method": display,
                "workload": f"m{m}",
                "pattern_length": m,
                "index_MiB": f"{mib(idx.stat().st_size):.2f}",
                "move_r_base_MiB": f"{mib(intval(last, 'size_m_lf') + intval(last, 'size_l_') + intval(last, 'size_l_prev') + intval(last, 'size_l_next') + intval(last, 'size_sa_s')):.2f}",
                "reference_MiB": f"{mib(intval(last, 'size_r')):.2f}",
                "partial_data_MiB": f"{mib(partial):.2f}",
                "enhanced_blocks": intval(last, "size_partial_block_ids") // 4,
                "theta_rlz": selected.get(slug, "") if kind else "",
                "RLZSA_occ_coverage": f"{100 * rlz / (phi + rlz):.1f}" if phi + rlz else "",
                "ns_per_pattern_median": f"{median(ts):.1f}",
                "ns_per_pattern_min": f"{min(ts):.1f}",
                "ns_per_pattern_max": f"{max(ts):.1f}",
                "speedup_vs_Move_r": f"{move_time[m] / median(ts):.3f}",
                "speed_relative_to_Move_r_RLZ": f"{rlz_time[m] / median(ts):.3f}",
                "num_queries": intval(last, "num_patterns"),
                "num_occurrences": intval(last, "num_occurrences"),
                "occurrence_checksum": intval(last, "occurrence_checksum"),
                "correctness": "PASS" if correct else "FAIL",
            })
    write_csv(result_dir / "final_test_performance.csv", perf_rows)

    stats_rows: list[dict[str, object]] = []
    for m in WORKLOADS:
        detail = result_dir / f"occ-detail-m{m}.csv"
        detail.unlink(missing_ok=True)
        run([LOCATE, "-compare-detail", detail, "-m", result_dir / f"occ-stats-m{m}.result", text_name, built["move-r"], pattern_dir / f"{text_name}-m{m}-test-n500.patterns"], result_dir / f"occ-stats-m{m}.log")
        with detail.open(newline="", encoding="utf-8") as inp:
            occs = [int(row["occ"]) for row in csv.DictReader(inp)]
        stats_rows.append({
            "dataset": text_name,
            "workload": f"m{m}",
            "pattern_length": m,
            "num_patterns": len(occs),
            "occ_avg": f"{statistics.mean(occs):.2f}",
            "occ_min": min(occs),
            "occ_max": max(occs),
            "occ_median": f"{statistics.median(occs):.1f}",
        })
    write_csv(result_dir / "workload_occ_stats.csv", stats_rows)

    build_rows: list[dict[str, object]] = []
    for display, slug, _support, _kind, _gap, _budget in all_methods:
        rows = parse_result(result_dir / f"build-{slug}.result")
        row = rows[-1] if rows else {}
        build_rows.append({
            "dataset": text_name,
            "method": display,
            "index_MiB": f"{mib(built[slug].stat().st_size):.2f}",
            "build_time_ns": intval(row, "time_construction"),
            "peak_mem_usage_reported": intval(row, "peak_mem_usage"),
            "max_rss_kb_time_v": parse_rss(result_dir / f"build-{slug}.log"),
        })
    write_csv(result_dir / "build_summary_selected.csv", build_rows)
    write_summary(result_dir, text_name, selected, perf_rows, stats_rows, build_rows)


def write_summary(result_dir: Path, text_name: str, selected: dict[str, int], perf_rows: list[dict[str, object]], stats_rows: list[dict[str, object]], build_rows: list[dict[str, object]]) -> None:
    md = [
        f"# {text_name} m4/m8/m16/m32/m64/m128 selected-method experiment\n\n",
        "Methods: Move-r, Move-r-RLZ, Move-r-RLZEnd, r-RLZSA@25%, r-RLZSA@12.5%. `Move-r-RLZ` denotes the rlz-only (`locate_rlzsa`) index.\n\n",
        f"Selected theta: `{selected}`\n\n",
        "## Build And Space\n\n| method | index MiB | build time ns | max RSS KB |\n|---|---:|---:|---:|\n",
    ]
    for row in build_rows:
        md.append(f"| {row['method']} | {row['index_MiB']} | {row['build_time_ns']} | {row['max_rss_kb_time_v']} |\n")
    md.append("\n## Workload Occurrences\n\n| workload | avg | min | median | max |\n|---|---:|---:|---:|---:|\n")
    for row in stats_rows:
        md.append(f"| {row['workload']} | {row['occ_avg']} | {row['occ_min']} | {row['occ_median']} | {row['occ_max']} |\n")
    md.append("\n## Final Performance\n\n| workload | method | index MiB | theta | RLZSA % | ns/pat | speedup vs Move-r | vs Move-r-RLZ | correctness |\n|---:|---|---:|---:|---:|---:|---:|---:|---|\n")
    for m in WORKLOADS:
        for row in [r for r in perf_rows if r["workload"] == f"m{m}"]:
            theta = row["theta_rlz"] if row["theta_rlz"] != "" else "-"
            coverage = row["RLZSA_occ_coverage"] if row["RLZSA_occ_coverage"] != "" else "-"
            md.append(f"| {m} | {row['method']} | {row['index_MiB']} | {theta} | {coverage} | {row['ns_per_pattern_median']} | {row['speedup_vs_Move_r']} | {row['speed_relative_to_Move_r_RLZ']} | {row['correctness']} |\n")
    (result_dir / "final_test_summary.md").write_text("".join(md), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--text", required=True)
    parser.add_argument("--name", default=None)
    parser.add_argument("--out-name", required=True)
    parser.add_argument("--patterns", type=int, default=1000)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--build-threads", type=int, default=1)
    parser.add_argument("--reuse-indexes", action="store_true")
    args = parser.parse_args()

    text = TEXT_DIR / args.text
    if not text.exists():
        raise FileNotFoundError(text)
    text_name = args.name or args.text
    result_dir = RESULT_ROOT / args.out_name
    pattern_dir = PATTERN_ROOT / args.out_name
    index_dir = INDEX_ROOT / args.out_name
    result_dir.mkdir(parents=True, exist_ok=True)
    pattern_dir.mkdir(parents=True, exist_ok=True)
    index_dir.mkdir(parents=True, exist_ok=True)
    num_blocks = ((text.stat().st_size + 1) + 1023) // 1024
    all_methods = methods(num_blocks)
    print(f"[text] {text} {text.stat().st_size} bytes, build_threads={args.build_threads}, patterns={args.patterns}, repeats={args.repeats}", flush=True)
    print(f"[budget] r-RLZSA@25%={all_methods[3][5]}, r-RLZSA@12.5%={all_methods[4][5]}", flush=True)
    prepare_patterns(text, text_name, pattern_dir, result_dir, args.patterns, args.reuse_indexes)
    built = existing_indexes(text_name, index_dir, all_methods) if args.reuse_indexes else build_indexes(text, text_name, pattern_dir, index_dir, result_dir, args.build_threads, all_methods)
    selected = select_theta(text_name, pattern_dir, result_dir, built, args.repeats)
    final_test(text_name, pattern_dir, result_dir, built, selected, args.repeats, all_methods)
    print(result_dir / "final_test_summary.md", flush=True)


if __name__ == "__main__":
    main()
