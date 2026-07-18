#!/usr/bin/env python3
"""
dataset_summary.py — Print statistics for a Makruk-SF self-play dataset.

Reads the JSONL game file (from tools/selfplay/selfplay.py) and optionally
the position TSV (from convert.py).  Prints game-level and position-level
statistics useful for diagnosing dataset quality before training.

Usage:
    # Game-level stats only (fast, no convert.py needed)
    python tools/training/dataset_summary.py --jsonl tools/selfplay/out/selfplay.jsonl

    # Full stats including position-level score distribution
    python tools/training/dataset_summary.py \\
        --jsonl tools/selfplay/out/selfplay.jsonl \\
        --tsv   tools/training/train.tsv

    # Duplicate FEN estimate (slower, needs TSV)
    python tools/training/dataset_summary.py \\
        --jsonl tools/selfplay/out/selfplay.jsonl \\
        --tsv   tools/training/train.tsv \\
        --duplicates

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import csv
import json
import math
import sys
from collections import Counter
from pathlib import Path


# ---------------------------------------------------------------------------
# Statistics helpers
# ---------------------------------------------------------------------------

def _stats(values: list[float]) -> dict:
    if not values:
        return {}
    n = len(values)
    s = sorted(values)
    mean = sum(values) / n
    std  = math.sqrt(sum((x - mean) ** 2 for x in values) / n)
    return {
        "n":      n,
        "min":    s[0],
        "max":    s[-1],
        "mean":   mean,
        "std":    std,
        "p25":    s[n // 4],
        "median": s[n // 2],
        "p75":    s[3 * n // 4],
    }


def _bar(count: int, total: int, width: int = 36) -> str:
    if total == 0:
        return ""
    filled = round(count * width / total)
    return "#" * filled


# ---------------------------------------------------------------------------
# JSONL reader
# ---------------------------------------------------------------------------

def read_jsonl(path: Path) -> list[dict]:
    records = []
    with open(path) as f:
        for lineno, raw in enumerate(f, 1):
            raw = raw.strip()
            if not raw:
                continue
            try:
                records.append(json.loads(raw))
            except json.JSONDecodeError as exc:
                print(f"  WARN: line {lineno} parse error: {exc}", file=sys.stderr)
    return records


def summarise_jsonl(games: list[dict]) -> None:
    n = len(games)
    if n == 0:
        print("  (no games)")
        return

    # Result distribution
    results: Counter = Counter(g["result"] for g in games)
    decisive = sum(1 for g in games if g.get("decisive", g["result"] != "1/2-1/2"))
    draw_pct = 100 * results.get("1/2-1/2", 0) / n
    dec_pct  = 100 * decisive / n

    print(f"  Games       : {n}")
    print(f"  W (1-0)     : {results.get('1-0', 0):5d}  ({100*results.get('1-0',0)/n:.1f}%)")
    print(f"  D (1/2-1/2) : {results.get('1/2-1/2', 0):5d}  ({draw_pct:.1f}%)")
    print(f"  B (0-1)     : {results.get('0-1', 0):5d}  ({100*results.get('0-1',0)/n:.1f}%)")
    print(f"  Decisive    : {decisive:5d}  ({dec_pct:.1f}%)")

    if draw_pct > 80:
        print()
        print("  WARNING: >80% draws — dataset is draw-heavy.")
        print("           See docs/dataset_guide.md for how to generate decisive games.")

    # Termination breakdown
    print()
    print("  Termination breakdown:")
    terms: Counter = Counter(g.get("termination", "unknown") for g in games)
    for term, cnt in terms.most_common():
        bar = _bar(cnt, n)
        print(f"    {term:<22s}: {cnt:5d}  {bar}")

    # Ply count stats
    plies = [g.get("ply_count", 0) for g in games]
    sc    = _stats(plies)
    if sc:
        print()
        print(f"  Ply count   : min={sc['min']:.0f}  max={sc['max']:.0f}"
              f"  mean={sc['mean']:.1f}  median={sc['median']:.0f}")

    # Total positions (ply_count ≈ positions per game)
    total_pos = sum(g.get("ply_count", 0) for g in games)
    print(f"  Positions   : ~{total_pos:,}  (sum of ply counts; "
          "actual training positions depend on convert.py filters)")

    # Mode breakdown (new field, optional)
    modes: Counter = Counter(g.get("mode", "normal") for g in games)
    if len(modes) > 1 or list(modes.keys()) != ["normal"]:
        print()
        print("  Mode breakdown:")
        for mode, cnt in modes.most_common():
            bar = _bar(cnt, n)
            print(f"    {mode:<22s}: {cnt:5d}  {bar}")

    # Score stats from game-level scores (may be noisy / sparse)
    all_scores: list[int] = []
    for g in games:
        all_scores.extend(s for s in g.get("scores", []) if s is not None)

    if all_scores:
        ss = _stats(all_scores)
        print()
        print(f"  Score cp    : min={ss['min']}  max={ss['max']}"
              f"  mean={ss['mean']:.1f}  median={ss['median']:.0f}")
        zero_pct = 100 * sum(1 for s in all_scores if s == 0) / len(all_scores)
        print(f"  Zero scores : {zero_pct:.1f}% of positions "
              "(high % → engine sees draw; typical for draw-heavy datasets)")


# ---------------------------------------------------------------------------
# TSV reader (position-level)
# ---------------------------------------------------------------------------

def summarise_tsv(path: Path, check_duplicates: bool) -> None:
    scores: list[int]   = []
    wdls:   list[float] = []
    modes:  list[str]   = []
    decisive_col: list[int] = []
    fens:   list[str]   = []

    with open(path, newline="") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            try:
                scores.append(int(row["score_cp"]))
                wdls.append(float(row["wdl"]))
            except (KeyError, ValueError):
                continue
            if "fen" in row:
                fens.append(row["fen"])
            if "mode" in row:
                modes.append(row["mode"])
            if "decisive" in row:
                decisive_col.append(int(row["decisive"]))

    n = len(scores)
    if n == 0:
        print("  (no positions in TSV)")
        return

    sc = _stats(scores)
    print(f"  Positions   : {n:,}")
    print(f"  Score cp    : min={sc['min']}  max={sc['max']}"
          f"  mean={sc['mean']:.1f}  std={sc['std']:.1f}")
    print(f"               p25={sc['p25']}  median={sc['median']}  p75={sc['p75']}")

    w_cnt = sum(1 for w in wdls if w == 1.0)
    d_cnt = sum(1 for w in wdls if w == 0.5)
    b_cnt = sum(1 for w in wdls if w == 0.0)
    print(f"  WDL label   : W={w_cnt} ({100*w_cnt/n:.1f}%)"
          f"  D={d_cnt} ({100*d_cnt/n:.1f}%)"
          f"  B={b_cnt} ({100*b_cnt/n:.1f}%)")

    if decisive_col:
        dec = sum(decisive_col)
        print(f"  Decisive pos: {dec} ({100*dec/n:.1f}%)")

    if modes:
        mc: Counter = Counter(modes)
        print("  Mode breakdown:")
        for mode, cnt in mc.most_common():
            print(f"    {mode:<22s}: {cnt:,}")

    # Score distribution histogram
    buckets = [
        (">2000",        lambda s: s > 2000),
        ("1001–2000",    lambda s: 1000 < s <= 2000),
        ("101–1000",     lambda s: 100 < s <= 1000),
        ("0–100",        lambda s: 0 <= s <= 100),
        ("-100–0",       lambda s: -100 <= s < 0),
        ("-1000– -101",  lambda s: -1000 <= s < -100),
        ("-2000– -1001", lambda s: -2000 <= s < -1000),
        ("<-2000",       lambda s: s < -2000),
    ]
    print("  Score distribution:")
    for label, pred in buckets:
        cnt = sum(1 for s in scores if pred(s))
        bar = _bar(cnt, n)
        print(f"    {label:>15s}: {cnt:6d}  {bar}")

    # Duplicate FEN estimate
    if check_duplicates and fens:
        total  = len(fens)
        unique = len(set(fens))
        dupes  = total - unique
        print(f"\n  FEN duplicates : {dupes:,} / {total:,}  ({100*dupes/total:.1f}% repeated)")
        if dupes / total > 0.1:
            print("  WARNING: >10% duplicate FENs — consider deduplication before training.")
    elif check_duplicates and not fens:
        print("  (duplicate check skipped — TSV has no 'fen' column)")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    ap = argparse.ArgumentParser(
        description="Makruk-SF dataset statistics",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument("--jsonl",       default=None,
                    help="selfplay.jsonl from tools/selfplay/selfplay.py")
    ap.add_argument("--tsv",         default=None,
                    help="train.tsv from tools/training/convert.py (optional)")
    ap.add_argument("--duplicates",  action="store_true",
                    help="Check for duplicate FENs in TSV (requires --tsv, may be slow)")
    args = ap.parse_args()

    if not args.jsonl and not args.tsv:
        ap.print_help()
        sys.exit(1)

    if args.jsonl:
        path = Path(args.jsonl)
        if not path.exists():
            print(f"ERROR: not found: {path}", file=sys.stderr)
            sys.exit(1)
        print("=" * 60)
        print(f" Game-level summary ({path.name})")
        print("=" * 60)
        games = read_jsonl(path)
        summarise_jsonl(games)

    if args.tsv:
        path = Path(args.tsv)
        if not path.exists():
            print(f"ERROR: not found: {path}", file=sys.stderr)
            sys.exit(1)
        print()
        print("=" * 60)
        print(f" Position-level summary ({path.name})")
        print("=" * 60)
        summarise_tsv(path, args.duplicates)


if __name__ == "__main__":
    main()
