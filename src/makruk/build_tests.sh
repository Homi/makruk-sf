#!/usr/bin/env bash
# Build and run Makruk unit tests on Linux.
# Run from the src/ directory:  bash makruk/build_tests.sh

set -e
cd "$(dirname "$0")/.."

cp ../net/1877415756.bin . 2>/dev/null || true

ENGINE_SRCS="bitboard.cpp misc.cpp movepick.cpp position.cpp search.cpp \
  thread.cpp timeman.cpp tt.cpp uci.cpp ucioption.cpp evaluate.cpp \
  nnue/evaluate_nnue.cpp nnue/features/half_ka_v2_makruk.cpp"

TEST_SRCS="makruk/makruk_counting.cpp makruk/makruk_eval.cpp makruk/test_movegen.cpp makruk/test_legality.cpp makruk/test_counting.cpp makruk/test_eval.cpp makruk/test_runner.cpp"

g++ -std=c++20 -O2 -DNDEBUG -DIS_64BIT \
    $ENGINE_SRCS $TEST_SRCS \
    -lpthread -o makruk_tests

echo "Build OK — running tests..."
./makruk_tests
