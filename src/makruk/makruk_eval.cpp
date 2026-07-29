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

// Pawn: central placement and advancement toward promotion (rank 4 = one step away).
// White pawns start rank 2, promote on rank 5. Ranks 0–1 and 5–7 are zero.
// Expanded from half-board [rank][file a–d] with files e–h mirroring d–a.
constexpr Score PawnPST[64] = {
    // rank 0 — impossible for pawn
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 1 — impossible
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 2 — starting rank
    S(-18, -4), S( -2, -5), S(  9,  5), S( 14,  4), S( 14,  4), S(  9,  5), S( -2, -5), S(-18, -4),
    // rank 3 — one advance
    S(-17,  3), S( 10,  3), S( 15, -8), S( 20, -3), S( 20, -3), S( 15, -8), S( 10,  3), S(-17,  3),
    // rank 4 — one step from promotion
    S( -6,  8), S(  1,  9), S(  8,  7), S(  9, -6), S(  9, -6), S(  8,  7), S(  1,  9), S( -6,  8),
    // rank 5 — promotion rank (pawn becomes Met; PST not used here)
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 6 — impossible
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
    // rank 7 — impossible
    S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0), S(  0,  0),
};

// Knight (Ma): centralization and forward advancement bonus.
// Expanded from half-board [rank][file a–d] with files e–h mirroring d–a.
constexpr Score KnightPST[64] = {
    S(-175,-96), S( -92,-65), S( -74,-49), S( -73,-21), S( -73,-21), S( -74,-49), S( -92,-65), S(-175,-96),
    S( -77,-67), S( -41,-54), S( -27,-18), S(   0,  8), S(   0,  8), S( -27,-18), S( -41,-54), S( -77,-67),
    S( -61,-40), S(  -2,-27), S(   0, -8), S(  12, 29), S(  12, 29), S(   0, -8), S(  -2,-27), S( -61,-40),
    S( -35,-35), S(   8, -2), S(  40, 13), S(  49, 28), S(  49, 28), S(  40, 13), S(   8, -2), S( -35,-35),
    S( -34,-45), S(  13,-16), S(  44,  9), S(  51, 39), S(  51, 39), S(  44,  9), S(  13,-16), S( -34,-45),
    S(  -9,-51), S(  22,-44), S(  58,-16), S(  53, 17), S(  53, 17), S(  58,-16), S(  22,-44), S(  -9,-51),
    S( -67,-69), S( -27,-50), S(   4,-51), S(  37, 12), S(  37, 12), S(   4,-51), S( -27,-50), S( -67,-69),
    S(-201,-100), S( -83,-88), S( -56,-56), S( -26,-17), S( -26,-17), S( -56,-56), S( -83,-88), S(-201,-100),
};

// Khon (BISHOP slot): short-range step piece; forward reach and centralization.
// Expanded from half-board [rank][file a–d] with files e–h mirroring d–a.
constexpr Score KhonPST[64] = {
    S(-175,-96), S( -92,-65), S( -74,-49), S( -73,-21), S( -73,-21), S( -74,-49), S( -92,-65), S(-175,-96),
    S( -37,-67), S( -21,-54), S(  -1,-18), S(  -1,  8), S(  -1,  8), S(  -1,-18), S( -21,-54), S( -37,-67),
    S(  -3,-40), S(   0,-27), S( 151, -8), S( 157, 29), S( 157, 29), S( 151, -8), S(   0,-27), S(  -3,-40),
    S(  -1,-35), S(  68, -2), S( 170, 13), S( 179, 28), S( 179, 28), S( 170, 13), S(  68, -2), S(  -1,-35),
    S(  -2,-45), S(  73,-16), S( 174,  9), S( 181, 39), S( 181, 39), S( 174,  9), S(  73,-16), S(  -2,-45),
    S(  -1,-51), S(  82,-44), S( 183,-16), S( 188, 17), S( 188, 17), S( 183,-16), S(  82,-44), S(  -1,-51),
    S( -67,-69), S( -27,-50), S(   4,-51), S(  37, 12), S(  37, 12), S(   4,-51), S( -27,-50), S( -67,-69),
    S(-201,-100), S( -83,-88), S( -56,-56), S( -26,-17), S( -26,-17), S( -56,-56), S( -83,-88), S(-201,-100),
};

