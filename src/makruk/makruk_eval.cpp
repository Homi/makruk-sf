// Makruk classical evaluation implementation.
// See makruk_eval.h for design notes.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "makruk_eval.h"
#include "../position.h"
#include "../bitboard.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Nebula {

// ---------------------------------------------------------------------------
// Phase
// ---------------------------------------------------------------------------

// Phase weights per piece type (pawns and king excluded).
// Starting total: 2*(2*4 + 2*1 + 2*1 + 1*2) = 28 (MaxPhase)
constexpr int PhaseWeights[PIECE_TYPE_NB] = {0, 0, 1, 1, 4, 2, 0};
constexpr int MaxPhase = 28;

// ---------------------------------------------------------------------------
// PST tables
// ---------------------------------------------------------------------------
// Indexed by square (a1=0..h8=63). White's perspective.
// Black uses sq^56 (vertical flip) before lookup.

// Compact calcScore alias for readability
constexpr Score S(int mg, int eg) { return calcScore(mg, eg); }

// Pawn: reward central placement and advancement toward promotion (rank 5).
// White pawns start rank 2, promote rank 5.
constexpr Score PawnPST[64] = {
    // rank 0 — impossible for pawn
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 1 — impossible
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 2 — starting rank
    S( -5, -5), S(  0,  0), S(  3,  3), S(  5,  5), S(  5,  5), S(  3,  3), S(  0,  0), S( -5, -5),
    // rank 3 — one advance
    S( -5,  0), S(  0,  5), S(  5,  8), S(  8, 10), S(  8, 10), S(  5,  8), S(  0,  5), S( -5,  0),
    // rank 4 — two advances
    S( -5,  5), S(  0, 10), S(  5, 13), S( 10, 15), S( 10, 15), S(  5, 13), S(  0, 10), S( -5,  5),
    // rank 5 — one step from promotion
    S( -5, 15), S(  0, 20), S(  5, 25), S( 15, 30), S( 15, 30), S(  5, 25), S(  0, 20), S( -5, 15),
    // rank 6 — promotion rank (pawn becomes Met; PST not used here)
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 7 — impossible
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
};

// Knight (Ma): centralization bonus.
constexpr Score KnightPST[64] = {
    S(-20,-20), S(-10,-10), S(-10,-10), S(-10,-10), S(-10,-10), S(-10,-10), S(-10,-10), S(-20,-20),
    S(-10,-10), S(  0,  0), S(  5,  5), S(  5,  5), S(  5,  5), S(  5,  5), S(  0,  0), S(-10,-10),
    S(-10,-10), S(  5,  5), S( 10, 10), S( 12, 12), S( 12, 12), S( 10, 10), S(  5,  5), S(-10,-10),
    S(-10,-10), S(  5,  5), S( 12, 12), S( 15, 15), S( 15, 15), S( 12, 12), S(  5,  5), S(-10,-10),
    S(-10,-10), S(  5,  5), S( 12, 12), S( 15, 15), S( 15, 15), S( 12, 12), S(  5,  5), S(-10,-10),
    S(-10,-10), S(  5,  5), S( 10, 10), S( 12, 12), S( 12, 12), S( 10, 10), S(  5,  5), S(-10,-10),
    S(-10,-10), S(  0,  0), S(  5,  5), S(  5,  5), S(  5,  5), S(  5,  5), S(  0,  0), S(-10,-10),
    S(-20,-20), S(-10,-10), S(-10,-10), S(-10,-10), S(-10,-10), S(-10,-10), S(-10,-10), S(-20,-20),
};

// Khon (BISHOP slot): short-range piece; centralization is critical.
constexpr Score KhonPST[64] = {
    S(-15,-15), S( -5,-10), S( -5,-10), S( -5,-10), S( -5,-10), S( -5,-10), S( -5,-10), S(-15,-15),
    S( -5,-10), S(  5,  0), S(  8,  5), S(  8,  5), S(  8,  5), S(  8,  5), S(  5,  0), S( -5,-10),
    S( -5, -5), S(  8,  5), S( 12, 10), S( 15, 12), S( 15, 12), S( 12, 10), S(  8,  5), S( -5, -5),
    S( -5, -5), S(  8,  5), S( 15, 12), S( 18, 15), S( 18, 15), S( 15, 12), S(  8,  5), S( -5, -5),
    S( -5, -5), S(  8,  5), S( 15, 12), S( 18, 15), S( 18, 15), S( 15, 12), S(  8,  5), S( -5, -5),
    S( -5, -5), S(  8,  5), S( 12, 10), S( 15, 12), S( 15, 12), S( 12, 10), S(  8,  5), S( -5, -5),
    S( -5,-10), S(  5,  0), S(  8,  5), S(  8,  5), S(  8,  5), S(  8,  5), S(  5,  0), S( -5,-10),
    S(-15,-15), S( -5,-10), S( -5,-10), S( -5,-10), S( -5,-10), S( -5,-10), S( -5,-10), S(-15,-15),
};

