// Regression tests for the pawn correction history (Round 29).
//
// Background: this backport adds a static-eval bias-correction table keyed by
// Position::pawnKey(), analogous to modern Stockfish's correction history --
// see CLAUDE.md's Round 29 write-up for the full design rationale. This file
// checks the mechanism in isolation (table semantics, real Thread::clear()
// integration, and Search::correctedStaticEval()'s output-range safety),
// not full game play -- that's what the gauntlet is for.
//
// NOTE: a directly-constructed `Thread t(0)` (bypassing ThreadPool) crashes
// even on this project's unmodified code -- confirmed empirically while
// writing this test. Thread's startup depends on global state (UCI Options)
// that only ThreadPool::set() establishes correctly, matching main.cpp's own
// init order (Uci::init(options) before threads.set(...)). This test
// replicates that exact sequence rather than constructing a Thread directly.
//
// NOTE: build_tests.sh compiles with -DNDEBUG, which strips assert()
// entirely -- every check here is an explicit runtime comparison that
// prints and calls std::exit(1) on failure, not assert().
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "../movepick.h"
#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../uci.h"
#include <cstdlib>
#include <deque>
#include <iostream>
#include <string>

namespace Nebula {
namespace {

int g_fail = 0;
int g_checks = 0;

void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_fail;
        std::cerr << "FAIL: " << what << "\n";
    }
}

void setPos(Position& pos, StateListPtr& states, const std::string& fen) {
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, false, &states->back(), nullptr);
}

} // namespace

void runCorrectionHistoryTests() {
    // 1. Table starts at zero after fill(0).
    {
        PawnCorrectionHistory table;
        table.fill(0);
        check(static_cast<int16_t>(table[WHITE][0]) == 0, "table[WHITE][0] starts at 0");
        check(static_cast<int16_t>(table[BLACK][PawnCorrHistSize - 1]) == 0,
              "table[BLACK][last] starts at 0");
        check(static_cast<int16_t>(table[WHITE][PawnCorrHistSize / 2]) == 0,
              "table[WHITE][mid] starts at 0");
    }

    // 2. Bonus/malus moves the entry in the expected direction, and repeated
    //    same-sign bonuses converge toward the divisor (saturation ceiling
    //    the constants in movepick.h rely on: Grain*MaxCorrection==Divisor).
    {
        PawnCorrectionHistory table;
        table.fill(0);
        table[WHITE][100] << PawnCorrHistUpdateCap;
        check(static_cast<int16_t>(table[WHITE][100]) > 0,
              "positive bonus moves entry positive");

        table[BLACK][200] << -PawnCorrHistUpdateCap;
        check(static_cast<int16_t>(table[BLACK][200]) < 0,
              "negative bonus moves entry negative");

        for (int i = 0; i < 200; ++i)
            table[WHITE][300] << PawnCorrHistUpdateCap;
        const int16_t saturated = table[WHITE][300];
        check(saturated > 0 && saturated <= PawnCorrHistDivisor,
              "repeated positive bonus saturates within (0, Divisor]");
        check(saturated > PawnCorrHistDivisor * 9 / 10,
              "repeated positive bonus gets close to the Divisor ceiling");
    }

    // 3+4. Thread::clear() -- the real integration point added in thread.cpp
    //    -- actually resets pawnCorrectionHistory, and Search::correctedStaticEval()
    //    never produces a value outside the mate/TB-safe range.
    //
    //    Uses the real ThreadPool (Nebula::threads), not a directly-constructed
    //    Thread: a raw `Thread t(0)` bypassing ThreadPool crashes even on this
    //    project's unmodified code (confirmed empirically -- Thread's startup
    //    depends on global state, e.g. UCI Options, that only ThreadPool::set()
    //    establishes correctly), so this test replicates the exact init sequence
    //    main.cpp uses: Uci::init(options) before threads.set(...).
    {
        Uci::init(options);
        threads.set(1);
        Thread* th = threads.main();

        th->pawnCorrectionHistory[WHITE][42] << PawnCorrHistUpdateCap;
        th->pawnCorrectionHistory[BLACK][1000] << -PawnCorrHistUpdateCap;
        check(static_cast<int16_t>(th->pawnCorrectionHistory[WHITE][42]) != 0,
              "poked entry is nonzero before clear()");
        th->clear();
        check(static_cast<int16_t>(th->pawnCorrectionHistory[WHITE][42]) == 0,
              "Thread::clear() resets pawnCorrectionHistory[WHITE][42]");
        check(static_cast<int16_t>(th->pawnCorrectionHistory[BLACK][1000]) == 0,
              "Thread::clear() resets pawnCorrectionHistory[BLACK][1000]");

        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");

        // Realistic mid-range eval, neutral table: correction should be 0,
        // output should equal input exactly.
        const Value ev1 = static_cast<Value>(30);
        const Value out1 = Search::correctedStaticEval(ev1, WHITE, pos, th);
        check(out1 == ev1, "neutral table applies zero correction");

        // Adversarial: saturate the entry this position's pawnKey maps to,
        // with ev pushed right up against VALUE_TB_WIN_IN_MAX_PLY.
        const uint64_t idx = pos.pawnKey() & (PawnCorrHistSize - 1);
        for (int i = 0; i < 200; ++i)
            th->pawnCorrectionHistory[WHITE][idx] << PawnCorrHistUpdateCap;
        const Value ev2 = static_cast<Value>(VALUE_TB_WIN_IN_MAX_PLY - 1);
        const Value out2 = Search::correctedStaticEval(ev2, WHITE, pos, th);
        check(out2 < VALUE_TB_WIN_IN_MAX_PLY, "saturated-positive correction stays below TB-win range");
        check(out2 > VALUE_TB_LOSS_IN_MAX_PLY, "saturated-positive correction stays above TB-loss range");
        check(out2 != VALUE_NONE && out2 != VALUE_INFINITE && out2 != VALUE_MATE,
              "saturated-positive correction never collides with a sentinel value");

        for (int i = 0; i < 400; ++i)
            th->pawnCorrectionHistory[BLACK][idx] << -PawnCorrHistUpdateCap;
        const Value ev3 = static_cast<Value>(VALUE_TB_LOSS_IN_MAX_PLY + 1);
        const Value out3 = Search::correctedStaticEval(ev3, BLACK, pos, th);
        check(out3 > VALUE_TB_LOSS_IN_MAX_PLY, "saturated-negative correction stays above TB-loss range");
        check(out3 < VALUE_TB_WIN_IN_MAX_PLY, "saturated-negative correction stays below TB-win range");

        threads.set(0);
    }

    if (g_fail) {
        std::cerr << g_fail << " correction-history check(s) FAILED (out of "
                  << g_checks << ")\n";
        std::exit(1);
    }
    std::cout << "PASS: pawn correction history (" << g_checks << " checks)\n";
}

} // namespace Nebula
