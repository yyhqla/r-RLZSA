#!/usr/bin/env python3
# calc_dataset_stats.py
# 计算 |T|、sigma、BWT run 数 r、|T|/r
#
# 推荐安装：
#   pip install numpy pydivsufsort pandas
#
# 使用示例：
#   python calc_dataset_stats.py /path/einstein.en.txt /path/world_leaders.txt --out dataset_stats
#
# 输出：
#   dataset_stats.csv
#   dataset_stats.md

import argparse
import os
from pathlib import Path

import numpy as np
import pandas as pd


def read_bytes(path: Path) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def infer_dataset_type(name: str, data: bytes) -> str:
    lower = name.lower()

    if any(x in lower for x in ["dna", "sars", "cov", "chr", "candida", "simons"]):
        return "DNA"

    # 简单判断是否为 DNA 字母表
    sample = data[: min(len(data), 1_000_000)].upper()
    dna_chars = set(b"ACGTN\n\r")
    if sample and set(sample).issubset(dna_chars):
        return "DNA"

    if any(x in lower for x in ["boost", "code", "src"]):
        return "代码"

    return "文本"


def remap_bytes_with_sentinel(data: bytes):
    """
    将原始 byte 映射到 1..sigma，并追加 0 作为唯一终止符。
    这样可以保证终止符不与原文本字符冲突。
    """
    alphabet = sorted(set(data))
    sigma = len(alphabet)

    if sigma > 255:
        # byte 数据最多 256 种，正常不会超过
        raise ValueError("字母表大小超过 255，输入数据可能不是普通 byte 序列。")

    trans = np.zeros(256, dtype=np.uint16)
    for i, c in enumerate(alphabet, start=1):
        trans[c] = i

    arr = np.frombuffer(data, dtype=np.uint8)
    mapped = trans[arr]

    # 追加唯一终止符 0
    mapped = np.concatenate([mapped, np.array([0], dtype=np.uint16)])
    return mapped, sigma


def build_sa(seq: np.ndarray) -> np.ndarray:
    """
    使用 pydivsufsort 构造后缀数组。
    若未安装 pydivsufsort，请先：
        pip install pydivsufsort
    """
    try:
        import pydivsufsort
    except ImportError as e:
        raise RuntimeError(
            "缺少 pydivsufsort。请先运行：pip install pydivsufsort"
        ) from e

    # pydivsufsort 通常支持 numpy array。
    # 若你的环境版本不支持 uint16，可改成 int32。
    try:
        return pydivsufsort.divsufsort(seq)
    except Exception:
        return pydivsufsort.divsufsort(seq.astype(np.int32))


def count_bwt_runs(seq: np.ndarray, sa: np.ndarray) -> int:
    """
    seq 为已经追加终止符的整数序列。
    BWT[i] = seq[(SA[i] - 1) mod N]
    r 为 BWT 中连续相同字符段数量。
    """
    n = len(seq)
    bwt = seq[(sa - 1) % n]

    if len(bwt) == 0:
        return 0

    return int(1 + np.count_nonzero(bwt[1:] != bwt[:-1]))


def calc_one_file(path: Path, type_override: str | None = None):
    data = read_bytes(path)

    n_original = len(data)
    dataset_name = path.stem

    mapped, sigma = remap_bytes_with_sentinel(data)
    sa = build_sa(mapped)
    r = count_bwt_runs(mapped, sa)

    ratio = n_original / r if r else 0

    return {
        "数据集": dataset_name,
        "类型": type_override if type_override else infer_dataset_type(dataset_name, data),
        "|T|": n_original,
        "sigma": sigma,
        "r": r,
        "|T|/r": round(ratio, 2),
    }


def parse_type_overrides(type_args):
    """
    支持：
        --type einstein=文本 --type boost=代码
    """
    mp = {}
    for item in type_args or []:
        if "=" not in item:
            raise ValueError(f"--type 参数格式错误：{item}，应为 name=类型")
        k, v = item.split("=", 1)
        mp[k] = v
    return mp


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "files",
        nargs="+",
        help="输入数据集文件路径，例如 einstein.txt boost.txt",
    )
    parser.add_argument(
        "--type",
        action="append",
        default=[],
        help="手动指定类型，例如 --type einstein=文本 --type boost=代码",
    )
    parser.add_argument(
        "--out",
        default="dataset_stats",
        help="输出文件前缀，默认 dataset_stats",
    )
    args = parser.parse_args()

    type_map = parse_type_overrides(args.type)

    rows = []
    for f in args.files:
        path = Path(f)
        if not path.exists():
            print(f"[跳过] 文件不存在：{path}")
            continue

        name = path.stem
        type_override = type_map.get(name)

        print(f"[处理] {path}")
        row = calc_one_file(path, type_override)
        rows.append(row)

        print(
            f"  |T|={row['|T|']}, sigma={row['sigma']}, "
            f"r={row['r']}, |T|/r={row['|T|/r']}"
        )

    df = pd.DataFrame(rows)

    csv_path = f"{args.out}.csv"
    md_path = f"{args.out}.md"

    df.to_csv(csv_path, index=False, encoding="utf-8-sig")

    with open(md_path, "w", encoding="utf-8") as f:
        f.write(df.to_markdown(index=False))

    print()
    print(f"[完成] 已输出：{csv_path}")
    print(f"[完成] 已输出：{md_path}")


if __name__ == "__main__":
    main()