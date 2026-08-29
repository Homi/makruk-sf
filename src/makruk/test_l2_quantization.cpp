// VERSION3 (MKN3, int16-quantized L2) correctness tests.
//
// Unlike test_nnue_incremental.cpp's bit-exact checks (valid there because
// int32 accumulation is exact/associative), int16 L2 quantization is
// deliberately lossy -- there is no "bit-exact" reference to check against.
// Instead this builds a small synthetic net with random-but-fixed weights,
// exports it in BOTH VERSION2 (float32 L2) and VERSION3 (int16 L2, quantized
// from the exact same float32 weights, mirroring export_int16.py's
// --quantize-l2 calibration) forms, and asserts the two evaluators' outputs
// stay within a small, empirically-justified centipawn tolerance across many
// positions -- the "fast-vs-slow-reference" methodology this project uses
// elsewhere (test_see.cpp, test_nnue_incremental.cpp), adapted for a lossy
// rather than exact comparison.
//
// Also verifies the load()-time overflow safety guard (mknn_evaluator.h)
// actually rejects a net whose quantized L2 weights could overflow the int32
// accumulator, following the same "verify the test has teeth" precedent as
// Round 19/20's SEE fix (test_see.cpp's header comment).
//
// IMPORTANT: build_tests.sh compiles with -DNDEBUG, which strips assert()
// entirely -- every check here is an explicit runtime comparison that prints
// and calls std::exit(1) on failure, not assert().
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "../position.h"
#include "../movegen.h"
#include "../nnue/mknn_evaluator.h"
#include "../nnue/features/half_ka_v2_makruk.h"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace Nebula {
namespace {

int g_fail = 0;
int g_checks = 0;
int g_maxAbsDiff = 0;

// Must match mknn_evaluator.h's L2InputScale / load()'s overflow-check budget
// exactly, and export_int16.py's L2_INPUT_SCALE / L2_ACC_BUDGET -- this test
// intentionally re-derives the same constants independently (not #include-ing
// mknn_evaluator.h's private ones) so a future accidental drift between the
// production formula and this test's synthetic-net construction shows up as
// a test failure rather than the test silently tracking whatever the
// production code does.
constexpr int L2InputScaleRef = 127;
constexpr int64_t L2AccBudgetRef = int64_t(1) << 30;

void appendU32(std::string& s, uint32_t v) {
    s.append(reinterpret_cast<const char*>(&v), sizeof(v));
}
void appendI16(std::string& s, int16_t v) {
    s.append(reinterpret_cast<const char*>(&v), sizeof(v));
}
void appendF32(std::string& s, float v) {
    s.append(reinterpret_cast<const char*>(&v), sizeof(v));
}

struct SyntheticNet {
    int L1, L2, L3;
    std::vector<int16_t> ftBias, ftWeight;   // [L1], [dims*L1]
    std::vector<float>   l2Bias, l2Weight;   // [L2], [L2*2*L1]
    std::vector<float>   l3Bias, l3Weight;   // [L3], [L3*L2]
    float                outBias;
    std::vector<float>   outWeight;          // [L3]
};

SyntheticNet makeSyntheticNet(int L1, int L2, int L3, uint32_t seed, float l2WeightAbsMax) {
    const int dims = int(Eval::Nnue::Features::HalfKAv2Makruk::Dimensions);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> ftW(-1.0f, 1.0f);
    std::uniform_real_distribution<float> l2W(-l2WeightAbsMax, l2WeightAbsMax);
    std::uniform_real_distribution<float> smallW(-0.5f, 0.5f);

    SyntheticNet net;
    net.L1 = L1; net.L2 = L2; net.L3 = L3;

    // FT: int16 directly, small magnitude (headroom against int32 overflow
    // over MaxActiveDimensions=32 active features is not this test's concern).
    net.ftBias.resize(L1);
    for (auto& x : net.ftBias) x = int16_t(std::lround(ftW(rng) * 200));
    net.ftWeight.resize(size_t(dims) * L1);
    for (auto& x : net.ftWeight) x = int16_t(std::lround(ftW(rng) * 200));

    net.l2Bias.resize(L2);
    for (auto& x : net.l2Bias) x = smallW(rng);
    net.l2Weight.resize(size_t(L2) * 2 * L1);
    for (auto& x : net.l2Weight) x = l2W(rng);

    net.l3Bias.resize(L3);
    for (auto& x : net.l3Bias) x = smallW(rng);
    net.l3Weight.resize(size_t(L3) * L2);
    for (auto& x : net.l3Weight) x = smallW(rng);

    net.outBias = smallW(rng);
    net.outWeight.resize(L3);
    for (auto& x : net.outWeight) x = smallW(rng);

    return net;
}

// Mirrors export_int16.py's compute_l2_scale(): calibrate the largest int16
// scale that keeps the worst-case L2 accumulator sum within budget.
int computeL2Scale(const std::vector<float>& l2Weight, int L1) {
    float absmax = 0.0f;
    for (float w : l2Weight) absmax = std::max(absmax, std::abs(w));
    if (absmax <= 0.0f) return 1;
    const double raw = double(L2AccBudgetRef) / (2.0 * L1 * L2InputScaleRef * absmax);
    int scale = int(raw);
    return std::max(1, std::min(scale, 32767));
}

std::string encodeVersion2(const SyntheticNet& net) {
    const int dims = int(Eval::Nnue::Features::HalfKAv2Makruk::Dimensions);
    std::string s;
    appendU32(s, Eval::Nnue::MknnEvaluator::MAGIC);
    appendU32(s, Eval::Nnue::MknnEvaluator::VERSION2);
    appendU32(s, uint32_t(dims));
    appendU32(s, uint32_t(net.L1));
    appendU32(s, uint32_t(net.L2));
    appendU32(s, uint32_t(net.L3));
    appendU32(s, 1024);  // ft_scale (unused by this test's callers, just needs to be >=1)
    for (auto v : net.ftBias)   appendI16(s, v);
    for (auto v : net.ftWeight) appendI16(s, v);
    for (auto v : net.l2Bias)   appendF32(s, v);
    for (auto v : net.l2Weight) appendF32(s, v);
    for (auto v : net.l3Bias)   appendF32(s, v);
    for (auto v : net.l3Weight) appendF32(s, v);
    appendF32(s, net.outBias);
    for (auto v : net.outWeight) appendF32(s, v);
    return s;
}

// quantScaleOverride: pass 0 to auto-calibrate (computeL2Scale); pass a
// specific value to force an (optionally unsafe) scale for the
// overflow-rejection test.
std::string encodeVersion3(const SyntheticNet& net, int quantScaleOverride = 0) {
    const int dims = int(Eval::Nnue::Features::HalfKAv2Makruk::Dimensions);
    const int l2Scale = quantScaleOverride > 0 ? quantScaleOverride
                                                : computeL2Scale(net.l2Weight, net.L1);
    std::string s;
    appendU32(s, Eval::Nnue::MknnEvaluator::MAGIC);
    appendU32(s, Eval::Nnue::MknnEvaluator::VERSION3);
    appendU32(s, uint32_t(dims));
    appendU32(s, uint32_t(net.L1));
    appendU32(s, uint32_t(net.L2));
    appendU32(s, uint32_t(net.L3));
    appendU32(s, 1024);           // ft_scale
    appendU32(s, uint32_t(l2Scale));
    for (auto v : net.ftBias)   appendI16(s, v);
    for (auto v : net.ftWeight) appendI16(s, v);
    for (auto v : net.l2Bias)   appendF32(s, v);
    for (float v : net.l2Weight) {
        long q = std::lround(double(v) * l2Scale);
        q = std::max<long>(-32768, std::min<long>(32767, q));
        appendI16(s, int16_t(q));
    }
    for (auto v : net.l3Bias)   appendF32(s, v);
    for (auto v : net.l3Weight) appendF32(s, v);
    appendF32(s, net.outBias);
    for (auto v : net.outWeight) appendF32(s, v);
    return s;
}

void setPos(Position& pos, StateListPtr& states, const std::string& fen) {
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, false, &states->back(), nullptr);
}