// Met (QUEEN slot): diagonal-only step piece; central squares give more coverage.
constexpr Score MetPST[64] = {
    S(-10,-10), S( -5, -5), S( -5, -5), S( -5, -5), S( -5, -5), S( -5, -5), S( -5, -5), S(-10,-10),
    S( -5, -5), S(  5,  5), S(  5,  5), S(  8,  8), S(  8,  8), S(  5,  5), S(  5,  5), S( -5, -5),
    S( -5, -5), S(  5,  5), S( 10, 10), S( 12, 12), S( 12, 12), S( 10, 10), S(  5,  5), S( -5, -5),
    S( -5, -5), S(  8,  8), S( 12, 12), S( 15, 15), S( 15, 15), S( 12, 12), S(  8,  8), S( -5, -5),
    S( -5, -5), S(  8,  8), S( 12, 12), S( 15, 15), S( 15, 15), S( 12, 12), S(  8,  8), S( -5, -5),
    S( -5, -5), S(  5,  5), S( 10, 10), S( 12, 12), S( 12, 12), S( 10, 10), S(  5,  5), S( -5, -5),
    S( -5, -5), S(  5,  5), S(  5,  5), S(  8,  8), S(  8,  8), S(  5,  5), S(  5,  5), S( -5, -5),
    S(-10,-10), S( -5, -5), S( -5, -5), S( -5, -5), S( -5, -5), S( -5, -5), S( -5, -5), S(-10,-10),
};

// Rook (Ruea): slight back-rank penalty; 7th/8th rank bonus.
// Open-file bonus is handled separately in rookActivity().
constexpr Score RookPST[64] = {
    S( -5,  0), S(  0,  0), S(  0,  0), S(  3,  0), S(  3,  0), S(  0,  0), S(  0,  0), S( -5,  0),
    S( -5,  0), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S( -5,  0),
    S( -5,  0), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S( -5,  0),
    S( -5,  0), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S( -5,  0),
    S( -5,  0), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S( -5,  0),
    S( -5,  0), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S(  0,  5), S( -5,  0),
    S(  5, 10), S( 10, 15), S( 10, 15), S( 10, 15), S( 10, 15), S( 10, 15), S( 10, 15), S(  5, 10),
    S( 10, 10), S( 15, 15), S( 15, 15), S( 15, 15), S( 15, 15), S( 15, 15), S( 15, 15), S( 10, 10),
};

// King: MG = safety (prefer corners/edges); EG = activity (prefer center).
constexpr Score KingPST[64] = {
    S( 25,-20), S( 30,-10), S( 10,-10), S(  0,-15), S(  0,-15), S( 10,-10), S( 30,-10), S( 25,-20),
    S(  5,-10), S(  5, -5), S( -5, -5), S(-10,-10), S(-10,-10), S( -5, -5), S(  5, -5), S(  5,-10),
    S(-10,-10), S(-10, -5), S(-15, -5), S(-20,  5), S(-20,  5), S(-15, -5), S(-10, -5), S(-10,-10),
    S(-20,-10), S(-20, -5), S(-25,  5), S(-30, 15), S(-30, 15), S(-25,  5), S(-20, -5), S(-20,-10),
    S(-20,-10), S(-20, -5), S(-25,  5), S(-30, 15), S(-30, 15), S(-25,  5), S(-20, -5), S(-20,-10),
    S(-10,-10), S(-10, -5), S(-15, -5), S(-20,  5), S(-20,  5), S(-15, -5), S(-10, -5), S(-10,-10),
    S(  5,-10), S(  5, -5), S( -5, -5), S(-10,-10), S(-10,-10), S( -5, -5), S(  5, -5), S(  5,-10),
    S( 25,-20), S( 30,-10), S( 10,-10), S(  0,-15), S(  0,-15), S( 10,-10), S( 30,-10), S( 25,-20),
};

