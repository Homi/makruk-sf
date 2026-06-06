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

// ---- Fairy-compatible behaviour tests -------------------------------------

void testShouldActivateFalseForStartPos() {
    const char* fen = "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    Color claimant = WHITE;
    assert(!shouldActivateMakrukCounting(sig, claimant, MakrukCountingMode::Fairy) &&
           "shouldActivate must be false at start position");
    assert(!shouldActivateMakrukCounting(sig, claimant, MakrukCountingMode::Off) &&
           "shouldActivate must be false in Off mode");
    std::cout << "PASS: shouldActivate=false at start position\n";
}

void testShouldActivateTrueForBareKing() {
    // White bare King vs Black Rook+King — Black should be detected as the claimant.
    // Wait: WHITE is bare, so WHITE is the claimant.
    const char* fen = "k7/8/8/8/8/8/8/r1K5 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    const auto sig = computeMakrukMaterialSignature(pos);
    Color claimant = BLACK;  // will be overwritten
    assert(shouldActivateMakrukCounting(sig, claimant, MakrukCountingMode::Fairy) &&
           "shouldActivate must be true when White is bare");
    assert(claimant == WHITE && "White is the bare side — should be set as claimant");
    std::cout << "PASS: shouldActivate=true, claimant=WHITE for bare-king position\n";
}

void testActivateCountingInitialState() {
    // White Rook b1 + King a1 vs Black bare King a8.
    // After activation for Black claimant, check all fields.
    const char* fen = "k7/8/8/8/8/8/8/KR6 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, pos, BLACK, MakrukCountingMode::Fairy);

    assert(cs.active                              && "counting must be active after activate");
    assert(cs.claimant == BLACK                   && "claimant must be BLACK");
    assert(cs.mode == MakrukCountingMode::Fairy   && "mode must be Fairy");
    assert(cs.count == 0                          && "count must start at 0");
    assert(cs.limit == CountingLimits::Rook       && "limit must be Rook (32 plies)");
    assert(cs.matSig.rooks[WHITE] == 1            && "matSig should record White Rook");
    assert(!isMakrukCountingDraw(cs)              && "not a draw at count=0");
    std::cout << "PASS: activateMakrukCounting initialises state correctly\n";
}

void testUpdateIncrementsCount() {
    // Call update 5 times with the same position; count should reach 5.
    const char* fen = "k7/8/8/8/8/8/8/KR6 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, pos, BLACK, MakrukCountingMode::Fairy);

    for (int i = 1; i <= 5; ++i) {
        updateMakrukCountingState(cs, pos);
        assert(cs.count == i && "count must increment by 1 each ply");
        assert(cs.active     && "counting must remain active");
    }
    std::cout << "PASS: updateMakrukCountingState increments count correctly\n";
}

void testNoDrawBeforeLimit() {
    const char* fen = "k7/8/8/8/8/8/8/KR6 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, pos, BLACK, MakrukCountingMode::Fairy);

    // Drive count to limit-1.
    for (int i = 0; i < cs.limit - 1; ++i)
        updateMakrukCountingState(cs, pos);
    assert(!isMakrukCountingDraw(cs) && "must not be draw one ply before limit");
    std::cout << "PASS: no draw one ply before limit\n";
}

void testDrawExactlyAtLimit() {
    const char* fen = "k7/8/8/8/8/8/8/KR6 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, pos, BLACK, MakrukCountingMode::Fairy);

    const int lim = cs.limit;
    for (int i = 0; i < lim; ++i)
        updateMakrukCountingState(cs, pos);

    assert(cs.count == lim         && "count must equal limit");
    assert(isMakrukCountingDraw(cs) && "draw must be detected at limit");
    std::cout << "PASS: draw detected exactly at limit=" << lim << " plies\n";
}

