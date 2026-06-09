#!/usr/bin/env python3
"""Compare baseline Move-r with the hybrid Move-r-rlz implementation.

The default experiment uses measurements/texts/einstein.en.txt and the
pre-generated einstein.en.txt-patterns-bal / einstein.en.txt-patterns-phi files.
It builds comparable indexes, runs locate queries, and writes machine-readable
CSV plus a short Markdown summary.
"""

from __future__ import annotations

import argparse
import csv
import os
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEXT = REPO_ROOT / "measurements" / "texts" / "einstein.en.txt"
DEFAULT_PATTERNS = {
    "bal": REPO_ROOT / "measurements" / "patterns" / "einstein.en.txt-patterns-bal",
    "phi": REPO_ROOT / "measurements" / "patterns" / "einstein.en.txt-patterns-phi",
}


@dataclass(frozen=True)
class Config:
    name: str
    support: str
    hybrid: bool = False
    adaptive: bool = False
    adaptive_strategy: str = "score"


def run(cmd: list[str], log_path: Path, *, cwd: Path = REPO_ROOT) -> float:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    with log_path.open("w", encoding="utf-8", errors="replace") as log:
        log.write("$ " + " ".join(shlex.quote(part) for part in cmd) + "\n\n")
        log.flush()
        proc = subprocess.run(
            cmd,
            cwd=cwd,
            stdout=log,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    elapsed = time.perf_counter() - started
    if proc.returncode != 0:
        raise RuntimeError(f"command failed with exit code {proc.returncode}: {' '.join(cmd)}\nlog: {log_path}")
    return elapsed


def parse_result_file(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    result_line = ""
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("RESULT"):
            result_line = line
    if not result_line:
        return {}

    data: dict[str, str] = {}
    for token in result_line.split()[1:]:
        if "=" not in token:
            continue
        key, value = token.split("=", 1)
        data[key] = value
    return data


def as_int(row: dict[str, object], key: str) -> int | None:
    value = row.get(key)
    if value in (None, ""):
        return None
    try:
        return int(float(str(value)))
    except ValueError:
        return None


def as_float(row: dict[str, object], key: str) -> float | None:
    value = row.get(key)
    if value in (None, ""):
        return None
    try:
        return float(str(value))
    except ValueError:
        return None


def fmt_ratio(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.3f}x"


def fmt_percent(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.2f}%"


def fmt_float(value: float | None, digits: int = 2) -> str:
    if value is None:
        return "n/a"
    return f"{value:.{digits}f}"


def ensure_inputs(text: Path, patterns: Iterable[Path]) -> None:
    missing = [path for path in [text, *patterns] if not path.exists()]
    if missing:
        joined = "\n".join(f"  - {path}" for path in missing)
        raise FileNotFoundError(f"missing required input files:\n{joined}")


def build_index(
    config: Config,
    args: argparse.Namespace,
    index_dir: Path,
    result_dir: Path,
) -> dict[str, object]:
    move_r_build = REPO_ROOT / "build" / "cli" / "move-r-build"
    prefix = index_dir / f"{args.text_name}.a{args.a}.{config.name}"
    build_metrics = result_dir / f"build-{config.name}.result"
    mds_metrics = result_dir / f"build-{config.name}.mds.result"
    build_log = result_dir / f"build-{config.name}.log"

    cmd = [
        str(move_r_build),
        "-s",
        config.support,
        "-p",
        str(args.threads),
        "-a",
        str(args.a),
        "-o",
        str(prefix),
        "-m_idx",
        str(build_metrics),
        "-m_mds",
        str(mds_metrics),
    ]
    if config.hybrid:
        cmd.append("-hybrid")
        cmd.extend(["-hybrid-thr", str(args.hybrid_thr)])
        cmd.extend([
            "-hybrid-cost",
            str(args.hybrid_cost_phi),
            str(args.hybrid_cost_rlz_init),
            str(args.hybrid_cost_rlz_phrase),
            str(args.hybrid_cost_rlz_decode),
        ])
    if config.adaptive:
        cmd.extend(["-adaptive-samples", str(args.adaptive_patterns), str(args.adaptive_budget)])
        cmd.extend(["-adaptive-strategy", config.adaptive_strategy])
        cmd.extend(["-adaptive-min-occ", str(args.adaptive_min_occ)])
        cmd.extend(["-adaptive-max-occ", str(args.adaptive_max_occ)])
        cmd.extend(["-adaptive-max-distance", str(args.adaptive_max_distance)])
    cmd.append(str(args.text))

    for path in (build_metrics, mds_metrics):
        path.unlink(missing_ok=True)

    print(f"[build] {config.name}")
    wall_seconds = run(cmd, build_log)
    suffix = ".move-r-rlzsa" if config.support == "locate_rlzsa" else ".move-r"
    index_path = Path(str(prefix) + suffix)
    metrics = parse_result_file(build_metrics)
    metrics.update({
        "phase": "build",
        "config": config.name,
        "support": config.support,
        "hybrid": int(config.hybrid),
        "adaptive": int(config.adaptive),
        "adaptive_strategy": config.adaptive_strategy if config.adaptive else "",
        "index_path": str(index_path),
        "index_bytes": index_path.stat().st_size if index_path.exists() else "",
        "wall_seconds": f"{wall_seconds:.6f}",
        "log": str(build_log),
    })
    return metrics


def locate_patterns(
    config: Config,
    index_path: Path,
    pattern_kind: str,
    pattern_path: Path,
    args: argparse.Namespace,
    result_dir: Path,
) -> dict[str, object]:
    move_r_locate = REPO_ROOT / "build" / "cli" / "move-r-locate"
    locate_metrics = result_dir / f"locate-{config.name}-{pattern_kind}.result"
    locate_log = result_dir / f"locate-{config.name}-{pattern_kind}.log"
    locate_metrics.unlink(missing_ok=True)

    cmd = [
        str(move_r_locate),
        "-m",
        str(locate_metrics),
        args.text_name,
    ]
    if args.check:
        cmd.extend(["-i", str(args.text), "-c"])
    if args.compare_forced and config.hybrid:
        cmd.append("-compare")
    cmd.extend([str(index_path), str(pattern_path)])

    print(f"[locate] {config.name} / {pattern_kind}")
    wall_seconds = run(cmd, locate_log)
    metrics = parse_result_file(locate_metrics)
    metrics.update({
        "phase": "locate",
        "config": config.name,
        "support": config.support,
        "hybrid": int(config.hybrid),
        "adaptive": int(config.adaptive),
        "adaptive_strategy": config.adaptive_strategy if config.adaptive else "",
        "pattern_kind": pattern_kind,
        "patterns_path": str(pattern_path),
        "index_path": str(index_path),
        "index_bytes": index_path.stat().st_size if index_path.exists() else "",
        "wall_seconds": f"{wall_seconds:.6f}",
        "log": str(locate_log),
    })
    return metrics


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    keys: list[str] = []
    for row in rows:
        for key in row:
            if key not in keys:
                keys.append(key)
    with path.open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=keys)
        writer.writeheader()
        writer.writerows(rows)


def summarize(result_dir: Path, build_rows: list[dict[str, object]], locate_rows: list[dict[str, object]]) -> None:
    baseline_name = "move-r"
    hybrid_name = "move-r-rlzsa-hybrid"
    adaptive_name = "move-r-rlzsa-hybrid-adaptive"
    uniform_name = "move-r-rlzsa-hybrid-uniform"
    md = result_dir / "summary.md"
    lines: list[str] = []
    lines.append("# Einstein Move-r Comparison")
    lines.append("")
    lines.append("Baseline is original Move-r (`locate_move`). Current implementation is Hybrid Move-r-rlzsa.")
    lines.append("")
    lines.append("## Build")
    lines.append("")
    lines.append("| config | index MiB | construction s | script wall s | n | r | z | hybrid | adaptive | strategy |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---|")
    for row in build_rows:
        index_mib = as_float(row, "index_bytes")
        index_mib = None if index_mib is None else index_mib / (1024 * 1024)
        construction_ns = as_float(row, "time_construction")
        construction_s = None if construction_ns is None else construction_ns / 1_000_000_000
        lines.append(
            f"| {row.get('config', '')} | "
            f"{fmt_float(index_mib, 2)} | "
            f"{fmt_float(construction_s, 3)} | "
            f"{row.get('wall_seconds', 'n/a')} | "
            f"{row.get('n', 'n/a')} | {row.get('r', 'n/a')} | {row.get('z', 'n/a')} | "
            f"{row.get('hybrid', '0')} | {row.get('adaptive', '0')} | {row.get('adaptive_strategy', '')} |"
        )

    lines.append("")
    lines.append("## Locate")
    lines.append("")
    lines.append("| patterns | config | occ | locate ns | ns/pattern | ns/occ | phi queries | rlzsa queries | phrase estimates | phi occ % | speedup vs Move-r |")
    lines.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    by_pattern: dict[str, dict[str, dict[str, object]]] = {}
    for row in locate_rows:
        by_pattern.setdefault(str(row.get("pattern_kind", "")), {})[str(row.get("config", ""))] = row

    for pattern_kind, rows in by_pattern.items():
        base_time = as_float(rows.get(baseline_name, {}), "time_locate")
        for config, row in rows.items():
            time_locate = as_float(row, "time_locate")
            num_patterns = as_float(row, "num_patterns")
            num_occ = as_float(row, "num_occurrences")
            ns_per_pattern = None if time_locate is None or not num_patterns else time_locate / num_patterns
            ns_per_occ = None if time_locate is None or not num_occ else time_locate / num_occ
            speedup = None if base_time is None or time_locate in (None, 0) else base_time / time_locate
            phi_occ = as_float(row, "hybrid_phi_occurrences")
            phi_occ_ratio = None if phi_occ is None or not num_occ else (phi_occ / num_occ) * 100
            lines.append(
                f"| {pattern_kind} | {config} | {row.get('num_occurrences', 'n/a')} | "
                f"{row.get('time_locate', 'n/a')} | "
                f"{fmt_float(ns_per_pattern, 1)} | "
                f"{fmt_float(ns_per_occ, 1)} | "
                f"{row.get('hybrid_phi_queries', 'n/a')} | {row.get('hybrid_rlzsa_queries', 'n/a')} | "
                f"{row.get('hybrid_phrase_estimate_queries', 'n/a')} | "
                f"{fmt_percent(phi_occ_ratio)} | "
                f"{fmt_ratio(speedup)} |"
            )

        if baseline_name in rows and hybrid_name in rows:
            base = rows[baseline_name]
            hybrid = rows[hybrid_name]
            base_time = as_float(base, "time_locate")
            hybrid_time = as_float(hybrid, "time_locate")
            base_size = as_float(base, "index_bytes")
            hybrid_size = as_float(hybrid, "index_bytes")
            speedup = None if base_time is None or hybrid_time in (None, 0) else base_time / hybrid_time
            size_overhead = None if base_size in (None, 0) or hybrid_size is None else (hybrid_size / base_size - 1) * 100
            lines.append("")
            lines.append(
                f"For `{pattern_kind}`, Hybrid speedup vs original Move-r: {fmt_ratio(speedup)}; "
                f"index size overhead: {fmt_percent(size_overhead)}."
            )
            lines.append("")

        if hybrid_name in rows and adaptive_name in rows:
            hybrid = rows[hybrid_name]
            adaptive = rows[adaptive_name]
            hybrid_time = as_float(hybrid, "time_locate")
            adaptive_time = as_float(adaptive, "time_locate")
            speedup = None if hybrid_time is None or adaptive_time in (None, 0) else hybrid_time / adaptive_time
            hit_count = as_float(adaptive, "adaptive_sample_hits")
            query_count = as_float(adaptive, "adaptive_sample_queries")
            hit_rate = None if hit_count is None or not query_count else (hit_count / query_count) * 100
            pred_hits = as_float(adaptive, "adaptive_sample_predecessor_hits")
            exact_hits = as_float(adaptive, "adaptive_sample_exact_hits")
            skipped = as_float(adaptive, "adaptive_sample_skipped_by_occ")
            lines.append(
                f"For `{pattern_kind}`, Adaptive speedup vs Hybrid: {fmt_ratio(speedup)}; "
                f"adaptive sample hit rate: {fmt_percent(hit_rate)}; "
                f"exact/predecessor hits: {fmt_float(exact_hits, 0)} / {fmt_float(pred_hits, 0)}; "
                f"skipped by occ: {fmt_float(skipped, 0)}."
            )
            lines.append("")

        if uniform_name in rows and adaptive_name in rows:
            uniform = rows[uniform_name]
            adaptive = rows[adaptive_name]
            uniform_time = as_float(uniform, "time_locate")
            adaptive_time = as_float(adaptive, "time_locate")
            speedup = None if uniform_time is None or adaptive_time in (None, 0) else uniform_time / adaptive_time
            uniform_hits = as_float(uniform, "adaptive_sample_hits")
            uniform_queries = as_float(uniform, "adaptive_sample_queries")
            adaptive_hits = as_float(adaptive, "adaptive_sample_hits")
            adaptive_queries = as_float(adaptive, "adaptive_sample_queries")
            uniform_hit_rate = None if uniform_hits is None or not uniform_queries else (uniform_hits / uniform_queries) * 100
            adaptive_hit_rate = None if adaptive_hits is None or not adaptive_queries else (adaptive_hits / adaptive_queries) * 100
            lines.append(
                f"For `{pattern_kind}`, Score-adaptive speedup vs Uniform-adaptive: {fmt_ratio(speedup)}; "
                f"hit rates score/uniform: {fmt_percent(adaptive_hit_rate)} / {fmt_percent(uniform_hit_rate)}."
            )
            lines.append("")

    md.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare Move-r and Hybrid Move-r-rlzsa on einstein.en.txt.")
    parser.add_argument("--text", type=Path, default=DEFAULT_TEXT)
    parser.add_argument("--text-name", default="einstein.en.txt")
    parser.add_argument("--patterns", choices=["bal", "phi", "all"], default="all")
    parser.add_argument("--threads", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--a", type=int, default=8)
    parser.add_argument("--output-dir", type=Path, default=REPO_ROOT / "measurements" / "results" / "einstein_compare")
    parser.add_argument("--index-dir", type=Path, default=REPO_ROOT / "measurements" / "indexes" / "einstein_compare")
    parser.add_argument("--reuse-indexes", action="store_true", help="skip builds when expected index files already exist")
    parser.add_argument("--skip-cmake-build", action="store_true", help="do not rebuild CLI binaries before running")
    parser.add_argument("--check", action="store_true", help="enable occurrence correctness checking; this loads the full text for locate")
    parser.add_argument("--compare-forced", action="store_true", help="for hybrid indexes, also time forced Phi and forced RLZSA locate")
    parser.add_argument("--include-plain-rlzsa", action="store_true", help="also build and query non-hybrid locate_rlzsa")
    parser.add_argument("--include-adaptive", action="store_true", help="also build and query Hybrid + Adaptive RLZSA samples")
    parser.add_argument("--include-uniform-adaptive", action="store_true", help="also build and query a uniform adaptive-sampling baseline")
    parser.add_argument("--adaptive-patterns", type=Path, default=DEFAULT_PATTERNS["phi"])
    parser.add_argument("--adaptive-budget", type=int, default=10000)
    parser.add_argument("--adaptive-min-occ", type=int, default=16)
    parser.add_argument("--adaptive-max-occ", type=int, default=4096)
    parser.add_argument("--adaptive-max-distance", type=int, default=0)
    parser.add_argument("--hybrid-thr", type=int, default=32)
    parser.add_argument("--hybrid-cost-phi", type=float, default=7.0)
    parser.add_argument("--hybrid-cost-rlz-init", type=float, default=48.0)
    parser.add_argument("--hybrid-cost-rlz-phrase", type=float, default=4.0)
    parser.add_argument("--hybrid-cost-rlz-decode", type=float, default=1.25)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    pattern_items = DEFAULT_PATTERNS.items() if args.patterns == "all" else [(args.patterns, DEFAULT_PATTERNS[args.patterns])]
    pattern_items = list(pattern_items)
    required_patterns = [path for _, path in pattern_items]
    if args.include_adaptive or args.include_uniform_adaptive:
        required_patterns.append(args.adaptive_patterns)
    ensure_inputs(args.text, required_patterns)

    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.index_dir.mkdir(parents=True, exist_ok=True)

    if not args.skip_cmake_build:
        print("[build] CLI targets")
        run(
            ["cmake", "--build", "build", "--target", "move-r-build", "move-r-locate", "-j", str(args.threads)],
            args.output_dir / "cmake-build.log",
        )

    configs = [
        Config("move-r", "locate_move"),
        Config("move-r-rlzsa-hybrid", "locate_rlzsa", hybrid=True),
    ]
    if args.include_plain_rlzsa:
        configs.insert(1, Config("move-r-rlzsa", "locate_rlzsa"))
    if args.include_adaptive:
        configs.append(Config("move-r-rlzsa-hybrid-adaptive", "locate_rlzsa", hybrid=True, adaptive=True, adaptive_strategy="score"))
    if args.include_uniform_adaptive:
        configs.append(Config("move-r-rlzsa-hybrid-uniform", "locate_rlzsa", hybrid=True, adaptive=True, adaptive_strategy="uniform"))

    build_rows: list[dict[str, object]] = []
    locate_rows: list[dict[str, object]] = []
    index_paths: dict[str, Path] = {}

    for config in configs:
        suffix = ".move-r-rlzsa" if config.support == "locate_rlzsa" else ".move-r"
        prefix = args.index_dir / f"{args.text_name}.a{args.a}.{config.name}"
        expected_index = Path(str(prefix) + suffix)
        if args.reuse_indexes and expected_index.exists():
            print(f"[build] {config.name} (reusing {expected_index})")
            build_metrics = args.output_dir / f"build-{config.name}.result"
            row = parse_result_file(build_metrics)
            row.update({
                "phase": "build",
                "config": config.name,
                "support": config.support,
                "hybrid": int(config.hybrid),
                "adaptive": int(config.adaptive),
                "adaptive_strategy": config.adaptive_strategy if config.adaptive else "",
                "index_path": str(expected_index),
                "index_bytes": expected_index.stat().st_size,
                "wall_seconds": "0.000000",
                "reused": 1,
            })
        else:
            row = build_index(config, args, args.index_dir, args.output_dir)
        build_rows.append(row)
        index_paths[config.name] = Path(str(row["index_path"]))

    for pattern_kind, pattern_path in pattern_items:
        for config in configs:
            row = locate_patterns(config, index_paths[config.name], pattern_kind, pattern_path, args, args.output_dir)
            locate_rows.append(row)

    build_csv = args.output_dir / "build.csv"
    locate_csv = args.output_dir / "locate.csv"
    write_csv(build_csv, build_rows)
    write_csv(locate_csv, locate_rows)
    summarize(args.output_dir, build_rows, locate_rows)

    print("")
    print(f"Wrote build data:  {build_csv}")
    print(f"Wrote locate data: {locate_csv}")
    print(f"Wrote summary:     {args.output_dir / 'summary.md'}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
