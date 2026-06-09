#!/usr/bin/env python3
"""Run intuitive experiments for innovation point 2.

The experiment compares:
  1. Hybrid Move-r-rlzsa without adaptive samples.
  2. Score-adaptive RLZSA samples.
  3. Uniform-adaptive RLZSA samples under the same sample budget.

It writes raw CSV files and a Markdown report with compact tables and ASCII
bars, so the results can be read directly when writing the thesis chapter.
"""

from __future__ import annotations

import argparse
import csv
import os
import random
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEXT = REPO_ROOT / "measurements" / "texts" / "einstein.en.txt"
DEFAULT_TRAIN = REPO_ROOT / "measurements" / "patterns" / "einstein.en.txt-patterns-phi"
DEFAULT_TESTS = {
    "bal": REPO_ROOT / "measurements" / "patterns" / "einstein.en.txt-patterns-bal",
    "phi": REPO_ROOT / "measurements" / "patterns" / "einstein.en.txt-patterns-phi",
}


@dataclass(frozen=True)
class Config:
    name: str
    support: str = "locate_rlzsa"
    hybrid: bool = True
    adaptive: bool = False
    strategy: str = ""
    budget: int = 0
    max_distance: int = 0


def run(cmd: list[str], log_path: Path, *, cwd: Path = REPO_ROOT) -> float:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    with log_path.open("w", encoding="utf-8", errors="replace") as log:
        log.write("$ " + " ".join(shlex.quote(part) for part in cmd) + "\n\n")
        log.flush()
        proc = subprocess.run(cmd, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, text=True, check=False)
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
    data: dict[str, str] = {}
    for token in result_line.split()[1:]:
        if "=" in token:
            key, value = token.split("=", 1)
            data[key] = value
    return data


def as_float(row: dict[str, object], key: str) -> float | None:
    value = row.get(key)
    if value in (None, ""):
        return None
    try:
        return float(str(value))
    except ValueError:
        return None


def fmt_float(value: float | None, digits: int = 2) -> str:
    if value is None:
        return "n/a"
    return f"{value:.{digits}f}"


def fmt_ratio(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.3f}x"


def fmt_percent(value: float | None) -> str:
    if value is None:
        return "n/a"
    return f"{value:.2f}%"


def parse_int_list(value: str) -> list[int]:
    return [int(part) for part in value.split(",") if part.strip()]


def parse_str_list(value: str) -> list[str]:
    return [part.strip() for part in value.split(",") if part.strip()]


def parse_named_paths(values: list[str] | None) -> dict[str, Path]:
    if not values:
        return dict(DEFAULT_TESTS)
    tests: dict[str, Path] = {}
    for item in values:
        if "=" not in item:
            raise ValueError(f"test pattern must be name=path, got: {item}")
        name, path = item.split("=", 1)
        tests[name] = Path(path)
    return tests


def ensure_inputs(paths: Iterable[Path]) -> None:
    missing = [path for path in paths if not path.exists()]
    if missing:
        joined = "\n".join(f"  - {path}" for path in missing)
        raise FileNotFoundError(f"missing required files:\n{joined}")


def read_patterns(path: Path) -> tuple[str, int, list[bytes]]:
    data = path.read_bytes()
    header, payload = data.split(b"\n", 1)
    header_text = header.decode("utf-8", errors="replace")
    number_match = re.search(r"number=(\d+)", header_text)
    length_match = re.search(r"length=(\d+)", header_text)
    if not number_match or not length_match:
        raise ValueError(f"invalid Pizza&Chili pattern header: {path}")
    number = int(number_match.group(1))
    length = int(length_match.group(1))
    patterns = [payload[i * length:(i + 1) * length] for i in range(number)]
    patterns = [pattern for pattern in patterns if len(pattern) == length]
    return header_text, length, patterns


