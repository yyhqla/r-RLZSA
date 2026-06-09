#!/usr/bin/env python3
from __future__ import annotations

import csv
import shlex
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DUMP = ROOT / "build" / "cli" / "move-r-dump-fields"
OUT = ROOT / "measurements" / "results" / "vrrlzsa_field_frequency"

DATASETS = {
    "boost": ROOT / "measurements/indexes/r_rlzsa_boost_full_m4_128_rerun_20260603/boost.a8.r-rlzsa25.move-r-rlzsa",
    "einstein.en.txt": ROOT / "measurements/indexes/r_rlzsa_einstein_full_m4_128_rerun_20260603/einstein.en.txt.a8.r-rlzsa25.move-r-rlzsa",
    "SARS-Cov-2_40": ROOT / "measurements/indexes/r_rlzsa_SARS-Cov-2_40_m4_128_rerun_20260603/SARS-Cov-2_40.a8.r-rlzsa25.move-r-rlzsa",
    "world_leaders": ROOT / "measurements/indexes/r_rlzsa_world_leaders_m4_128_rerun_20260603/world_leaders.a8.r-rlzsa25.move-r-rlzsa",
    "DNA": ROOT / "measurements/indexes/r_rlzsa_DNA_full_m4_128_rerun_20260603/DNA.a8.r-rlzsa25.move-r-rlzsa",
    "candida_auris": ROOT / "measurements/indexes/r_rlzsa_candida_auris_full_m4_128_rerun_20260607/candida_auris.a8.r-rlzsa25.move-r-rlzsa",
    "english_200MB": ROOT / "measurements/indexes/r_rlzsa_english_200MB_m4_128_rerun_20260607/english_200MB.a8.r-rlzsa25.move-r-rlzsa",
}


def run(cmd: list[object], log: Path) -> None:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w", encoding="utf-8", errors="replace") as out:
        out.write("$ " + " ".join(shlex.quote(str(x)) for x in cmd) + "\n\n")
        out.flush()
        proc = subprocess.run([str(x) for x in cmd], cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, text=True)
    if proc.returncode:
        raise RuntimeError(f"command failed ({proc.returncode}): {' '.join(str(x) for x in cmd)}\nlog: {log}")


def read_summary(dataset: str, path: Path) -> dict[str, str]:
    with path.open(newline="", encoding="utf-8") as inp:
        row = next(csv.DictReader(inp))
    row["dataset"] = dataset
    return row


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    summaries: list[dict[str, str]] = []
    missing: list[str] = []
    for dataset, index in DATASETS.items():
        if not index.exists():
            missing.append(f"{dataset}: {index}")
            continue
        out_dir = OUT / dataset
        print(f"[dump] {dataset}", flush=True)
        run([DUMP, index, out_dir], OUT / "logs" / f"{dataset}.log")
        summaries.append(read_summary(dataset, out_dir / "field_summary.csv"))

    if summaries:
        fields = ["dataset"] + [key for key in summaries[0].keys() if key != "dataset"]
        with (OUT / "all_field_summary.csv").open("w", newline="", encoding="utf-8") as out:
            writer = csv.DictWriter(out, fieldnames=fields)
            writer.writeheader()
            writer.writerows(summaries)

    md = ["# Vr-RLZSA Field Frequency Data\n\n"]
    md.append("Each dataset directory contains `sr_rank_frequency.csv`, `cpl_rank_frequency.csv`, and compatibility aliases `p_rank_frequency.csv`, `l_rank_frequency.csv`.\n\n")
    md.append("| dataset | copy phrases | distinct SR | distinct CPL | top1 SR freq | top1 CPL freq | SR entropy | CPL entropy |\n")
    md.append("|---|---:|---:|---:|---:|---:|---:|---:|\n")
    for row in summaries:
        md.append(
            f"| {row['dataset']} | {row['total_copy_phrases']} | {row['distinct_sr_values']} | "
            f"{row['distinct_cpl_values']} | {row['top1_sr_frequency']} | {row['top1_cpl_frequency']} | "
            f"{float(row['sr_entropy']):.4f} | {float(row['cpl_entropy']):.4f} |\n"
        )
    if missing:
        md.append("\n## Missing Indexes\n\n")
        for item in missing:
            md.append(f"- `{item}`\n")
    (OUT / "field_frequency_summary.md").write_text("".join(md), encoding="utf-8")
    print(OUT / "field_frequency_summary.md", flush=True)


if __name__ == "__main__":
    main()
