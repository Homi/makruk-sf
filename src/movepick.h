#pragma once
#include <array>
#include "movegen.h"
#include "position.h"
#include "types.h"

namespace Nebula{
  template <typename T, int D>
  class StatsEntry{
    T entry;
  public:
    void operator=(const T& v){ entry=v; }
    T* operator&(){ return &entry; }
    T* operator->(){ return &entry; }
    operator const T&() const{ return entry; }

    void operator<<(const int bonus){
      const int delta=bonus-static_cast<int>(entry)*std::abs(bonus)/D;

      entry=static_cast<T>(
        static_cast<int>(entry)+delta
      );
    }
  };

  template <typename T, int D, int Size, int... Sizes>
  struct Statistics : std::array<Statistics<T, D, Sizes...>, Size>{
    using Stats = Statistics;

    void fill(const T& v){
      using Entry = StatsEntry<T, D>;
      Entry* p=reinterpret_cast<Entry*>(this);
      std::fill(p,p+sizeof(*this)/sizeof(Entry),v);
    }
  };

  template <typename T, int D, int Size>
  struct Statistics<T, D, Size> : std::array<StatsEntry<T, D>, Size>{};

  enum StatsParams :uint8_t{ NOT_USED=0 };

  enum StatsType :uint8_t{ NoCaptures, Captures };

  using ButterflyHistory = Statistics<int16_t, 7183, COLOR_NB, static_cast<int>(SQUARE_NB)*static_cast<int>(
    SQUARE_NB)>;
  using CounterMoveHistory = Statistics<Move, NOT_USED, PIECE_NB, SQUARE_NB>;
  using CapturePieceToHistory = Statistics<int16_t, 10692, PIECE_NB, SQUARE_NB, PIECE_TYPE_NB>;
  using PieceToHistory = Statistics<int16_t, 29952, PIECE_NB, SQUARE_NB>;
  using ContinuationHistory = Statistics<PieceToHistory, NOT_USED, PIECE_NB, SQUARE_NB>;

  // Pawn correction history (Round 29): corrects a systematic static-eval bias keyed by
  // pawn structure (Position::pawnKey()), analogous to modern Stockfish's correction
  // history. Untuned first-pass constants -- see CLAUDE.md Round 29 for rationale.
  // Grain * MaxCorrection == Divisor by design, so the table's own decay-saturation
  // ceiling and the hard correction clamp in Search::correctedStaticEval() agree.
  constexpr int PawnCorrHistSize          = 16384;  // 2^14, power-of-two index range
  constexpr int PawnCorrHistDivisor       = 1024;   // decay divisor D for StatsEntry<<
  constexpr int PawnCorrHistUpdateCap     = 256;    // clamp on bonus fed into <<
  constexpr int PawnCorrHistGrain         = 16;     // divisor converting entry -> cp
  constexpr int PawnCorrHistMaxCorrection = 64;     // hard clamp on applied correction, cp

  using PawnCorrectionHistory = Statistics<int16_t, PawnCorrHistDivisor, COLOR_NB, PawnCorrHistSize>;

  // Material/minor-piece/non-pawn correction history: the rest of the full
  // 4-table Stockfish suite, keyed on the new Position::materialKey()/
  // minorPieceKey()/nonPawnKey() (see position.h -- deliberately maintained by
  // a full recompute from board state, not true incremental XOR, to avoid the
  // seeGe()-class incremental-divergence bug risk). Same shape and same
  // untuned first-pass constants as PawnCorrectionHistory above (no stronger
  // rationale yet to differentiate them -- a tuning sweep, if this shows a
  // real effect, is future work, same as Round 29's own open item).
  constexpr int MaterialCorrHistSize   = 16384;
  constexpr int MinorPieceCorrHistSize = 16384;
  constexpr int NonPawnCorrHistSize    = 16384;

  using MaterialCorrectionHistory   = Statistics<int16_t, PawnCorrHistDivisor, COLOR_NB, MaterialCorrHistSize>;
  using MinorPieceCorrectionHistory = Statistics<int16_t, PawnCorrHistDivisor, COLOR_NB, MinorPieceCorrHistSize>;
  using NonPawnCorrectionHistory    = Statistics<int16_t, PawnCorrHistDivisor, COLOR_NB, NonPawnCorrHistSize>;

  // Combined cap on the SUM of all 4 tables' individually-clamped corrections
  // (each up to +-PawnCorrHistMaxCorrection=64, so up to +-256 if all 4 agreed
  // in sign uncapped) -- matches the magnitude of the existing NNUE blend's
  // own +-100cp cap elsewhere in this codebase, a reasonable ceiling for a
  // single static-eval adjustment.
  constexpr int CorrHistTotalMaxCorrection = 100;

  class MovePicker{
    enum PickType :uint8_t{ Next, Best };
  public:
    MovePicker(const MovePicker&) = delete;
    MovePicker& operator=(const MovePicker&) = delete;
    MovePicker(const Position&, Move, Depth, const ButterflyHistory*,
      const CapturePieceToHistory*,
      const PieceToHistory**,
      Move,
      const Move*);
    MovePicker(const Position&, Move, Depth, const ButterflyHistory*,
      const CapturePieceToHistory*,
      const PieceToHistory**,
      Square);
    MovePicker(const Position&, Move, Value, Depth, const CapturePieceToHistory*);
    Move nextMove(bool skipQuiets=false);
  private:
    template <PickType T, typename Pred>
    Move select(Pred);
    template <GenType>
    void score() const;
    [[nodiscard]] ExtMove* begin() const{ return cur; }
    [[nodiscard]] ExtMove* end() const{ return endMoves; }
    const Position& pos;
    const ButterflyHistory* mainHistory;
    const CapturePieceToHistory* captureHistory;
    const PieceToHistory** continuationHistory;
    Move ttMove;
    ExtMove refutations[3], *cur, *endMoves, *endBadCaptures;
    int stage;
    Square recaptureSquare;
    Value threshold;
    Depth depth;
    ExtMove moves[maxMoves];
  };
}
