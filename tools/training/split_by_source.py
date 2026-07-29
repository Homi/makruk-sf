#!/usr/bin/env python3
"""
split_by_source.py — Reconstruct teacher-labeled data for a chosen subset of
raw (pre-label) source TSVs, by joining on FEN against an already-labeled TSV.

Motivation: convert.py/teacher_label.py/combine_v7.py's existing pipeline merges
every gauntlet config into one file before teacher-labeling, which loses the
ability to include/exclude individual sources afterward. This tool splits that
back apart using the FEN as the join key — no new engine calls needed, since
every FEN was already scored once during teacher-labeling.

Output is a labeled TSV in the same format combine_v7.py expects as --gauntlet,
so the normal pipeline (combine_v7.py --base ... --gauntlet <this output>) still
applies the blend-range filter and merges with the base dataset.

Usage:
    python3 split_by_source.py \
        --labeled round12_fairy_d10.tsv \
        --source configA=round12_configA.tsv \
        --source configC=round12_configC.tsv \
        --output round13_configAC_labeled.tsv
"""
import argparse
import csv
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser(
        description='Reconstruct per-source labeled TSVs by FEN join against an '
                     'already-labeled merged TSV, for source-aware dataset mixing.')
    ap.add_argument('--labeled', required=True,
                     help='Already teacher-labeled merged TSV (FEN -> score lookup)')
    ap.add_argument('--source', action='append', default=[], metavar='LABEL=PATH',
                     help='Raw (pre-label) source TSV to include, tagged with LABEL. '
                          'Repeatable. Sources not listed here are implicitly excluded.')
    ap.add_argument('--output', required=True)
    args = ap.parse_args()

    if not args.source:
        print('ERROR: at least one --source LABEL=PATH is required', file=sys.stderr)
        sys.exit(1)

    labeled_path = Path(args.labeled)
    if not labeled_path.exists():
        print(f'ERROR: labeled file not found: {labeled_path}', file=sys.stderr)
        sys.exit(1)

    print(f'Loading labeled lookup: {labeled_path}...')
    with open(labeled_path) as f:
        reader = csv.DictReader(f, delimiter='\t')
        header = reader.fieldnames
        lookup = {row['fen']: row for row in reader}
    print(f'  {len(lookup)} labeled FENs available')

    out_rows = []
    for spec in args.source:
        if '=' not in spec:
            print(f'ERROR: --source must be LABEL=PATH, got: {spec}', file=sys.stderr)
            sys.exit(1)
        label, path_str = spec.split('=', 1)
        path = Path(path_str)
        if not path.exists():
            print(f'ERROR: source not found: {path}', file=sys.stderr)
            sys.exit(1)

        with open(path) as f:
            reader = csv.DictReader(f, delimiter='\t')
            fens = [row['fen'] for row in reader]

        found, missing = 0, 0
        for fen in fens:
            row = lookup.get(fen)
            if row is None:
                missing += 1
                continue
            out_rows.append(row)
            found += 1
        print(f'  source "{label}" ({path}): {len(fens)} raw FENs -> '
              f'{found} labeled, {missing} not in lookup (dropped during teacher-labeling)')

    out_path = Path(args.output)
    with open(out_path, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header, delimiter='\t', extrasaction='ignore')
        writer.writeheader()
        writer.writerows(out_rows)

    print(f'\nWrote {len(out_rows)} labeled positions from {len(args.source)} source(s) '
          f'to: {out_path}')
    print('\nNext step — filter to blend range and combine with a base dataset:')
    print('  python3 combine_v7.py \\')
    print(f'    --base <base>.tsv --gauntlet {out_path} \\')
    print('    --output <combined>.tsv')


if __name__ == '__main__':
    main()
