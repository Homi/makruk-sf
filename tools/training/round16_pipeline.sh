#!/bin/bash
# Round 16 pipeline: same data-gen/train recipe as Round 14, but using the new
# book-paired gauntlet methodology (tools/books/makruk_no_counting.epd) instead
# of --opening-plies, for both data generation and final evaluation.
# Run from project root. Intended to be launched via nohup + disown.
set -e
set -o pipefail
cd /home/bj69/Myprojects/Makruk-SF

FAIRY=/home/bj69/Myprojects/Fairy-Stockfish-NNUE/Fairy-Stockfish/src/fairy-stockfish-makruk-nnue
BOOK=tools/books/makruk_no_counting.epd
OUT=tools/gauntlet_out/round16
TRAIN=tools/training

echo "=== [1/9] Config A data-gen: 150 games, movetime-a=200 depth-b=4, book, seed=11001 ==="
cd tools
python3 gauntlet.py \
    --engine-b "$FAIRY" \
    --games 150 --movetime-a 200 --depth-b 4 \
    --book ../$BOOK \
    --adjudication-threshold 500 --adjudication-streak 5 \
    --seed 11001 \
    --output ../$OUT/configA_book.jsonl 2>&1 | tee ../$OUT/configA_book.log
cd ..

echo "=== [2/9] Config C data-gen: 150 games, movetime-a=150 depth-b=3, book, seed=11002 ==="
cd tools
python3 gauntlet.py \
    --engine-b "$FAIRY" \
    --games 150 --movetime-a 150 --depth-b 3 \
    --book ../$BOOK \
    --adjudication-threshold 500 --adjudication-streak 5 \
    --seed 11002 \
    --output ../$OUT/configC_book.jsonl 2>&1 | tee ../$OUT/configC_book.log
cd ..

echo "=== [3/9] convert.py (min-ply=4) each config ==="
python3 $TRAIN/convert.py --input $OUT/configA_book.jsonl --output $TRAIN/round16_configA.tsv --min-ply 4
python3 $TRAIN/convert.py --input $OUT/configC_book.jsonl --output $TRAIN/round16_configC.tsv --min-ply 4

echo "=== [4/9] concatenate raw (single header) ==="
{ head -1 $TRAIN/round16_configA.tsv; tail -n +2 $TRAIN/round16_configA.tsv; tail -n +2 $TRAIN/round16_configC.tsv; } > $TRAIN/round16_raw.tsv
wc -l $TRAIN/round16_raw.tsv

echo "=== [5/9] teacher-label with Fairy-SF depth=10 ==="
python3 $TRAIN/teacher_label.py \
    --input $TRAIN/round16_raw.tsv \
    --output $TRAIN/round16_fairy_d10.tsv \
    --teacher "$FAIRY" \
    --depth 10

echo "=== [6/9] filter to blend range + combine with Round 14 base ==="
python3 $TRAIN/combine_v7.py \
    --base $TRAIN/train_round14_combined.tsv \
    --gauntlet $TRAIN/round16_fairy_d10.tsv \
    --output $TRAIN/train_round16_combined.tsv

echo "=== dataset_summary sanity check ==="
python3 $TRAIN/dataset_summary.py --tsv $TRAIN/train_round16_combined.tsv --duplicates

echo "=== [7/9] training (epochs=60, lam=0.7, score-boost=2.0, color-augment, resume-enabled) ==="
cd $TRAIN
python3 -u train.py \
    --input train_round16_combined.tsv \
    --output makruk_round16.pt \
    --epochs 60 --lam 0.7 --score-boost 2.0 --color-augment --batch-size 256 --resume
cd ../..

echo "=== [8/9] export int16 MKN2 ==="
cd $TRAIN
python3 export_int16.py --input makruk_round16.pt --output /tmp/round16_export.bin
cd ../..
CRC=$(python3 -c "import zlib; print(zlib.crc32(open('/tmp/round16_export.bin','rb').read()) & 0xFFFFFFFF)")
cp /tmp/round16_export.bin src/${CRC}.bin
echo "Round16 net: src/${CRC}.bin (CRC32=${CRC})"
echo "$CRC" > $OUT/round16_net_crc.txt

echo "=== [9/9] build both nets (no deploy yet -- deploy decision made after reviewing results) ==="
OLD_NET=$(grep 'NnueNetDefaultName' src/evaluate.h | grep -o '"[^"]*"' | tr -d '"')

echo "--- building Round14 (currently-committed) binary for comparison ---"
(cd src && make -j$(nproc) ARCH=x86-64-bmi2 build)
cp src/sf-kernel src/sf-kernel-round14

echo "--- building Round16 binary (net not yet deployed/committed) ---"
sed -i "s|NnueNetDefaultName.*|NnueNetDefaultName   \"${CRC}.bin\"|" src/evaluate.h
(cd src && make -j$(nproc) ARCH=x86-64-bmi2 build)
cp src/sf-kernel src/sf-kernel-round16

echo "--- restoring repo to originally-committed state (Round14 default) ---"
sed -i "s|NnueNetDefaultName.*|NnueNetDefaultName   \"${OLD_NET}\"|" src/evaluate.h
(cd src && make -j$(nproc) ARCH=x86-64-bmi2 build)
git diff --stat src/evaluate.h  # should be empty

bash src/makruk/build_tests.sh 2>&1 | tail -20

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