static const Score* pstFor(PieceType pt) {
    switch (pt) {
        case PAWN:   return PawnPST;
        case KNIGHT: return KnightPST;
        case BISHOP: return KhonPST;
        case ROOK:   return RookPST;
        case QUEEN:  return MetPST;
        case KING:   return KingPST;
        default:     return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Add bonus to the score with the correct sign for the given color.
// score is always White - Black (positive = good for White).
static void add(Score& score, Score bonus, Color c) {
    if (c == WHITE) score += static_cast<int>(bonus);
    else            score += static_cast<int>(-bonus);
}

// ---------------------------------------------------------------------------
// Heuristics
// ---------------------------------------------------------------------------

// Bonus for rooks on open files (no own pawns) or semi-open files (no own, but enemy pawns).
static Score rookActivity(const Position& pos, Color c) {
    Score s = SCORE_ZERO;
    uint64_t rooks = pos.pieces(c, ROOK);
    while (rooks) {
        const Square sq = popLsb(rooks);
        const uint64_t file = fileBb(sq);
        const bool ownPawn   = static_cast<bool>(pos.pieces(c,    PAWN) & file);
        const bool enemyPawn = static_cast<bool>(pos.pieces(~c,   PAWN) & file);
        if (!ownPawn && !enemyPawn) s += static_cast<int>(S(20, 10));
        else if (!ownPawn)          s += static_cast<int>(S(10,  5));
    }
    return s;
}

// Bonus for own pawns shielding the king (MG only).
static Score kingSafety(const Position& pos, Color c) {
    const Square ksq = pos.square<KING>(c);
    const int shield = popcnt(attacksBb<KING>(ksq) & pos.pieces(c, PAWN));
    return S(shield * 10, 0);
}

// ---------------------------------------------------------------------------
// Endgame recognition and mop-up evaluation
// ---------------------------------------------------------------------------
// In Makruk all non-rook pieces move only one step, so without endgame
// guidance the search cannot reliably find forced mate: it needs explicit
// "push weak king to edge" and "close own king" bonuses.

static bool isBareKing(const Position& pos, Color c) {
    return pos.count<PAWN>(c)   == 0
        && pos.count<KNIGHT>(c) == 0
        && pos.count<BISHOP>(c) == 0  // Khon
        && pos.count<ROOK>(c)   == 0
        && pos.count<QUEEN>(c)  == 0; // Met
}

// How far a square is from the nearest board edge (0 = edge/corner, 3 = d4/d5/e4/e5).
static int distFromEdge(Square s) {
    return std::min({(int)fileOf(s), 7 - (int)fileOf(s),
                     (int)rankOf(s), 7 - (int)rankOf(s)});
}

// Chebyshev (king-move) distance between two squares.
static int chebyshev(Square a, Square b) {
    return std::max(std::abs((int)fileOf(a) - (int)fileOf(b)),
                    std::abs((int)rankOf(a) - (int)rankOf(b)));
}

// Returns true when the stronger side cannot force checkmate against a bare king.
// Single minor piece (Knight, Khon, Met) and 2-Knight or 2-Met configurations
// cannot force mate; Makruk counting rules also bound the 2-Knight case.
static bool isInsufficientMaterial(const Position& pos, Color stronger) {
    const int r = pos.count<ROOK>(stronger);
    const int p = pos.count<PAWN>(stronger);
    if (r > 0 || p > 0) return false;          // rook or pawn can force win
    const int n = pos.count<KNIGHT>(stronger);
    const int b = pos.count<BISHOP>(stronger);  // Khon
    const int q = pos.count<QUEEN>(stronger);   // Met
    const int total = n + b + q;
    if (total <= 1)                 return true; // lone minor piece
    if (n == 2 && b == 0 && q == 0) return true; // KNNvK — counting draw
    if (q >= 1 && n == 0 && b == 0) return true; // Met(s) only — insufficient
    return false;
}

// Mop-up bonus for winning endgames where the weaker side has only a king.
// Adds positional urgency on top of material advantage:
//   – push weak king toward the edge/corner  (Makruk mates always happen there)
//   – close own king to assist the mate
static Score mopupBonus(const Position& pos, Color stronger) {
    const Square sKsq = pos.square<KING>(stronger);
    const Square wKsq = pos.square<KING>(~stronger);
    // (3 - distFromEdge) in [0..3]; multiply by 30 to give 0–90 cp per axis
    const int edgeBonus  = (3 - distFromEdge(wKsq)) * 30;
    // (7 - chebyshev) in [0..6]; each step closer = +12 cp
    const int closeBonus = (7 - chebyshev(sKsq, wKsq)) * 12;
    return S(edgeBonus + closeBonus, edgeBonus + closeBonus);
}

// ---------------------------------------------------------------------------
// Phase
// ---------------------------------------------------------------------------

static int computePhase(const Position& pos) {
    int phase = 0;
    for (Color c : {WHITE, BLACK}) {
        phase += PhaseWeights[KNIGHT] * pos.count<KNIGHT>(c);
        phase += PhaseWeights[BISHOP] * pos.count<BISHOP>(c);
        phase += PhaseWeights[ROOK]   * pos.count<ROOK>(c);
        phase += PhaseWeights[QUEEN]  * pos.count<QUEEN>(c);
    }
    return std::clamp(phase, 0, MaxPhase);
}

// ---------------------------------------------------------------------------
// Core evaluation
// ---------------------------------------------------------------------------

// Accumulate material + PST scores for all pieces. Returns (White - Black) Score.
static Score evalMaterialAndPST(const Position& pos) {
    Score score = SCORE_ZERO;
    for (Color c : {WHITE, BLACK}) {
        for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            const Score* pst = pstFor(pt);
            uint64_t bb = pos.pieces(c, pt);
            while (bb) {
                const Square sq = popLsb(bb);
                // PST square: flip vertically for Black
                const Square pstSq = (c == WHITE) ? sq : Square(static_cast<int>(sq) ^ 56);
                Score piece = S(pieceValue[MG][pt], pieceValue[EG][pt]);
                piece += static_cast<int>(pst[pstSq]);
                add(score, piece, c);
            }
        }
    }
    return score;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Value makrukClassicalEval(const Position& pos) {
    Score score = evalMaterialAndPST(pos);

    for (Color c : {WHITE, BLACK}) {
        add(score, rookActivity(pos, c), c);
        add(score, kingSafety(pos, c), c);
    }

    // Bare-king endgame handling: at most one side can be bare at a time.
    for (Color stronger : {WHITE, BLACK}) {
        if (!isBareKing(pos, ~stronger)) continue;
        if (isInsufficientMaterial(pos, stronger)) {
            // Scale heavily toward draw — keep only a tiny residual for contempt.
            score = calcScore(mgValue(score) / 8, egValue(score) / 8);
        } else {
            // Winning endgame: add mop-up guidance so the engine drives the
            // enemy king to an edge/corner and closes its own king.
            add(score, mopupBonus(pos, stronger), stronger);
        }
        break;
    }

    const int phase = computePhase(pos);
    const Value v = (mgValue(score) * phase + egValue(score) * (MaxPhase - phase)) / MaxPhase;

    return (pos.stm() == WHITE) ? v : -v;
}

// ---------------------------------------------------------------------------
// Debug report
// ---------------------------------------------------------------------------

std::string makrukEvalReport(const Position& pos) {
    Score matPST = evalMaterialAndPST(pos);
    Score rookBonus = SCORE_ZERO;
    Score kingBonus = SCORE_ZERO;
    Score mopup     = SCORE_ZERO;
    const char* egDesc = "none";

    for (Color c : {WHITE, BLACK}) {
        add(rookBonus, rookActivity(pos, c), c);
        add(kingBonus, kingSafety(pos, c), c);
    }

    for (Color stronger : {WHITE, BLACK}) {
        if (!isBareKing(pos, ~stronger)) continue;
        if (isInsufficientMaterial(pos, stronger)) {
            egDesc = "draw (insufficient material)";
        } else {
            // mopupBonus returns a positive Score; apply correct sign for the report.
            const Score raw = mopupBonus(pos, stronger);
            mopup = (stronger == WHITE)
                    ? raw
                    : static_cast<Score>(-static_cast<int>(raw));
            egDesc = (stronger == WHITE) ? "win (white)" : "win (black)";
        }
        break;
    }

    const int phase = computePhase(pos);
    Score total = static_cast<Score>(
        static_cast<int>(matPST)   +
        static_cast<int>(rookBonus) +
        static_cast<int>(kingBonus) +
        static_cast<int>(mopup)
    );
    if (egDesc == std::string("draw (insufficient material)"))
        total = calcScore(mgValue(total) / 8, egValue(total) / 8);

    const Value blended = (mgValue(total) * phase + egValue(total) * (MaxPhase - phase)) / MaxPhase;
    const Value fromStm = (pos.stm() == WHITE) ? blended : -blended;

    std::ostringstream ss;
    ss << "makrukeval\n";
    ss << "  phase          : " << phase << " / " << MaxPhase << "\n";
    ss << "  endgame        : " << egDesc << "\n";
    ss << "  material+pst   : mg=" << mgValue(matPST)    << " eg=" << egValue(matPST)    << "\n";
    ss << "  rook activity  : mg=" << mgValue(rookBonus)  << " eg=" << egValue(rookBonus)  << "\n";
    ss << "  king safety    : mg=" << mgValue(kingBonus)  << " eg=" << egValue(kingBonus)  << "\n";
    ss << "  mop-up         : mg=" << mgValue(mopup)      << " eg=" << egValue(mopup)      << "\n";
    ss << "  total          : mg=" << mgValue(total)      << " eg=" << egValue(total)      << "\n";
    ss << "  blended        : " << blended << " cp (white)\n";
    ss << "  from stm       : " << fromStm << " cp\n";
    return ss.str();
}

} // namespace Nebula
