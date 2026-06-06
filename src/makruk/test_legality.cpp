// Makruk legality, check, checkmate, stalemate, and pin tests.
// Call runMakrukLegalityTests() after Bitboards::init() and Position::init().
//
// FEN piece notation for these tests (src/position.cpp pieceToChar):
//   White: P=pawn  N=knight  S=Khon  R=rook  M=Met  K=king
//   Black: p=pawn  n=knight  s=khon  r=rook  m=met  k=king

#include <cassert>
#include <cstdint>
#include <iostream>
#include "../bitboard.h"
#include "../movegen.h"
#include "../position.h"
#include "../types.h"

namespace Nebula {

namespace {

// ---- helpers ----------------------------------------------------------------

bool hasLegalMove(Position& pos, Square from, Square to) {
    for (const Move m : MoveList<LEGAL>(pos))
        if (fromSq(m) == from && toSq(m) == to)
            return true;
    return false;
}

// ---- individual tests -------------------------------------------------------

void testMetGivesCheck() {
    // Black Met on d5 attacks e4 diagonally (one step NW from e4's perspective,
    // or SE from d5's perspective).  White King on e4 is in check.
    // pieceToChar: 'm'=Black Met, 'K'=White King, 'k'=Black King
    const char* fen = "k7/8/8/3m4/4K3/8/8/8 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    assert(pos.checkers() && "White King on e4 should be in check from Black Met on d5");
    // The checker bitboard should contain d5.
    assert((pos.checkers() & SQ_D5) && "Checker should be Black Met on d5");
    std::cout << "PASS: Black Met on d5 gives check to White King on e4\n";
}

void testKhonGivesCheck() {
    // Black Khon on e5 attacks d4 in the SW direction (Black Khon can move south
    // or diagonally).  White King on d4 is in check.
    // pieceToChar: 's'=Black Khon, 'K'=White King, 'k'=Black King
    const char* fen = "8/8/8/4s3/3K4/8/8/k7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    assert(pos.checkers() && "White King on d4 should be in check from Black Khon on e5");
    assert((pos.checkers() & SQ_E5) && "Checker should be Black Khon on e5");
    std::cout << "PASS: Black Khon on e5 gives check to White King on d4\n";
}

void testPawnGivesCheck() {
    // Black pawn on d5 attacks e4 (diagonal forward for Black = SE).
    // White King on e4 is in check.
    const char* fen = "k7/8/8/3p4/4K3/8/8/8 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    assert(pos.checkers() && "White King on e4 should be in check from Black pawn on d5");
    assert((pos.checkers() & SQ_D5) && "Checker should be Black pawn on d5");
    std::cout << "PASS: Black pawn on d5 gives check to White King on e4\n";
}

void testKingCannotMoveAdjacentToEnemyKing() {
    // White King on a1, Black King on c3.
    // White King wants to move to b2 — but b2 is adjacent to Black King on c3,
    // so that move must be illegal.
    const char* fen = "8/8/8/8/8/2k5/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    // b2 is attacked by Black King on c3; moving White King there is illegal.
    assert(!hasLegalMove(pos, SQ_A1, SQ_B2) && "King must not move adjacent to enemy king (b2)");
    // a2 and b1 are not attacked by Black King; those moves should be legal.
    assert(hasLegalMove(pos, SQ_A1, SQ_A2) && "King should be able to move to a2");
    assert(hasLegalMove(pos, SQ_A1, SQ_B1) && "King should be able to move to b1");
    std::cout << "PASS: White King cannot move to b2 (adjacent to Black King on c3)\n";
}

void testPinnedRookCannotLeaveFile() {
    // White King on e1, White Rook on e4, Black Rook on e8, Black King on h8.
    // White Rook on e4 is pinned along the e-file by Black Rook on e8.
    // All White Rook moves must stay on file e.
    const char* fen = "4r2k/8/8/8/4R3/8/8/4K3 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    assert((pos.blockersForKing(WHITE) & SQ_E4) && "White Rook on e4 should be a pin blocker");

    int rookMoveCount = 0;
    for (const Move m : MoveList<LEGAL>(pos)) {
        if (fromSq(m) == SQ_E4) {
            assert(fileOf(toSq(m)) == FILE_E &&
                "Pinned Rook must only move along the e-file");
            ++rookMoveCount;
        }
    }
    // Rook on e4 can reach e2, e3, e5, e6, e7, e8 = 6 squares along pin file.
    assert(rookMoveCount == 6 && "Pinned Rook should have exactly 6 moves along e-file");
    std::cout << "PASS: Pinned Rook on e4 restricted to e-file (" << rookMoveCount << " moves)\n";
}

void testCheckmate() {
    // Black Rook on a8 checks White King on a1 along file a.
    // Black King on c2 covers b1 and b2.  White has no legal moves → checkmate.
    // pieceToChar: 'r'=Black Rook, 'k'=Black King, 'K'=White King
    const char* fen = "r7/8/8/8/8/8/2k5/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    assert(pos.checkers() && "White King must be in check (checkmate position)");
    assert(MoveList<LEGAL>(pos).size() == 0 && "No legal moves — should be checkmate");
    std::cout << "PASS: Checkmate detected (White has no legal moves and is in check)\n";
}

void testStalemate() {
    // White King on a1, Black King on c2, Black pawn on b3.
    // White King's three escape squares: a2 (attacked by Black pawn), b1 and b2
    // (both attacked by Black King).  White is NOT in check → stalemate.
    // pieceToChar: 'p'=Black pawn, 'k'=Black King, 'K'=White King
    const char* fen = "8/8/8/8/8/1p6/2k5/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    assert(!pos.checkers() && "White King must NOT be in check (stalemate, not mate)");
    assert(MoveList<LEGAL>(pos).size() == 0 && "No legal moves — should be stalemate");
    std::cout << "PASS: Stalemate detected (White has no legal moves and is not in check)\n";
}

void testPromotionGivesCheck() {
    // White pawn on e5 promotes to Met on e6.
    // Met on e6 attacks d7 diagonally (NW one step). Black King on d7 is in check.
    const char* fen = "8/3k4/8/4P3/8/8/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    // Find the promotion move e5→e6.
    Move promoMove = MOVE_NONE;
    for (const Move m : MoveList<LEGAL>(pos))
        if (typeOf(m) == PROMOTION && fromSq(m) == SQ_E5 && toSq(m) == SQ_E6)
            promoMove = m;

    assert(promoMove != MOVE_NONE && "Promotion e5->e6 should be legal");
    assert(pos.givesCheck(promoMove) && "Promotion to Met on e6 should give check to Black King on d7");
    std::cout << "PASS: Promotion e5->e6 (Met) gives check to Black King on d7\n";
}

void testNoPseudoLegalDoublePush() {
    // Verify pseudoLegal rejects a double-step pawn push in Makruk.
    // White pawn on a3, constructing a NORMAL move from a3 to a5 (two squares).
    const char* fen = "k7/8/8/8/8/P7/8/K7 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const Move doubleStep = makeMove(SQ_A3, SQ_A5);
    assert(!pos.pseudoLegal(doubleStep) &&
        "Double-step pawn push must be rejected by pseudoLegal in Makruk");
    std::cout << "PASS: Double-step pawn push correctly rejected by pseudoLegal\n";
}

} // namespace

void runMakrukLegalityTests() {
    std::cout << "=== Makruk legality tests ===\n";
    testMetGivesCheck();
    testKhonGivesCheck();
    testPawnGivesCheck();
    testKingCannotMoveAdjacentToEnemyKing();
    testPinnedRookCannotLeaveFile();
    testCheckmate();
    testStalemate();
    testPromotionGivesCheck();
    testNoPseudoLegalDoublePush();
    std::cout << "=== All legality tests passed ===\n";
}

} // namespace Nebula
