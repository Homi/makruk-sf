// MKNN NNUE evaluator for Makruk-SF.
//
// Supports three binary formats:
//   VERSION 1 (MKN1): float32 FT weight [L1 × DIMS], float32 L2/L3/out
//   VERSION 2 (MKN2): int16  FT weight [DIMS × L1] (transposed), float32 L2/L3/out
//   VERSION 3 (MKN3): as MKN2, plus int16 L2 weight [L2 × 2*L1] (quantized at
//                      export time, scale stored in the header) -- L3/out stay
//                      float32 (too small -- 32×32 and 32×1 -- to be worth it).
//
// The int16 format (VERSION 2/3) gives cache-friendly column access during
// FT accumulation and enables SIMD auto-vectorisation of the inner loop.
//
// Architecture: sparse FT(22528→256) two-perspective → L2(512→32) → L3(32→32) → out(1)
// Output scale: logit_raw * 400 → centipawns (matches sigmoid(cp/400) training target).
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <iostream>
#include <vector>
#include "features/half_ka_v2_makruk.h"
#include "mknn_accumulator.h"
#include "../types.h"

namespace Nebula { class Position; }

namespace Nebula::Eval::Nnue {

// Compile-time caps for the L2/L3 "narrow bottleneck" layers, analogous to
// MknnMaxL1 (mknn_accumulator.h). Every net trained in this project so far
// uses L2=L3=32; these caps give generous headroom (4x) while still letting
// evaluate() use fixed-size stack buffers instead of heap allocation for the
// common case. A net exceeding any of MknnMaxL1/MknnMaxL2/MknnMaxL3 still
// loads and evaluates correctly via the original std::vector-based path
// (see MknnEvaluator::load()'s fastEvalOk_ flag) -- no capability is lost,
// only the allocation-avoidance optimization is skipped for that net.
inline constexpr int MknnMaxL2 = 128;
inline constexpr int MknnMaxL3 = 128;

// VERSION3's L2 input quantization scale: crelu output is bounded to [0, 1],
// quantized here to an unsigned 7-bit fixed point [0, 127]. A fixed constant
// (not stored per-net) since it's derived from crelu's architecture-fixed
// range, not a learned parameter needing calibration -- unlike the L2 weight
// scale, which does vary net to net and is stored in the file header.
inline constexpr int L2InputScale = 127;

class MknnEvaluator {
public:
    static constexpr uint32_t MAGIC    = 0x4D4B4E4Eu; // 'MKNN'
    static constexpr uint32_t VERSION1 = 1u;
    static constexpr uint32_t VERSION2 = 2u;
    static constexpr uint32_t VERSION3 = 3u;

