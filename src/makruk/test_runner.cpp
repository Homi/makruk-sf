// Makruk test runner entry point.
// Build with the rest of the engine sources plus test_movegen.cpp and test_legality.cpp.
// See src/makruk/build_tests.sh for the exact compile command.

#include "../bitboard.h"
#include "../position.h"
#include "../uci.h"

namespace Nebula {
    void runMakrukMovegenTests();
    void runMakrukLegalityTests();
    void runMakrukCountingTests();
    void runMakrukEvalTests();
    void runSeeTests();
    void runMknnIncrementalTests();
}

int main() {
    Nebula::Bitboards::init();
    Nebula::Position::init();
    Nebula::runMakrukMovegenTests();
    Nebula::runMakrukLegalityTests();
    Nebula::runMakrukCountingTests();
    Nebula::runMakrukEvalTests();
    Nebula::runSeeTests();
    Nebula::runMknnIncrementalTests();
    return 0;
}
