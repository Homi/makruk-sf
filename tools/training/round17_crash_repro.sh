#!/bin/bash
# Round 17: bounded reproduction attempt for the unfixed SIGSEGV at search depth>=9.
# Replicates the exact original discovery recipe (--no-counting, movetime-based free
# iterative deepening -- deliberately NO --strong-depth cap) across both known
# imbalanced-FEN pools, several seeds each, on a freshly clean-built binary.
# Core dumps enabled (ulimit -c unlimited) so a raw segfault -- as opposed to the
# now-hardened abort() paths -- would leave forensic evidence.
set -o pipefail
cd /home/bj69/Myprojects/Makruk-SF

ulimit -c unlimited
OUT=tools/gauntlet_out/round17_repro
mkdir -p "$OUT"

echo "core_pattern: $(cat /proc/sys/kernel/core_pattern)"
echo "ulimit -c: $(ulimit -c)"
echo "engine binary: $(md5sum src/sf-kernel)"

cd tools/selfplay

RUN=0
for FENFILE in imbalanced_fens.txt imbalanced_fens_v5.txt; do
    for SEED in 4001 4002 4003; do
        RUN=$((RUN+1))
        NAME="pool_${FENFILE%.txt}_seed${SEED}"
        echo "=== [$RUN/6] $NAME: 25 games, depth_handicap depth-cap=1, movetime=300, no-counting, no strong-depth ==="
        python3 selfplay.py \
            --games 25 --mode depth_handicap --depth-cap 1 \
            --movetime 300 --opening-plies 6 --opening-top-n 4 \
            --fens-file "$FENFILE" \
            --no-counting \
            --adjudication-threshold 500 --adjudication-streak 5 \
            --seed $SEED \
            --pgn ${NAME}.pgn --jsonl ${NAME}.jsonl \
            --outdir ../../$OUT \
            > ../../$OUT/${NAME}.stdout.log 2> ../../$OUT/${NAME}.stderr.log
        RC=$?
        echo "    exit code: $RC"
        if [ $RC -ne 0 ]; then
            echo "    *** NONZERO EXIT -- possible crash, check ${NAME}.stderr.log ***"
        fi
        if grep -q "FATAL" ../../$OUT/${NAME}.stderr.log 2>/dev/null; then
            echo "    *** FATAL: diagnostic found in ${NAME}.stderr.log ***"
            grep "FATAL" -A 3 ../../$OUT/${NAME}.stderr.log
        fi
    done
done

cd ../..
echo "=== checking for core dump files ==="
find . -maxdepth 3 -name "core" -o -name "core.*" 2>/dev/null | grep -v "^\./tools/gauntlet_out/round17_repro$" || true
ls -la core core.* 2>/dev/null || echo "no core file in project root"
ls -la tools/selfplay/core tools/selfplay/core.* 2>/dev/null || echo "no core file in tools/selfplay"

echo "=== REPRODUCTION ATTEMPT COMPLETE ==="
