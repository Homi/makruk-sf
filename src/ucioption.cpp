#include <algorithm>
#include "misc.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "makruk/makruk_counting.h"
using std::string;

namespace Nebula{
  Uci::OptionsMap options;

  namespace Uci{
    namespace{
      void onHashSize(const Option& o){ tt.resize(std::max<size_t>(1,o.asSize())); }
      void onThreads(const Option& o){ threads.set(std::max<size_t>(1,o.asSize())); }
      // Toggle Makruk counting draw. Default on (real rules); off is for training
      // data generation only, so material-imbalance endgames are played out.
      void onUseCounting(const Option& o){
        gMakrukCountingMode = o.asBool() ? MakrukCountingMode::Fairy
                                         : MakrukCountingMode::Off;
      }
    }

    bool CaseInsensitiveLess::operator()(const string& s1, const string& s2) const{
      return std::ranges::lexicographical_compare(s1,s2,
        [](const char c1, const char c2){ return tolower(c1)<tolower(c2); });
    }

    void init(OptionsMap& o){
      constexpr int maxHashMb=is64Bit?33554432:2048;
      o["Threads"]<<Option(1,1,512,onThreads);
      o["Hash"]<<Option(16,1,maxHashMb,onHashSize);
      o["MultiPV"]<<Option(1,1,500);
      o["Ponder"]<<Option(false);
      o["UCI_Variant"]<<Option("makruk","makruk");
      o["UseCounting"]<<Option(true,onUseCounting);
    }

    std::ostream& operator<<(std::ostream& os, const OptionsMap& om){
      for (const auto& [name, o] : om){
        os<<"\noption name "<<name<<" type "<<o.type;

        if (o.type=="string"||o.type=="check")
          os<<" default "<<o.defaultValue;

        if (o.type=="combo")
          os<<" default "<<o.defaultValue<<" var "<<o.defaultValue;

        if (o.type=="spin")
          os<<" default "<<o.asInt()
            <<" min "<<o.min
            <<" max "<<o.max;
      }
      return os;
    }
  }
}
