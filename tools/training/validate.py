#!/usr/bin/env python3
"""
validate.py — Dataset statistics for a training TSV produced by convert.py.

Prints position count, score distribution, result distribution, and
optionally samples random positions for visual inspection.

Usage:
    python validate.py --input train.tsv
    python validate.py --input train.tsv --sample 5

Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.
"""

import argparse
import csv
import math
import random
import sys
from pathlib import Path


def _stats(values: list) -> dict:
    if not values:
        return {}
    n = len(values)
    s = sorted(values)
    mean = sum(values) / n
    std  = math.sqrt(sum((x - mean) ** 2 for x in values) / n)
    return {
        'n': n, 'min': s[0], 'max': s[-1],
        'mean': mean, 'std': std,
        'p25': s[n // 4], 'median': s[n // 2], 'p75': s[3 * n // 4],
    }


def main() -> None:
    ap = argparse.ArgumentParser(
        description='Validate NNUE training TSV',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument('--input',  required=True, help='train.tsv from convert.py')
    ap.add_argument('--sample', type=int, default=0,
                    help='Print N random sample positions (0 = none)')
    args = ap.parse_args()

    path = Path(args.input)
    if not path.exists():
        print(f'ERROR: not found: {path}', file=sys.stderr)
        sys.exit(1)

    scores: list[int] = []
    wdls:   list[float] = []
    fens:   list[str] = []

    with open(path, newline='') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            scores.append(int(row['score_cp']))
            wdls.append(float(row['wdl']))
            fens.append(row['fen'])

    if not scores:
        print('No positions found.')
        return

    sc = _stats(scores)
    print(f'Positions : {sc["n"]}')
    print(f'Score cp  : min={sc["min"]}  max={sc["max"]}'
          f'  mean={sc["mean"]:.1f}  std={sc["std"]:.1f}')
    print(f'          : p25={sc["p25"]}  median={sc["median"]}  p75={sc["p75"]}')

    n = len(wdls)
    w_count = sum(1 for w in wdls if w == 1.0)
    d_count = sum(1 for w in wdls if w == 0.5)
    b_count = sum(1 for w in wdls if w == 0.0)
    print(f'Result    : W={w_count} ({100*w_count/n:.1f}%)'
          f'  D={d_count} ({100*d_count/n:.1f}%)'
          f'  B={b_count} ({100*b_count/n:.1f}%)')

    buckets = [
        ('>2000',         lambda s: s > 2000),
        ('1001–2000',     lambda s: 1000 < s <= 2000),
        ('101–1000',      lambda s: 100 < s <= 1000),
        ('0–100',         lambda s: 0 <= s <= 100),
        ('-100–0',        lambda s: -100 <= s < 0),
        ('-1000– -101',   lambda s: -1000 <= s < -100),
        ('-2000– -1001',  lambda s: -2000 <= s < -1000),
        ('<-2000',        lambda s: s < -2000),
    ]
    print('Score dist:')
    bar_width = 40
    for label, pred in buckets:
        cnt = sum(1 for s in scores if pred(s))
        bar = '#' * (cnt * bar_width // n) if n else ''
        print(f'  {label:>16s}: {cnt:6d}  {bar}')

    if args.sample > 0:
        k = min(args.sample, n)
        print(f'\nSample ({k} random positions):')
        for i in random.sample(range(n), k):
            print(f'  [{i:6d}]  score={scores[i]:+5d}  wdl={wdls[i]}  {fens[i]}')


if __name__ == '__main__':
    main()
