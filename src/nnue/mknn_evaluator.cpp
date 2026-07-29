// MKNN float32/int16 NNUE evaluator — forward pass implementation.
// See mknn_evaluator.h for format and architecture details.
//
// Makruk-SF is a GPLv3 project derived from sf-kernel / Stockfish.
// Makruk-SF-specific modifications maintained by Homi <bhome1@hotmail.com>.

#include "mknn_evaluator.h"
#include "../position.h"

namespace Nebula::Eval::Nnue {

Value MknnEvaluator::evaluate(const Nebula::Position& pos) const {
    using Features::HalfKAv2Makruk;

    // Accumulate FT for both perspectives.
    HalfKAv2Makruk::IndexList active_w, active_b;
    HalfKAv2Makruk::appendActiveIndices(pos, WHITE, active_w);
    HalfKAv2Makruk::appendActiveIndices(pos, BLACK, active_b);

    std::vector<float> acc_w, acc_b;
    if (ver_ == VERSION2) {
        // int16 FT: transposed weight layout, int32 accumulation, float output.
        accumulate_i16(active_w, acc_w);
        accumulate_i16(active_b, acc_b);
    } else {
        // float32 FT: non-transposed layout.
        accumulate_f32(active_w, acc_w);
        accumulate_f32(active_b, acc_b);
    }

    // L2 input: [acc_stm_crelu; acc_opp_crelu] — STM perspective first.
    const bool white_stm = (pos.stm() == WHITE);
    const std::vector<float>& stm = white_stm ? acc_w : acc_b;
    const std::vector<float>& opp = white_stm ? acc_b : acc_w;

    // CReLU = clamp(0, 1), matching train.py's .clamp(0.0, 1.0).
    std::vector<float> l2_in(2 * L1_);
    for (int i = 0; i < L1_; ++i) {
        l2_in[i]        = crelu(stm[i]);
        l2_in[L1_ + i]  = crelu(opp[i]);
    }

    // L2 → CReLU.
    std::vector<float> l2_out(L2_);
    for (int i = 0; i < L2_; ++i) {
        float s = l2_bias_[i];
        const float* row = l2_weight_.data() + i * 2 * L1_;
        for (int j = 0; j < 2 * L1_; ++j)
            s += row[j] * l2_in[j];
        l2_out[i] = crelu(s);
    }

    // L3 → CReLU.
    std::vector<float> l3_out(L3_);
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

} // namespace Nebula::Eval::Nnue