// Met (QUEEN slot): diagonal-only step piece; central coverage is critical.
// Expanded from half-board [rank][file a–d] with files e–h mirroring d–a.
constexpr Score MetPST[64] = {
    S(-175,-96), S( -92,-65), S( -74,-49), S( -73,-21), S( -73,-21), S( -74,-49), S( -92,-65), S(-175,-96),
    S( -77,-67), S( -41,-54), S( -27,-18), S( -15,  8), S( -15,  8), S( -27,-18), S( -41,-54), S( -77,-67),
    S( -61,-40), S( -17,-27), S( 151, -8), S( 257, 29), S( 257, 29), S( 151, -8), S( -17,-27), S( -61,-40),
    S( -15,-35), S(  68, -2), S( 257, 13), S( 273, 28), S( 273, 28), S( 257, 13), S(  68, -2), S( -15,-35),
    S( -14,-45), S(  73,-16), S( 261,  9), S( 287, 39), S( 287, 39), S( 261,  9), S(  73,-16), S( -14,-45),
    S(  -9,-51), S(  82,-44), S( 267,-16), S( 290, 17), S( 290, 17), S( 267,-16), S(  82,-44), S(  -9,-51),
    S( -67,-69), S( -27,-50), S(   4,-51), S(  37, 12), S(  37, 12), S(   4,-51), S( -27,-50), S( -67,-69),
    S(-201,-100), S( -83,-88), S( -56,-56), S( -26,-17), S( -26,-17), S( -56,-56), S( -83,-88), S(-201,-100),
};

// Rook (Ruea): file activity; 7th-rank aggression.
// Open-file bonus is handled separately in rookActivity().
// Expanded from half-board [rank][file a–d] with files e–h mirroring d–a.
constexpr Score RookPST[64] = {
    S( -31, -9), S( -20,-13), S( -14,-10), S(  -5, -9), S(  -5, -9), S( -14,-10), S( -20,-13), S( -31, -9),
    S( -21,-12), S( -13, -9), S(  -8, -1), S(   6, -2), S(   6, -2), S(  -8, -1), S( -13, -9), S( -21,-12),
    S( -25,  6), S( -11, -8), S(  -1, -2), S(   3, -6), S(   3, -6), S(  -1, -2), S( -11, -8), S( -25,  6),
    S( -13, -6), S(  -5,  1), S(  -4, -9), S(  -6,  7), S(  -6,  7), S(  -4, -9), S(  -5,  1), S( -13, -6),
    S( -27, -5), S( -15,  8), S(  -4,  7), S(   3, -6), S(   3, -6), S(  -4,  7), S( -15,  8), S( -27, -5),
    S( -22,  6), S(  -2,  1), S(   6, -7), S(  12, 10), S(  12, 10), S(   6, -7), S(  -2,  1), S( -22,  6),
    S(  -2,  4), S(  12,  5), S(  16, 20), S(  18, -5), S(  18, -5), S(  16, 20), S(  12,  5), S(  -2,  4),
    S( -17, 18), S( -19,  0), S(  -1, 19), S(   9, 13), S(   9, 13), S(  -1, 19), S( -19,  0), S( -17, 18),
};

