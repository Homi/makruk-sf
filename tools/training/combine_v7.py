#!/usr/bin/env python3
"""
combine_v7.py — Combine existing training data with near-equal gauntlet positions
to create the v7 combined training dataset.

Near-equal filter: keep gauntlet positions where 30 <= |fairy_score| <= 300 cp.
These are positions in the NNUE blend gate where Fairy-SF has meaningful signal.
"""
import argparse
import csv
import statistics
import sys
from pathlib import Path


def analyze(scores, label):
    n = len(scores)
    if n == 0:
        print(f'{label}: 0 positions')
        return
    near_zero = sum(1 for s in scores if abs(s) < 30)
    blend = sum(1 for s in scores if 30 <= abs(s) <= 300)
    high = sum(1 for s in scores if abs(s) > 300)
    print(f'{label}: {n} positions | near-zero(<30): {near_zero} ({100*near_zero/n:.0f}%) | '
          f'blend(30-300): {blend} ({100*blend/n:.0f}%) | high(>300): {high} ({100*high/n:.0f}%)')
    print(f'  std={statistics.stdev(scores):.0f} cp, median={statistics.median(scores):.0f} cp')


def main():
    ap = argparse.ArgumentParser(description='Combine v5 training data with near-equal gauntlet positions')
    ap.add_argument('--base',    default='train_v5_combined.tsv',
                    help='Base training TSV (train_v5_combined)')
    ap.add_argument('--gauntlet', default='gauntlet_positions_fairy_d10.tsv',
                    help='Teacher-labeled gauntlet positions')
    ap.add_argument('--output',  default='train_v7_combined.tsv')
    ap.add_argument('--min-fairy', type=int, default=30,
                    help='Min |fairy_score| to include from gauntlet data (default 30)')
    ap.add_argument('--max-fairy', type=int, default=400,
                    help='Max |fairy_score| to include from gauntlet data (default 400)')
    args = ap.parse_args()

    base_path = Path(args.base)
    gauntlet_path = Path(args.gauntlet)
    out_path = Path(args.output)

    if not base_path.exists():
        print(f'ERROR: base not found: {base_path}', file=sys.stderr)
        sys.exit(1)
    if not gauntlet_path.exists():
        print(f'ERROR: gauntlet not found: {gauntlet_path}', file=sys.stderr)
        sys.exit(1)

    # Read base data
    print('Loading base data...')
    with open(base_path) as f:
        reader = csv.DictReader(f, delimiter='\t')
        header = reader.fieldnames
        base_rows = list(reader)
    base_scores = [int(r['score_cp']) for r in base_rows]
    analyze(base_scores, 'Base (v5_combined)')

    # Read gauntlet data with near-equal filter
    print(f'\nLoading gauntlet data (filter |fairy| {args.min_fairy}-{args.max_fairy} cp)...')
    accepted, rejected_low, rejected_high = [], 0, 0
    with open(gauntlet_path) as f:
        reader = csv.DictReader(f, delimiter='\t')
        for row in reader:
            sc = int(row['score_cp'])
            if abs(sc) < args.min_fairy:
                rejected_low += 1
            elif abs(sc) > args.max_fairy:
                rejected_high += 1
            else:
                accepted.append(row)
    gauntlet_scores = [int(r['score_cp']) for r in accepted]
    print(f'Accepted: {len(accepted)} | rejected near-zero: {rejected_low} | rejected high: {rejected_high}')
    if gauntlet_scores:
        analyze(gauntlet_scores, 'Gauntlet accepted')

    # Combine
    all_rows = base_rows + accepted
    print(f'\nCombined total: {len(all_rows)} positions')
    all_scores = [int(r['score_cp']) for r in all_rows]
    analyze(all_scores, 'Combined')

    # Write output
    with open(out_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header, delimiter='\t', extrasaction='ignore')
        writer.writeheader()
        writer.writerows(all_rows)
    print(f'\nWrote: {out_path}')
    print('\nNext step — train v7:')
    print('  python3 train.py \\')
    print(f'    --input {out_path} \\')
    print('    --output makruk_v7.pt \\')
    print('    --epochs 60 --lam 0.7 --score-boost 2.0 --color-augment --batch-size 256')


if __name__ == '__main__':
    main()
