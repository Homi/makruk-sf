#pragma once
// HalfKAv2 feature set for Makruk (Thai Chess).
// Piece slots follow the engine's type aliases:
//   BISHOP slot = Khon  (one step forward or diagonally)
//   QUEEN  slot = Met   (one step diagonally)
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "../nnue_common.h"
#include "../../evaluate.h"
#include "../../misc.h"

namespace Nebula{
  struct StateInfo;
}

namespace Nebula::Eval::Nnue::Features{
  class HalfKAv2Makruk{
    enum :uint16_t{
      PS_NONE     = 0,
      PS_W_PAWN   = 0,
      PS_B_PAWN   = 1*SQUARE_NB,
      PS_W_KNIGHT = 2*SQUARE_NB,  // Ma
      PS_B_KNIGHT = 3*SQUARE_NB,
      PS_W_KHON   = 4*SQUARE_NB,  // Khon (BISHOP slot)
      PS_B_KHON   = 5*SQUARE_NB,
      PS_W_ROOK   = 6*SQUARE_NB,  // Ruea
      PS_B_ROOK   = 7*SQUARE_NB,
      PS_W_MET    = 8*SQUARE_NB,  // Met (QUEEN slot)
      PS_B_MET    = 9*SQUARE_NB,
      PS_KING     = 10*SQUARE_NB, // Khun
      PS_NB       = 11*SQUARE_NB
    };

    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB]={
      // White perspective
      {
        PS_NONE, PS_W_PAWN, PS_W_KNIGHT, PS_W_KHON, PS_W_ROOK, PS_W_MET, PS_KING, PS_NONE,
        PS_NONE, PS_B_PAWN, PS_B_KNIGHT, PS_B_KHON, PS_B_ROOK, PS_B_MET, PS_KING, PS_NONE
      },
      // Black perspective (swap W/B)
      {
        PS_NONE, PS_B_PAWN, PS_B_KNIGHT, PS_B_KHON, PS_B_ROOK, PS_B_MET, PS_KING, PS_NONE,
        PS_NONE, PS_W_PAWN, PS_W_KNIGHT, PS_W_KHON, PS_W_ROOK, PS_W_MET, PS_KING, PS_NONE
      }
    };

    static Square orient(Color perspective, Square s, Square ksq);
    static IndexType makeIndex(Color perspective, Square s, Piece pc, Square ksq);

  public:
    // Distinct hash — ensures chess nets cannot be accidentally loaded as Makruk nets.
    static constexpr std::uint32_t HashValue = 0xA3D5B2C1u;

    static constexpr IndexType Dimensions =
      static_cast<IndexType>(SQUARE_NB) * static_cast<IndexType>(PS_NB) / 2;

    // King bucket: mirror king to the right half (e-h files), 32 buckets.
    static constexpr int KingBuckets[64]={
      -1,-1,-1,-1,31,30,29,28,
      -1,-1,-1,-1,27,26,25,24,
      -1,-1,-1,-1,23,22,21,20,
      -1,-1,-1,-1,19,18,17,16,
      -1,-1,-1,-1,15,14,13,12,
      -1,-1,-1,-1,11,10, 9, 8,
      -1,-1,-1,-1, 7, 6, 5, 4,
      -1,-1,-1,-1, 3, 2, 1, 0
    };

    // Max active features per position: up to 32 non-king pieces + king itself.
    static constexpr IndexType MaxActiveDimensions = 32;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;

    static void appendActiveIndices(
      const Position& pos,
      Color perspective,
      IndexList& active);

    static void appendChangedIndices(
      Square ksq,
      const DirtyPiece& dp,
      Color perspective,
      IndexList& removed,
      IndexList& added);

    static int updateCost(const StateInfo* st);
    static int refreshCost(const Position& pos);
    static bool requiresRefresh(const StateInfo* st, Color perspective);
  };
}