// King: MG = safety near home rank; EG = activity toward center.
// Expanded from half-board [rank][file a–d] with files e–h mirroring d–a.
constexpr Score KingPST[64] = {
    S(   0,  1), S(   0, 45), S(  32, 85), S( 285, 76), S( 285, 76), S(  32, 85), S(   0, 45), S(   0,  1),
    S(  91, 53), S( 158,100), S( 120,133), S( 152,135), S( 152,135), S( 120,133), S( 158,100), S(  91, 53),
    S(  99, 88), S( 126,130), S(  84,169), S(  60,175), S(  60,175), S(  84,169), S( 126,130), S(  99, 88),
    S(  84,103), S(  95,156), S(  68,172), S(  54,172), S(  54,172), S(  68,172), S(  95,156), S(  84,103),
    S(  72, 96), S(  88,166), S(  56,199), S(  34,199), S(  34,199), S(  56,199), S(  88,166), S(  72, 96),
    S(  61, 92), S(  79,172), S(  42,184), S(  18,191), S(  18,191), S(  42,184), S(  79,172), S(  61, 92),
    S(  43, 47), S(  60,121), S(  32,116), S(  12,131), S(  12,131), S(  32,116), S(  60,121), S(  43, 47),
    S(   0, 11), S(  44, 59), S(  24, 73), S(  10, 78), S(  10, 78), S(  24, 73), S(  44, 59), S(   0, 11),
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

// Returns true for light squares (file + rank is even).
static bool isLightSquare(Square s) {
    return ((int)fileOf(s) + (int)rankOf(s)) % 2 == 0;
}

// Returns true if all Mets (QUEEN slot) of color c are on the same square color.
static bool metsAllSameColor(const Position& pos, Color c) {
    uint64_t bb = pos.pieces(c, QUEEN);
    if (!bb) return true;
    const bool firstLight = isLightSquare(lsb(bb));
    while (bb) {
        if (isLightSquare(popLsb(bb)) != firstLight) return false;
    }
    return true;
}

// Returns true when the weak king is at a Khon back corner where mate is impossible.
// White Khon back corners (king has only one checking square): a1, h1.
// Black Khon back corners: a8, h8.
static bool isKBKBackCorner(const Position& pos, Color stronger) {
    const Square wksq = pos.square<KING>(~stronger);
    return (stronger == WHITE) ? (wksq == SQ_A1 || wksq == SQ_H1)
                               : (wksq == SQ_A8 || wksq == SQ_H8);
}

// Returns the nearest Khon front corner (where checkmate is achievable).
// White Khon front corners: a8, h8.  Black Khon front corners: a1, h1.
static Square nearestKhonFrontCorner(Square sq, Color khonColor) {
    const Square c1 = (khonColor == WHITE) ? SQ_A8 : SQ_A1;
    const Square c2 = (khonColor == WHITE) ? SQ_H8 : SQ_H1;
    return (chebyshev(sq, c1) <= chebyshev(sq, c2)) ? c1 : c2;
}

// Returns true when the stronger side cannot force checkmate against a bare king.
// Note: single Khon (KBK) is excluded — it CAN mate at front corners.
static bool isInsufficientMaterial(const Position& pos, Color stronger) {
    const int r  = pos.count<ROOK>(stronger);
    const int p  = pos.count<PAWN>(stronger);
    if (r > 0 || p > 0) return false;          // rook or pawn can force win

    const int n  = pos.count<KNIGHT>(stronger);
    const int kh = pos.count<BISHOP>(stronger);  // Khon
    const int q  = pos.count<QUEEN>(stronger);   // Met

    // Lone knight — cannot force checkmate
    if (n == 1 && kh == 0 && q == 0) return true;
    // Single Met — diagonal-only, cannot force checkmate
    if (n == 0 && kh == 0 && q == 1) return true;
    // Two knights — counting rule also constrains; draw
    if (n == 2 && kh == 0 && q == 0) return true;
    // Multiple Mets all on same square color — cannot cover all corners
    if (n == 0 && kh == 0 && q >= 2 && metsAllSameColor(pos, stronger)) return true;
    // Single Khon (KBK) is position-dependent — handled separately.
    return false;
}

// Returns true when we have exactly K+Khon vs bare K.
// Caller must already have confirmed isBareKing(~stronger).
static bool isKBK(const Position& pos, Color stronger) {
    return pos.count<BISHOP>(stronger) == 1
        && pos.count<KNIGHT>(stronger) == 0
        && pos.count<ROOK>(stronger)   == 0
        && pos.count<QUEEN>(stronger)  == 0
        && pos.count<PAWN>(stronger)   == 0;
}

// Mop-up for KBK: guide weak king toward the nearest Khon front corner,
// and close the strong king to assist.
static Score kbkMopup(const Position& pos, Color stronger) {
    const Square wKsq  = pos.square<KING>(~stronger);
    const Square sKsq  = pos.square<KING>(stronger);
    const Square target = nearestKhonFrontCorner(wKsq, stronger);
    const int cornerBonus = (7 - chebyshev(wKsq, target)) * 25;
    const int closeBonus  = (7 - chebyshev(sKsq, wKsq))  * 10;
    return S(cornerBonus + closeBonus, cornerBonus + closeBonus);
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
            score = calcScore(mgValue(score) / 8, egValue(score) / 8);
        } else if (isKBK(pos, stronger)) {
            if (isKBKBackCorner(pos, stronger)) {
                // Weak king hides in a back corner the Khon cannot cover — draw.
                score = calcScore(mgValue(score) / 8, egValue(score) / 8);
            } else {
                add(score, kbkMopup(pos, stronger), stronger);
            }
        } else {
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
        } else if (isKBK(pos, stronger)) {
            if (isKBKBackCorner(pos, stronger)) {
                egDesc = "draw (KBK back corner)";
            } else {
                const Score raw = kbkMopup(pos, stronger);
                mopup = (stronger == WHITE) ? raw : static_cast<Score>(-static_cast<int>(raw));
                egDesc = (stronger == WHITE) ? "KBK win (white)" : "KBK win (black)";
            }
        } else {
            const Score raw = mopupBonus(pos, stronger);
            mopup = (stronger == WHITE) ? raw : static_cast<Score>(-static_cast<int>(raw));
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
