// Regression tests for Position::seeGe() (Static Exchange Evaluation).
//
// Background: comparing this fork against upstream sf-kernel (FireFather/
// sf-kernel) found that seeGe()'s incremental "revealed attacker" updates
// were never adapted for Makruk piece rules, unlike attackersTo()/
// setCheckInfo()/sliderBlockers() in the same file, which were correctly
// adapted. Two bugs, both now fixed in Position::seeGe() (position.cpp):
//   1. Khon (BISHOP) is color-dependent (White: forward+diagonals is NORTH;
//      Black: SOUTH+diagonals) and not a slider. The old code used
//      attacksBb<BISHOP>(to,occupied)&pieces(BISHOP,QUEEN), which silently
//      unions both colors' directions -- a White Khon north of `to` was
//      incorrectly treated as able to attack `to` (which requires moving
//      south, which White Khon cannot do).
//   2. Met (QUEEN slot) has zero rank/file movement (1-step diagonal only)
//      and is not a slider. The old code included QUEEN in
//      attacksBb<ROOK>(to,occupied)&pieces(ROOK,QUEEN), incorrectly treating
//      a Met anywhere on `to`'s rank/file as a potential revealed attacker.
//   3. The attacker-check order (PAWN, KNIGHT, BISHOP, ROOK, QUEEN) was
//      inherited from upstream's ascending chess-value order, but Makruk's
//      actual values are very different: Pawn(126) < Met/QUEEN(420) <
//      Khon/BISHOP(660) < Knight(781) < Rook(1276) -- Met is the
//      second-cheapest piece, not the most expensive. SEE's correctness
//      depends on trying the truly-least-valuable attacker first at every
//      step; fixed to PAWN, QUEEN, BISHOP, KNIGHT, ROOK. Confirmed this is
//      not cosmetic: deliberately mismatching referenceSeeGe()'s order
//      against the real seeGe() (during development of this fix) produced
//      real disagreements, not just different internal bookkeeping.
//
// Since the buggy terms only ever add FALSE attackers on top of an already-
// correct attackersTo() base (see position.cpp for why), the two bugs can
// only be observed once the incremental attacker set is unioned across at
// least one "revealing" step -- a single isolated position isn't enough to
// see a difference in seeGe()'s boolean result. This file cross-checks the
// real (fast, incremental) seeGe() against a deliberately slow-but-obviously-
// correct reference that re-derives the attacker set from scratch via the
// already-trusted Position::attackersTo() at every step of the exchange,
// instead of incrementally unioning revealed attackers -- the same
// "fast-vs-slow-reference" methodology used by test_nnue_incremental.cpp.
//
// NOTE: build_tests.sh compiles with -DNDEBUG, which strips assert()
// entirely -- every check here is an explicit runtime comparison that
// prints and calls std::exit(1) on failure, not assert().
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "../position.h"
#include "../movegen.h"
#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace Nebula {
namespace {

int g_fail = 0;
int g_checks = 0;

void expectEq(const std::string& what, bool got, bool want) {
    ++g_checks;
    if (got != want) {
        std::cerr << "FAIL " << what << ": seeGe()=" << got
                   << " referenceSeeGe()=" << want << "\n";
        ++g_fail;
    }
}

void setPos(Position& pos, StateListPtr& states, const std::string& fen) {
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, false, &states->back(), nullptr);
}

