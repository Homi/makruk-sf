#!/bin/bash
# Makruk-SF training pipeline for decisive_v3 data
# Run after decisive_v3 selfplay completes.
# Usage: bash run_pipeline.sh [--skip-convert] [--skip-teacher] [--skip-combine] [--skip-train] [--skip-export]

set -e
REPO=/home/bj69/Myprojects/Makruk-SF
FAIRY=/home/bj69/Myprojects/fairy-stockfish/src/bin/stockfish
TRAINING=$REPO/tools/training
SELFPLAY_OUT=$REPO/tools/selfplay/out/decisive_v3

SKIP_CONVERT=0
SKIP_TEACHER=0
SKIP_COMBINE=0
SKIP_TRAIN=0
SKIP_EXPORT=0

for arg in "$@"; do
  case $arg in
    --skip-convert) SKIP_CONVERT=1 ;;
    --skip-teacher) SKIP_TEACHER=1 ;;
    --skip-combine) SKIP_COMBINE=1 ;;
    --skip-train)   SKIP_TRAIN=1 ;;
    --skip-export)  SKIP_EXPORT=1 ;;
  esac
done

log() { echo "[$(date '+%H:%M:%S')] $*"; }

# Step 1: Wait for decisive_v3 to complete
log "Waiting for decisive_v3 to reach 1000 games..."
while true; do
  count=$(python3 -c "import json; s=json.load(open('$SELFPLAY_OUT/selfplay_state.json')); print(s['games_completed'])" 2>/dev/null || echo 0)
  if [ "$count" -ge 1000 ]; then
    log "decisive_v3 complete: $count games"
    break
  fi
  log "  $count/1000 games done, waiting..."
  sleep 120
done

# Step 2: Convert decisive_v3 to TSV
if [ $SKIP_CONVERT -eq 0 ]; then
  log "Step 1: Converting decisive_v3 JSONL to TSV..."
  cd $TRAINING
  python3 convert.py \
    --input  $SELFPLAY_OUT/selfplay.jsonl \
    --output $TRAINING/decisive_v3.tsv \
    --min-ply 16 --max-ply 400 --max-score 2500
  log "  decisive_v3.tsv created: $(wc -l < decisive_v3.tsv) lines"
else
  log "Skipping convert (--skip-convert)"
fi

# Step 3: Teacher-label decisive_v3 with Fairy-SF depth=10
if [ $SKIP_TEACHER -eq 0 ]; then
  log "Step 2: Teacher-labeling decisive_v3 with Fairy-SF depth=10..."
  cd $TRAINING
  python3 teacher_label.py \
    --input  $TRAINING/decisive_v3.tsv \
    --output $TRAINING/decisive_v3_fairy_d10.tsv \
    --teacher $FAIRY \
    --depth 10 \
    --resume
  log "  decisive_v3_fairy_d10.tsv: $(wc -l < decisive_v3_fairy_d10.tsv) lines"
else
  log "Skipping teacher-label (--skip-teacher)"
fi

# Step 4: Combine datasets
if [ $SKIP_COMBINE -eq 0 ]; then
  log "Step 3: Combining train_fairy_d8_nobare.tsv + decisive_v3_fairy_d10.tsv..."
  cd $TRAINING
  head -1 train_fairy_d8_nobare.tsv > train_combined.tsv
  tail -n +2 train_fairy_d8_nobare.tsv >> train_combined.tsv
  tail -n +2 decisive_v3_fairy_d10.tsv >> train_combined.tsv
  COMBINED_N=$(wc -l < train_combined.tsv)
  log "  train_combined.tsv: $COMBINED_N lines"
  # Print score distribution
  python3 -c "
import csv
scores = []
with open('train_combined.tsv') as f:
    for row in csv.DictReader(f, delimiter='\t'):
        scores.append(abs(int(row['score_cp'])))
n = len(scores)
over500 = sum(1 for s in scores if s >= 500)
print(f'  Combined: {n} positions, |score|>500: {over500} ({100*over500/n:.1f}%)')
"
else
  log "Skipping combine (--skip-combine)"
fi

# Step 5: Train
if [ $SKIP_TRAIN -eq 0 ]; then
  log "Step 4: Training (50 epochs, lam=0.7, score-boost=10)..."
  cd $TRAINING
  python3 train.py \
    --input  train_combined.tsv \
    --output makruk_combined.pt \
    --epochs 50 \
    --lam 0.7 \
    --score-boost 10.0
  log "  Training complete. Checkpoint: makruk_combined.pt"
else
  log "Skipping training (--skip-train)"
fi

# Step 6: Export binary
if [ $SKIP_EXPORT -eq 0 ]; then
  log "Step 5: Exporting binary..."
  cd $TRAINING
  python3 export.py --input makruk_combined.pt --output /tmp/makruk_combined_net.bin --info
  # Compute CRC32 to get the canonical name
  CRC=$(python3 -c "
import binascii
with open('/tmp/makruk_combined_net.bin','rb') as f:
    data = f.read()
print(binascii.crc32(data) & 0xFFFFFFFF)
")
  log "  CRC32: $CRC"
  cp /tmp/makruk_combined_net.bin $REPO/src/${CRC}.bin
  log "  Exported: src/${CRC}.bin"

  # Update evaluate.h
  OLD_NAME=$(grep 'NnueNetDefaultName' $REPO/src/evaluate.h | grep -oP '"[^"]+"')
  NEW_NAME="\"${CRC}.bin\""
  sed -i "s|$OLD_NAME|$NEW_NAME|g" $REPO/src/evaluate.h
  log "  Updated evaluate.h: $OLD_NAME -> $NEW_NAME"

  # Rebuild sf-kernel
  log "Step 6: Rebuilding sf-kernel..."
  cd $REPO/src
  touch evaluate.cpp
  make ARCH=x86-64-modern EXE=sf-kernel all 2>&1 | tail -5
  log "  Build complete: $(ls -lh sf-kernel | awk '{print $5}')"
  log "  Binary: $(ls -la sf-kernel)"
else
  log "Skipping export (--skip-export)"
fi

log "Pipeline complete!"
log ""
log "Next step: run standard baseline gauntlet:"
log "  python3 tools/gauntlet.py \\"
log "    --engine-a src/sf-kernel \\"
log "    --engine-b /home/bj69/Myprojects/fairy-stockfish/src/bin/stockfish \\"
log "    --games 50 --movetime-a 200 --depth-b 3 \\"
log "    --opening-plies 6 --opening-top-n 4 \\"
log "    --adjudication-threshold 500 --adjudication-streak 5 \\"
log "    --seed 99 \\"
log "    --jsonl tools/gauntlet_out/vs_fairy_d3_adj_combined_net.jsonl"
