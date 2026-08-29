#!/bin/bash
# build_verify.sh -- standard post-build verification gate for Makruk-SF.
#
# Always does a full clean rebuild (never trusts incremental build state -- a
# verify step run on a possibly-corrupted incremental build, e.g. after an
# interrupted `make build` from a machine reboot, would be worthless), runs the
# full unit test suite, and runs a compact regression probe of the historically
# risky depth>=9 / imbalanced-position / UseCounting combinations tied to the
# unfixed SIGSEGV documented in CLAUDE.md ("Round 10" / "Round 17").
#
# Run this after any `make build`, before trusting a binary for a training
# round or gauntlet. Usage: bash tools/build_verify.sh   (from project root)
set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"

echo "=== [1/3] clean rebuild ==="
(cd src && make clean > /dev/null 2>&1 && make -j"$(nproc)" ARCH=x86-64-bmi2 build)
echo "OK: clean rebuild succeeded"
echo

echo "=== [2/3] unit test suite ==="
bash src/makruk/build_tests.sh
echo

echo "=== [3/3] crash-trigger regression probe (depth>=9, imbalanced positions) ==="
# One representative FEN from each imbalanced-FEN pool, including the exact FEN
# that first surfaced the crash (r2mksnr/... from imbalanced_fens_v5.txt).
FENS=(
    "1nsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"
    "r2mksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1"
)
DEPTHS=(9 12 16 20)
COUNTING_MODES=(true false)

PROBE_DIR=$(mktemp -d)
trap 'rm -rf "$PROBE_DIR"' EXIT

FAIL=0
RUN=0
for fen in "${FENS[@]}"; do
    for depth in "${DEPTHS[@]}"; do
        for counting in "${COUNTING_MODES[@]}"; do
            RUN=$((RUN+1))
            OUTF="$PROBE_DIR/probe_${RUN}.log"
            if {
                echo "uci"
                echo "isready"
                echo "setoption name UseCounting value $counting"
                echo "position fen $fen"
                echo "go depth $depth"
                echo "quit"
            } | timeout 30 "$ROOT/src/sf-kernel" > "$OUTF" 2>&1; then
                rc=0
            else
                rc=$?
            fi
            if [ $rc -ne 0 ]; then
                echo "FAIL [$RUN/16]: depth=$depth counting=$counting fen=\"$fen\" -- exit code $rc"
                if [ $rc -gt 128 ]; then
                    sig=$((rc-128))
                    echo "       killed by signal $sig ($(kill -l $sig 2>/dev/null || echo unknown))"
                fi
                tail -5 "$OUTF" | sed 's/^/       /'
                FAIL=$((FAIL+1))
            fi
        done
    done
done

echo "Probe: $((RUN-FAIL))/$RUN invocations exited cleanly"
echo

if [ $FAIL -gt 0 ]; then
    echo "=== BUILD VERIFY FAILED: $FAIL/$RUN crash-probe invocations did not exit cleanly ==="
    exit 1
fi

echo "=== BUILD VERIFIED OK ==="
