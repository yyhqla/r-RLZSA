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


METHODS = [
    ("Move-r", "move-r", "locate_move", "", 0, None),
    ("Move-r-LZEnd", "move-r-lzend", "locate_lzendsa", "", 0, None),
    ("Move-r-RLZ", "move-r-rlz", "locate_rlzsa", "", 0, None),
    ("Periodic-LocalR-g4", "periodic-g4", "locate_rlzsa", "periodic", 4, None),
    ("Periodic-LocalR-g8", "periodic-g8", "locate_rlzsa", "periodic", 8, None),
    ("Adaptive-Mixed@25%", "r-rlzsa25", "locate_rlzsa", "adaptive", 4, 25601),
    ("Adaptive-Mixed@12.5%", "r-rlzsa12", "locate_rlzsa", "adaptive", 8, 12801),
]


def run(cmd: list[str], log: Path, cwd: Path = ROOT) -> float:
    log.parent.mkdir(parents=True, exist_ok=True)
    start = time.perf_counter()
    with log.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(shlex.quote(x) for x in cmd) + "\n\n")
        out.flush()
        proc = subprocess.run(cmd, cwd=cwd, stdout=out, stderr=subprocess.STDOUT, text=True)
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
                k, v = token.split("=", 1)
                row[k] = v
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
    n = int(re.search(r"number=(\d+)", text).group(1))
    m = int(re.search(r"length=(\d+)", text).group(1))
    return n, m, [body[i * m:(i + 1) * m] for i in range(n)]


