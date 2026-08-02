#!/bin/bash
# Resumes Round 16 pipeline from stage 9 (book-paired evaluation gauntlets only) --
# data-gen, teacher-labeling, training and binary builds already completed before
# a reboot interrupted the original round16_pipeline.sh mid-compile.
set -e
set -o pipefail
cd /home/bj69/Myprojects/Makruk-SF

FAIRY=/home/bj69/Myprojects/Fairy-Stockfish-NNUE/Fairy-Stockfish/src/fairy-stockfish-makruk-nnue
BOOK=tools/books/makruk_no_counting.epd
OUT=tools/gauntlet_out/round16

echo "=== book-paired gauntlet: Round16 net vs Fairy-SF-NNUE depth=3 (seed=99, seed=4242) ==="
cd tools
for SEED in 99 4242; do
    python3 gauntlet.py --engine-a ../src/sf-kernel-round16 --engine-b "$FAIRY" \
        --games 100 --movetime-a 200 --depth-b 3 \
        --book ../$BOOK \
        --adjudication-threshold 500 --adjudication-streak 5 \
        --seed $SEED \
        --output ../$OUT/eval_round16_seed${SEED}.jsonl 2>&1 | tee ../$OUT/eval_round16_seed${SEED}.log
done

echo "=== book-paired gauntlet: Round14 net vs Fairy-SF-NNUE depth=3 (seed=99, seed=4242) ==="
for SEED in 99 4242; do
    python3 gauntlet.py --engine-a ../src/sf-kernel-round14 --engine-b "$FAIRY" \
        --games 100 --movetime-a 200 --depth-b 3 \
        --book ../$BOOK \
        --adjudication-threshold 500 --adjudication-streak 5 \
        --seed $SEED \
        --output ../$OUT/eval_round14_seed${SEED}.jsonl 2>&1 | tee ../$OUT/eval_round14_seed${SEED}.log
done
cd ..

echo "=== PIPELINE COMPLETE ==="