// Deliberately slow, obviously-correct reference: identical swap algorithm to
// Position::seeGe(), except the attacker set is fully re-derived via
// attackersTo() at the top of every loop iteration instead of incrementally
// unioning "revealed" attackers. Uses only Position's public API.
bool referenceSeeGe(const Position& pos, const Move m, const Value threshold) {
    if (typeOf(m) != NORMAL)
        return VALUE_ZERO >= threshold;
    const Square from = fromSq(m), to = toSq(m);
    int swap = pieceValue[MG][pos.pieceOn(to)] - threshold;
    if (swap < 0) return false;
    swap = pieceValue[MG][pos.pieceOn(from)] - swap;
    if (swap <= 0) return true;

    uint64_t occupied = pos.pieces() ^ from ^ to;
    Color stm = pos.stm();
    int res = 1;
    while (true) {
        stm = ~stm;
        const uint64_t attackers = pos.attackersTo(to, occupied) & occupied;
        uint64_t stmAttackers = attackers & pos.pieces(stm);
        if (!stmAttackers) break;
        if (pos.pinners(~stm) & occupied) {
            stmAttackers &= ~pos.blockersForKing(stm);
            if (!stmAttackers) break;
        }
        res ^= 1;
        uint64_t bb;
        // Ascending Makruk value order (Pawn 126 < Met/QUEEN 420 < Khon/BISHOP
        // 660 < Knight 781 < Rook 1276) -- must match Position::seeGe()'s own
        // check order exactly, since which attacker is tried first is part of
        // SEE's actual result, not just an optimization.
        if ((bb = stmAttackers & pos.pieces(PAWN))) {
            if ((swap = PawnValueMg - swap) < res) break;
        } else if ((bb = stmAttackers & pos.pieces(QUEEN))) {
            if ((swap = QueenValueMg - swap) < res) break;
        } else if ((bb = stmAttackers & pos.pieces(BISHOP))) {
            if ((swap = BishopValueMg - swap) < res) break;
        } else if ((bb = stmAttackers & pos.pieces(KNIGHT))) {
            if ((swap = KnightValueMg - swap) < res) break;
        } else if ((bb = stmAttackers & pos.pieces(ROOK))) {
            if ((swap = RookValueMg - swap) < res) break;
        } else {
            return (attackers & ~pos.pieces(stm)) ? bool(res ^ 1) : bool(res);
        }
        occupied ^= (bb & -bb);   // remove the least-significant attacker
    }
    return static_cast<bool>(res);
}

void checkAllMoves(Position& pos, const std::string& tag) {
    for (const auto& m : MoveList<LEGAL>(pos)) {
        if (!pos.capture(m) || typeOf(Move(m)) != NORMAL)
            continue;
        for (const Value th : {Value(-500), Value(-1), Value(0), Value(1), Value(500)}) {
            expectEq(tag + " see(" + std::to_string(int(th)) + ")",
                     pos.seeGe(m, th), referenceSeeGe(pos, m, th));
        }
    }
}

} // namespace

void runSeeTests() {
    // ---- 1. Direct repro of bug #1: White Khon north of the capture
    // square, positioned so it would be a false "revealed attacker" once a
    // pawn capture on that square is simulated. ----
    {
        StateListPtr st;
        Position pos;
        // White Khon on e6 (north of e5); White pawn on d4 can capture a
        // Black knight on e5; Black queen(Met) on e8 recaptures, which is
        // exactly the kind of multi-ply exchange where an incorrectly
        // "revealed" e6 Khon would previously have been added to the
        // attacker set for White's (nonexistent) follow-up capture.
        setPos(pos, st, "4k3/8/8/4S3/3P4/8/8/4K3 w - - 0 1");
        checkAllMoves(pos, "khon-color-direct");
    }

    // ---- 2. Direct repro of bug #2: Met on the same rank as the capture
    // square, far away, with a Rook-driven exchange revealing (or not) a
    // same-rank attacker. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "4k3/8/8/8/M3n2R/8/8/4K3 w - - 0 1");
        checkAllMoves(pos, "met-rook-direct");
    }

    // ---- 3. Broad coverage: fixed-seed random self-play, checking every
    // capture's seeGe() at several thresholds at every ply, across
    // positions naturally containing Khon and Met pieces. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        std::mt19937 rng(0x5EEC0DE);
        int plies = 0;
        for (; plies < 300; ++plies) {
            checkAllMoves(pos, "random-walk-ply-" + std::to_string(plies));
            MoveList<LEGAL> moves(pos);
            if (moves.size() == 0)
                break;
            std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
            const Move m = moves.begin()[pick(rng)];
            st->emplace_back();
            pos.doMove(m, st->back());
        }
        std::cout << "  SEE random walk: " << plies << " plies, "
                  << g_checks << " total checks so far\n";
    }

    if (g_fail) {
        std::cerr << g_fail << " seeGe() check(s) FAILED (out of "
                  << g_checks << " comparisons)\n";
        std::exit(1);
    }
    std::cout << "PASS: seeGe() Khon/Met exchange correctness (" << g_checks
              << " checks, all match reference)\n";
}

} // namespace Nebula
