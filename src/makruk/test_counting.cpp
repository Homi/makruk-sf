// Makruk counting subsystem tests.
// Call runMakrukCountingTests() after Bitboards::init() and Position::init().
//
// FEN piece notation (src/position.cpp pieceToChar):
//   White: P=pawn  N=knight  S=Khon  R=rook  M=Met  K=king
//   Black: p=pawn  n=knight  s=khon  r=rook  m=met  k=king

#include <cassert>
#include <iostream>
#include <string>
#include "makruk_counting.h"
#include "../bitboard.h"
#include "../movegen.h"
#include "../position.h"

namespace Nebula {

namespace {

void testStartPosNotEligible() {
    // Starting position has pawns and all pieces — counting never eligible.
    const char* fen = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(!sig.bareKing(WHITE) && "White should not be bare in start position");
    assert(!sig.bareKing(BLACK) && "Black should not be bare in start position");
    assert(sig.anyPawn()        && "Pawns should be present in start position");
    assert(!isMakrukCountingEligible(sig, WHITE, MakrukCountingMode::Fairy));
    assert(!isMakrukCountingEligible(sig, BLACK, MakrukCountingMode::Fairy));
    std::cout << "PASS: start position is not counting-eligible\n";
}

void testBareKingDetection() {
    // White: King c1 only.  Black: King a8, Rook a1.
    // pieceToChar: K=White King, k=Black King, r=Black Rook
    const char* fen = "k7/8/8/8/8/8/8/r1K5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(sig.bareKing(WHITE)  && "White should be bare king");
    assert(!sig.bareKing(BLACK) && "Black has a rook — not bare");
    assert(sig.rooks[BLACK] == 1);
    assert(sig.rooks[WHITE] == 0);
    std::cout << "PASS: bare king detection (White bare, Black has rook)\n";
}

void testRookVsBareKingEligible() {
    // Black Rook+King vs White bare King.
    // White (bare) is eligible to invoke counting in Fairy mode.
    const char* fen = "k7/8/8/8/8/8/8/r1K5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(isMakrukCountingEligible(sig, WHITE, MakrukCountingMode::Fairy) &&
           "White bare king should be eligible (Fairy)");
    assert(!isMakrukCountingEligible(sig, BLACK, MakrukCountingMode::Fairy) &&
           "Black has pieces — not eligible to invoke counting");

