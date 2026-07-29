// HalfKAv2 feature implementation for Makruk (Thai Chess).
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "half_ka_v2_makruk.h"
#include "../../position.h"

namespace Nebula::Eval::Nnue::Features{

  inline Square HalfKAv2Makruk::orient(
      const Color perspective, const Square s, const Square ksq){
    // Flip vertically for Black; mirror horizontally when king is on a-d files.
    return static_cast<Square>(
      static_cast<int>(s)
      ^ (static_cast<bool>(perspective) * SQ_A8)
      ^ ((fileOf(ksq) < FILE_E) * SQ_H1));
  }

  inline IndexType HalfKAv2Makruk::makeIndex(
      const Color perspective, const Square s, const Piece pc, const Square ksq){
    const Square oKsq = orient(perspective, ksq, ksq);
    return orient(perspective, s, ksq)
         + PieceSquareIndex[perspective][pc]
         + PS_NB * KingBuckets[oKsq];
  }

  void HalfKAv2Makruk::appendActiveIndices(
      const Position& pos, const Color perspective, IndexList& active){
    const Square ksq = pos.square<KING>(perspective);
    uint64_t bb = pos.pieces();
    while (bb){
      const Square s = popLsb(bb);
      active.pushBack(makeIndex(perspective, s, pos.pieceOn(s), ksq));
    }
  }

  void HalfKAv2Makruk::appendChangedIndices(
      const Square ksq, const DirtyPiece& dp,
      const Color perspective, IndexList& removed, IndexList& added){
    for (int i = 0; i < dp.dirty_num; ++i){
      if (dp.from[i] != SQ_NONE)
        removed.pushBack(makeIndex(perspective, dp.from[i], dp.piece[i], ksq));
      if (dp.to[i] != SQ_NONE)
        added.pushBack(makeIndex(perspective, dp.to[i], dp.piece[i], ksq));
    }
  }

  int HalfKAv2Makruk::updateCost(const StateInfo* st){
    return st->dirtyPiece.dirty_num;
  }

  int HalfKAv2Makruk::refreshCost(const Position& pos){
    return pos.count<ALL_PIECES>();
  }

  bool HalfKAv2Makruk::requiresRefresh(const StateInfo* st, const Color perspective){
    return st->dirtyPiece.piece[0] == makePiece(perspective, KING);
  }

}
