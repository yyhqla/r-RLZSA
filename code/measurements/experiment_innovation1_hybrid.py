#!/usr/bin/env python3
"""Run experiments for innovation point 1 on the Einstein dataset.

The report focuses on the cost-aware Phi/RLZSA hybrid locate strategy:
  - baseline Move-r (Phi locate)
  - plain Move-r-rlzsa
  - Hybrid Move-r-rlzsa
  - forced Phi / forced RLZSA timings from the hybrid index
  - routing ratios and a weighted mixed-workload summary
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


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_TEXT = REPO_ROOT / "measurements" / "texts" / "einstein.en.txt"


@dataclass(frozen=True)
class Config:
    name: str
    support: str
    hybrid: bool = False
    auto_rlzsa_only: bool = False


CONFIGS = [
    Config("move-r", "locate_move"),
    Config("move-r-rlzsa", "locate_rlzsa"),
    Config("move-r-rlzsa-hybrid", "locate_rlzsa", hybrid=True),
]


def experiment_configs(args: argparse.Namespace) -> list[Config]:
    configs = list(CONFIGS)
    if args.auto_rlzsa_only:
        configs.append(Config("move-r-rlzsa-auto", "locate_rlzsa", auto_rlzsa_only=True))
    return configs


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
    out: dict[str, str] = {}
    for token in result_line.split()[1:]:
        if "=" in token:
            key, value = token.split("=", 1)
            out[key] = value
    return out


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


def pattern_path(args: argparse.Namespace, length: int) -> Path:
    return args.pattern_dir / f"{args.text_name}-m{length}-n{args.num_patterns}.patterns"


def ensure_patterns(args: argparse.Namespace) -> dict[str, Path]:
    gen_patterns = REPO_ROOT / "build" / "cli" / "gen-patterns"
    patterns: dict[str, Path] = {}
    for length in args.lengths:
        out = pattern_path(args, length)
        patterns[f"m{length}"] = out
        if args.reuse_patterns and out.exists():
            continue
        print(f"[patterns] m={length}")
        run(
            [str(gen_patterns), str(args.text), str(length), str(args.num_patterns), str(out)],
            args.output_dir / f"gen-patterns-m{length}.log",
        )
    return patterns


def index_path(args: argparse.Namespace, config: Config) -> Path:
    suffix = ".move-r-rlzsa" if config.support == "locate_rlzsa" else ".move-r"
    return Path(str(args.index_dir / f"{args.text_name}.a{args.a}.{config.name}") + suffix)


def build_index(args: argparse.Namespace, config: Config) -> tuple[Path, dict[str, object]]:
    move_r_build = REPO_ROOT / "build" / "cli" / "move-r-build"
    prefix = args.index_dir / f"{args.text_name}.a{args.a}.{config.name}"
    out_index = index_path(args, config)
    result = args.output_dir / f"build-{config.name}.result"
    mds = args.output_dir / f"build-{config.name}.mds.result"
    log = args.output_dir / f"build-{config.name}.log"

    if args.reuse_indexes and out_index.exists() and out_index.stat().st_size > 0:
        row = parse_result_file(result)
        row.update({"reused": 1, "wall_seconds": "reused"})
    else:
        result.unlink(missing_ok=True)
        mds.unlink(missing_ok=True)
        cmd = [
            str(move_r_build),
            "-s", config.support,
            "-p", str(args.threads),
            "-a", str(args.a),
            "-o", str(prefix),
            "-m_idx", str(result),
            "-m_mds", str(mds),
        ]
        if config.hybrid:
            cmd.extend([
                "-hybrid",
                "-hybrid-thr", str(args.hybrid_thr),
                "-hybrid-min-occ", str(args.hybrid_min_occ),
                "-hybrid-max-pattern", str(args.hybrid_max_pattern),
                "-hybrid-cost",
                str(args.hybrid_cost_phi),
                str(args.hybrid_cost_rlz_init),
                str(args.hybrid_cost_rlz_phrase),
                str(args.hybrid_cost_rlz_decode),
            ])
        cmd.append(str(args.text))
        print(f"[build] {config.name}")
        wall = run(cmd, log)
        row = parse_result_file(result)
        row.update({"reused": 0, "wall_seconds": f"{wall:.6f}"})

    row.update({
        "phase": "build",
        "config": config.name,
        "support": config.support,
        "hybrid": int(config.hybrid),
        "auto_rlzsa_only": int(config.auto_rlzsa_only),
        "index_path": str(out_index),
        "index_bytes": out_index.stat().st_size if out_index.exists() else "",
        "log": str(log),
    })
    return out_index, row


def locate(args: argparse.Namespace, config: Config, idx: Path, workload: str, patterns: Path) -> dict[str, object]:
    move_r_locate = REPO_ROOT / "build" / "cli" / "move-r-locate"
    result = args.output_dir / f"locate-{config.name}-{workload}.result"
    log = args.output_dir / f"locate-{config.name}-{workload}.log"
    detail = args.output_dir / f"detail-{config.name}-{workload}.csv"
    if args.reuse_locate and result.exists() and (
        not (config.hybrid and args.detail) or detail.exists()
    ):
        row = parse_result_file(result)
        row.update({
            "phase": "locate",
            "config": config.name,
            "support": config.support,
            "hybrid": int(config.hybrid),
            "auto_rlzsa_only": int(config.auto_rlzsa_only),
            "workload": workload,
            "patterns_path": str(patterns),
            "index_path": str(idx),
            "index_bytes": idx.stat().st_size if idx.exists() else "",
            "wall_seconds": "reused",
            "log": str(log),
            "detail_path": str(detail) if config.hybrid and args.detail else "",
        })
        return row

    result.unlink(missing_ok=True)
    detail.unlink(missing_ok=True)
    cmd = [str(move_r_locate), "-m", str(result), args.text_name]
    if args.check:
        cmd.extend(["-i", str(args.text), "-c"])
    if config.hybrid:
        cmd.append("-compare")
        if args.detail:
            cmd.extend(["-compare-detail", str(detail)])
    cmd.extend([str(idx), str(patterns)])
    print(f"[locate] {config.name} / {workload}")
    wall = run(cmd, log)
    row = parse_result_file(result)
    row.update({
        "phase": "locate",
        "config": config.name,
        "support": config.support,
        "hybrid": int(config.hybrid),
        "auto_rlzsa_only": int(config.auto_rlzsa_only),
        "workload": workload,
        "patterns_path": str(patterns),
        "index_path": str(idx),
        "index_bytes": idx.stat().st_size if idx.exists() else "",
        "wall_seconds": f"{wall:.6f}",
        "log": str(log),
        "detail_path": str(detail) if config.hybrid and args.detail else "",
    })
    return row


def ns_per_pattern(row: dict[str, object], time_key: str = "time_locate") -> float | None:
    t = as_float(row, time_key)
    n = as_float(row, "num_patterns")
    if t is None or not n:
        return None
    return t / n


def occurrence_bin(occ: int) -> str:
    if occ <= 1:
        return "occ<=1"
    if occ < 10:
        return "2<=occ<10"
    if occ < 100:
        return "10<=occ<100"
    if occ < 1000:
        return "100<=occ<1000"
    return "occ>=1000"


def load_occurrence_bins(detail_path: Path) -> list[dict[str, object]]:
    if not detail_path.exists():
        return []
    order = ["occ<=1", "2<=occ<10", "10<=occ<100", "100<=occ<1000", "occ>=1000"]
    bins: dict[str, dict[str, float]] = {
        key: {
            "queries": 0,
            "phi_queries": 0,
            "rlzsa_queries": 0,
            "time_hybrid_ns": 0,
            "time_phi_forced_ns": 0,
            "time_rlzsa_forced_ns": 0,
        }
        for key in order
    }
    with detail_path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            occ = int(row["occ"])
            key = occurrence_bin(occ)
            bucket = bins[key]
            bucket["queries"] += 1
            if row["chosen_strategy"] == "phi":
                bucket["phi_queries"] += 1
            elif row["chosen_strategy"] == "rlzsa":
                bucket["rlzsa_queries"] += 1
            bucket["time_hybrid_ns"] += float(row["time_hybrid_ns"])
            bucket["time_phi_forced_ns"] += float(row["time_phi_forced_ns"])
            bucket["time_rlzsa_forced_ns"] += float(row["time_rlzsa_forced_ns"])
    out: list[dict[str, object]] = []
    for key in order:
        bucket = bins[key]
        queries = bucket["queries"]
        if not queries:
            continue
        phi_ns = bucket["time_phi_forced_ns"] / queries
        rlzsa_ns = bucket["time_rlzsa_forced_ns"] / queries
        hybrid_ns = bucket["time_hybrid_ns"] / queries
        forced_best = "phi" if phi_ns < rlzsa_ns else "rlzsa"
        best_ns = min(phi_ns, rlzsa_ns)
        chosen_best_percent = (
            100 * bucket["phi_queries"] / queries
            if forced_best == "phi"
            else 100 * bucket["rlzsa_queries"] / queries
        )
        out.append({
            "occ_bin": key,
            "queries": int(queries),
            "phi_query_percent": 100 * bucket["phi_queries"] / queries,
            "rlzsa_query_percent": 100 * bucket["rlzsa_queries"] / queries,
            "hybrid_ns_per_pattern": hybrid_ns,
            "phi_forced_ns_per_pattern": phi_ns,
            "rlzsa_forced_ns_per_pattern": rlzsa_ns,
            "forced_best": forced_best,
            "chosen_best_percent": chosen_best_percent,
            "hybrid_over_best": None if best_ns == 0 else hybrid_ns / best_ns,
        })
    return out


def load_oracle_diagnostics(detail_path: Path) -> dict[str, object]:
    rows = list(csv.DictReader(detail_path.open(newline="", encoding="utf-8"))) if detail_path.exists() else []
    chosen_phi = 0
    chosen_rlzsa = 0
    phi_faster = 0
    rlzsa_faster = 0
    chosen_phi_correct = 0
    chosen_rlzsa_correct = 0
    hybrid_time = 0.0
    oracle_time = 0.0
    phi_time = 0.0
    rlzsa_time = 0.0
    for row in rows:
        chosen = row["chosen_strategy"]
        t_hybrid = float(row["time_hybrid_ns"])
        t_phi = float(row["time_phi_forced_ns"])
        t_rlzsa = float(row["time_rlzsa_forced_ns"])
        phi_wins = t_phi < t_rlzsa
        if chosen == "phi":
            chosen_phi += 1
            if phi_wins:
                chosen_phi_correct += 1
        elif chosen == "rlzsa":
            chosen_rlzsa += 1
            if not phi_wins:
                chosen_rlzsa_correct += 1
        if phi_wins:
            phi_faster += 1
        else:
            rlzsa_faster += 1
        hybrid_time += t_hybrid
        oracle_time += min(t_phi, t_rlzsa)
        phi_time += t_phi
        rlzsa_time += t_rlzsa

    n = len(rows)
    return {
        "queries": n,
        "chosen_phi": chosen_phi,
        "chosen_rlzsa": chosen_rlzsa,
        "phi_faster": phi_faster,
        "rlzsa_faster": rlzsa_faster,
        "phi_precision": None if chosen_phi == 0 else 100 * chosen_phi_correct / chosen_phi,
        "phi_recall": None if phi_faster == 0 else 100 * chosen_phi_correct / phi_faster,
        "rlzsa_precision": None if chosen_rlzsa == 0 else 100 * chosen_rlzsa_correct / chosen_rlzsa,
        "rlzsa_recall": None if rlzsa_faster == 0 else 100 * chosen_rlzsa_correct / rlzsa_faster,
        "hybrid_ns_per_pattern": None if n == 0 else hybrid_time / n,
        "oracle_ns_per_pattern": None if n == 0 else oracle_time / n,
        "phi_forced_ns_per_pattern": None if n == 0 else phi_time / n,
        "rlzsa_forced_ns_per_pattern": None if n == 0 else rlzsa_time / n,
        "hybrid_over_oracle": None if oracle_time == 0 else hybrid_time / oracle_time,
    }


def aggregate_occurrence_bins(rows: list[dict[str, object]]) -> list[dict[str, object]]:
    order = ["occ<=1", "2<=occ<10", "10<=occ<100", "100<=occ<1000", "occ>=1000"]
    totals: dict[str, dict[str, float]] = {
        key: {
            "queries": 0,
            "phi_queries": 0,
            "rlzsa_queries": 0,
            "time_hybrid_ns": 0,
            "time_phi_forced_ns": 0,
            "time_rlzsa_forced_ns": 0,
        }
        for key in order
    }
    for row in rows:
        key = str(row["occ_bin"])
        queries = float(row["queries"])
        bucket = totals[key]
        bucket["queries"] += queries
        bucket["phi_queries"] += queries * float(row["phi_query_percent"]) / 100
        bucket["rlzsa_queries"] += queries * float(row["rlzsa_query_percent"]) / 100
        bucket["time_hybrid_ns"] += queries * float(row["hybrid_ns_per_pattern"])
        bucket["time_phi_forced_ns"] += queries * float(row["phi_forced_ns_per_pattern"])
        bucket["time_rlzsa_forced_ns"] += queries * float(row["rlzsa_forced_ns_per_pattern"])

    aggregated: list[dict[str, object]] = []
    for key in order:
        bucket = totals[key]
        queries = bucket["queries"]
        if not queries:
            continue
        phi_ns = bucket["time_phi_forced_ns"] / queries
        rlzsa_ns = bucket["time_rlzsa_forced_ns"] / queries
        hybrid_ns = bucket["time_hybrid_ns"] / queries
        forced_best = "phi" if phi_ns < rlzsa_ns else "rlzsa"
        best_ns = min(phi_ns, rlzsa_ns)
        chosen_best_percent = (
            100 * bucket["phi_queries"] / queries
            if forced_best == "phi"
            else 100 * bucket["rlzsa_queries"] / queries
        )
        aggregated.append({
            "occ_bin": key,
            "queries": int(queries),
            "phi_query_percent": 100 * bucket["phi_queries"] / queries,
            "rlzsa_query_percent": 100 * bucket["rlzsa_queries"] / queries,
            "hybrid_ns_per_pattern": hybrid_ns,
            "phi_forced_ns_per_pattern": phi_ns,
            "rlzsa_forced_ns_per_pattern": rlzsa_ns,
            "forced_best": forced_best,
            "chosen_best_percent": chosen_best_percent,
            "hybrid_over_best": None if best_ns == 0 else hybrid_ns / best_ns,
        })
    return aggregated


def summarize(args: argparse.Namespace, build_rows: list[dict[str, object]], locate_rows: list[dict[str, object]]) -> None:
    md = args.output_dir / "summary.md"
    by_workload: dict[str, dict[str, dict[str, object]]] = {}
    for row in locate_rows:
        by_workload.setdefault(str(row["workload"]), {})[str(row["config"])] = row

    lines: list[str] = []
    lines.append("# Innovation 1 Hybrid Locate Experiment")
    lines.append("")
    lines.append(f"This report evaluates cost-aware Phi/RLZSA hybrid locate on {args.text_name}.")
    lines.append("")
    lines.append("## Build")
    lines.append("")
    lines.append("| config | index MiB | build wall s | hybrid |")
    lines.append("|---|---:|---:|---:|")
    for row in build_rows:
        size = as_float(row, "index_bytes")
        size_mib = None if size is None else size / (1024 * 1024)
        lines.append(f"| {row['config']} | {fmt_float(size_mib, 2)} | {row.get('wall_seconds', 'n/a')} | {row.get('hybrid', 0)} |")

    lines.append("")
    lines.append("## Locate Performance")
    lines.append("")
    lines.append("| workload | avg occ | Move-r ns/pat | RLZSA ns/pat | Hybrid ns/pat | Hybrid vs Move-r | Hybrid vs RLZSA |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|")
    for workload, rows in by_workload.items():
        move = rows.get("move-r", {})
        rlz = rows.get("move-r-rlzsa", {})
        hyb = rows.get("move-r-rlzsa-hybrid", {})
        avg_occ = None
        occ = as_float(hyb, "num_occurrences")
        pats = as_float(hyb, "num_patterns")
        if occ is not None and pats:
            avg_occ = occ / pats
        move_nsp = ns_per_pattern(move)
        rlz_nsp = ns_per_pattern(rlz)
        hyb_nsp = ns_per_pattern(hyb)
        hyb_vs_move = None if move_nsp in (None, 0) or hyb_nsp in (None, 0) else move_nsp / hyb_nsp
        hyb_vs_rlz = None if rlz_nsp in (None, 0) or hyb_nsp in (None, 0) else rlz_nsp / hyb_nsp
        lines.append(
            f"| {workload} | {fmt_float(avg_occ, 1)} | {fmt_float(move_nsp, 1)} | "
            f"{fmt_float(rlz_nsp, 1)} | {fmt_float(hyb_nsp, 1)} | "
            f"{fmt_ratio(hyb_vs_move)} | {fmt_ratio(hyb_vs_rlz)} |"
        )

    lines.append("")
    lines.append("## Forced-Path Ablation")
    lines.append("")
    lines.append("| workload | Forced Phi ns/pat | Forced RLZSA ns/pat | Hybrid ns/pat | mismatches |")
    lines.append("|---|---:|---:|---:|---:|")
    for workload, rows in by_workload.items():
        hyb = rows.get("move-r-rlzsa-hybrid", {})
        lines.append(
            f"| {workload} | {fmt_float(ns_per_pattern(hyb, 'time_phi_forced'), 1)} | "
            f"{fmt_float(ns_per_pattern(hyb, 'time_rlzsa_forced'), 1)} | "
            f"{fmt_float(ns_per_pattern(hyb), 1)} | {hyb.get('compare_mismatches', 'n/a')} |"
        )

    lines.append("")
    lines.append("## Hybrid Routing")
    lines.append("")
    lines.append("| workload | Phi query % | RLZSA query % | Phi occ % | RLZSA occ % | phrase-estimate % |")
    lines.append("|---|---:|---:|---:|---:|---:|")
    for workload, rows in by_workload.items():
        hyb = rows.get("move-r-rlzsa-hybrid", {})
        pats = as_float(hyb, "num_patterns")
        occ = as_float(hyb, "num_occurrences")
        phi_q = as_float(hyb, "hybrid_phi_queries")
        rlz_q = as_float(hyb, "hybrid_rlzsa_queries")
        phi_occ = as_float(hyb, "hybrid_phi_occurrences")
        rlz_occ = as_float(hyb, "hybrid_rlzsa_occurrences")
        estimates = as_float(hyb, "hybrid_phrase_estimate_queries")
        lines.append(
            f"| {workload} | {fmt_percent(None if not pats or phi_q is None else 100 * phi_q / pats)} | "
            f"{fmt_percent(None if not pats or rlz_q is None else 100 * rlz_q / pats)} | "
            f"{fmt_percent(None if not occ or phi_occ is None else 100 * phi_occ / occ)} | "
            f"{fmt_percent(None if not occ or rlz_occ is None else 100 * rlz_occ / occ)} | "
            f"{fmt_percent(None if not pats or estimates is None else 100 * estimates / pats)} |"
        )

    occ_bin_rows: list[dict[str, object]] = []
    for workload, rows in by_workload.items():
        hyb = rows.get("move-r-rlzsa-hybrid", {})
        detail_path = hyb.get("detail_path")
        if not detail_path:
            continue
        for bucket in load_occurrence_bins(Path(str(detail_path))):
            bucket = {"workload": workload, **bucket}
            occ_bin_rows.append(bucket)

    if occ_bin_rows:
        occ_bin_csv = args.output_dir / "occurrence_bins.csv"
        write_csv(occ_bin_csv, occ_bin_rows)
        lines.append("")
        lines.append("## Occurrence Bins")
        lines.append("")
        lines.append("| workload | occ bin | queries | Phi query % | RLZSA query % | Forced Phi ns/pat | Forced RLZSA ns/pat | Hybrid ns/pat | forced best | chosen-best % | Hybrid/best |")
        lines.append("|---|---|---:|---:|---:|---:|---:|---:|---|---:|---:|")
        for row in occ_bin_rows:
            lines.append(
                f"| {row['workload']} | {row['occ_bin']} | {row['queries']} | "
                f"{fmt_percent(float(row['phi_query_percent']))} | "
                f"{fmt_percent(float(row['rlzsa_query_percent']))} | "
                f"{fmt_float(float(row['phi_forced_ns_per_pattern']), 1)} | "
                f"{fmt_float(float(row['rlzsa_forced_ns_per_pattern']), 1)} | "
                f"{fmt_float(float(row['hybrid_ns_per_pattern']), 1)} | "
                f"{row['forced_best']} | "
                f"{fmt_percent(float(row['chosen_best_percent']))} | "
                f"{fmt_ratio(float(row['hybrid_over_best']))} |"
            )

        aggregate_rows = aggregate_occurrence_bins(occ_bin_rows)
        aggregate_csv = args.output_dir / "occurrence_bins_aggregate.csv"
        write_csv(aggregate_csv, aggregate_rows)
        lines.append("")
        lines.append("## Aggregated Occurrence Bins")
        lines.append("")
        lines.append("This table merges all generated pattern-length workloads and directly answers whether low-occurrence queries favor Phi or RLZSA.")
        lines.append("")
        lines.append("| occ bin | queries | Phi query % | RLZSA query % | Forced Phi ns/pat | Forced RLZSA ns/pat | Hybrid ns/pat | forced best | chosen-best % | Hybrid/best |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|---|---:|---:|")
        for row in aggregate_rows:
            lines.append(
                f"| {row['occ_bin']} | {row['queries']} | "
                f"{fmt_percent(float(row['phi_query_percent']))} | "
                f"{fmt_percent(float(row['rlzsa_query_percent']))} | "
                f"{fmt_float(float(row['phi_forced_ns_per_pattern']), 1)} | "
                f"{fmt_float(float(row['rlzsa_forced_ns_per_pattern']), 1)} | "
                f"{fmt_float(float(row['hybrid_ns_per_pattern']), 1)} | "
                f"{row['forced_best']} | "
                f"{fmt_percent(float(row['chosen_best_percent']))} | "
                f"{fmt_ratio(float(row['hybrid_over_best']))} |"
            )

    oracle_rows: list[dict[str, object]] = []
    for workload, rows in by_workload.items():
        hyb = rows.get("move-r-rlzsa-hybrid", {})
        detail_path = hyb.get("detail_path")
        if not detail_path:
            continue
        oracle_rows.append({"workload": workload, **load_oracle_diagnostics(Path(str(detail_path)))})

    if oracle_rows:
        oracle_csv = args.output_dir / "oracle_diagnostics.csv"
        write_csv(oracle_csv, oracle_rows)
        lines.append("")
        lines.append("## Oracle Diagnostics")
        lines.append("")
        lines.append("| workload | chosen Phi | Phi faster | Phi precision | Phi recall | chosen RLZSA | RLZSA precision | RLZSA recall | Hybrid ns/pat | Oracle ns/pat | Hybrid/Oracle |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for row in oracle_rows:
            lines.append(
                f"| {row['workload']} | {row['chosen_phi']} | {row['phi_faster']} | "
                f"{fmt_percent(row['phi_precision'])} | {fmt_percent(row['phi_recall'])} | "
                f"{row['chosen_rlzsa']} | {fmt_percent(row['rlzsa_precision'])} | "
                f"{fmt_percent(row['rlzsa_recall'])} | "
                f"{fmt_float(row['hybrid_ns_per_pattern'], 1)} | "
                f"{fmt_float(row['oracle_ns_per_pattern'], 1)} | "
                f"{fmt_ratio(row['hybrid_over_oracle'])} |"
            )

    has_auto = any("move-r-rlzsa-auto" in rows for rows in by_workload.values())
    if has_auto:
        lines.append("")
        lines.append("## Auto RLZSA-Only")
        lines.append("")
        lines.append("This configuration automatically disables the Phi structure and uses the RLZSA-only index when calibration indicates RLZSA dominates.")
        lines.append("")
        lines.append("| workload | Move-r ns/pat | RLZSA ns/pat | Hybrid ns/pat | Auto ns/pat | Auto vs Move-r | Auto vs RLZSA | Auto vs Hybrid |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|---:|")
        for workload, rows in by_workload.items():
            move_nsp = ns_per_pattern(rows.get("move-r", {}))
            rlz_nsp = ns_per_pattern(rows.get("move-r-rlzsa", {}))
            hyb_nsp = ns_per_pattern(rows.get("move-r-rlzsa-hybrid", {}))
            auto_nsp = ns_per_pattern(rows.get("move-r-rlzsa-auto", {}))
            lines.append(
                f"| {workload} | {fmt_float(move_nsp, 1)} | {fmt_float(rlz_nsp, 1)} | "
                f"{fmt_float(hyb_nsp, 1)} | {fmt_float(auto_nsp, 1)} | "
                f"{fmt_ratio(None if not auto_nsp or not move_nsp else move_nsp / auto_nsp)} | "
                f"{fmt_ratio(None if not auto_nsp or not rlz_nsp else rlz_nsp / auto_nsp)} | "
                f"{fmt_ratio(None if not auto_nsp or not hyb_nsp else hyb_nsp / auto_nsp)} |"
            )

        lines.append("")
        lines.append("### Auto Space")
        lines.append("")
        lines.append("| config | index MiB | vs RLZSA |")
        lines.append("|---|---:|---:|")
        rlz_size = next((as_float(row, "index_bytes") for row in build_rows if row.get("config") == "move-r-rlzsa"), None)
        for row in build_rows:
            if row.get("config") not in {"move-r-rlzsa", "move-r-rlzsa-hybrid", "move-r-rlzsa-auto"}:
                continue
            size = as_float(row, "index_bytes")
            size_mib = None if size is None else size / (1024 * 1024)
            lines.append(
                f"| {row['config']} | {fmt_float(size_mib, 2)} | "
                f"{fmt_ratio(None if not size or not rlz_size else size / rlz_size)} |"
            )

    if len(args.lengths) >= 2:
        lines.append("")
        lines.append("## Weighted Mixed Workload")
        lines.append("")
        lines.append("The mixed workload is the arithmetic mean of all generated pattern-length workloads.")
        keys = [f"m{length}" for length in args.lengths]
        mixed: dict[str, float] = {}
        for config in ["move-r", "move-r-rlzsa", "move-r-rlzsa-hybrid"]:
            vals = [ns_per_pattern(by_workload.get(key, {}).get(config, {})) for key in keys]
            vals = [v for v in vals if v is not None]
            if vals:
                mixed[config] = sum(vals) / len(vals)
        lines.append("")
        lines.append("| Move-r ns/pat | RLZSA ns/pat | Hybrid ns/pat | Hybrid vs Move-r | Hybrid vs RLZSA |")
        lines.append("|---:|---:|---:|---:|---:|")
        h = mixed.get("move-r-rlzsa-hybrid")
        lines.append(
            f"| {fmt_float(mixed.get('move-r'), 1)} | {fmt_float(mixed.get('move-r-rlzsa'), 1)} | "
            f"{fmt_float(h, 1)} | {fmt_ratio(None if not h else mixed.get('move-r') / h)} | "
            f"{fmt_ratio(None if not h else mixed.get('move-r-rlzsa') / h)} |"
        )

    md.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run innovation-1 hybrid locate experiments.")
    parser.add_argument("--text", type=Path, default=DEFAULT_TEXT)
    parser.add_argument("--text-name", default="einstein.en.txt")
    parser.add_argument("--output-dir", type=Path, default=REPO_ROOT / "measurements" / "results" / "innovation1_hybrid_einstein")
    parser.add_argument("--index-dir", type=Path, default=REPO_ROOT / "measurements" / "indexes" / "innovation1_hybrid_einstein")
    parser.add_argument("--pattern-dir", type=Path, default=REPO_ROOT / "measurements" / "patterns" / "innovation1_einstein")
    parser.add_argument("--lengths", type=parse_int_list, default=parse_int_list("8,100,200"))
    parser.add_argument(
        "--scheme12",
        action="store_true",
        help="Use optimization-plan schemes 1 and 2: add long low-occurrence lengths 500,1000,2000 and occurrence-bin analysis.",
    )
    parser.add_argument("--num-patterns", type=int, default=10000)
    parser.add_argument("--threads", type=int, default=max(1, min(4, os.cpu_count() or 1)))
    parser.add_argument("--a", type=int, default=8)
    parser.add_argument("--reuse-indexes", action="store_true")
    parser.add_argument("--reuse-patterns", action="store_true")
    parser.add_argument("--reuse-locate", action="store_true")
    parser.add_argument("--auto-rlzsa-only", action="store_true", help="Add an auto configuration that disables Phi and reuses the RLZSA-only index.")
    parser.add_argument("--skip-cmake-build", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--hybrid-thr", type=int, default=32)
    parser.add_argument("--hybrid-min-occ", type=int, default=2)
    parser.add_argument("--hybrid-max-pattern", type=int, default=64)
    parser.add_argument("--hybrid-cost-phi", type=float, default=7.0)
    parser.add_argument("--hybrid-cost-rlz-init", type=float, default=48.0)
    parser.add_argument("--hybrid-cost-rlz-phrase", type=float, default=4.0)
    parser.add_argument("--hybrid-cost-rlz-decode", type=float, default=1.25)
    parser.add_argument("--no-detail", dest="detail", action="store_false", help="Disable per-pattern detail CSVs.")
    parser.set_defaults(detail=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.scheme12:
        args.lengths = parse_int_list("8,100,200,500,1000,2000")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.index_dir.mkdir(parents=True, exist_ok=True)
    args.pattern_dir.mkdir(parents=True, exist_ok=True)

    if not args.skip_cmake_build:
        print("[build] CLI targets")
        run(
            ["cmake", "--build", "build", "--target", "move-r-build", "move-r-locate", "gen-patterns", "-j", str(args.threads)],
            args.output_dir / "cmake-build.log",
        )

    patterns = ensure_patterns(args)

    build_rows: list[dict[str, object]] = []
    locate_rows: list[dict[str, object]] = []
    indexes: dict[str, Path] = {}
    configs = experiment_configs(args)
    for config in configs:
        if config.auto_rlzsa_only:
            if "move-r-rlzsa" not in indexes:
                raise RuntimeError("auto RLZSA-only config requires the move-r-rlzsa index")
            idx = indexes["move-r-rlzsa"]
            base_row = next(row for row in build_rows if row["config"] == "move-r-rlzsa")
            row = dict(base_row)
            row.update({
                "config": config.name,
                "hybrid": 0,
                "auto_rlzsa_only": 1,
                "index_path": str(idx),
                "index_bytes": idx.stat().st_size if idx.exists() else "",
                "wall_seconds": "reused(auto)",
                "log": str(args.output_dir / f"build-{config.name}.log"),
            })
        else:
            idx, row = build_index(args, config)
        indexes[config.name] = idx
        build_rows.append(row)

    for workload, pattern_file in patterns.items():
        for config in configs:
            locate_rows.append(locate(args, config, indexes[config.name], workload, pattern_file))

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
