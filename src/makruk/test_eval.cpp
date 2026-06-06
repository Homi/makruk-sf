// Makruk classical evaluation tests.
// Tests cover material values, PST bonuses, rook heuristics, and symmetry.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "makruk_eval.h"
#include "../position.h"
#include <cassert>
#include <iostream>

namespace Nebula {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void setPos(Position& pos, StateListPtr& states, const std::string& fen) {
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, false, &states->back(), nullptr);
}

static void pass(const std::string& name) {
    std::cout << "[PASS] " << name << "\n";
}

// ---------------------------------------------------------------------------
// Test 1: Start position evaluates to exactly 0 (symmetric).
// ---------------------------------------------------------------------------
static void testStartPositionNearZero() {
    const std::string name = "testStartPositionNearZero";
    Position pos; StateListPtr st;
    setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
    const Value v = makrukClassicalEval(pos);
    assert(v == 0 && "Start position must evaluate to 0");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 2: Side with extra rook scores higher.
// ---------------------------------------------------------------------------
static void testExtraRookScoresHigher() {
    const std::string name = "testExtraRookScoresHigher";
    Position posA; StateListPtr stA;
    setPos(posA, stA, "4k3/8/8/8/8/8/8/4K2R w - - 0 1");   // White K+R vs Black K

    Position posB; StateListPtr stB;
    setPos(posB, stB, "4k3/8/8/8/8/8/8/3RK2R w - - 0 1");  // White K+2R vs Black K

    const Value vOne = makrukClassicalEval(posA);
    const Value vTwo = makrukClassicalEval(posB);
    assert(vTwo > vOne && "Extra rook must increase eval");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 3: Khon material value is 660 MG / 640 EG.
// ---------------------------------------------------------------------------
static void testKhonValueIsCorrect() {
    const std::string name = "testKhonValueIsCorrect";
    assert(BishopValueMg == 660 && "Khon MG value must be 660");
    assert(BishopValueEg == 640 && "Khon EG value must be 640");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 4: Met material value is 420 MG / 450 EG.
// ---------------------------------------------------------------------------
static void testMetValueIsCorrect() {
    const std::string name = "testMetValueIsCorrect";
    assert(QueenValueMg == 420 && "Met MG value must be 420");
    assert(QueenValueEg == 450 && "Met EG value must be 450");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 5: Central Khon scores higher than corner Khon (PST).
// ---------------------------------------------------------------------------
static void testCentralKhonScoresHigher() {
    const std::string name = "testCentralKhonScoresHigher";
    Position posCenter; StateListPtr stCenter;
    setPos(posCenter, stCenter, "4k3/8/8/8/3S4/8/8/4K3 w - - 0 1"); // Khon on d4

    Position posCorner; StateListPtr stCorner;
    setPos(posCorner, stCorner, "4k3/8/8/8/8/8/8/S3K3 w - - 0 1");  // Khon on a1

    const Value vCenter = makrukClassicalEval(posCenter);
    const Value vCorner = makrukClassicalEval(posCorner);
    assert(vCenter > vCorner && "Central Khon must score higher than corner Khon");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 6: makrukEvalReport includes rook activity section.
// ---------------------------------------------------------------------------
static void testRookOpenFileBonusInReport() {
    const std::string name = "testRookOpenFileBonusInReport";
    Position pos; StateListPtr st;
    setPos(pos, st, "4k3/8/8/8/8/8/8/R3K3 w - - 0 1"); // Rook on a1, open a-file
    const std::string report = makrukEvalReport(pos);
    assert(report.find("rook activity") != std::string::npos);
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 7: Perfectly symmetric position evaluates to 0.
// ---------------------------------------------------------------------------
static void testSymmetricPositionIsZero() {
    const std::string name = "testSymmetricPositionIsZero";
    Position pos; StateListPtr st;
    setPos(pos, st, "r3k3/8/8/8/8/8/8/R3K3 w - - 0 1"); // K+R vs K+R mirrored
    const Value v = makrukClassicalEval(pos);
    assert(v == 0 && "Symmetric position must evaluate to 0");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 8: Eval negates when side to move changes.
// ---------------------------------------------------------------------------
static void testEvalFlipsForBlack() {
    const std::string name = "testEvalFlipsForBlack";
    Position posW; StateListPtr stW;
    setPos(posW, stW, "4k3/8/8/8/8/8/8/3MK3 w - - 0 1"); // White K+Met, White to move

    Position posB; StateListPtr stB;
    setPos(posB, stB, "4k3/8/8/8/8/8/8/3MK3 b - - 0 1"); // same, Black to move

    const Value vW = makrukClassicalEval(posW);
    const Value vB = makrukClassicalEval(posB);
    assert(vW > 0   && "White to move: positive");
    assert(vB < 0   && "Black to move: negative");
    assert(vW == -vB && "Eval must negate when stm flips");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 9: Advanced pawn scores higher than back-rank pawn (PST advancement).
// ---------------------------------------------------------------------------
static void testAdvancedPawnScoresHigher() {
    const std::string name = "testAdvancedPawnScoresHigher";
    Position posStart; StateListPtr stStart;
    setPos(posStart, stStart, "4k3/8/8/8/8/P7/8/4K3 w - - 0 1"); // pawn on a3 (rank 2)

    Position posAdv; StateListPtr stAdv;
    setPos(posAdv, stAdv, "4k3/8/8/P7/8/8/8/4K3 w - - 0 1"); // pawn on a5 (rank 4)

    const Value vStart = makrukClassicalEval(posStart);
    const Value vAdv   = makrukClassicalEval(posAdv);
    assert(vAdv > vStart && "Advanced pawn must score higher via PST");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 10: makrukEvalReport includes all expected sections (including endgame).
// ---------------------------------------------------------------------------
static void testEvalReportContainsSections() {
    const std::string name = "testEvalReportContainsSections";
    Position pos; StateListPtr st;
    setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
    const std::string r = makrukEvalReport(pos);
    assert(r.find("phase")         != std::string::npos);
    assert(r.find("endgame")       != std::string::npos);
    assert(r.find("material+pst")  != std::string::npos);
    assert(r.find("rook activity") != std::string::npos);
    assert(r.find("king safety")   != std::string::npos);
    assert(r.find("mop-up")        != std::string::npos);
    assert(r.find("blended")       != std::string::npos);
    assert(r.find("from stm")      != std::string::npos);
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 11: KRK — mop-up adds to score; weak king on edge > weak king at center.
// ---------------------------------------------------------------------------
static void testKRKMopupPushesToEdge() {
    const std::string name = "testKRKMopupPushesToEdge";
    // White K+R vs Black bare King on edge (a8) vs bare King at center (d5).
    // White to move — more-cornered position should score higher.
    Position posEdge; StateListPtr stE;
    setPos(posEdge, stE, "k7/8/8/8/8/8/8/R3K3 w - - 0 1");   // Black King on a8 (edge)

    Position posCenter; StateListPtr stC;
    setPos(posCenter, stC, "8/8/8/3k4/8/8/8/R3K3 w - - 0 1"); // Black King on d5 (center)

    const Value vEdge   = makrukClassicalEval(posEdge);
    const Value vCenter = makrukClassicalEval(posCenter);
    assert(vEdge > vCenter && "KRK: cornered weak king must score higher for strong side");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 12: KNK — draw (single knight cannot force checkmate).
// ---------------------------------------------------------------------------
static void testKNKIsDraw() {
    const std::string name = "testKNKIsDraw";
    // White K+N vs Black bare King; score should be near zero (draw).
    Position pos; StateListPtr st;
    setPos(pos, st, "4k3/8/8/8/8/8/8/3NK3 w - - 0 1");
    const Value v = makrukClassicalEval(pos);
    // Score should be much smaller than a rook's value (1276 cp)
    assert(std::abs(v) < 200 && "KNK must evaluate near draw (|v| < 200 cp)");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 13: KBK (Khon) — draw (single Khon cannot force checkmate).
// ---------------------------------------------------------------------------
static void testKBKIsDraw() {
    const std::string name = "testKBKIsDraw";
    Position pos; StateListPtr st;
    setPos(pos, st, "4k3/8/8/8/8/8/8/3SK3 w - - 0 1"); // S = Khon
    const Value v = makrukClassicalEval(pos);
    assert(std::abs(v) < 200 && "KBK (Khon) must evaluate near draw (|v| < 200 cp)");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 14: KNNK — draw (2 knights cannot force checkmate + counting).
// ---------------------------------------------------------------------------
static void testKNNKIsDraw() {
    const std::string name = "testKNNKIsDraw";
    Position pos; StateListPtr st;
    setPos(pos, st, "4k3/8/8/8/8/8/8/2NNK3 w - - 0 1");
    const Value v = makrukClassicalEval(pos);
    assert(std::abs(v) < 200 && "KNNK must evaluate near draw (|v| < 200 cp)");
    pass(name);
}

// ---------------------------------------------------------------------------
// Test 15: KRK kings-closer bonus — own king closer to weak king = higher eval.
// ---------------------------------------------------------------------------
static void testKRKKingCloseBonus() {
    const std::string name = "testKRKKingCloseBonus";
    // Black bare King on a8 (corner). Compare: White King near (b6) vs far (h1).
    Position posNear; StateListPtr stN;
    setPos(posNear, stN, "k7/8/1K6/8/8/8/8/R7 w - - 0 1"); // W King b6, near a8

    Position posFar; StateListPtr stF;
    setPos(posFar, stF, "k7/8/8/8/8/8/8/R6K w - - 0 1");    // W King h1, far

    const Value vNear = makrukClassicalEval(posNear);
    const Value vFar  = makrukClassicalEval(posFar);
    assert(vNear > vFar && "KRK: own king close to weak king must score higher");
    pass(name);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void runMakrukEvalTests() {
    std::cout << "\n=== Makruk Classical Evaluation Tests ===\n";
    testStartPositionNearZero();
    testExtraRookScoresHigher();
    testKhonValueIsCorrect();
    testMetValueIsCorrect();
    testCentralKhonScoresHigher();
    testRookOpenFileBonusInReport();
    testSymmetricPositionIsZero();
    testEvalFlipsForBlack();
    testAdvancedPawnScoresHigher();
    testEvalReportContainsSections();
    testKRKMopupPushesToEdge();
    testKNKIsDraw();
    testKBKIsDraw();
    testKNNKIsDraw();
    testKRKKingCloseBonus();
    std::cout << "All eval tests passed.\n";
}

} // namespace Nebula
