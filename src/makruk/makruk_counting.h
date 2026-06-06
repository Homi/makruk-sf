// Makruk counting subsystem.
//
// Makruk (Thai Chess) has a draw rule based on counting moves in the endgame.
// This is NOT a reuse of the chess 50-move rule. Key differences:
//   - The limit depends on which pieces the stronger side has.
//   - Only the weaker side (bare king) may invoke counting.
//   - Stalemate, checkmate, and repetition remain separate win/draw conditions.
//   - Counting draw is only triggered when the count reaches the limit.
//
// This file provides data structures and functions for a standalone counting
// subsystem. It is NOT wired into search or game-end detection in this PR.
// Wire-up is deferred to a future PR once the subsystem is verified correct.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#pragma once
#include <string>
#include "../types.h"

namespace Nebula {

class Position;

// ---------------------------------------------------------------------------
// Mode
// ---------------------------------------------------------------------------

enum class MakrukCountingMode : uint8_t {
    Off,        // counting disabled
    Fairy,      // Fairy-Stockfish-compatible behaviour (first target)
    ThaiStrict  // TODO: strict Thai Chess Federation rules (future PR)
};

// ---------------------------------------------------------------------------
// Material signature
// ---------------------------------------------------------------------------

// Snapshot of piece counts at a given position.
// NOTE: promoted pawns are stored in the Met (QUEEN) slot alongside original
// Mets because they share the same piece-type slot. There is currently no
// move-history tracking to distinguish them.
// TODO: add promoted-pawn origin tracking in a future PR if needed.
struct MakrukMaterialSignature {
    int rooks[COLOR_NB]   = {};  // Ruea
    int knights[COLOR_NB] = {};  // Ma
    int khons[COLOR_NB]   = {};  // Khon  (BISHOP slot)
    int mets[COLOR_NB]    = {};  // Met   (QUEEN slot; includes promoted pawns)
    int pawns[COLOR_NB]   = {};  // Bia (unpromoted)

    [[nodiscard]] bool bareKing(Color c) const {
        return rooks[c]==0 && knights[c]==0 && khons[c]==0
               && mets[c]==0 && pawns[c]==0;
    }
    [[nodiscard]] bool anyPawn()  const { return pawns[WHITE]>0  || pawns[BLACK]>0;  }
    [[nodiscard]] bool anyRook()  const { return rooks[WHITE]>0  || rooks[BLACK]>0;  }
    [[nodiscard]] int  totalPieces(Color c) const {
        return rooks[c]+knights[c]+khons[c]+mets[c]+pawns[c];
    }
};

// ---------------------------------------------------------------------------
// Counting state
// ---------------------------------------------------------------------------

struct MakrukCountingState {
    bool   active               = false;
    Color  claimant             = WHITE; // side that invoked counting (the bare/weaker side)
    int    count                = 0;     // plies elapsed since counting started
    int    limit                = 0;     // ply limit for this endgame type (see CountingLimits)
    int    reversiblePlyAtStart = 0;     // position's rule50 value when counting began
    MakrukMaterialSignature matSig;      // material at the moment counting was invoked
    MakrukCountingMode mode     = MakrukCountingMode::Off;
};

// ---------------------------------------------------------------------------
// Named counting limits (in plies = half-moves)
// ---------------------------------------------------------------------------
// These represent the number of plies the stronger side has to deliver checkmate
// once counting has been invoked by the weaker (bare) side.
//
// Values are derived from commonly-cited Makruk endgame rules.
// TODO: verify each limit against the Thai Chess Federation official rulebook.
// TODO: verify against Fairy-Stockfish's exact implementation for Fairy mode.
namespace CountingLimits {
    constexpr int Rook      = 32;  // rook vs bare king: 16 moves × 2  TODO: verify
    constexpr int TwoBish   = 44;  // 2 Khon vs bare king: 22 moves    TODO: verify
    constexpr int TwoKnight = 64;  // 2 Knight vs bare king: 32 moves  TODO: verify
    constexpr int OneBish   = 88;  // 1 Khon vs bare king: 44 moves    TODO: verify
    constexpr int OneKnight = 128; // 1 Knight vs bare king: 64 moves  TODO: verify
    constexpr int Board     = 128; // fallback / board-counting default TODO: verify
}  // namespace CountingLimits

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Build a MakrukMaterialSignature from the current board position.
MakrukMaterialSignature computeMakrukMaterialSignature(const Position& pos);

// Return the counting limit (in plies) applicable to the given claimant.
// 'claimant' is the side invoking counting (typically the bare/weaker side).
// The limit is computed from the stronger side's pieces.
// Returns 0 if mode is Off or no applicable limit is found.
int getMakrukCountingLimit(const MakrukMaterialSignature& sig,
                            Color claimant,
                            MakrukCountingMode mode);

// Return true if 'claimant' is eligible to invoke counting.
// Fairy mode: bare king AND no pawns anywhere on the board.
// ThaiStrict: TODO.
bool isMakrukCountingEligible(const MakrukMaterialSignature& sig,
                               Color claimant,
                               MakrukCountingMode mode);

// Check whether any side should begin counting in the current position.
// Sets outClaimant to the eligible side (WHITE checked first).
// Returns false if no side is eligible or mode is Off.
bool shouldActivateMakrukCounting(const MakrukMaterialSignature& sig,
                                   Color& outClaimant,
                                   MakrukCountingMode mode);

// Initialise and activate a counting state from scratch.
// Records current material, computes the applicable limit, and sets count=0.
// Caller is responsible for checking shouldActivateMakrukCounting() first.
void activateMakrukCounting(MakrukCountingState& cs,
                              const Position& pos,
                              Color claimant,
                              MakrukCountingMode mode);

// Advance the counting state by one ply (call after every doMove()).
// Behaviour when cs.active is true:
//   1. Recheck claimant eligibility; deactivate if no longer eligible.
//   2. If the stronger side's material changed (lost a piece), recalculate the
//      limit based on the new material — the count is NOT reset.
//   3. Increment cs.count.
// No-op when cs.active is false.
void updateMakrukCountingState(MakrukCountingState& cs, const Position& pos);

// Return true if the counting limit has been reached (draw).
bool isMakrukCountingDraw(const MakrukCountingState& cs);

// Return a human-readable debug report for the counting subsystem.
// Shows mode, active/inactive, eligibility, count, limit, and material.
std::string makrukCountingReport(const Position& pos,
                                  const MakrukCountingState& cs,
                                  MakrukCountingMode mode);

}  // namespace Nebula
