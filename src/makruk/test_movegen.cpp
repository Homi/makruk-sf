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
    // 8 pawn pushes + Ra1->a2 + Rh1->h2 + Nb1->d2 + Ng1->e2
    // + Sc1(Khon): b2,c2,d2 + Sf1(Khon): e2,f2,g2 + Me1(Met): d2,f2 + Kd1: c2,d2,e2
    std::cout << "Perft(1) = " << d1 << "  (expected 23)\n";
    assert(d1 == 23 && "Perft(1) mismatch from Makruk start");

    std::cout << "Perft(2) = " << d2 << "\n";

    std::cout << "PASS: start position perft\n";
}

void testPromotionTypeIsMet() {
    // Verify all generated promotion moves encode MET as the promotion piece.
    const char* fen = "k7/8/8/4P3/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int promotionCount = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION) {
            ++promotionCount;
            assert(promotionType(m) == MET    && "Promotion piece is not MET");
            assert(promotionType(m) != ROOK   && "Chess ROOK promotion generated");
            assert(promotionType(m) != KNIGHT && "Chess KNIGHT promotion generated");
            assert(promotionType(m) != BISHOP && "Chess BISHOP promotion generated");
        }
    }
    assert(promotionCount >= 1 && "Expected at least 1 promotion");
    std::cout << "PASS: all promotions encode MET (" << promotionCount << " promotion(s))\n";
}

void testWhiteQuietPromotion() {
    // White pawn on e5 with empty e6 — exactly one quiet promotion.
    const char* fen = "k7/8/8/4P3/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int promotionCount = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION) {
            ++promotionCount;
            assert(promotionType(m) == MET && "Promotion not to Met");
            assert(toSq(m) == SQ_E6 && "Quiet promotion should land on e6");
        }
    }
    assert(promotionCount == 1 && "Expected exactly 1 quiet promotion from e5");
    std::cout << "PASS: White pawn e5->e6 generates 1 Met quiet promotion\n";
}

void testWhiteCapturePromotion() {
    // White pawn on d5, enemy piece on e6 — one capture-promotion.
    const char* fen = "k7/8/4n3/3P4/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int capturePromo = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION && toSq(m) == SQ_E6) {
            ++capturePromo;
            assert(promotionType(m) == MET && "Capture-promotion piece is not MET");
        }
    }
    assert(capturePromo == 1 && "Expected 1 capture-promotion d5xe6");
    std::cout << "PASS: White pawn d5xe6 generates 1 Met capture-promotion\n";
}

void testBlackQuietPromotion() {
    // Black pawn on e4 with empty e3 — exactly one quiet promotion for Black.
    const char* fen = "k7/8/8/8/4p3/8/8/K7 b - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int promotionCount = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION) {
            ++promotionCount;
            assert(promotionType(m) == MET && "Black promotion not to Met");
            assert(toSq(m) == SQ_E3 && "Black quiet promotion should land on e3");
        }
    }
    assert(promotionCount == 1 && "Expected exactly 1 quiet promotion from Black e4");
    std::cout << "PASS: Black pawn e4->e3 generates 1 Met quiet promotion\n";
}

void testBlackCapturePromotion() {
    // Black pawn on d4, white piece on e3 — one capture-promotion for Black.
    const char* fen = "k7/8/8/8/3p4/4N3/8/K7 b - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int capturePromo = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION && toSq(m) == SQ_E3) {
            ++capturePromo;
            assert(promotionType(m) == MET && "Black capture-promotion piece is not MET");
        }
    }
    assert(capturePromo == 1 && "Expected 1 Black capture-promotion d4xe3");
    std::cout << "PASS: Black pawn d4xe3 generates 1 Met capture-promotion\n";
}

void testMakeUnmakeHashRoundTrip() {
    // Execute a promotion then undo it; verify hash and board state are restored.
    const char* fen = "k7/8/8/4P3/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->front(), nullptr);

    const uint64_t hashBefore = pos.key();

    Move promoMove = MOVE_NONE;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION) { promoMove = m; break; }
    }
    assert(promoMove != MOVE_NONE && "No promotion move found");

    StateInfo st;
    pos.doMove(promoMove, st);
    assert(pos.count<PAWN>(WHITE) == 0  && "Pawn should be gone after promotion");
    assert(pos.count<QUEEN>(WHITE) == 1 && "Met should appear after promotion");

    pos.undoMove(promoMove);
    assert(pos.key() == hashBefore       && "Hash not restored after undoMove");
    assert(pos.count<PAWN>(WHITE) == 1   && "Pawn count not restored after undoMove");
    assert(pos.count<QUEEN>(WHITE) == 0  && "Met should be gone after undoMove");

    std::cout << "PASS: make/unmake hash round-trip for promotion\n";
}

void testPromotedMetMoves() {
    // A Met on e6 should have exactly 4 diagonal moves (d5, f5, d7, f7).
    const char* fen = "k7/8/4M3/8/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    int metMoves = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (fromSq(m) == SQ_E6) ++metMoves;
    }
    assert(metMoves == 4 && "Met on e6 should have exactly 4 diagonal moves");
    std::cout << "PASS: Met on e6 has exactly 4 moves (diagonal steps)\n";
}

void testNoChessPromotion() {
    // Confirm no ROOK, KNIGHT, or BISHOP underpromotions are generated.
    const char* fen = "k7/8/8/4P3/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    for (const Move m : MoveList<LEGAL>(pos)) {
        if (typeOf(m) == PROMOTION) {
            assert(promotionType(m) != ROOK   && "Chess ROOK promotion generated");
            assert(promotionType(m) != KNIGHT && "Chess KNIGHT promotion generated");
            assert(promotionType(m) != BISHOP && "Chess BISHOP promotion generated");
        }
    }
    std::cout << "PASS: no chess underpromotion (rook/knight/bishop) generated\n";
}

} // namespace

void runMakrukMovegenTests() {
    std::cout << "=== Makruk move generation tests ===\n";
    testNoSpecialMoves();
    testStartPositionPerft();
    testPromotionTypeIsMet();
    testWhiteQuietPromotion();
    testWhiteCapturePromotion();
    testBlackQuietPromotion();
    testBlackCapturePromotion();
    testMakeUnmakeHashRoundTrip();
    testPromotedMetMoves();
    testNoChessPromotion();
    std::cout << "=== All tests passed ===\n";
}

} // namespace Nebula
