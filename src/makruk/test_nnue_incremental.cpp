// Incremental NNUE accumulator correctness tests.
//
// Verifies MknnEvaluator's incremental accumulator path (see mknn_evaluator.cpp,
// updateAccumulator()) produces results bit-exact against a from-scratch
// reference rebuild, across move sequences chosen to exercise every DirtyPiece
// shape (quiet move, capture, promotion, capturing promotion, castling-free
// king move) and every branch of the backward-walk algorithm (1-hop, multi-hop,
// multi-hop spanning a king move, budget exhaustion, null move, sibling reuse).
//
// IMPORTANT: build_tests.sh compiles with -DNDEBUG, which strips assert()
// entirely (see the comment in MEMORY.md / CLAUDE.md's Round 17 writeup) --
// every check here is therefore an explicit runtime comparison that prints and
// calls std::exit(1) on failure, not assert().
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "../position.h"
#include "../movegen.h"
#include "../evaluate.h"
#include "../nnue/mknn_evaluator.h"
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace Nebula {
namespace {

int g_fail = 0;
int g_checks = 0;

void expectEq(const std::string& what, long long got, long long want) {
    if (got != want) {
        std::cerr << "FAIL " << what << ": got " << got << " want " << want << "\n";
        ++g_fail;
    }
}

void expectTrue(const std::string& what, bool cond) {
    if (!cond) {
        std::cerr << "FAIL " << what << "\n";
        ++g_fail;
    }
}

// Bit-exact check: the accumulator populated by ev.evaluate(pos) (which runs
// the incremental path when available) must match a from-scratch rebuild for
// both perspectives, plus a second-order check that an entirely independent
// Position built from the same FEN evaluates to the identical Value.
void checkExact(const Eval::Nnue::MknnEvaluator& ev, Position& pos, const std::string& tag) {
    ++g_checks;
    const Value vInc = ev.evaluate(pos);

    std::vector<std::int32_t> refW, refB;
    ev.accumulateFromScratch(pos, WHITE, refW);
    ev.accumulateFromScratch(pos, BLACK, refB);

    const auto& a = pos.state()->mknnAcc;
    for (int i = 0; i < ev.l1(); ++i) {
        expectEq(tag + " acc[W][" + std::to_string(i) + "]", a.acc[WHITE][i], refW[i]);
        expectEq(tag + " acc[B][" + std::to_string(i) + "]", a.acc[BLACK][i], refB[i]);
    }

    StateListPtr fresh(new std::deque<StateInfo>(1));
    Position freshPos;
    freshPos.set(pos.fen(), false, &fresh->back(), nullptr);
    const Value vFresh = ev.evaluate(freshPos);
    expectEq(tag + " value-vs-fresh-rebuild", int(vInc), int(vFresh));
}

void setPos(Position& pos, StateListPtr& states, const std::string& fen) {
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, false, &states->back(), nullptr);
}

// Finds the first legal move matching pred, or MOVE_NONE.
template <typename Pred>
Move findMove(Position& pos, Pred pred) {
    for (const auto& m : MoveList<LEGAL>(pos))
        if (pred(m))
            return m;
    return MOVE_NONE;
}

bool isKingMove(Position& pos, Move m) { return typeOf(pos.movedPiece(m)) == KING; }
bool isQuietNonKing(Position& pos, Move m) { return !pos.capture(m) && !isKingMove(pos, m); }

} // namespace

