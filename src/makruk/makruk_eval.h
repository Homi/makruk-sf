// Makruk classical evaluation subsystem.
//
// Hand-crafted evaluation for Makruk positions. Not a fallback: evaluate.cpp
// blends this with NNUE at all times (see CLAUDE.md "Evaluation").
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
