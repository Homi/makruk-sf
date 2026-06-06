// Makruk counting subsystem implementation.
// See makruk_counting.h for full design notes.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "makruk_counting.h"
#include "../position.h"
#include <sstream>

namespace Nebula {

// ---------------------------------------------------------------------------
// Material signature
// ---------------------------------------------------------------------------

MakrukMaterialSignature computeMakrukMaterialSignature(const Position& pos) {
    MakrukMaterialSignature sig;
    for (const Color c : {WHITE, BLACK}) {
        sig.rooks[c]   = pos.count<ROOK>(c);
        sig.knights[c] = pos.count<KNIGHT>(c);
        sig.khons[c]   = pos.count<BISHOP>(c);  // BISHOP slot = Khon
        sig.mets[c]    = pos.count<QUEEN>(c);   // QUEEN slot = Met (incl. promoted pawns)
        sig.pawns[c]   = pos.count<PAWN>(c);
    }
    return sig;
}

// ---------------------------------------------------------------------------
// Counting limit
// ---------------------------------------------------------------------------

int getMakrukCountingLimit(const MakrukMaterialSignature& sig,
                            const Color claimant,
                            const MakrukCountingMode mode) {
    if (mode == MakrukCountingMode::Off)
        return 0;

    // The claimant is the weaker (bare) side; the stronger side is the opponent.
    const Color stronger = ~claimant;
    const int r = sig.rooks[stronger];
    const int n = sig.knights[stronger];
    const int b = sig.khons[stronger];

    if (mode == MakrukCountingMode::Fairy || mode == MakrukCountingMode::ThaiStrict) {
        // Priority: strongest piece determines the limit.
        if (r > 0)                return CountingLimits::Rook;
        if (b >= 2 && n == 0)     return CountingLimits::TwoBish;
        if (n >= 2 && b == 0)     return CountingLimits::TwoKnight;
        if (b == 1 && n == 0)     return CountingLimits::OneBish;
        if (n == 1 && b == 0)     return CountingLimits::OneKnight;
        // TODO: mixed Khon+Knight, Met-only, and multi-piece endgames.
        return CountingLimits::Board;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Eligibility
// ---------------------------------------------------------------------------

bool isMakrukCountingEligible(const MakrukMaterialSignature& sig,
                               const Color claimant,
                               const MakrukCountingMode mode) {
    if (mode == MakrukCountingMode::Off)
        return false;

    // The claimant must have only their king remaining.
    if (!sig.bareKing(claimant))
        return false;

    if (mode == MakrukCountingMode::Fairy) {
        // Piece counting (Fairy): eligible only when no pawns remain on the board.
        // Board counting (with pawns) is a separate mechanism — TODO for a future PR.
        return !sig.anyPawn();
    }

    if (mode == MakrukCountingMode::ThaiStrict) {
        // TODO: implement strict Thai Chess Federation eligibility rules.
        // Placeholder: same as Fairy for now.
        return !sig.anyPawn();
    }

    return false;
}

// ---------------------------------------------------------------------------
// State update
// ---------------------------------------------------------------------------

void updateMakrukCountingState(MakrukCountingState& cs, const Position& /*pos*/) {
    if (!cs.active) return;
    ++cs.count;
    // TODO: if a pawn is pushed or a piece is captured, counting eligibility may
    // change and the count may need to be reset or counting stopped.
    // For now the count is always incremented; the caller checks isMakrukCountingDraw().
}

// ---------------------------------------------------------------------------
// Draw detection
// ---------------------------------------------------------------------------

bool isMakrukCountingDraw(const MakrukCountingState& cs) {
    return cs.active && cs.limit > 0 && cs.count >= cs.limit;
}

// ---------------------------------------------------------------------------
// Debug report
// ---------------------------------------------------------------------------

std::string makrukCountingReport(const Position& pos,
                                  const MakrukCountingState& cs,
                                  const MakrukCountingMode mode) {
    const MakrukMaterialSignature sig = computeMakrukMaterialSignature(pos);

    const char* modeStr =
        mode == MakrukCountingMode::Off        ? "off"
      : mode == MakrukCountingMode::Fairy      ? "fairy"
      : mode == MakrukCountingMode::ThaiStrict ? "thai-strict"
      : "unknown";

    std::ostringstream ss;
    ss << "makrukcount\n";
    ss << "  mode         : " << modeStr << "\n";
    ss << "  active       : " << (cs.active ? "yes" : "no") << "\n";

    if (cs.active) {
        ss << "  claimant     : " << (cs.claimant == WHITE ? "white" : "black") << "\n";
        ss << "  count        : " << cs.count << "\n";
        ss << "  limit        : " << cs.limit << "\n";
        ss << "  draw reached : " << (isMakrukCountingDraw(cs) ? "yes" : "no") << "\n";
    }

    ss << "  eligible[w]  : "
       << (isMakrukCountingEligible(sig, WHITE, mode) ? "yes" : "no") << "\n";
    ss << "  eligible[b]  : "
       << (isMakrukCountingEligible(sig, BLACK, mode) ? "yes" : "no") << "\n";

    ss << "  material\n";
    for (const Color c : {WHITE, BLACK}) {
        ss << "    " << (c == WHITE ? "white" : "black") << ":"
           << " R=" << sig.rooks[c]
           << " N=" << sig.knights[c]
           << " S=" << sig.khons[c]
           << " M=" << sig.mets[c]
           << " P=" << sig.pawns[c]
           << (sig.bareKing(c) ? " (bare king)" : "") << "\n";
    }

    return ss.str();
}

}  // namespace Nebula
