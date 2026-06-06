// Makruk classical evaluation subsystem.
//
// Provides a hand-crafted evaluation function for Makruk positions.
// Used as the primary evaluation until a Makruk-specific NNUE is trained.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#pragma once
#include <string>
#include "../types.h"

namespace Nebula {

class Position;

// Classical Makruk evaluation.
// Returns a score from the perspective of the side to move.
// Positive = good for the side to move.
Value makrukClassicalEval(const Position& pos);

// Human-readable breakdown of material, PST, and heuristic contributions.
std::string makrukEvalReport(const Position& pos);

} // namespace Nebula
