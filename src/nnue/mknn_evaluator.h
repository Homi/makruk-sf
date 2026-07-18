// MKNN NNUE evaluator for Makruk-SF.
//
// Supports two binary formats:
//   VERSION 1 (MKN1): float32 FT weight [L1 × DIMS], float32 L2/L3/out
//   VERSION 2 (MKN2): int16  FT weight [DIMS × L1] (transposed), float32 L2/L3/out
//
// The int16 format (VERSION 2) gives cache-friendly column access during
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
#include <istream>
#include <vector>
#include "features/half_ka_v2_makruk.h"
#include "../types.h"

namespace Nebula { class Position; }

namespace Nebula::Eval::Nnue {

class MknnEvaluator {
public:
    static constexpr uint32_t MAGIC    = 0x4D4B4E4Eu; // 'MKNN'
    static constexpr uint32_t VERSION1 = 1u;
    static constexpr uint32_t VERSION2 = 2u;

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
        if (version != VERSION1 && version != VERSION2) return false;
        if (dims    != Features::HalfKAv2Makruk::Dimensions) return false;
        if (l1 < 1 || l2 < 1 || l3 < 1) return false;

        L1_  = (int)l1;
        L2_  = (int)l2;
        L3_  = (int)l3;
        ver_ = version;

        if (version == VERSION2) {
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
        return true;
    }

    bool isLoaded()  const { return loaded_; }
    int  version()   const { return (int)ver_; }
    int  ftScale()   const { return ft_scale_; }

    // Returns evaluation from the side-to-move perspective, in centipawns.
    Value evaluate(const Nebula::Position& pos) const;

private:
    int      L1_ = 256, L2_ = 32, L3_ = 32;
    bool     loaded_ = false;
    uint32_t ver_    = 0;

    // VERSION 1 — float32 FT (weight layout: [L1 × DIMS])
    std::vector<float> ft_weight_;    // [L1 × DIMS]
    std::vector<float> ft_bias_;      // [L1]

    // VERSION 2 — int16 FT (weight layout: [DIMS × L1], transposed)
    std::vector<int16_t> ft_weight_i16_;  // [DIMS × L1]
    std::vector<int16_t> ft_bias_i16_;    // [L1]
    int ft_scale_ = 1024;

    // Shared: L2 / L3 / output (float32 in both versions)
    std::vector<float> l2_weight_;    // [L2 × 2*L1]
    std::vector<float> l2_bias_;      // [L2]
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
};

} // namespace Nebula::Eval::Nnue