def write_patterns(path: Path, length: int, patterns: list[bytes], text_name: str, note: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    header = f"# number={len(patterns)} length={length} file={text_name} {note} forbidden=\n".encode()
    path.write_bytes(header + b"".join(patterns))


def ensure_text(text_name: str, source_name: str | None, max_bytes: int, result_dir: Path) -> Path:
    out = TEXT_DIR / text_name
    source = TEXT_DIR / source_name if source_name else out
    lines = []
    if out.exists():
        lines.append(f"- Reused text: `{out}` ({out.stat().st_size} bytes)\n")
    elif source.exists():
        with source.open("rb") as src, out.open("wb") as dst:
            dst.write(src.read(max_bytes))
        lines.append(f"- Created text: `head -c {max_bytes} {source} > {out}`\n")
    else:
        raise FileNotFoundError(f"missing source text for {text_name}: {source}")
    (result_dir / "text_prepare.md").write_text("".join(lines), encoding="utf-8")
    if out.stat().st_size > max_bytes:
        raise RuntimeError(f"{out} exceeds max bytes {max_bytes}")
    return out


def prepare_patterns(text: Path, text_name: str, pattern_dir: Path, result_dir: Path, reuse: bool) -> None:
    pattern_dir.mkdir(parents=True, exist_ok=True)
    for m in (8, 100, 200):
        raw = pattern_dir / f"{text_name}-m{m}-n1000.patterns"
        if not (reuse and raw.exists()):
            run([str(GEN_PATTERNS), str(text), str(m), "1000", str(raw)], result_dir / f"gen-patterns-m{m}.log")
        _, length, pats = read_patterns(raw)
        ids = list(range(len(pats)))
        random.Random(20260530).shuffle(ids)
        train500 = sorted(ids[:500])
        test500 = sorted(ids[500:])
        write_patterns(pattern_dir / f"{text_name}-m{m}-train-n500.patterns", length, [pats[i] for i in train500], text_name, "split=train500 seed=20260530")
        write_patterns(pattern_dir / f"{text_name}-m{m}-test-n500.patterns", length, [pats[i] for i in test500], text_name, "split=test500 seed=20260530")
        ids2 = list(range(500))
        random.Random(20260531).shuffle(ids2)
        train400 = sorted(ids2[:400])
        valid100 = sorted(ids2[400:])
        train_pats = [pats[i] for i in train500]
        write_patterns(pattern_dir / f"{text_name}-m{m}-train400.patterns", length, [train_pats[i] for i in train400], text_name, "split=train400 seed=20260531")
        write_patterns(pattern_dir / f"{text_name}-m{m}-valid100.patterns", length, [train_pats[i] for i in valid100], text_name, "split=valid100 seed=20260531")
    for split in ("train", "valid"):
        manifest = pattern_dir / f"mixed_{split}.manifest"
        body = ["# mixed_patterns_manifest=1\n"]
        suffix = "train400" if split == "train" else "valid100"
        for m in (8, 100, 200):
            body.append(str((pattern_dir / f"{text_name}-m{m}-{suffix}.patterns").resolve()) + "\n")
        manifest.write_text("".join(body), encoding="utf-8")


def index_path(index_dir: Path, text_name: str, slug: str, support: str) -> Path:
    suffix = ".move-r-rlzsa" if support == "locate_rlzsa" else ".move-r"
    return Path(str(index_dir / f"{text_name}.a8.{slug}") + suffix)


def build_indexes(text: Path, text_name: str, pattern_dir: Path, index_dir: Path, result_dir: Path, reuse: bool) -> dict[str, Path]:
    built: dict[str, Path] = {}
    for display, slug, support, kind, gap, budget in METHODS:
        idx = index_path(index_dir, text_name, slug, support)
        built[slug] = idx
        if reuse and idx.exists():
            continue
        result = result_dir / f"build-{slug}.result"
        mds = result_dir / f"build-{slug}.mds"
        result.unlink(missing_ok=True)
        cmd = [str(BUILD), "-s", support, "-p", "1", "-a", "8", "-o", str(index_dir / f"{text_name}.a8.{slug}"), "-m_idx", str(result), "-m_mds", str(mds)]
        if kind:
            cmd += ["-hybrid", "-hybrid-thr", "32", "-hybrid-min-occ", "2", "-hybrid-max-pattern", "64"]
            if kind == "periodic":
                cmd += ["-partial-rlzsa", "1024", str(gap)]
            else:
                cmd += ["-partial-rlzsa-adaptive", "1024", str(budget), str(pattern_dir / "mixed_train.manifest"), "-partial-train-thr", "100"]
        cmd.append(str(text))
        time_cmd = ["/usr/bin/time", "-v"] + cmd
        run(time_cmd, result_dir / f"build-{slug}.log")
    return built


def locate_one(text_name: str, slug: str, idx: Path, patterns: Path, result: Path, log: Path, kind: str, gap: int, theta: int | None) -> None:
    cmd = [str(LOCATE)]
    if kind:
        cmd += ["-block-hybrid", "1024", str(gap), "100", "-block-hybrid-rlz-thr", str(theta or 0)]
    cmd += ["-m", str(result), text_name, str(idx), str(patterns)]
    run(cmd, log)


def select_theta(text_name: str, pattern_dir: Path, index_dir: Path, result_dir: Path, built: dict[str, Path], repeats: int) -> dict[str, int]:
    selected: dict[str, int] = {}
    for m in (8, 100, 200):
        result = result_dir / f"valid-move-r-m{m}.result"
        result.unlink(missing_ok=True)
        for rep in range(repeats):
            locate_one(text_name, "move-r", built["move-r"], pattern_dir / f"{text_name}-m{m}-valid100.patterns", result, result_dir / f"valid-move-r-m{m}-rep{rep+1}.log", "", 0, None)
    move_total_by_rep = []
    move_rows = {m: parse_result(result_dir / f"valid-move-r-m{m}.result") for m in (8, 100, 200)}
    for rep in range(repeats):
        move_total_by_rep.append(sum(intval(move_rows[m][rep], "time_locate") for m in (8, 100, 200)))
    move_total = median([float(x) for x in move_total_by_rep])
    scan_rows: list[dict[str, object]] = []
    for slug, gap in (("r-rlzsa25", 4), ("r-rlzsa12", 8)):
        totals = []
        for theta in (0, 32, 64, 128, 256, 512, 1024):
            for m in (8, 100, 200):
                result = result_dir / f"valid-{slug}-m{m}-theta{theta}.result"
                result.unlink(missing_ok=True)
                for rep in range(repeats):
                    locate_one(text_name, slug, built[slug], pattern_dir / f"{text_name}-m{m}-valid100.patterns", result, result_dir / f"valid-{slug}-m{m}-theta{theta}-rep{rep+1}.log", "block", gap, theta)
            total_by_rep = []
            for rep in range(repeats):
                total_by_rep.append(sum(intval(parse_result(result_dir / f"valid-{slug}-m{m}-theta{theta}.result")[rep], "time_locate") for m in (8, 100, 200)))
            totals.append((theta, median([float(x) for x in total_by_rep])))
        best_theta, best_total = min(totals, key=lambda x: x[1])
        selected[slug] = best_theta
        for theta, total in totals:
            for m in (8, 100, 200):
                rows = parse_result(result_dir / f"valid-{slug}-m{m}-theta{theta}.result")
                ts = [intval(r, "time_locate") / intval(r, "num_patterns") for r in rows]
                last = rows[-1]
                phi, rlz = intval(last, "block_hybrid_phi_occurrences"), intval(last, "block_hybrid_rlzsa_occurrences")
                base_rows = move_rows[m]
                base_ts = [intval(r, "time_locate") / intval(r, "num_patterns") for r in base_rows]
                scan_rows.append({
                    "method": slug,
                    "theta_rlz": theta,
                    "validation_workload": f"m{m}",
                    "ns_per_pattern_median": f"{median(ts):.1f}",
                    "speedup_vs_move_r": f"{median(base_ts) / median(ts):.3f}",
                    "rlzsa_occ_coverage": f"{100 * rlz / (phi + rlz):.1f}" if phi + rlz else "0.0",
                    "selected_global_theta": int(theta == best_theta),
                    "mixed_total_time_median_ns": int(total),
                    "mixed_total_speedup_vs_move_r": f"{move_total / total:.3f}",
                })
    write_csv(result_dir / "validation_theta_scan.csv", scan_rows)
    (result_dir / "selected_theta.txt").write_text("\n".join(f"{k} {v}" for k, v in selected.items()) + "\n", encoding="utf-8")
    return selected


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


def parse_rss(log: Path) -> int:
    if not log.exists():
        return 0
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        if "Maximum resident set size" in line:
            return int(line.rsplit(":", 1)[1].strip())
    return 0


def final_test(text_name: str, pattern_dir: Path, index_dir: Path, result_dir: Path, built: dict[str, Path], selected: dict[str, int], repeats: int) -> None:
    for display, slug, support, kind, gap, _budget in METHODS:
        for m in (8, 100, 200):
            result = result_dir / f"test-{slug}-m{m}.result"
            result.unlink(missing_ok=True)
            theta = selected.get(slug, 0)
            for rep in range(repeats):
                locate_one(text_name, slug, built[slug], pattern_dir / f"{text_name}-m{m}-test-n500.patterns", result, result_dir / f"test-{slug}-m{m}-rep{rep+1}.log", kind, gap, theta)

    ref = {m: parse_result(result_dir / f"test-move-r-m{m}.result")[-1] for m in (8, 100, 200)}
    move_time = {}
    rlz_time = {}
    for m in (8, 100, 200):
        rows = parse_result(result_dir / f"test-move-r-m{m}.result")
        move_time[m] = median([intval(r, "time_locate") / intval(r, "num_patterns") for r in rows])
        rows = parse_result(result_dir / f"test-move-r-rlz-m{m}.result")
        rlz_time[m] = median([intval(r, "time_locate") / intval(r, "num_patterns") for r in rows])

    rows_out: list[dict[str, object]] = []
    for display, slug, support, kind, gap, _budget in METHODS:
        idx = built[slug]
        for m in (8, 100, 200):
            rows = parse_result(result_dir / f"test-{slug}-m{m}.result")
            ts = [intval(r, "time_locate") / intval(r, "num_patterns") for r in rows]
            last = rows[-1]
            phi, rlz = intval(last, "block_hybrid_phi_occurrences"), intval(last, "block_hybrid_rlzsa_occurrences")
            partial = sum(intval(last, k) for k in [
                "size_partial_offsets", "size_partial_copy_offsets", "size_partial_literal_offsets",
                "size_partial_block_ids", "size_partial_pt", "size_partial_cpl", "size_partial_sr", "size_partial_lp"])
            correct = (
                intval(last, "num_patterns") == intval(ref[m], "num_patterns") and
                intval(last, "num_occurrences") == intval(ref[m], "num_occurrences") and
                intval(last, "occurrence_checksum") == intval(ref[m], "occurrence_checksum")
            )
            rows_out.append({
                "dataset": text_name,
                "method": display,
                "workload": f"m{m}",
                "pattern_length": m,
                "index_MiB": f"{mib(idx.stat().st_size):.2f}",
                "move_r_base_MiB": f"{mib(intval(last, 'size_m_lf') + intval(last, 'size_l_') + intval(last, 'size_l_prev') + intval(last, 'size_l_next') + intval(last, 'size_sa_s')):.2f}",
                "reference_MiB": f"{mib(intval(last, 'size_r')):.2f}",
                "partial_data_MiB": f"{mib(partial):.2f}",
                "enhanced_blocks": intval(last, "size_partial_block_ids") // 4,
                "theta_rlz": selected.get(slug, "") if kind == "adaptive" else (0 if kind == "periodic" else ""),
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
    write_csv(result_dir / "final_test_performance.csv", rows_out)

    # Workload occurrence stats from Move-r test runs.
    stats = []
    for m in (8, 100, 200):
        detail = result_dir / f"occ-detail-m{m}.csv"
        detail.unlink(missing_ok=True)
        cmd = [str(LOCATE), "-compare-detail", str(detail), "-m", str(result_dir / f"occ-stats-m{m}.result"), text_name, str(built["move-r"]), str(pattern_dir / f"{text_name}-m{m}-test-n500.patterns")]
        run(cmd, result_dir / f"occ-stats-m{m}.log")
        occs = []
        with detail.open(newline="", encoding="utf-8") as inp:
            for row in csv.DictReader(inp):
                occs.append(int(row["occ"]))
        stats.append({
            "dataset": text_name, "workload": f"m{m}", "pattern_length": m, "num_patterns": len(occs),
            "occ_avg": f"{statistics.mean(occs):.2f}", "occ_min": min(occs), "occ_max": max(occs),
            "occ_median": f"{statistics.median(occs):.1f}",
        })
    write_csv(result_dir / "workload_occ_stats.csv", stats)

    build_rows = []
    for display, slug, support, kind, _gap, _budget in METHODS:
        br = parse_result(result_dir / f"build-{slug}.result")
        row = br[-1] if br else {}
        build_rows.append({
            "dataset": text_name,
            "method": display,
            "index_MiB": f"{mib(built[slug].stat().st_size):.2f}",
            "build_time_ns": intval(row, "time_construction"),
            "peak_mem_usage_reported": intval(row, "peak_mem_usage"),
            "max_rss_kb_time_v": parse_rss(result_dir / f"build-{slug}.log"),
        })
    write_csv(result_dir / "build_summary.csv", build_rows)

    write_summary(result_dir, text_name, rows_out, stats, build_rows)


def write_summary(result_dir: Path, text_name: str, perf: list[dict[str, object]], stats: list[dict[str, object]], build_rows: list[dict[str, object]]) -> None:
    md = [f"# r-RLZSA {text_name} Final Test\n\n"]
    md.append("Adaptive indexes use mixed_train once; theta_rlz is selected on mixed_valid once; test500 is only used for final reporting.\n\n")
    md.append("## Workload Occurrences\n\n| workload | avg | min | median | max |\n|---|---:|---:|---:|---:|\n")
    for s in stats:
        md.append(f"| {s['workload']} | {s['occ_avg']} | {s['occ_min']} | {s['occ_median']} | {s['occ_max']} |\n")
    md.append("\n## Final Performance\n\n| workload | method | index MiB | theta | RLZSA % | ns/pat | speedup vs Move-r | correctness |\n|---:|---|---:|---:|---:|---:|---:|---|\n")
    for m in ("m8", "m100", "m200"):
        for row in [r for r in perf if r["workload"] == m]:
            theta = row["theta_rlz"] if row["theta_rlz"] != "" else "-"
            coverage = row["RLZSA_occ_coverage"] if row["RLZSA_occ_coverage"] != "" else "-"
            md.append(f"| {m[1:]} | {row['method']} | {row['index_MiB']} | {theta} | {coverage} | {row['ns_per_pattern_median']} | {row['speedup_vs_Move_r']} | {row['correctness']} |\n")
    result_dir.joinpath("final_test_summary.md").write_text("".join(md), encoding="utf-8")


def run_dataset(name: str, source: str | None, args: argparse.Namespace) -> None:
    result_dir = RESULT_ROOT / f"r_rlzsa_{name}"
    pattern_dir = PATTERN_ROOT / f"r_rlzsa_{name}"
    index_dir = INDEX_ROOT / f"r_rlzsa_{name}"
    result_dir.mkdir(parents=True, exist_ok=True)
    pattern_dir.mkdir(parents=True, exist_ok=True)
    index_dir.mkdir(parents=True, exist_ok=True)
    text = ensure_text(name, source, args.max_bytes, result_dir)
    prepare_patterns(text, name, pattern_dir, result_dir, args.reuse)
    built = build_indexes(text, name, pattern_dir, index_dir, result_dir, args.reuse)
    selected = select_theta(name, pattern_dir, index_dir, result_dir, built, args.repeats)
    final_test(name, pattern_dir, index_dir, result_dir, built, selected, args.repeats)


def aggregate(dataset_names: list[str]) -> None:
    out_dir = RESULT_ROOT / "r_rlzsa_100MB_cross_dataset"
    out_dir.mkdir(parents=True, exist_ok=True)
    query_rows, build_rows, pareto_rows, space_rows = [], [], [], []
    for name in dataset_names:
        result_dir = RESULT_ROOT / f"r_rlzsa_{name}"
        perf = list(csv.DictReader((result_dir / "final_test_performance.csv").open()))
        builds = list(csv.DictReader((result_dir / "build_summary.csv").open()))
        query_rows.extend(perf)
        build_rows.extend(builds)
        seen = sorted({r["method"] for r in perf})
        for method in seen:
            rows = [r for r in perf if r["method"] == method]
            row = {"dataset": name, "method": method, "index_MiB": rows[0]["index_MiB"]}
            for r in rows:
                row[f"{r['workload']}_ns_per_pattern"] = r["ns_per_pattern_median"]
                row[f"{r['workload']}_speedup_vs_Move_r"] = r["speedup_vs_Move_r"]
            pareto_rows.append(row)
            space_rows.append({
                "dataset": name, "method": method, "index_MiB": rows[0]["index_MiB"],
                "reference_MiB": rows[0]["reference_MiB"], "partial_data_MiB": rows[0]["partial_data_MiB"],
                "enhanced_blocks": rows[0]["enhanced_blocks"],
            })
    write_csv(out_dir / "final_query_summary.csv", query_rows)
    write_csv(out_dir / "final_build_summary.csv", build_rows)
    write_csv(out_dir / "pareto_points.csv", pareto_rows)
    write_csv(out_dir / "final_space_summary.csv", space_rows)
    md = ["# r-RLZSA 100MB Cross Dataset Summary\n\n"]
    for name in dataset_names:
        md.append(f"- `{name}`: `{RESULT_ROOT / f'r_rlzsa_{name}'}`\n")
    md.append("\n`world_leaders_100MB` is not included unless a source text named `world_leaders` or `world_leaders_100MB` exists in `measurements/texts`.\n")
    (out_dir / "final_summary.md").write_text("".join(md), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--datasets", nargs="+", default=["boost_100MB"])
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--max-bytes", type=int, default=200 * 1024 * 1024)
    parser.add_argument("--reuse", action="store_true")
    args = parser.parse_args()

    available = []
    for name in args.datasets:
        source = None
        if name == "boost_100MB":
            source = "boost"
        elif name == "world_leaders_100MB":
            source = "world_leaders"
            if not (TEXT_DIR / name).exists() and not (TEXT_DIR / source).exists():
                print(f"[skip] {name}: missing measurements/texts/{source} or {name}")
                continue
        run_dataset(name, source, args)
        available.append(name)
    if available:
        aggregate(available)


if __name__ == "__main__":
    main()
