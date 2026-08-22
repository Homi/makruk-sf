// Incremental accumulator cache for MknnEvaluator (MKN2 int16 format only).
//
// Lives in StateInfo (see position.h), riding along the search's doMove/undoMove
// StateInfo chain the same way DirtyPiece already does, so it inherits the same
// per-thread isolation with zero extra synchronization.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#pragma once
#include <cstdint>

namespace Nebula::Eval::Nnue {

// Compile-time cap on the feature-transformer width supported by the incremental
// accumulator cache. The currently deployed net (2020277415.bin) uses L1=512;
// earlier nets used 256. A VERSION2 net with L1 > MknnMaxL1 still loads and
// evaluates correctly, but MknnEvaluator falls back to full-refresh on every
// call (see MknnEvaluator::load()'s incremental_ flag) rather than overflow.
inline constexpr int MknnMaxL1 = 512;

struct alignas(64) MknnAccumulator {
    // [perspective][unit] — int32 to match accumulate_i16()'s native accumulation
    // precision exactly (up to 32 features x ~2150 max weight overflows int16).
    std::int32_t acc[2][MknnMaxL1];
    bool computed[2] = { false, false };
};

} // namespace Nebula::Eval::Nnue