    const int limit = getMakrukCountingLimit(sig, WHITE, MakrukCountingMode::Fairy);
    assert(limit == CountingLimits::Rook &&
           "Rook vs bare king limit should be CountingLimits::Rook");
    std::cout << "PASS: Rook vs bare king — White eligible, limit=" << limit << " plies\n";
}

void testPawnsBlockFairyEligibility() {
    // White bare King, but Black has a pawn remaining — Fairy: NOT eligible.
    // FEN: Black King a8, Black pawn a7, Black Rook b1, White King c1.
    const char* fen = "k7/p7/8/8/8/8/8/1rK5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(sig.bareKing(WHITE) && "White should be bare");
    assert(sig.anyPawn()       && "A pawn should be present");
    assert(!isMakrukCountingEligible(sig, WHITE, MakrukCountingMode::Fairy) &&
           "Fairy counting blocked when pawns remain");
    std::cout << "PASS: bare king with pawns — Fairy counting not eligible\n";
}

void testMaterialSignatureContents() {
    // White: Rook b1, Knight c1, Khon d1, King h1.
    // Black: Met e8, King a8.
    // FEN rank 8: k3m3  rank 1: 1RNS3K
    const char* fen = "k3m3/8/8/8/8/8/8/1RNS3K w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(sig.rooks[WHITE]   == 1 && "White Rook");
    assert(sig.knights[WHITE] == 1 && "White Knight");
    assert(sig.khons[WHITE]   == 1 && "White Khon");
    assert(sig.mets[WHITE]    == 0 && "No White Met");
    assert(sig.pawns[WHITE]   == 0 && "No White Pawn");
    assert(sig.mets[BLACK]    == 1 && "Black Met");
    assert(sig.rooks[BLACK]   == 0 && "No Black Rook");
    assert(!sig.bareKing(BLACK)    && "Black has Met — not bare");
    std::cout << "PASS: material signature contents (R+N+S vs m+K)\n";
}

void testTwoKnightLimit() {
    // White: King a1, Knight b1, Knight c1.  Black: bare King a8.
    const char* fen = "k7/8/8/8/8/8/8/KNN5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(sig.bareKing(BLACK));
    assert(!sig.anyPawn());
    assert(isMakrukCountingEligible(sig, BLACK, MakrukCountingMode::Fairy));

    const int limit = getMakrukCountingLimit(sig, BLACK, MakrukCountingMode::Fairy);
    assert(limit == CountingLimits::TwoKnight && "2-Knight vs bare king limit");
    std::cout << "PASS: 2-Knight vs bare king limit=" << limit << " plies\n";
}

void testOneKhonLimit() {
    // White: King a1, Khon b1.  Black: bare King a8.
    const char* fen = "k7/8/8/8/8/8/8/KS6 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(sig.bareKing(BLACK));
    const int limit = getMakrukCountingLimit(sig, BLACK, MakrukCountingMode::Fairy);
    assert(limit == CountingLimits::OneBish && "1-Khon vs bare king limit");
    std::cout << "PASS: 1-Khon vs bare king limit=" << limit << " plies\n";
}

void testOffModeNeverEligible() {
    // Off mode: counting is never eligible regardless of position.
    const char* fen = "k7/8/8/8/8/8/8/r1K5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    assert(!isMakrukCountingEligible(sig, WHITE, MakrukCountingMode::Off));
    assert(!isMakrukCountingEligible(sig, BLACK, MakrukCountingMode::Off));
    assert(getMakrukCountingLimit(sig, WHITE, MakrukCountingMode::Off) == 0);
    std::cout << "PASS: Off mode — never eligible, limit always 0\n";
}

void testCountingDrawDetection() {
    // Manually drive a MakrukCountingState to its limit.
    MakrukCountingState cs;
    cs.active  = true;
    cs.claimant = WHITE;
    cs.limit   = 4;
    cs.count   = 0;

    assert(!isMakrukCountingDraw(cs) && "count=0 should not be draw");
    cs.count = 3;
    assert(!isMakrukCountingDraw(cs) && "count=3 < limit=4 not draw");
    cs.count = 4;
    assert(isMakrukCountingDraw(cs)  && "count=4 >= limit=4 should be draw");
    std::cout << "PASS: counting draw detection at limit boundary\n";
}

void testCountingReportFields() {
    // Report from a bare-king position should mention all required fields.
    const char* fen = "k7/8/8/8/8/8/8/r1K5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    MakrukCountingState cs;
    const std::string report =
        makrukCountingReport(pos, cs, MakrukCountingMode::Fairy);

    assert(report.find("mode")        != std::string::npos && "report: mode");
    assert(report.find("fairy")       != std::string::npos && "report: mode value");
    assert(report.find("active")      != std::string::npos && "report: active");
    assert(report.find("eligible[w]") != std::string::npos && "report: eligibility");
    assert(report.find("material")    != std::string::npos && "report: material");
    assert(report.find("bare king")   != std::string::npos && "report: bare king label");
    std::cout << "PASS: counting report contains all required fields\n";
    std::cout << report;
}

}  // namespace

void runMakrukCountingTests() {
    std::cout << "=== Makruk counting tests ===\n";
    testStartPosNotEligible();
    testBareKingDetection();
    testRookVsBareKingEligible();
    testPawnsBlockFairyEligibility();
    testMaterialSignatureContents();
    testTwoKnightLimit();
    testOneKhonLimit();
    testOffModeNeverEligible();
    testCountingDrawDetection();
    testCountingReportFields();
    std::cout << "=== All counting tests passed ===\n";
}

}  // namespace Nebula
