#!/bin/bash
# run_v5_pipeline.sh — Full pipeline after decisive_v5 selfplay is complete.
# Run from project root: bash tools/training/run_v5_pipeline.sh
set -e

V5_JSONL="out/decisive_v5/decisive_v5.jsonl"
V5_TSV="tools/training/decisive_v5.tsv"
V5_FAIRY="tools/training/decisive_v5_fairy_d10.tsv"
COMBINED="tools/training/train_v5_combined.tsv"
FAIRY_SF="/home/bj69/Myprojects/fairy-stockfish/src/bin/stockfish"

echo "=== Step 1: convert.py (min-ply 4 for short games) ==="
python3 tools/training/convert.py \
    --input "$V5_JSONL" \
    --output "$V5_TSV" \
    --min-ply 4 --max-ply 400 --max-score 2500
python3 tools/training/dataset_summary.py --tsv "$V5_TSV"

echo ""
echo "=== Step 2: teacher_label.py (Fairy-SF depth=10, skip bare-king) ==="
python3 -u tools/training/teacher_label.py \
    --input "$V5_TSV" \
    --output "$V5_FAIRY" \
    --teacher "$FAIRY_SF" \
    --depth 10

echo ""
echo "=== Step 3: combine datasets ==="
python3 -c "
import csv
files = [
    'tools/training/train_fairy_d8_nobare.tsv',
    'tools/training/decisive_v3_fairy_d10.tsv',
    'tools/training/decisive_v4_fairy_d10.tsv',
    '$V5_FAIRY',
]
out = '$COMBINED'
total = 0
with open(out, 'w', newline='') as fout:
    writer = None
    for path in files:
        with open(path, newline='') as fin:
            reader = csv.DictReader(fin, delimiter='\t')
            if writer is None:
                writer = csv.DictWriter(fout, fieldnames=reader.fieldnames, delimiter='\t')
                writer.writeheader()
            for row in reader:
                writer.writerow(row)
                total += 1
        print(f'  Loaded: {path}')
print(f'Total: {total:,} positions → {out}')
"
python3 tools/training/dataset_summary.py --tsv "$COMBINED"

echo ""
echo "=== Step 4: train v5 net ==="
python3 -u tools/training/train.py \
    --input "$COMBINED" \
    --output tools/training/makruk_v5.pt \
    --epochs 60 --lam 0.7 \
    --score-boost 5.0 \
    --color-augment \
    --batch-size 256 \
    2>&1 | tee tools/training/train_v5.log

echo ""
echo "=== Step 5: deploy net ==="
bash tools/training/deploy_net.sh tools/training/makruk_v5.pt

echo ""
echo "=== Step 6: rebuild ==="
make -C src -j$(nproc) ARCH=x86-64-avx2 sf-kernel

echo ""
echo "=== Pipeline complete — run gauntlet next ==="
echo "python3 -u tools/gauntlet.py \\"
echo "    --engine-a src/sf-kernel \\"
echo "    --engine-b $FAIRY_SF \\"
echo "    --games 50 --movetime-a 200 --depth-b 3 \\"
echo "    --opening-plies 6 --opening-top-n 4 \\"
echo "    --adjudication-threshold 500 --adjudication-streak 5 \\"
echo "    --seed 99 --output tools/gauntlet_out/vs_fairy_d3_adj_v5net.jsonl"
