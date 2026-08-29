// MKNN float32/int16 NNUE evaluator — forward pass implementation.
// See mknn_evaluator.h for format and architecture details.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "mknn_evaluator.h"
#include "../position.h"
#include <cstring>

namespace Nebula::Eval::Nnue {

// ---------------------------------------------------------------------------
// Incremental accumulator support (VERSION2 / int16 only, see incremental_).
//
// Ported from the legacy Stockfish FeatureTransformer::updateAccumulator
// algorithm (src/nnue/nnue_feature_transformer.h) onto MknnAccumulator/
// int32 storage. See CLAUDE.md for the design writeup.
// ---------------------------------------------------------------------------

void MknnEvaluator::refreshInto(const Nebula::Position& pos, const Color persp,
                                 std::int32_t* dst) const {
    using Features::HalfKAv2Makruk;
    HalfKAv2Makruk::IndexList active;
    HalfKAv2Makruk::appendActiveIndices(pos, persp, active);
    for (int i = 0; i < L1_; ++i)
        dst[i] = static_cast<std::int32_t>(ft_bias_i16_[i]);
    for (const auto idx : active)
        addColumn(idx, dst);
}

void MknnEvaluator::addColumn(const IndexType idx, std::int32_t* dst) const {
    const int16_t* col = ft_weight_i16_.data() + idx * L1_;
    for (int i = 0; i < L1_; ++i)
        dst[i] += static_cast<std::int32_t>(col[i]);
}

void MknnEvaluator::subColumn(const IndexType idx, std::int32_t* dst) const {
    const int16_t* col = ft_weight_i16_.data() + idx * L1_;
    for (int i = 0; i < L1_; ++i)
        dst[i] -= static_cast<std::int32_t>(col[i]);
}

void MknnEvaluator::updateAccumulator(const Nebula::Position& pos, const Color persp) const {
    using Features::HalfKAv2Makruk;

    StateInfo* const target = pos.state();
    if (target->mknnAcc.computed[persp])
        return;   // hot path: already valid, nothing to do

    // Walk backwards looking for the nearest ancestor with a valid cached
    // accumulator, recording the hops visited so they can be replayed forward.
    // Bounded by both a cost budget (mirrors the legacy algorithm: give up if
    // replaying would cost more than a full refresh) and a hard cap (defensive
    // only -- the budget check makes this unreachable in practice).
    constexpr int MaxChain = 40;
    StateInfo* chain[MaxChain];
    int n = 0;
    int gain = HalfKAv2Makruk::refreshCost(pos);
    StateInfo* st = target;

    while (st->previous && !st->mknnAcc.computed[persp]) {
        if (HalfKAv2Makruk::requiresRefresh(st, persp))          // our king moved on this hop
            break;
        if ((gain -= HalfKAv2Makruk::updateCost(st) + 1) < 0)     // incremental too expensive
            break;
        if (n >= MaxChain)
            break;
        chain[n++] = st;
        st = st->previous;
    }

    std::int32_t* const dst = target->mknnAcc.acc[persp];

    if (n > 0 && st->mknnAcc.computed[persp]) {
        // Found a usable computed ancestor within budget: replay forward.
        // requiresRefresh gated every hop above, so our king did not move
        // anywhere in this span -- one king square is valid for all of it.
        const Square ksq = pos.square<KING>(persp);
        std::memcpy(dst, st->mknnAcc.acc[persp],
                    static_cast<size_t>(L1_) * sizeof(std::int32_t));

        for (int j = n - 1; j >= 0; --j) {   // oldest hop first
            HalfKAv2Makruk::IndexList removed, added;
            HalfKAv2Makruk::appendChangedIndices(ksq, chain[j]->dirtyPiece, persp, removed, added);
            for (const auto idx : removed) subColumn(idx, dst);
            for (const auto idx : added)   addColumn(idx, dst);

            // Materialize the hop closest to the found ancestor too (not just
            // the final target), so sibling nodes one ply below it become
            // 1-hop lookups instead of re-walking this whole span again.
            if (j == n - 1 && chain[j] != target) {
                std::memcpy(chain[j]->mknnAcc.acc[persp], dst,
                            static_cast<size_t>(L1_) * sizeof(std::int32_t));
                chain[j]->mknnAcc.computed[persp] = true;
            }
        }
        target->mknnAcc.computed[persp] = true;
        return;
    }

    // No usable ancestor (root, a king-move refresh boundary, or budget
    // exhausted): full rebuild, identical to the original always-from-scratch
    // behaviour.
    refreshInto(pos, persp, dst);
    target->mknnAcc.computed[persp] = true;
}

void MknnEvaluator::accumulateFromScratch(const Nebula::Position& pos, const Color persp,
                                           std::vector<std::int32_t>& out) const {
    out.resize(L1_);
    refreshInto(pos, persp, out.data());
}

// Raw-pointer analogue of accumulate_f32() for the fast path. Only reachable
// when fastEvalOk_ && !incremental_, i.e. a VERSION1 net with L1_ within
// MknnMaxL1 (every VERSION2 net within cap is incremental_ by construction).
void MknnEvaluator::accumulateF32Into(const IndexList& indices, float* dst) const {
    for (int i = 0; i < L1_; ++i)
        dst[i] = ft_bias_[i];
    const float* w    = ft_weight_.data();
    const int    dims = (int)Features::HalfKAv2Makruk::Dimensions;
    for (const auto idx : indices)
        for (int i = 0; i < L1_; ++i)
            dst[i] += w[i * dims + idx];
}

// Shared L2/L3/out math for the fast path. acc_w/acc_b are exactly L1_ floats
// each (already dequantized/converted), owned by the caller's stack buffers.
Value MknnEvaluator::forwardPass(const Nebula::Position& pos, const float* acc_w,
                                  const float* acc_b) const {
    const bool  white_stm = (pos.stm() == WHITE);
    const float* stm = white_stm ? acc_w : acc_b;
    const float* opp = white_stm ? acc_b : acc_w;

    // CReLU = clamp(0, 1), matching train.py's .clamp(0.0, 1.0).
    alignas(64) float l2_in[2 * MknnMaxL1];
    for (int i = 0; i < L1_; ++i) {
        l2_in[i]       = crelu(stm[i]);
        l2_in[L1_ + i] = crelu(opp[i]);
    }

    // L2 → CReLU.
    alignas(64) float l2_out[MknnMaxL2];
    if (l2Quantized_) {
        // VERSION3: int16 weight x int16 (quantized crelu) input -> int32
        // accumulator, dequantized by (l2_scale_ * L2InputScale). load()
        // already verified this can't overflow int32 for the loaded weights.
        alignas(64) int16_t l2_in_q[2 * MknnMaxL1];
        for (int j = 0; j < 2 * L1_; ++j)
            l2_in_q[j] = static_cast<int16_t>(l2_in[j] * float(L2InputScale) + 0.5f);

        const float deq = 1.0f / (float(l2_scale_) * float(L2InputScale));
        for (int i = 0; i < L2_; ++i) {
            int32_t s = 0;
            const int16_t* row = l2_weight_i16_.data() + i * 2 * L1_;
            for (int j = 0; j < 2 * L1_; ++j)
                s += int32_t(row[j]) * int32_t(l2_in_q[j]);
            l2_out[i] = crelu(l2_bias_[i] + float(s) * deq);
        }
    } else {
        for (int i = 0; i < L2_; ++i) {
            float s = l2_bias_[i];
            const float* row = l2_weight_.data() + i * 2 * L1_;
            for (int j = 0; j < 2 * L1_; ++j)
                s += row[j] * l2_in[j];
            l2_out[i] = crelu(s);
        }
    }

    // L3 → CReLU.
    alignas(64) float l3_out[MknnMaxL3];
    for (int i = 0; i < L3_; ++i) {
        float s = l3_bias_[i];
        const float* row = l3_weight_.data() + i * L2_;
        for (int j = 0; j < L2_; ++j)
            s += row[j] * l2_out[j];
        l3_out[i] = crelu(s);
    }

    // Output scalar (pre-sigmoid logit).
    float raw = out_bias_v_[0];
    for (int j = 0; j < L3_; ++j)
        raw += out_weight_[j] * l3_out[j];

    // Exact decode: training target = sigmoid(score_cp/400), so logit(v) = raw = score_cp/400.
    // Using raw*400 avoids computing sigmoid and then its inverse.
    const int cp = static_cast<int>(std::clamp(raw * 400.0f, -29000.0f, 29000.0f));
    return Value(cp);
}

Value MknnEvaluator::evaluate(const Nebula::Position& pos) const {
    using Features::HalfKAv2Makruk;

    if (fastEvalOk_) {
        // No heap allocation: fixed-size stack buffers sized by the
        // compile-time caps (MknnMaxL1/L2/L3), which load() already checked
        // this net fits within.
        alignas(64) float acc_w[MknnMaxL1];
        alignas(64) float acc_b[MknnMaxL1];

        if (incremental_) {
            updateAccumulator(pos, WHITE);
            updateAccumulator(pos, BLACK);
            const auto& a   = pos.state()->mknnAcc;
            const float inv = 1.0f / static_cast<float>(ft_scale_);
            for (int i = 0; i < L1_; ++i) {
                acc_w[i] = static_cast<float>(a.acc[WHITE][i]) * inv;
                acc_b[i] = static_cast<float>(a.acc[BLACK][i]) * inv;
            }
        } else {
            // fastEvalOk_ && !incremental_ implies VERSION1 (see load()).
            HalfKAv2Makruk::IndexList active_w, active_b;
            HalfKAv2Makruk::appendActiveIndices(pos, WHITE, active_w);
            HalfKAv2Makruk::appendActiveIndices(pos, BLACK, active_b);
            accumulateF32Into(active_w, acc_w);
            accumulateF32Into(active_b, acc_b);
        }

        return forwardPass(pos, acc_w, acc_b);
    }

    // Slow path: a net exceeding MknnMaxL1/L2/L3 (none in this project's
    // history so far). Unchanged from before the fast path was added.
    std::vector<float> acc_w, acc_b;

    if (incremental_) {
        updateAccumulator(pos, WHITE);
        updateAccumulator(pos, BLACK);
        const auto& a  = pos.state()->mknnAcc;
        const float inv = 1.0f / static_cast<float>(ft_scale_);
        acc_w.resize(L1_);
        acc_b.resize(L1_);
        for (int i = 0; i < L1_; ++i) {
            acc_w[i] = static_cast<float>(a.acc[WHITE][i]) * inv;
            acc_b[i] = static_cast<float>(a.acc[BLACK][i]) * inv;
        }
    } else {
        HalfKAv2Makruk::IndexList active_w, active_b;
        HalfKAv2Makruk::appendActiveIndices(pos, WHITE, active_w);
        HalfKAv2Makruk::appendActiveIndices(pos, BLACK, active_b);
        if (ver_ == VERSION2) {
            accumulate_i16(active_w, acc_w);
            accumulate_i16(active_b, acc_b);
        } else {
            accumulate_f32(active_w, acc_w);
            accumulate_f32(active_b, acc_b);
        }
    }

    // L2 input: [acc_stm_crelu; acc_opp_crelu] — STM perspective first.
    const bool white_stm = (pos.stm() == WHITE);
    const std::vector<float>& stm = white_stm ? acc_w : acc_b;
    const std::vector<float>& opp = white_stm ? acc_b : acc_w;

    std::vector<float> l2_in(2 * L1_);
    for (int i = 0; i < L1_; ++i) {
        l2_in[i]        = crelu(stm[i]);
        l2_in[L1_ + i]  = crelu(opp[i]);
    }

    std::vector<float> l2_out(L2_);
    for (int i = 0; i < L2_; ++i) {
        float s = l2_bias_[i];
        const float* row = l2_weight_.data() + i * 2 * L1_;
        for (int j = 0; j < 2 * L1_; ++j)
            s += row[j] * l2_in[j];
        l2_out[i] = crelu(s);
    }

    std::vector<float> l3_out(L3_);
    for (int i = 0; i < L3_; ++i) {
        float s = l3_bias_[i];
        const float* row = l3_weight_.data() + i * L2_;
        for (int j = 0; j < L2_; ++j)
            s += row[j] * l2_out[j];
        l3_out[i] = crelu(s);
    }

    float raw = out_bias_v_[0];
    for (int j = 0; j < L3_; ++j)
        raw += out_weight_[j] * l3_out[j];

    const int cp = static_cast<int>(std::clamp(raw * 400.0f, -29000.0f, 29000.0f));
    return Value(cp);
}

} // namespace Nebula::Eval::Nnue