    bool load(std::istream& stream) {
        auto read_u32 = [&]() -> uint32_t {
            uint32_t v = 0;
            stream.read(reinterpret_cast<char*>(&v), 4);
            return v;
        };
        auto read_f32 = [&](std::vector<float>& buf, int n) {
            buf.resize(n);
            stream.read(reinterpret_cast<char*>(buf.data()), n * sizeof(float));
        };
        auto read_i16 = [&](std::vector<int16_t>& buf, int n) {
            buf.resize(n);
            stream.read(reinterpret_cast<char*>(buf.data()), n * sizeof(int16_t));
        };

        if (!stream) return false;
        const uint32_t magic   = read_u32();
        const uint32_t version = read_u32();
        const uint32_t dims    = read_u32();
        const uint32_t l1      = read_u32();
        const uint32_t l2      = read_u32();
        const uint32_t l3      = read_u32();

        if (!stream)            return false;
        if (magic   != MAGIC)   return false;
        if (version != VERSION1 && version != VERSION2 && version != VERSION3) return false;
        if (dims    != Features::HalfKAv2Makruk::Dimensions) return false;
        if (l1 < 1 || l2 < 1 || l3 < 1) return false;

        L1_  = (int)l1;
        L2_  = (int)l2;
        L3_  = (int)l3;
        ver_ = version;

        if (version == VERSION3) {
            // MKN3: as MKN2 (int16 FT, transposed), plus int16 L2 weight with
            // a stored per-net scale. L3/out stay float32 -- too small to
            // matter (32×32 and 32×1 vs L2's 32×1024).
            ft_scale_ = (int)read_u32();
            l2_scale_ = (int)read_u32();
            if (!stream || ft_scale_ < 1 || l2_scale_ < 1) return false;

            // VERSION3 requires the fast (stack-buffer) path -- there is no
            // int16-L2 fallback for an oversized net, so reject up front
            // rather than silently falling back to behaviour that doesn't
            // exist. (Every net trained in this project so far fits.)
            if (L1_ > Eval::Nnue::MknnMaxL1 || L2_ > MknnMaxL2 || L3_ > MknnMaxL3) {
                std::cerr << "MKNN: VERSION3 net exceeds MknnMaxL1/L2/L3 caps "
                             "(no slow-path fallback for quantized L2); rejecting load"
                          << std::endl;
                return false;
            }

            read_i16(ft_bias_i16_,   L1_);
            read_i16(ft_weight_i16_, (int)dims * L1_);  // layout: [DIMS × L1]
            read_f32(l2_bias_,       L2_);
            read_i16(l2_weight_i16_, L2_ * 2 * L1_);
            read_f32(l3_bias_,       L3_);
            read_f32(l3_weight_,     L3_ * L2_);
            read_f32(out_bias_v_,    1);
            read_f32(out_weight_,    L3_);
            if (!stream) return false;

            // Overflow safety: the int32 L2 accumulator sums 2*L1_ terms of
            // (int16 weight) * (int16 input, capped at L2InputScale). Reject
            // the net if the worst-case sum (all weights at the observed
            // max magnitude, all inputs saturated) could overflow int32 --
            // this guards the *export-time* scale choice, not just this
            // loader, since a badly calibrated exporter could otherwise
            // produce a file that silently corrupts eval at runtime.
            int w_absmax = 0;  // int, not int16_t: abs(INT16_MIN) doesn't fit in int16_t
            for (const int16_t w : l2_weight_i16_)
                w_absmax = std::max(w_absmax, std::abs(int(w)));
            const int64_t worstCaseSum =
                static_cast<int64_t>(2 * L1_) * L2InputScale * w_absmax;
            if (worstCaseSum > (int64_t(1) << 30)) {  // 2x headroom below INT32_MAX
                std::cerr << "MKNN: VERSION3 L2 quantization scale unsafe "
                             "(worst-case accumulator sum " << worstCaseSum
                          << " exceeds safety margin); rejecting load" << std::endl;
                return false;
            }

            l2Quantized_ = true;
        } else if (version == VERSION2) {
            // MKN2: int16 FT with transposed weight layout, float32 L2/L3/out.
            ft_scale_ = (int)read_u32();
            if (!stream || ft_scale_ < 1) return false;

            read_i16(ft_bias_i16_,   L1_);
            read_i16(ft_weight_i16_, (int)dims * L1_);  // layout: [DIMS × L1]
            read_f32(l2_bias_,       L2_);
            read_f32(l2_weight_,     L2_ * 2 * L1_);
            read_f32(l3_bias_,       L3_);
            read_f32(l3_weight_,     L3_ * L2_);
            read_f32(out_bias_v_,    1);
            read_f32(out_weight_,    L3_);
        } else {
            // MKN1: float32 throughout, FT weight layout: [L1 × DIMS].
            read_f32(ft_bias_,    L1_);
            read_f32(ft_weight_,  L1_ * (int)dims);
            read_f32(l2_bias_,    L2_);
            read_f32(l2_weight_,  L2_ * 2 * L1_);
            read_f32(l3_bias_,    L3_);
            read_f32(l3_weight_,  L3_ * L2_);
            read_f32(out_bias_v_, 1);
            read_f32(out_weight_, L3_);
        }

        if (!stream) return false;
        loaded_ = true;

        // Incremental accumulator updates (see updateAccumulator()) are only
        // implemented for VERSION2/VERSION3 (int16 FT) and only up to
        // MknnMaxL1. VERSION1 and any oversized net keep the original
        // always-full-refresh path.
        incremental_ = ((ver_ == VERSION2 || ver_ == VERSION3) && L1_ <= Eval::Nnue::MknnMaxL1);
        if ((ver_ == VERSION2 || ver_ == VERSION3) && !incremental_)
            std::cerr << "MKNN: L1=" << L1_ << " exceeds MknnMaxL1="
                      << Eval::Nnue::MknnMaxL1
                      << "; incremental accumulator disabled (slow path)" << std::endl;

        // Whether evaluate() can use fixed-size stack buffers (no heap
        // allocation) for this net. Independent of incremental_: a VERSION1
        // net with small enough L1/L2/L3 still qualifies (via
        // accumulateF32Into()), it just can't use the incremental cache.
        // A net exceeding any cap falls back to the original always-correct
        // std::vector-based path below -- no capability lost, only the
        // allocation-avoidance optimization skipped. (VERSION3 already
        // rejected an oversized net above, so this is always true for it.)
        fastEvalOk_ = (L1_ <= Eval::Nnue::MknnMaxL1 && L2_ <= MknnMaxL2 && L3_ <= MknnMaxL3);

        return true;
    }

