#pragma once
#include <string>
#include <optional>
#include "types.h"

namespace Nebula{
  class Position;

  namespace Eval{
    Value evaluate(const Position& pos, int* complexity=nullptr);
    extern std::string currentNnueNetName;
#define NnueNetDefaultName   "3769833465.bin"

    namespace Nnue{
      Value evaluate(const Position& pos, bool adjusted=false, int* complexity=nullptr);
      void init();
      bool loadEval(const std::string& name, std::istream& stream);
      bool saveEval(std::ostream& stream);
      bool saveEval(const std::optional<std::string>& filename);
    }
  }
}
