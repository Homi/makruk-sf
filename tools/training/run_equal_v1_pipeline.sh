#!/bin/bash
# Pipeline for equal_v1 near-equal position dataset (Option C).
# Run after equal_v1 self-play completes.
#
# Goal: collect near-equal middlegame positions (|fairy| 30-300 cp)
# where classical and Fairy-SF disagree — exactly what NNUE blend handles.
set -e

REPO=/home/bj69/Myprojects/Makruk-SF
FAIRY=/home/bj69/Myprojects/Fairy-Stockfish-NNUE/Fairy-Stockfish/src/fairy-stockfish-makruk-nnue
TRAINING=$REPO/tools/training
SELFPLAY_OUT=$REPO/tools/selfplay/out/equal_v1

log() { echo "[$(date '+%H:%M:%S')] $*"; }

# Step 1: Convert JSONL to TSV
log "Step 1: Converting equal_v1 JSONL → TSV..."
python3 $TRAINING/convert.py \
    --input  $SELFPLAY_OUT/selfplay.jsonl \
    --output $TRAINING/equal_v1.tsv \
    --min-ply 16 --max-ply 300 --max-score 2500
log "  equal_v1.tsv: $(wc -l < $TRAINING/equal_v1.tsv) lines"

# Step 2: Teacher-label with Fairy-SF depth=10
log "Step 2: Teacher-labeling with Fairy-SF depth=10..."
python3 $TRAINING/teacher_label.py \
    --input  $TRAINING/equal_v1.tsv \
    --output $TRAINING/equal_v1_fairy_d10.tsv \
    --teacher $FAIRY \
    --depth 10 \
    --resume
log "  equal_v1_fairy_d10.tsv: $(wc -l < $TRAINING/equal_v1_fairy_d10.tsv) lines"

# Step 3: Report score distribution
log "Step 3: Score distribution analysis..."
python3 -c "
import csv
scores = []
blend_range = 0
with open('$TRAINING/equal_v1_fairy_d10.tsv') as f:
    for row in csv.DictReader(f, delimiter='\t'):
        sc = int(row['score_cp'])
        scores.append(sc)
        if 30 <= abs(sc) <= 300:
            blend_range += 1
n = len(scores)
import statistics
print(f'Total positions: {n}')
print(f'Blend-range (30-300 cp): {blend_range} ({100*blend_range/n:.1f}%)')
print(f'Near-zero (|sc|<30): {sum(1 for s in scores if abs(s)<30)} ({100*sum(1 for s in scores if abs(s)<30)/n:.1f}%)')
print(f'High (|sc|>300): {sum(1 for s in scores if abs(s)>300)} ({100*sum(1 for s in scores if abs(s)>300)/n:.1f}%)')
print(f'Score std: {statistics.stdev(scores):.0f} cp')
"

# Step 4: Combine with train_v5_combined
log "Step 4: Combining equal_v1_fairy_d10 with train_v5_combined..."
python3 -c "
import csv
# Combine headers and data
sources = [
    '$TRAINING/train_v5_combined.tsv',
    '$TRAINING/equal_v1_fairy_d10.tsv',
]
header = None
rows = []
for src in sources:
    with open(src) as f:
        reader = csv.DictReader(f, delimiter='\t')
        if header is None:
            header = reader.fieldnames
        for row in reader:
            rows.append(row)
with open('$TRAINING/train_v7_combined.tsv', 'w', newline='') as f:
    writer = csv.DictWriter(f, fieldnames=header, delimiter='\t')
    writer.writeheader()
    writer.writerows(rows)
print(f'train_v7_combined.tsv: {len(rows)} positions')
"
log "  train_v7_combined.tsv ready: $(wc -l < $TRAINING/train_v7_combined.tsv) lines"

log ""
log "Pipeline complete. Next: train v7 net:"
log "  python3 $TRAINING/train.py \\"
log "    --input $TRAINING/train_v7_combined.tsv \\"
log "    --output $TRAINING/makruk_v7.pt \\"
log "    --epochs 60 --lam 0.7 --score-boost 3.0 --color-augment --batch-size 256"
