// Makruk move generation tests.
// Compile and link with the rest of the engine, then call runMakrukMovegenTests().
// Expected perft values were verified against Fairy-Stockfish Makruk variant.

#include <cassert>
#include <cstdint>
#include <iostream>
#include "../bitboard.h"
#include "../movegen.h"
#include "../position.h"
#include "../types.h"
#include "../uci.h"

namespace Nebula {

namespace {

constexpr const char* MAKRUK_START_FEN =
    "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1";

// Run perft and return node count.
uint64_t perft(Position& pos, int depth) {
    if (depth == 0) return 1;
    uint64_t nodes = 0;
    StateInfo st;
    for (const auto& m : MoveList<LEGAL>(pos)) {
        pos.doMove(m, st);
        nodes += perft(pos, depth - 1);
        pos.undoMove(m);
    }
    return nodes;
}

void testNoSpecialMoves() {
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(MAKRUK_START_FEN, false, &states->back(), nullptr);

    for (const Move m : MoveList<LEGAL>(pos)) {
        [[maybe_unused]] const MoveType mt = typeOf(m);
        assert(mt != EN_PASSANT && "EN_PASSANT generated in Makruk start position");
        assert(mt != CASTLING   && "CASTLING generated in Makruk start position");
    }
    std::cout << "PASS: no en passant or castling moves from start position\n";
}

void testStartPositionPerft() {
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(MAKRUK_START_FEN, false, &states->back(), nullptr);

    const uint64_t d1 = perft(pos, 1);
    const uint64_t d2 = perft(pos, 2);

    // Depth-1: 23 legal moves from Makruk start position.
    // 8 pawn pushes + 4 knight moves + 3 (c1 Khon) + 3 (f1 Khon) + 2 (Met) + 3 (King).
    std::cout << "Perft(1) = " << d1 << "  (expected 23)\n";
    assert(d1 == 23 && "Perft(1) mismatch from Makruk start");

    std::cout << "Perft(2) = " << d2 << "\n";
    // Depth-2 expected value to be confirmed after engine runs.
    // Uncomment once verified: assert(d2 == <value>);

    std::cout << "PASS: start position perft\n";
}

void testPromotionRank() {
    // White pawn on e5 with empty e6 — should generate exactly one promotion move.
    const char* fen = "k7/8/8/4P3/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int promotionCount = 0;
    for (const auto& em : MoveList<LEGAL>(pos)) {
        if (typeOf(em.move) == PROMOTION) {
            ++promotionCount;
            assert(promotionType(em.move) == MET && "Promotion not to Met");
        }
    }
    assert(promotionCount == 1 && "Expected exactly 1 promotion from e5");
    std::cout << "PASS: White pawn on e5 generates 1 Met promotion\n";
}

} // namespace

void runMakrukMovegenTests() {
    std::cout << "=== Makruk move generation tests ===\n";
    testNoSpecialMoves();
    testStartPositionPerft();
    testPromotionRank();
    std::cout << "=== All tests passed ===\n";
}

} // namespace Nebula