void runMknnIncrementalTests() {
    // NOTE: this test exercises Position::doNullMove(), which does
    // prefetch(tt.firstEntry(key())) (position.cpp) -- an advisory CPU
    // prefetch hint against the global transposition table. In the real
    // engine tt.resize() always runs first (main.cpp), so this is always a
    // valid pointer during real play. This bare test harness never resizes
    // tt (doing so requires the full Thread pool from main.cpp, which isn't
    // set up here, and calling tt.resize() without it dereferences a null
    // "main" thread and crashes -- confirmed by trying it). The result is a
    // harmless UBSan "member access within null pointer" diagnostic under
    // -fsanitize=undefined specifically for this path (prefetch() itself
    // never dereferences its argument, so nothing actually goes wrong) --
    // known and intentionally left as-is rather than entangling this test
    // with full engine bring-up.
    // Load the currently-embedded net by name (NnueNetDefaultName), not a
    // hardcoded filename -- a hardcoded name silently degrades to "SKIP" (no
    // checks run, no failure reported) the moment the embedded net is swapped
    // to a different file, which happened once already before this fix.
    Eval::Nnue::MknnEvaluator ev;
    std::ifstream f(NnueNetDefaultName, std::ios::binary);
    if (!f || !ev.load(f)) {
        std::cerr << "SKIP: " << NnueNetDefaultName << " not loadable -- "
                     "incremental accumulator test not run\n";
        return;
    }
    if (!ev.incremental()) {
        std::cerr << "FAIL: net loaded (L1=" << ev.l1()
                   << ") but incremental accumulator unexpectedly disabled\n";
        std::exit(1);
    }

    // ---- 1. Root position: forces a full refresh (no ancestor exists). ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        checkExact(ev, pos, "root");

        // ---- 2. Quiet move (dirty_num=1), then undo. ----
        const Move m = findMove(pos, [&](Move mv) { return isQuietNonKing(pos, mv); });
        expectTrue("quiet move exists from start position", m != MOVE_NONE);
        st->emplace_back();
        pos.doMove(m, st->back());
        checkExact(ev, pos, "quiet-move");
        pos.undoMove(m);
        checkExact(ev, pos, "quiet-move-undo");

        // ---- 12. Sibling reuse: a second, different move into the SAME
        // stack-local StateInfo slot that m's doMove just vacated via undo. ----
        const Move m2 = findMove(pos, [&](Move mv) {
            return isQuietNonKing(pos, mv) && mv != m;
        });
        expectTrue("second distinct quiet move exists", m2 != MOVE_NONE);
        pos.doMove(m2, st->back());   // reuses st->back(), not a fresh emplace_back
        checkExact(ev, pos, "sibling-reuse");
        pos.undoMove(m2);
    }

    // ---- 3. Capture (dirty_num=2, captured piece to[1]=SQ_NONE). ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1");
        checkExact(ev, pos, "pre-capture");
        const Move m = findMove(pos, [&](Move mv) { return pos.capture(mv); });
        expectTrue("capture move exists", m != MOVE_NONE);
        st->emplace_back();
        pos.doMove(m, st->back());
        checkExact(ev, pos, "capture");
        pos.undoMove(m);
        checkExact(ev, pos, "capture-undo");
    }

    // ---- 4. Promotion, pawn -> Met (to[0]=SQ_NONE, appended slot). ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/4P3/8/8/8/4K3 w - - 0 1");   // pawn on e5, one step from rank-6 promotion
        const Move m = findMove(pos, [&](Move mv) { return typeOf(mv) == PROMOTION; });
        expectTrue("promotion move exists", m != MOVE_NONE);
        st->emplace_back();
        pos.doMove(m, st->back());
        checkExact(ev, pos, "promotion");
        pos.undoMove(m);
        checkExact(ev, pos, "promotion-undo");
    }

    // ---- 5. Capturing promotion (dirty_num=3, the struct's hard maximum). ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/3n4/4P3/8/8/8/4K3 w - - 0 1");
        const Move m = findMove(pos, [&](Move mv) {
            return typeOf(mv) == PROMOTION && pos.capture(mv);
        });
        expectTrue("capturing promotion move exists", m != MOVE_NONE);
        st->emplace_back();
        pos.doMove(m, st->back());
        checkExact(ev, pos, "capturing-promotion");
        pos.undoMove(m);
        checkExact(ev, pos, "capturing-promotion-undo");
    }

    // ---- 6/7. Own king move (requiresRefresh for us), then opponent king
    // move (must NOT force our refresh; both perspectives still exact). ----
    // 8. Null move immediately after, checked for exact carry-through.
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        checkExact(ev, pos, "bare-kings-root");

        const Move wk = findMove(pos, [&](Move mv) { return isKingMove(pos, mv); });
        expectTrue("white king move exists", wk != MOVE_NONE);
        st->emplace_back();
        pos.doMove(wk, st->back());
        checkExact(ev, pos, "own-king-move");

        const Move bk = findMove(pos, [&](Move mv) { return isKingMove(pos, mv); });
        expectTrue("black king move exists", bk != MOVE_NONE);
        st->emplace_back();
        pos.doMove(bk, st->back());
        checkExact(ev, pos, "opponent-king-move");

        // Null move: parent's accumulator must carry through byte-identical.
        const auto parentAcc = pos.state()->mknnAcc;   // copy before the null move
        st->emplace_back();
        pos.doNullMove(st->back());
        checkExact(ev, pos, "null-move");
        expectTrue("null move acc[W] identical to parent",
            std::memcmp(pos.state()->mknnAcc.acc[WHITE], parentAcc.acc[WHITE],
                        sizeof(std::int32_t) * ev.l1()) == 0);
        expectTrue("null move acc[B] identical to parent",
            std::memcmp(pos.state()->mknnAcc.acc[BLACK], parentAcc.acc[BLACK],
                        sizeof(std::int32_t) * ev.l1()) == 0);
        pos.undoNullMove();
    }

    // ---- 9. Multi-hop gap: several consecutive moves with no evaluate()
    // call in between, reproducing the in-check / TT-hit / eval-gate pattern
    // that punches holes in the "computed" chain during real search. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        checkExact(ev, pos, "multihop-start");   // establish a computed root

        std::vector<Move> played;
        for (int i = 0; i < 4; ++i) {
            const Move m = findMove(pos, [&](Move mv) { return isQuietNonKing(pos, mv); });
            expectTrue("multi-hop quiet move " + std::to_string(i) + " exists", m != MOVE_NONE);
            if (m == MOVE_NONE) break;
            st->emplace_back();
            pos.doMove(m, st->back());   // deliberately no checkExact here
            played.push_back(m);
        }
        checkExact(ev, pos, "multihop-end");   // 4-hop incremental replay

        for (auto it = played.rbegin(); it != played.rend(); ++it)
            pos.undoMove(*it);
    }

    // ---- 10. Multi-hop spanning a king move: the walk must detect
    // requiresRefresh partway through and fall back to a full refresh,
    // not silently apply an invalid king-relative diff. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/8/8/8/8/4K3 w - - 0 1");
        checkExact(ev, pos, "multihop-king-start");

        // Position starts with White to move. Play White's king, then
        // Black's king, then White's king again, all uneval'd, so the span
        // both perspectives replay across includes a king move for each side.
        std::vector<Move> played;
        Move m = findMove(pos, [&](Move mv) { return isKingMove(pos, mv); });
        expectTrue("white king move (hop1) exists", m != MOVE_NONE);
        st->emplace_back(); pos.doMove(m, st->back()); played.push_back(m);

        m = findMove(pos, [&](Move mv) { return isKingMove(pos, mv); });
        expectTrue("black king move (hop2) exists", m != MOVE_NONE);
        st->emplace_back(); pos.doMove(m, st->back()); played.push_back(m);

        m = findMove(pos, [&](Move mv) { return isKingMove(pos, mv); });
        expectTrue("white king move (hop3) exists", m != MOVE_NONE);
        st->emplace_back(); pos.doMove(m, st->back()); played.push_back(m);

        checkExact(ev, pos, "multihop-king-end");

        for (auto it = played.rbegin(); it != played.rend(); ++it)
            pos.undoMove(*it);
    }

    // ---- 11. Budget exhaustion: a small-piece endgame (small refreshCost)
    // plus many uneval'd hops, so the incremental walk's cost budget goes
    // negative and it falls back to a full refresh rather than walking
    // arbitrarily far back. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/3n4/8/8/8/3NK3 w - - 0 1");   // both sides have a knight to shuffle
        checkExact(ev, pos, "budget-start");

        std::vector<Move> played;
        for (int i = 0; i < 10; ++i) {
            const Move m = findMove(pos, [&](Move mv) { return isQuietNonKing(pos, mv); });
            if (m == MOVE_NONE) break;
            st->emplace_back();
            pos.doMove(m, st->back());
            played.push_back(m);
        }
        expectTrue("budget-exhaustion sequence made progress", played.size() >= 6);
        checkExact(ev, pos, "budget-end");

        for (auto it = played.rbegin(); it != played.rend(); ++it)
            pos.undoMove(*it);
    }

    // ---- 13. Deep random walk: broad, unplanned coverage. Fixed seed for
    // reproducibility. Samples ~30% of nodes for exactness checks so gaps of
    // varying, organic length occur between checks. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        checkExact(ev, pos, "random-walk-start");

        std::mt19937 rng(0xC0FFEEu);
        std::uniform_real_distribution<double> coin(0.0, 1.0);
        int plies = 0;
        for (; plies < 500; ++plies) {
            MoveList<LEGAL> moves(pos);
            if (moves.size() == 0)
                break;   // checkmate/stalemate -- stop early, nothing wrong with that
            std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
            const Move m = moves.begin()[pick(rng)];
            st->emplace_back();
            pos.doMove(m, st->back());
            if (coin(rng) < 0.30)
                checkExact(ev, pos, "random-walk-ply-" + std::to_string(plies));
        }
        checkExact(ev, pos, "random-walk-end");
        std::cout << "  random walk: " << plies << " plies played, "
                  << g_checks << " total exactness checks so far\n";
    }

    if (g_fail) {
        std::cerr << g_fail << " NNUE incremental accumulator check(s) FAILED "
                  << "(out of " << g_checks << " checkExact calls)\n";
        std::exit(1);
    }
    std::cout << "PASS: NNUE incremental accumulator (" << g_checks
              << " exactness checks, all bit-exact)\n";
}

} // namespace Nebula