void checkClose(Eval::Nnue::MknnEvaluator& evF32, Eval::Nnue::MknnEvaluator& evI16,
                 Position& pos, const std::string& tag, int toleranceCp) {
    ++g_checks;
    const int vF32 = int(evF32.evaluate(pos));
    const int vI16 = int(evI16.evaluate(pos));
    const int diff = std::abs(vF32 - vI16);
    g_maxAbsDiff = std::max(g_maxAbsDiff, diff);
    if (diff > toleranceCp) {
        std::cerr << "FAIL " << tag << ": float32=" << vF32 << " int16=" << vI16
                   << " diff=" << diff << " exceeds tolerance " << toleranceCp << "\n";
        ++g_fail;
    }
}

} // namespace

void runL2QuantizationTests() {
    // ---- 1. Positive control: a realistically-scaled synthetic net (L1=64,
    // L2=8, L3=8, weight magnitude ~matching the real deployed net's L2
    // absmax of ~1.4) loads correctly in both formats and stays close. ----
    const SyntheticNet net = makeSyntheticNet(/*L1=*/64, /*L2=*/8, /*L3=*/8,
                                               /*seed=*/0x5EED1u, /*l2WeightAbsMax=*/1.4f);
    const std::string v2Bytes = encodeVersion2(net);
    const std::string v3Bytes = encodeVersion3(net);

    Eval::Nnue::MknnEvaluator evF32, evI16;
    {
        std::istringstream is2(v2Bytes, std::ios::binary);
        std::istringstream is3(v3Bytes, std::ios::binary);
        if (!evF32.load(is2)) { std::cerr << "FAIL: synthetic VERSION2 net failed to load\n"; std::exit(1); }
        if (!evI16.load(is3)) { std::cerr << "FAIL: synthetic VERSION3 net failed to load\n"; std::exit(1); }
    }
    if (evF32.version() != 2) { std::cerr << "FAIL: expected VERSION2, got " << evF32.version() << "\n"; std::exit(1); }
    if (evI16.version() != 3) { std::cerr << "FAIL: expected VERSION3, got " << evI16.version() << "\n"; std::exit(1); }

    // Tolerance: this synthetic net's small L2=8 (vs the real deployed net's
    // L2=32) has less downstream averaging to smooth out per-neuron
    // quantization noise, so its worst-case drift runs meaningfully higher
    // than the real net's (measured separately, directly on the real
    // deployed weights, at single-digit cp -- see CLAUDE.md's Round-N
    // writeup). Empirically this synthetic net's random-walk max drift is
    // ~55cp; 120cp gives ~2x headroom while still catching a genuinely
    // broken quantization path (which produced errors in the hundreds to
    // thousands of cp during development, e.g. a wrong dequantization
    // divisor or a transposed weight index).
    constexpr int ToleranceCp = 120;

    const std::vector<std::string> fens = {
        "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1",
        "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR b - - 0 1",
        "r2mksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1",
        "8/8/8/3k4/8/3K4/8/4R3 w - - 0 1",
        "8/8/2q5/3k4/8/3K4/8/8 b - - 0 1",
        "r1s1k1sr/2n2n2/p1p2p1p/1p2p1p1/1P2P1P1/P1P2P1P/2N2N2/R1S1K1SR w - - 4 12",
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
    };
    for (const auto& fen : fens) {
        StateListPtr st;
        Position pos;
        setPos(pos, st, fen);
        checkClose(evF32, evI16, pos, "fen:" + fen, ToleranceCp);
    }

    // ---- 2. Random walk: broad coverage, checked every ply. Fixed seed. ----
    {
        StateListPtr st;
        Position pos;
        setPos(pos, st, "rnsmksnr/8/pppppppp/8/8/PPPPPPPP/8/RNSKMSNR w - - 0 1");
        checkClose(evF32, evI16, pos, "random-walk-start", ToleranceCp);

        std::mt19937 rng(0xC0FFEEu);
        int plies = 0;
        for (; plies < 300; ++plies) {
            MoveList<LEGAL> moves(pos);
            if (moves.size() == 0) break;
            std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
            const Move m = moves.begin()[pick(rng)];
            st->emplace_back();
            pos.doMove(m, st->back());
            checkClose(evF32, evI16, pos, "random-walk-ply-" + std::to_string(plies), ToleranceCp);
        }
        std::cout << "  L2 quantization random walk: " << plies << " plies, "
                  << g_checks << " total closeness checks so far, max abs diff so far "
                  << g_maxAbsDiff << "cp\n";
    }

    // ---- 3. Overflow-safety guard has teeth: a net with L1 large enough
    // that maxed-out int16 L2 weights would overflow the int32 accumulator
    // must be REJECTED by load(), not silently accepted with corrupted eval.
    // L1=200: 2*200*127*32767 ~= 1.66e9 > the 2^30 budget. ----
    {
        const SyntheticNet bigNet = makeSyntheticNet(/*L1=*/200, /*L2=*/4, /*L3=*/4,
                                                       /*seed=*/0xBADu, /*l2WeightAbsMax=*/1.0f);
        // Force every quantized weight to +-32767 by encoding with an
        // enormous override scale (the encoder clamps to int16 range).
        const std::string unsafeBytes = encodeVersion3(bigNet, /*quantScaleOverride=*/1000000);
        Eval::Nnue::MknnEvaluator evUnsafe;
        std::istringstream isUnsafe(unsafeBytes, std::ios::binary);
        const bool loaded = evUnsafe.load(isUnsafe);
        if (loaded) {
            std::cerr << "FAIL: overflow-unsafe VERSION3 net was accepted by load() "
                         "(expected rejection)\n";
            ++g_fail;
        } else {
            std::cout << "  overflow-safety guard correctly rejected an unsafe net\n";
        }

        // Positive control for the guard itself: the SAME net auto-calibrated
        // (quantScaleOverride=0) must load fine -- proves the rejection above
        // is really about the scale being unsafe, not something else wrong
        // with this synthetic net's shape.
        const std::string safeBytes = encodeVersion3(bigNet);
        Eval::Nnue::MknnEvaluator evSafe;
        std::istringstream isSafe(safeBytes, std::ios::binary);
        if (!evSafe.load(isSafe)) {
            std::cerr << "FAIL: auto-calibrated VERSION3 net (same shape, safe scale) "
                         "was unexpectedly rejected by load()\n";
            ++g_fail;
        }
    }

    if (g_fail) {
        std::cerr << g_fail << " L2 quantization check(s) FAILED (out of "
                  << g_checks << " closeness checks)\n";
        std::exit(1);
    }
    std::cout << "PASS: L2 int16 quantization (" << g_checks
              << " closeness checks, max abs diff " << g_maxAbsDiff
              << "cp within " << ToleranceCp << "cp tolerance; overflow guard verified)\n";
}

} // namespace Nebula