    bool isLoaded()     const { return loaded_; }
    int  version()      const { return (int)ver_; }
    int  ftScale()      const { return ft_scale_; }
    bool incremental()  const { return incremental_; }
    int  l1()           const { return L1_; }

    // Returns evaluation from the side-to-move perspective, in centipawns.
    Value evaluate(const Nebula::Position& pos) const;

    // Test-only: from-scratch reference accumulation for one perspective, bypassing
    // the incremental cache entirely. Writes L1_ int32 values into out (resized).
    void accumulateFromScratch(const Nebula::Position& pos, Color persp,
                                std::vector<std::int32_t>& out) const;

private:
    int      L1_ = 256, L2_ = 32, L3_ = 32;
    bool     loaded_ = false;
    bool     incremental_ = false;
    bool     fastEvalOk_ = false;
    bool     l2Quantized_ = false;
    uint32_t ver_    = 0;

    // VERSION 1 — float32 FT (weight layout: [L1 × DIMS])
    std::vector<float> ft_weight_;    // [L1 × DIMS]
    std::vector<float> ft_bias_;      // [L1]

    // VERSION 2 — int16 FT (weight layout: [DIMS × L1], transposed)
    std::vector<int16_t> ft_weight_i16_;  // [DIMS × L1]
    std::vector<int16_t> ft_bias_i16_;    // [L1]
    int ft_scale_ = 1024;

    // Shared: L2 / L3 / output
    std::vector<float> l2_weight_;    // [L2 × 2*L1], VERSION1/2 only
    std::vector<int16_t> l2_weight_i16_; // [L2 × 2*L1], VERSION3 only
    int l2_scale_ = 1;                // VERSION3 only, stored per-net in the file
    std::vector<float> l2_bias_;      // [L2] (float32 in every version)
    std::vector<float> l3_weight_;    // [L3 × L2]
    std::vector<float> l3_bias_;      // [L3]
    std::vector<float> out_weight_;   // [L3]
    std::vector<float> out_bias_v_;   // [1]

    static inline float crelu(float x) {
        return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
    }

    using IndexList = Features::HalfKAv2Makruk::IndexList;

    // VERSION 1: float32 FT accumulation (weight stored [L1 × DIMS]).
    void accumulate_f32(const IndexList& indices, std::vector<float>& acc) const {
        acc = ft_bias_;
        const float* w    = ft_weight_.data();
        const int    dims = (int)Features::HalfKAv2Makruk::Dimensions;
        for (const auto idx : indices)
            for (int i = 0; i < L1_; ++i)
                acc[i] += w[i * dims + idx];
    }

    // VERSION 2: int16 FT accumulation (weight stored [DIMS × L1], transposed).
    // Inner loop is sequential — compiler auto-vectorises with SSE2/AVX2.
    void accumulate_i16(const IndexList& indices, std::vector<float>& acc) const {
        // int32 accumulator avoids overflow (32 feats × 2150 max_weight << INT32_MAX).
        std::vector<int32_t> raw(L1_);
        for (int i = 0; i < L1_; ++i)
            raw[i] = static_cast<int32_t>(ft_bias_i16_[i]);

        for (const auto idx : indices) {
            const int16_t* col = ft_weight_i16_.data() + idx * L1_;
            for (int i = 0; i < L1_; ++i)
                raw[i] += static_cast<int32_t>(col[i]);
        }

        // Convert to float in [0, 1] — same range as the float32 path pre-crelu.
        // crelu() in evaluate() will clamp values that exceed [0, 1].
        const float inv = 1.0f / static_cast<float>(ft_scale_);
        acc.resize(L1_);
        for (int i = 0; i < L1_; ++i)
            acc[i] = static_cast<float>(raw[i]) * inv;
    }

    // Incremental accumulator support (VERSION2 / int16 only -- see incremental_).
    // Defined in mknn_evaluator.cpp, where Position is a complete type.
    void refreshInto(const Nebula::Position& pos, Color persp, std::int32_t* dst) const;
    void addColumn(IndexType idx, std::int32_t* dst) const;
    void subColumn(IndexType idx, std::int32_t* dst) const;
    void updateAccumulator(const Nebula::Position& pos, Color persp) const;

    // Fast path (fastEvalOk_): fixed-size stack buffers, no heap allocation.
    // accumulateF32Into is the raw-pointer analogue of accumulate_f32(), used
    // only for the rare VERSION1-with-small-L1 case (VERSION2 nets within cap
    // always go via the incremental accumulator instead). forwardPass holds
    // the L2/L3/out math shared by both the incremental and from-scratch
    // fast-path accumulation results.
    void accumulateF32Into(const IndexList& indices, float* dst) const;
    Value forwardPass(const Nebula::Position& pos, const float* acc_w, const float* acc_b) const;
};

} // namespace Nebula::Eval::Nnue
