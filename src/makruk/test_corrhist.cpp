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

#include "../movegen.h"
#include "../movepick.h"
#include "../position.h"
#include "../search.h"
#include "../thread.h"
#include "../uci.h"
#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
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

    // 5. materialKey/minorPieceKey/nonPawnKey: full-recompute-every-move design
    //    (see position.h's StateInfo comment) -- these checks exercise the
    //    actual doMove()/set() call sites, not the recompute logic in isolation,
    //    since there is no separate "incremental" path to cross-check against.
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        const uint64_t mat0 = pos.materialKey();
        const uint64_t minor0 = pos.minorPieceKey();
        const uint64_t npW0 = pos.nonPawnKey(WHITE);
        const uint64_t npB0 = pos.nonPawnKey(BLACK);

        // A quiet pawn push changes none of these (no capture, no minor/non-pawn
        // piece moved).
        st->emplace_back();
        pos.doMove(makeMove(SQ_D3, SQ_D4), st->back());
        check(pos.materialKey() == mat0, "quiet pawn push leaves materialKey unchanged");
        check(pos.minorPieceKey() == minor0, "quiet pawn push leaves minorPieceKey unchanged");
        check(pos.nonPawnKey(WHITE) == npW0, "quiet pawn push leaves White nonPawnKey unchanged");
        check(pos.nonPawnKey(BLACK) == npB0, "quiet pawn push leaves Black nonPawnKey unchanged");

        // A knight move changes minorPieceKey and that side's nonPawnKey, but not
        // materialKey (no capture) and not the opponent's nonPawnKey. (b1-d2, not
        // the chess-familiar b1-c3/a3, because Makruk's start position has pawns
        // on all of rank 3, blocking both of those.)
        st->emplace_back();
        pos.doMove(makeMove(SQ_B1, SQ_D2), st->back());
        check(pos.materialKey() == mat0, "quiet knight move leaves materialKey unchanged");
        check(pos.minorPieceKey() != minor0, "quiet knight move changes minorPieceKey");
        check(pos.nonPawnKey(WHITE) != npW0, "quiet knight move changes White nonPawnKey");
        check(pos.nonPawnKey(BLACK) == npB0, "quiet knight move leaves Black nonPawnKey unchanged");
    }
    {
        // A capture changes materialKey (piece count changed) and the captured
        // side's nonPawnKey (its piece is gone); if the captured piece is a
        // minor, minorPieceKey changes too.
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/4n3/4R3/8/8/4K3 w - - 0 1"); // White Rook e4, Black Knight e5 (same file)
        const uint64_t mat0 = pos.materialKey();
        const uint64_t minor0 = pos.minorPieceKey();
        const uint64_t npB0 = pos.nonPawnKey(BLACK);
        st->emplace_back();
        pos.doMove(makeMove(SQ_E4, SQ_E5), st->back()); // Rxe5, capturing the knight
        check(pos.materialKey() != mat0, "capturing a knight changes materialKey");
        check(pos.minorPieceKey() != minor0, "capturing a knight changes minorPieceKey");
        check(pos.nonPawnKey(BLACK) != npB0, "capturing Black's knight changes Black's nonPawnKey");
    }
    {
        // Promotion changes materialKey (pawn count down, Met count up) and the
        // promoting side's nonPawnKey (a new Met appears), but not minorPieceKey
        // (Met is not a minor piece).
        StateListPtr st;
        Position pos;
        // White pawn e5 (relativeRank 4, one step from promotion); e6 is the
        // promotion square (relativeRank 5).
        setPos(pos, st, "4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");
        const uint64_t mat0 = pos.materialKey();
        const uint64_t minor0 = pos.minorPieceKey();
        const uint64_t npW0 = pos.nonPawnKey(WHITE);
        st->emplace_back();
        pos.doMove(make<PROMOTION>(SQ_E5, SQ_E6, QUEEN), st->back());
        check(pos.materialKey() != mat0, "promotion changes materialKey");
        check(pos.minorPieceKey() == minor0, "promotion to Met does not change minorPieceKey");
        check(pos.nonPawnKey(WHITE) != npW0, "promotion changes the promoting side's nonPawnKey");
    }
    {
        // Robustness: random-walk game, confirming no crash and that doMove()'s
        // maintained keys always match an independent fresh recompute (loading
        // the resulting FEN via a brand-new Position::set() call) -- this is a
        // genuine integration check (catches a missed/mismatched call site)
        // even though both paths ultimately call the same recomputeAuxKeys().
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        std::mt19937 rng(0xC077EC7);
        int plies = 0;
        for (; plies < 300; ++plies) {
            MoveList<LEGAL> moves(pos);
            if (moves.size() == 0)
                break;
            std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
            const Move m = moves.begin()[pick(rng)];
            st->emplace_back();
            pos.doMove(m, st->back());

            StateListPtr freshSt;
            Position freshPos;
            setPos(freshPos, freshSt, pos.fen());
            check(pos.materialKey() == freshPos.materialKey(),
                  "materialKey matches a fresh recompute at ply " + std::to_string(plies));
            check(pos.minorPieceKey() == freshPos.minorPieceKey(),
                  "minorPieceKey matches a fresh recompute at ply " + std::to_string(plies));
            check(pos.nonPawnKey(WHITE) == freshPos.nonPawnKey(WHITE),
                  "White nonPawnKey matches a fresh recompute at ply " + std::to_string(plies));
            check(pos.nonPawnKey(BLACK) == freshPos.nonPawnKey(BLACK),
                  "Black nonPawnKey matches a fresh recompute at ply " + std::to_string(plies));
        }
        std::cout << "  aux-key random walk: " << plies << " plies, "
                  << g_checks << " total checks so far\n";
    }

    if (g_fail) {
        std::cerr << g_fail << " correction-history check(s) FAILED (out of "
                  << g_checks << ")\n";
        std::exit(1);
    }
    std::cout << "PASS: pawn correction history (" << g_checks << " checks)\n";
}

} // namespace Nebula
