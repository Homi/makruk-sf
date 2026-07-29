#include <algorithm>
#include <fstream>
#include <iomanip>
#include <streambuf>
#include <vector>
#include "evaluate.h"
#include "misc.h"
#include "thread.h"
#include "nnue/mknn_evaluator.h"
#include "makruk/makruk_eval.h"
#include "position.h"
using namespace std;

// INCBIN embeds the default net binary into the executable at compile time.
// The symbols are in the global namespace.
#include "incbin/incbin.h"
#include "makruk/makruk_eval.h"
#if !defined(_MSC_VER) && !defined(NNUE_EMBEDDING_OFF)
#include "incbin/incbin.h"
INCBIN(EmbeddedNNUE, NnueNetDefaultName);
#else
constexpr unsigned char  gEmbeddedNNUEData[1]  = {};
const    unsigned char*  const gEmbeddedNNUEEnd = &gEmbeddedNNUEData[1];
constexpr unsigned int   gEmbeddedNNUESize      = 1;
#endif

namespace Nebula{
  namespace Eval{
    string currentNnueNetName;

    static Nnue::MknnEvaluator mknnEval;

    void Nnue::init(){
      const string evalFile = NnueNetDefaultName;
      if (mknnEval.isLoaded()) return;

#if !defined(NNUE_EMBEDDING_OFF)
      // 1. Try file on disk next to the binary or in the current directory.
      for (const string& dir : vector<string>{"", CommandLine::binaryDirectory}){
        if (mknnEval.isLoaded()) break;
        if (ifstream stream(dir + evalFile, ios::binary); stream){
          if (mknnEval.load(stream))
            currentNnueNetName = evalFile;
        }
      }

      // 2. Fall back to the binary embedded by INCBIN.
      if (!mknnEval.isLoaded() && gEmbeddedNNUESize > 1){
        class MemoryBuffer : public basic_streambuf<char>{
        public:
          MemoryBuffer(char* p, const size_t n){ setg(p,p,p+n); setp(p,p+n); }
        };
        MemoryBuffer buffer(
          const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)),
          gEmbeddedNNUESize);
        (void)gEmbeddedNNUEEnd;
        if (istream stream(&buffer); mknnEval.load(stream))
          currentNnueNetName = evalFile;
      }
#endif // !NNUE_EMBEDDING_OFF
    }
  }

  Value Eval::evaluate(const Position& pos, int* complexity){
    if (complexity)
      *complexity = 0;
    if (pos.state()->makrukCounting.active)
      return VALUE_DRAW;
    const Value classical = makrukClassicalEval(pos);
    if (Eval::mknnEval.isLoaded() && std::abs(int(classical)) < 300) {
      const Value nnue_pos  = Eval::mknnEval.evaluate(pos);
      const int   nnue_delta = std::clamp(int(nnue_pos) - int(classical), -100, 100);
      const int   blend      = int(classical) + nnue_delta;
      return Value(std::clamp(blend, int(VALUE_TB_LOSS_IN_MAX_PLY) + 1,
                                     int(VALUE_TB_WIN_IN_MAX_PLY)  - 1));
    }
    return std::clamp(classical, VALUE_TB_LOSS_IN_MAX_PLY + 1, VALUE_TB_WIN_IN_MAX_PLY - 1);
      *complexity=0;
    const Value v=currentNnueNetName.empty()
                  ?makrukClassicalEval(pos)
                  :Nnue::evaluate(pos,false,nullptr);
    return std::clamp(v,VALUE_TB_LOSS_IN_MAX_PLY+1,VALUE_TB_WIN_IN_MAX_PLY-1);
  }
}