def write_patterns(path: Path, length: int, patterns: list[bytes], source_name: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    header = f"# number={len(patterns)} length={length} file={source_name} forbidden=\n".encode()
    path.write_bytes(header + b"".join(patterns))


def make_hotspot_workloads(args: argparse.Namespace) -> tuple[Path, dict[str, Path]]:
    _, length, source_patterns = read_patterns(args.hotspot_source)
    if len(source_patterns) < args.hotspot_count + 1:
        raise ValueError("hotspot source has too few patterns")

    rng = random.Random(args.hotspot_seed)
    hot = source_patterns[:args.hotspot_count]
    cold = source_patterns[args.hotspot_count:]

    def draw(total: int, hot_ratio: float) -> list[bytes]:
        out: list[bytes] = []
        for _ in range(total):
            if rng.random() < hot_ratio:
                out.append(rng.choice(hot))
            else:
                out.append(rng.choice(cold))
        return out

    train = args.output_dir / "generated" / "hotspot-train.patterns"
    test_hot = args.output_dir / "generated" / "hotspot-test.patterns"
    test_mixed = args.output_dir / "generated" / "mixed-test.patterns"
    write_patterns(train, length, draw(args.hotspot_train_queries, 1.0), args.hotspot_source.name)
    write_patterns(test_hot, length, draw(args.hotspot_test_queries, 1.0), args.hotspot_source.name)
    write_patterns(test_mixed, length, draw(args.hotspot_test_queries, args.hotspot_ratio), args.hotspot_source.name)
    return train, {"hotspot": test_hot, "mixed": test_mixed}


def index_prefix(config: Config, args: argparse.Namespace) -> Path:
    return args.index_dir / (
        f"{args.text_name}.a{args.a}.{config.name}"
        f".min{args.adaptive_min_occ}.max{args.adaptive_max_occ}"
        f".d{config.max_distance}.k{config.budget}"
    )


def build_index(config: Config, args: argparse.Namespace) -> tuple[Path, dict[str, object]]:
    move_r_build = REPO_ROOT / "build" / "cli" / "move-r-build"
    prefix = index_prefix(config, args)
    index_path = Path(str(prefix) + ".move-r-rlzsa")
    metrics_path = args.output_dir / f"build-{config.name}.result"
    mds_path = args.output_dir / f"build-{config.name}.mds.result"
    log_path = args.output_dir / f"build-{config.name}.log"

    if args.reuse_indexes and index_path.exists():
        row = parse_result_file(metrics_path)
        row.update({"reused": 1, "wall_seconds": "0.000000"})
    else:
        metrics_path.unlink(missing_ok=True)
        mds_path.unlink(missing_ok=True)
        cmd = [
            str(move_r_build),
            "-s", config.support,
            "-p", str(args.threads),
            "-a", str(args.a),
            "-o", str(prefix),
            "-m_idx", str(metrics_path),
            "-m_mds", str(mds_path),
            "-hybrid",
            "-hybrid-thr", str(args.hybrid_thr),
            "-hybrid-cost",
            str(args.hybrid_cost_phi),
            str(args.hybrid_cost_rlz_init),
            str(args.hybrid_cost_rlz_phrase),
            str(args.hybrid_cost_rlz_decode),
        ]
        if config.adaptive:
            cmd.extend([
                "-adaptive-samples", str(args.train_patterns),
                str(config.budget),
                "-adaptive-strategy", config.strategy,
                "-adaptive-min-occ", str(args.adaptive_min_occ),
                "-adaptive-max-occ", str(args.adaptive_max_occ),
                "-adaptive-max-distance", str(config.max_distance),
            ])
        cmd.append(str(args.text))
        print(f"[build] {config.name}")
        wall = run(cmd, log_path)
        row = parse_result_file(metrics_path)
        row.update({"wall_seconds": f"{wall:.6f}", "reused": 0})

    row.update({
        "phase": "build",
        "config": config.name,
        "adaptive": int(config.adaptive),
        "adaptive_strategy": config.strategy,
        "adaptive_budget": config.budget,
        "adaptive_max_distance": config.max_distance,
        "index_path": str(index_path),
        "index_bytes": index_path.stat().st_size if index_path.exists() else "",
        "log": str(log_path),
    })
    return index_path, row


def locate(config: Config, index_path: Path, workload: str, patterns: Path, args: argparse.Namespace) -> dict[str, object]:
    move_r_locate = REPO_ROOT / "build" / "cli" / "move-r-locate"
    result_path = args.output_dir / f"locate-{config.name}-{workload}.result"
    log_path = args.output_dir / f"locate-{config.name}-{workload}.log"
    result_path.unlink(missing_ok=True)
    cmd = [str(move_r_locate), "-m", str(result_path), args.text_name]
    if args.check:
        cmd.extend(["-i", str(args.text), "-c"])
    if args.compare_forced:
        cmd.append("-compare")
    cmd.extend([str(index_path), str(patterns)])

    print(f"[locate] {config.name} / {workload}")
    wall = run(cmd, log_path)
    row = parse_result_file(result_path)
    row.update({
        "phase": "locate",
        "config": config.name,
        "workload": workload,
        "patterns_path": str(patterns),
        "adaptive": int(config.adaptive),
        "adaptive_strategy": config.strategy,
        "adaptive_budget": config.budget,
        "adaptive_max_distance": config.max_distance,
        "index_path": str(index_path),
        "index_bytes": index_path.stat().st_size if index_path.exists() else "",
        "wall_seconds": f"{wall:.6f}",
        "log": str(log_path),
    })
    return row


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


def ns_per_pattern(row: dict[str, object]) -> float | None:
    time_locate = as_float(row, "time_locate")
    num_patterns = as_float(row, "num_patterns")
    if time_locate is None or not num_patterns:
        return None
    return time_locate / num_patterns


def hit_rate(row: dict[str, object]) -> float | None:
    hits = as_float(row, "adaptive_sample_hits")
    queries = as_float(row, "adaptive_sample_queries")
    if hits is None or not queries:
        return None
    return 100.0 * hits / queries


def ascii_bar(value: float | None, max_value: float | None, width: int = 28) -> str:
    if value is None or not max_value:
        return ""
    used = max(1, round(width * value / max_value))
    return "#" * used


def summarize(args: argparse.Namespace, build_rows: list[dict[str, object]], locate_rows: list[dict[str, object]]) -> None:
    md = args.output_dir / "summary.md"
    by_workload: dict[str, list[dict[str, object]]] = {}
    for row in locate_rows:
        by_workload.setdefault(str(row["workload"]), []).append(row)

    lines: list[str] = []
    lines.append("# Vr-RLZSA Adaptive Sampling Experiment")
    lines.append("")
    lines.append("This report compares no adaptive sampling, score-adaptive sampling, and uniform-adaptive sampling under the same sample budgets.")
    lines.append("")
    lines.append("## Settings")
    lines.append("")
    lines.append(f"- text: `{args.text}`")
    lines.append(f"- training patterns: `{args.train_patterns}`")
    lines.append(f"- budgets: `{','.join(map(str, args.budgets))}`")
    lines.append(f"- max distances: `{','.join(map(str, args.max_distances))}`")
    lines.append(f"- occurrence window: [{args.adaptive_min_occ}, {args.adaptive_max_occ}]")
    lines.append("")

    lines.append("## Build Overview")
    lines.append("")
    lines.append("| config | strategy | K | max_dist | index MiB | build wall s |")
    lines.append("|---|---|---:|---:|---:|---:|")
    for row in build_rows:
        index_mib = as_float(row, "index_bytes")
        index_mib = None if index_mib is None else index_mib / (1024 * 1024)
        lines.append(
            f"| {row.get('config', '')} | {row.get('adaptive_strategy', '')} | "
            f"{row.get('adaptive_budget', '')} | {row.get('adaptive_max_distance', '')} | "
            f"{fmt_float(index_mib, 2)} | {row.get('wall_seconds', 'n/a')} |"
        )
    lines.append("")

    for workload, rows in by_workload.items():
        base = next((row for row in rows if row["config"] == "hybrid"), None)
        base_time = as_float(base or {}, "time_locate")
        max_ns = max((ns_per_pattern(row) or 0.0) for row in rows) or None

        lines.append(f"## Workload: {workload}")
        lines.append("")
        lines.append("| config | strategy | K | max_dist | ns/pattern | speedup vs hybrid | hit % | exact | pred | skipped | miss |")
        lines.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for row in sorted(rows, key=lambda r: (str(r.get("adaptive_strategy", "")), int(r.get("adaptive_budget") or 0), int(r.get("adaptive_max_distance") or 0))):
            nsp = ns_per_pattern(row)
            time_locate = as_float(row, "time_locate")
            speedup = None if base_time is None or time_locate in (None, 0) else base_time / time_locate
            lines.append(
                f"| {row.get('config', '')} | {row.get('adaptive_strategy', '')} | "
                f"{row.get('adaptive_budget', '')} | {row.get('adaptive_max_distance', '')} | "
                f"{fmt_float(nsp, 1)} | {fmt_ratio(speedup)} | {fmt_percent(hit_rate(row))} | "
                f"{row.get('adaptive_sample_exact_hits', 'n/a')} | {row.get('adaptive_sample_predecessor_hits', 'n/a')} | "
                f"{row.get('adaptive_sample_skipped_by_occ', 'n/a')} | {row.get('adaptive_sample_misses', 'n/a')} |"
            )
        lines.append("")
        lines.append("Locate time bars, lower is better:")
        lines.append("")
        lines.append("```text")
        for row in sorted(rows, key=lambda r: ns_per_pattern(r) or float("inf")):
            nsp = ns_per_pattern(row)
            label = f"{row.get('config', '')} k={row.get('adaptive_budget', '')} d={row.get('adaptive_max_distance', '')}"
            lines.append(f"{label[:34]:34} {fmt_float(nsp, 1):>10} ns  {ascii_bar(nsp, max_ns)}")
        lines.append("```")
        lines.append("")

        adaptive_rows = [row for row in rows if row.get("adaptive") == 1]
        if adaptive_rows:
            best = min(adaptive_rows, key=lambda row: ns_per_pattern(row) or float("inf"))
            best_speedup = None
            if base_time and as_float(best, "time_locate"):
                best_speedup = base_time / as_float(best, "time_locate")
            lines.append(
                f"Best adaptive config for `{workload}`: `{best.get('config')}` "
                f"(strategy={best.get('adaptive_strategy')}, K={best.get('adaptive_budget')}, "
                f"max_dist={best.get('adaptive_max_distance')}), speedup vs hybrid {fmt_ratio(best_speedup)}, "
                f"hit rate {fmt_percent(hit_rate(best))}."
            )
            lines.append("")

    md.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run intuitive experiments for innovation point 2 adaptive sampling.")
    parser.add_argument("--text", type=Path, default=DEFAULT_TEXT)
    parser.add_argument("--text-name", default="einstein.en.txt")
    parser.add_argument("--train-patterns", type=Path, default=DEFAULT_TRAIN)
    parser.add_argument("--test-pattern", action="append", help="test workload in name=path form; can be repeated")
    parser.add_argument("--output-dir", type=Path, default=REPO_ROOT / "measurements" / "results" / "innovation2_adaptive")
    parser.add_argument("--index-dir", type=Path, default=REPO_ROOT / "measurements" / "indexes" / "innovation2_adaptive")
    parser.add_argument("--budgets", type=parse_int_list, default=parse_int_list("1000,5000,10000"))
    parser.add_argument("--max-distances", type=parse_int_list, default=parse_int_list("0,8"))
    parser.add_argument("--strategies", type=parse_str_list, default=parse_str_list("score,uniform"))
    parser.add_argument("--adaptive-min-occ", type=int, default=16)
    parser.add_argument("--adaptive-max-occ", type=int, default=4096)
    parser.add_argument("--threads", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--a", type=int, default=8)
    parser.add_argument("--reuse-indexes", action="store_true")
    parser.add_argument("--skip-cmake-build", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--compare-forced", action="store_true")
    parser.add_argument("--hybrid-thr", type=int, default=32)
    parser.add_argument("--hybrid-cost-phi", type=float, default=7.0)
    parser.add_argument("--hybrid-cost-rlz-init", type=float, default=48.0)
    parser.add_argument("--hybrid-cost-rlz-phrase", type=float, default=4.0)
    parser.add_argument("--hybrid-cost-rlz-decode", type=float, default=1.25)

    parser.add_argument("--make-hotspot-workloads", action="store_true")
    parser.add_argument("--hotspot-source", type=Path, default=DEFAULT_TRAIN)
    parser.add_argument("--hotspot-count", type=int, default=64)
    parser.add_argument("--hotspot-ratio", type=float, default=0.8)
    parser.add_argument("--hotspot-train-queries", type=int, default=10000)
    parser.add_argument("--hotspot-test-queries", type=int, default=10000)
    parser.add_argument("--hotspot-seed", type=int, default=13)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.index_dir.mkdir(parents=True, exist_ok=True)

    test_patterns = parse_named_paths(args.test_pattern)
    if args.make_hotspot_workloads:
        args.train_patterns, test_patterns = make_hotspot_workloads(args)

    ensure_inputs([args.text, args.train_patterns, *test_patterns.values()])

    if not args.skip_cmake_build:
        print("[build] CLI targets")
        run(
            ["cmake", "--build", "build", "--target", "move-r-build", "move-r-locate", "-j", str(args.threads)],
            args.output_dir / "cmake-build.log",
        )

    configs = [Config("hybrid", adaptive=False)]
    for budget in args.budgets:
        for max_distance in args.max_distances:
            for strategy in args.strategies:
                configs.append(Config(
                    f"{strategy}-k{budget}-d{max_distance}",
                    adaptive=True,
                    strategy=strategy,
                    budget=budget,
                    max_distance=max_distance,
                ))

    build_rows: list[dict[str, object]] = []
    locate_rows: list[dict[str, object]] = []
    index_paths: dict[str, Path] = {}

    for config in configs:
        index_path, row = build_index(config, args)
        index_paths[config.name] = index_path
        build_rows.append(row)

    for workload, patterns in test_patterns.items():
        for config in configs:
            locate_rows.append(locate(config, index_paths[config.name], workload, patterns, args))

    build_csv = args.output_dir / "build.csv"
    locate_csv = args.output_dir / "locate.csv"
    write_csv(build_csv, build_rows)
    write_csv(locate_csv, locate_rows)
    summarize(args, build_rows, locate_rows)

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