void testUpdateDeactivatesWhenNotEligible() {
    // Activate counting for BLACK (bare) then pass a position where BLACK has
    // a Rook (no longer bare) — update must deactivate counting.
    const char* fenBare    = "k7/8/8/8/8/8/8/KR6 w - - 0 1"; // BLACK bare
    const char* fenNotBare = "kr6/8/8/8/8/8/8/K7 w - - 0 1";  // BLACK has Rook

    StateListPtr statesBare(new std::deque<StateInfo>(1));
    Position posBare;
    posBare.set(fenBare, false, &statesBare->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, posBare, BLACK, MakrukCountingMode::Fairy);
    assert(cs.active);

    StateListPtr statesNotBare(new std::deque<StateInfo>(1));
    Position posNotBare;
    posNotBare.set(fenNotBare, false, &statesNotBare->back(), nullptr);

    updateMakrukCountingState(cs, posNotBare);
    assert(!cs.active && "counting must deactivate when claimant is no longer bare");
    std::cout << "PASS: counting deactivates when claimant gains a piece\n";
}

void testLimitRecalculatesOnCapture() {
    // Activate counting with WHITE having 2 Knights (limit = TwoKnight = 64).
    // Then pass a position where WHITE has only 1 Knight.
    // Limit must recalculate to OneKnight (128) and count must still increment.
    const char* fenTwoKnights = "k7/8/8/8/8/8/8/KNN5 w - - 0 1";
    const char* fenOneKnight  = "k7/8/8/8/8/8/8/KN6 w - - 0 1";

    StateListPtr s1(new std::deque<StateInfo>(1));
    Position pos1;
    pos1.set(fenTwoKnights, false, &s1->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, pos1, BLACK, MakrukCountingMode::Fairy);
    assert(cs.limit == CountingLimits::TwoKnight && "initial limit should be TwoKnight");

    StateListPtr s2(new std::deque<StateInfo>(1));
    Position pos2;
    pos2.set(fenOneKnight, false, &s2->back(), nullptr);

    updateMakrukCountingState(cs, pos2);
    assert(cs.limit == CountingLimits::OneKnight &&
           "limit must recalculate to OneKnight after stronger side loses a Knight");
    assert(cs.count == 1 && "count must still increment after recalculation");
    assert(cs.active      && "counting must remain active");
    std::cout << "PASS: limit recalculates (TwoKnight→OneKnight) when stronger side loses Knight\n";
}

void testActiveReportShowsCountAndLimit() {
    // An active counting report must include count and limit values.
    const char* fen = "k7/8/8/8/8/8/8/KR6 w - - 0 1";
    StateListPtr states(new std::deque<StateInfo>(1));
    Position pos;
    pos.set(fen, false, &states->back(), nullptr);

    MakrukCountingState cs;
    activateMakrukCounting(cs, pos, BLACK, MakrukCountingMode::Fairy);
    updateMakrukCountingState(cs, pos);
    updateMakrukCountingState(cs, pos);  // count = 2

    const std::string report = makrukCountingReport(pos, cs, MakrukCountingMode::Fairy);
    assert(report.find("active       : yes") != std::string::npos && "report: active=yes");
    assert(report.find("claimant")           != std::string::npos && "report: claimant");
    assert(report.find("count")              != std::string::npos && "report: count");
    assert(report.find("limit")              != std::string::npos && "report: limit");
    assert(report.find("draw reached")       != std::string::npos && "report: draw reached");
    std::cout << "PASS: active counting report contains all expected fields\n";
}

}  // namespace

void runMakrukCountingTests() {
    std::cout << "=== Makruk counting tests ===\n";
    // --- original tests ---
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
    // --- Fairy-compatible behaviour tests ---
    testShouldActivateFalseForStartPos();
    testShouldActivateTrueForBareKing();
    testActivateCountingInitialState();
    testUpdateIncrementsCount();
    testNoDrawBeforeLimit();
    testDrawExactlyAtLimit();
    testUpdateDeactivatesWhenNotEligible();
    testLimitRecalculatesOnCapture();
    testActiveReportShowsCountAndLimit();
    std::cout << "=== All counting tests passed ===\n";
}

}  // namespace Nebula
