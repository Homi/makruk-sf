#include <algorithm>
#include <fstream>
#include <iomanip>
#include <streambuf>
#include <vector>
#include "evaluate.h"
#include "misc.h"
#include "thread.h"
#include "incbin/incbin.h"
#include "makruk/makruk_eval.h"
#if !defined(_MSC_VER) && !defined(NNUE_EMBEDDING_OFF)
INCBIN(EmbeddedNNUE,NnueNetDefaultName);
#else
constexpr unsigned char gEmbeddedNNUEData[1]={};
const unsigned char* const gEmbeddedNNUEEnd=&gEmbeddedNNUEData[1];
constexpr unsigned int gEmbeddedNNUESize=1;
#endif
using namespace std;

namespace Nebula{
  namespace Eval{
    string currentNnueNetName;

    void Nnue::init(){
      const string evalFile=NnueNetDefaultName;
      for (const vector<string> dirs={"<internal>","",CommandLine::binaryDirectory}; const string& directory : dirs)
        if (currentNnueNetName!=evalFile){
          if (directory!="<internal>"){
            if (ifstream stream(directory+evalFile,ios::binary); loadEval(evalFile,stream))
              currentNnueNetName=evalFile;
          }
          if (directory=="<internal>"&&evalFile==NnueNetDefaultName){
            class MemoryBuffer : public basic_streambuf<char>{
            public:
              MemoryBuffer(char* p, const size_t n){
                setg(p,p,p+n);
                setp(p,p+n);
              }
            };
            MemoryBuffer buffer(const_cast<char*>(reinterpret_cast<const char*>(gEmbeddedNNUEData)),
              gEmbeddedNNUESize);
            (void)gEmbeddedNNUEEnd;
            if (istream stream(&buffer); loadEval(evalFile,stream))
              currentNnueNetName=evalFile;
          }
        }
    }
  }

  Value Eval::evaluate(const Position& pos, int* complexity){
    if (complexity)
      *complexity=0;
    const Value v=makrukClassicalEval(pos);
    return std::clamp(v,VALUE_TB_LOSS_IN_MAX_PLY+1,VALUE_TB_WIN_IN_MAX_PLY-1);
  }
}
