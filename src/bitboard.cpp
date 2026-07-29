#include <algorithm>
#include <bitset>
#include "bitboard.h"
#include "misc.h"

namespace Nebula{
  uint8_t popCnt16[1<<16];
  uint8_t squareDistance[SQUARE_NB][SQUARE_NB];
  uint64_t squareBb[SQUARE_NB];
  uint64_t lineBb[SQUARE_NB][SQUARE_NB];
  uint64_t betweenBb[SQUARE_NB][SQUARE_NB];
  uint64_t pseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];
  uint64_t pawnAttacks[COLOR_NB][SQUARE_NB];
  uint64_t khonAttacks[COLOR_NB][SQUARE_NB];
  Magic rookMagics[SQUARE_NB];
  Magic bishopMagics[SQUARE_NB];

  namespace{
    uint64_t rookTable[0x19000];
    uint64_t bishopTable[0x1480];
    void initMagics(PieceType pt, uint64_t table[], Magic magics[]);

    uint64_t safeDestination(const Square s, const int step){
      const auto to=static_cast<Square>(s+step);
      return isOk(to)&&distance(s,to)<=2?getSquareBb(to):static_cast<uint64_t>(0);
    }
  }

  void Bitboards::init(){
    for (unsigned i=0; i<1<<16; ++i)
      popCnt16[i]=static_cast<std::uint8_t>(
        static_cast<unsigned>(std::bitset<16>(i).count())
      );

    for (Square s=SQ_A1; s<=SQ_H8; ++s)
      squareBb[s]=1ULL<<s;
    for (Square s1=SQ_A1; s1<=SQ_H8; ++s1)
      for (Square s2=SQ_A1; s2<=SQ_H8; ++s2)
        squareDistance[s1][s2]=static_cast<std::uint8_t>(
          std::max(distance<File>(s1,s2),distance<Rank>(s1,s2))
        );

    initMagics(ROOK,rookTable,rookMagics);
    initMagics(BISHOP,bishopTable,bishopMagics); // kept; bishop magic unused in Makruk

    // Per-square step and pseudo-attack tables
    for (Square s=SQ_A1; s<=SQ_H8; ++s){
      pawnAttacks[WHITE][s]=pawnAttacksBb<WHITE>(getSquareBb(s));
      pawnAttacks[BLACK][s]=pawnAttacksBb<BLACK>(getSquareBb(s));

      for (const int step : {-9,-8,-7,-1,1,7,8,9})
        pseudoAttacks[KING][s]|=safeDestination(s,step);
      for (const int step : {-17,-15,-10,-6,6,10,15,17})
        pseudoAttacks[KNIGHT][s]|=safeDestination(s,step);

      // Rook: full sliding attacks (only slider in Makruk)
      pseudoAttacks[ROOK][s]=rookMagics[s].attacks[rookMagics[s].index(0)];

      // Met (QUEEN): one step diagonally in any of 4 directions
      pseudoAttacks[QUEEN][s]=safeDestination(s,NORTH_EAST)
                             |safeDestination(s,NORTH_WEST)
                             |safeDestination(s,SOUTH_EAST)
                             |safeDestination(s,SOUTH_WEST);

      // Khon (BISHOP): one step forward or diagonally — color-dependent
      khonAttacks[WHITE][s]=safeDestination(s,NORTH)
                           |safeDestination(s,NORTH_EAST)
                           |safeDestination(s,NORTH_WEST)
                           |safeDestination(s,SOUTH_EAST)
                           |safeDestination(s,SOUTH_WEST);
      khonAttacks[BLACK][s]=safeDestination(s,SOUTH)
                           |safeDestination(s,NORTH_EAST)
                           |safeDestination(s,NORTH_WEST)
                           |safeDestination(s,SOUTH_EAST)
                           |safeDestination(s,SOUTH_WEST);
      // Union stored in pseudoAttacks[BISHOP] for use where color is unknown
      pseudoAttacks[BISHOP][s]=khonAttacks[WHITE][s]|khonAttacks[BLACK][s];
    }

    // lineBb and betweenBb: Rook only (only slider in Makruk)
    for (Square s1=SQ_A1; s1<=SQ_H8; ++s1)
      for (Square s2=SQ_A1; s2<=SQ_H8; ++s2){
        if (pseudoAttacks[ROOK][s1]&s2){
          lineBb[s1][s2]=(pseudoAttacks[ROOK][s1]&pseudoAttacks[ROOK][s2])|s1|s2;
          betweenBb[s1][s2]=attacksBb<ROOK>(s1,getSquareBb(s2))&attacksBb<ROOK>(s2,getSquareBb(s1));
        }
        betweenBb[s1][s2]|=s2;
      }
  }

  namespace{
    uint64_t slidingAttack(const PieceType pt, const Square sq, const uint64_t occupied){
      uint64_t attacks=0;
      Direction rookDirections[4]={NORTH,SOUTH,EAST,WEST};
      for (Direction bishopDirections[4]={NORTH_EAST,SOUTH_EAST,SOUTH_WEST,NORTH_WEST}; const Direction d
           : pt==ROOK?rookDirections:bishopDirections){
        Square s=sq;
        while (safeDestination(s,d)&&!(occupied&s))
          attacks|=s+=d;
      }
      return attacks;
    }

    void initMagics(const PieceType pt, uint64_t table[], Magic magics[]){
      static uint64_t occupancy[4096];
      static uint64_t reference[4096];
      static int epoch[4096];
      int cnt=0, size=0;
      for (Square s=SQ_A1; s<=SQ_H8; ++s){
        const int seeds[][RANK_NB]={
          {8977,44560,54343,38998,5731,95205,104912,17020},
          {728,10316,55013,32803,12281,15100,16645,255}
        };
        const uint64_t edges=((rank1Bb|rank8Bb)&~rankBb(s))|((fileAbb|fileHbb)&~fileBb(s));
        Magic& m=magics[s];
        m.mask=slidingAttack(pt,s,0)&~edges;
        m.shift=(is64Bit?64:32)-popcnt(m.mask);
        m.attacks=s==SQ_A1?table:magics[s-1].attacks+size;
        uint64_t b=size=0;
        do{
          occupancy[size]=b;
          reference[size]=slidingAttack(pt,s,b);
          if (hasPext)
            m.attacks[pext(b,m.mask)]=reference[size];
          size++;
          b=(b-m.mask)&m.mask;
        }
        while (b);
        if (hasPext)
          continue;
        Prng rng(seeds[is64Bit][rankOf(s)]);
        for (int i=0; i<size;){
          for (m.magic=0; popcnt((m.magic*m.mask)>>56)<6;)
            m.magic=rng.sparseRand<uint64_t>();
          for (++cnt,i=0; i<size; ++i){
            if (const unsigned idx=m.index(occupancy[i]); epoch[idx]<cnt){
              epoch[idx]=cnt;
              m.attacks[idx]=reference[i];
            }
            else if (m.attacks[idx]!=reference[i])
              break;
          }
        }
      }
    }
  }
}
