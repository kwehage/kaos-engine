#include "reverb_processor.h"
#include <algorithm>
#include <cmath>

namespace kaos_engine {

// ── Buf ───────────────────────────────────────────────────────────────────────

void ReverbProcessor::Buf::prepare(int min_size)
{
    int s = 1;
    while (s < min_size) s <<= 1;
    if (s == size) return;
    data.assign(static_cast<size_t>(s), 0.0f);
    size = s;
    mask = s - 1;
    pos  = 0;
}

void ReverbProcessor::Buf::write(float x)
{
    data[static_cast<size_t>(pos)] = x;
    pos = (pos + 1) & mask;
}

float ReverbProcessor::Buf::read(int d) const
{
    return data[static_cast<size_t>((pos - d + size) & mask)];
}

float ReverbProcessor::Buf::read_h(float d) const
{
    const int   di  = static_cast<int>(d);
    const float fr  = d - static_cast<float>(di);
    const float xm1 = data[static_cast<size_t>((pos - di - 1 + size) & mask)];
    const float x0  = data[static_cast<size_t>((pos - di     + size) & mask)];
    const float x1  = data[static_cast<size_t>((pos - di + 1 + size) & mask)];
    const float x2  = data[static_cast<size_t>((pos - di + 2 + size) & mask)];
    const float c0  = x0;
    const float c1  = 0.5f * (x1 - xm1);
    const float c2  = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3  = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * fr + c2) * fr + c1) * fr + c0;
}

// ── DSP helpers ───────────────────────────────────────────────────────────────

float ReverbProcessor::allpass(Buf& buf, int delay, float coeff, float x)
{
    const float d = buf.read(delay);
    const float v = x + coeff * d;
    buf.write(v);
    return d - coeff * v;
}

float ReverbProcessor::allpass_mod(Buf& buf, float delay, float coeff, float x)
{
    const float safe = std::max(1.0f, std::min(delay, static_cast<float>(buf.size - 2)));
    const float d    = buf.read_h(safe);
    const float v    = x + coeff * d;
    buf.write(v);
    return d - coeff * v;
}

float ReverbProcessor::comb(Buf& buf, int delay, float g, float& lp, float lp_coeff, float x)
{
    const float d = buf.read(delay);
    lp += lp_coeff * (d - lp);
    buf.write(x + g * lp);
    return d;
}

// ── Constructor ───────────────────────────────────────────────────────────────

ReverbProcessor::ReverbProcessor()
{
    for (int k = 0; k < kNumCombs;       ++k) { sch_comb_lp_l_[k] = sch_comb_lp_r_[k] = 0.0f; }
    for (int k = 0; k < kNumFdn;         ++k) { fdn_lp_[k] = 0.0f; fdn_delays_[k] = 1; }
    for (int k = 0; k < kNumCombs;       ++k) { sch_comb_delays_l_[k] = sch_comb_delays_r_[k] = 1; }
    for (int k = 0; k < kNumSchAps;      ++k) { sch_ap_delays_[k] = 1; }
    for (int k = 0; k < kNumGardAps;     ++k) { gard_ap_delays_l_[k] = gard_ap_delays_r_[k] = 1; }
    for (int k = 0; k < kNumMoorerCombs; ++k) {
        moorer_comb_lp_l_[k] = moorer_comb_lp_r_[k] = 0.0f;
        moorer_comb_dl_[k] = moorer_comb_dr_[k] = 1;
    }
    for (int k = 0; k < kNumMoorerAps;   ++k) moorer_ap_del_[k] = 1;
}

// ── prepare ───────────────────────────────────────────────────────────────────

void ReverbProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    const int max_pre = static_cast<int>(0.200 * sample_rate) + 8;
    pre_delay_buf_.prepare(max_pre);
    update_delays();
    update_coeffs();
    regenerate_vn_sequence();
    update_svf_coeffs();
    reset();
}

// ── update_delays ─────────────────────────────────────────────────────────────

void ReverbProcessor::update_delays()
{
    const float size_scale = 0.5f + size_ * 1.5f;
    const float sr_scale   = static_cast<float>(sample_rate_) / 44100.0f;
    const float total      = size_scale * sr_scale;
    const int   lfo_head   = 16;

    // Dattorro
    for (int k = 0; k < 4; ++k)
        in_ap_delays_[k] = std::max(1, static_cast<int>(kBaseInAp[k] * total));
    tank_ap1_delay_l_  = std::max(1, static_cast<int>(kBaseAp1L  * total));
    tank_ap1_delay_r_  = std::max(1, static_cast<int>(kBaseAp1R  * total));
    tank_ap2_delay_l_  = std::max(1, static_cast<int>(kBaseAp2L  * total));
    tank_ap2_delay_r_  = std::max(1, static_cast<int>(kBaseAp2R  * total));
    tank_post_delay_l_ = std::max(1, static_cast<int>(kBasePostL * total));
    tank_post_delay_r_ = std::max(1, static_cast<int>(kBasePostR * total));
    for (int k = 0; k < 4; ++k)  in_ap_[k].prepare(in_ap_delays_[k] + 4);
    tank_ap1_l_.prepare(tank_ap1_delay_l_ + lfo_head);
    tank_ap1_r_.prepare(tank_ap1_delay_r_ + lfo_head);
    tank_ap2_l_.prepare(tank_ap2_delay_l_ + 4);
    tank_ap2_r_.prepare(tank_ap2_delay_r_ + 4);
    tank_post_l_.prepare(tank_post_delay_l_ + 4);
    tank_post_r_.prepare(tank_post_delay_r_ + 4);

    // Schroeder (R detuned 3%)
    for (int k = 0; k < kNumCombs; ++k) {
        sch_comb_delays_l_[k] = std::max(1, static_cast<int>(kSchCombBase[k] * total));
        sch_comb_delays_r_[k] = std::max(1, static_cast<int>(kSchCombBase[k] * total * 1.03f));
        sch_comb_l_[k].prepare(sch_comb_delays_l_[k] + 4);
        sch_comb_r_[k].prepare(sch_comb_delays_r_[k] + 4);
    }
    for (int k = 0; k < kNumSchAps; ++k) {
        sch_ap_delays_[k] = std::max(1, static_cast<int>(kSchApBase[k] * total));
        sch_ap_l_[k].prepare(sch_ap_delays_[k] + lfo_head);
        sch_ap_r_[k].prepare(sch_ap_delays_[k] + lfo_head);
    }

    // FDN
    for (int k = 0; k < kNumFdn; ++k) {
        fdn_delays_[k] = std::max(1, static_cast<int>(kFdnBase[k] * total));
        fdn_[k].prepare(fdn_delays_[k] + lfo_head);
    }

    // Gardner (R detuned 3%)
    for (int k = 0; k < kNumGardAps; ++k) {
        gard_ap_delays_l_[k] = std::max(1, static_cast<int>(kGardApBase[k] * total));
        gard_ap_delays_r_[k] = std::max(1, static_cast<int>(kGardApBase[k] * total * 1.03f));
        gard_ap_l_[k].prepare(gard_ap_delays_l_[k] + lfo_head);
        gard_ap_r_[k].prepare(gard_ap_delays_r_[k] + lfo_head);
    }
    gard_delay_l_len_ = std::max(1, static_cast<int>(kGardDelayBase * total));
    gard_delay_r_len_ = std::max(1, static_cast<int>(kGardDelayBase * total * 1.03f));
    gard_delay_l_.prepare(gard_delay_l_len_ + 4);
    gard_delay_r_.prepare(gard_delay_r_len_ + 4);

    // Moorer (R detuned 3%)
    const int max_tap = std::max(1, static_cast<int>(kMoorerTapBase[kNumMoorerTaps - 1] * total));
    moorer_early_buf_.prepare(max_tap + 8);
    for (int k = 0; k < kNumMoorerTaps; ++k)
        moorer_tap_delays_[k] = std::max(1, static_cast<int>(kMoorerTapBase[k] * total));
    for (int k = 0; k < kNumMoorerCombs; ++k) {
        moorer_comb_dl_[k] = std::max(1, static_cast<int>(kMoorerCombBase[k] * total));
        moorer_comb_dr_[k] = std::max(1, static_cast<int>(kMoorerCombBase[k] * total * 1.03f));
        moorer_comb_l_[k].prepare(moorer_comb_dl_[k] + 4);
        moorer_comb_r_[k].prepare(moorer_comb_dr_[k] + 4);
    }
    for (int k = 0; k < kNumMoorerAps; ++k) {
        moorer_ap_del_[k] = std::max(1, static_cast<int>(kMoorerApBase[k] * total));
        moorer_ap_l_[k].prepare(moorer_ap_del_[k] + lfo_head);
        moorer_ap_r_[k].prepare(moorer_ap_del_[k] + lfo_head);
    }

    // Velvet Noise (buffer must hold full tail length)
    const float vn_tail_s    = 0.3f + size_ * 1.7f;
    const int   vn_tail_samp = static_cast<int>(vn_tail_s * static_cast<float>(sample_rate_)) + 8;
    vn_buf_l_.prepare(vn_tail_samp);
    vn_buf_r_.prepare(vn_tail_samp);

    // Shimmer grain buffers (pitch_factor up to 2x needs 2x grain size read headroom)
    shimmer_grain_size_ = std::max(64, static_cast<int>(kShimmerGrainBase * sr_scale));
    shimmer_grain_l_.prepare(shimmer_grain_size_ * 2 + 16);
    shimmer_grain_r_.prepare(shimmer_grain_size_ * 2 + 16);
}

// ── update_coeffs ─────────────────────────────────────────────────────────────

void ReverbProcessor::update_coeffs()
{
    in_ap_coeff_     = 0.4f + diffusion_ * 0.5f;
    decay_ap2_coeff_ = in_ap_coeff_;
    decay_fb_        = decay_ * 0.97f;

    const float min_hz  = 500.0f;
    const float max_hz  = 20000.0f;
    const float damp_hz = min_hz * std::pow(max_hz / min_hz, damping_);
    const float w_damp  = 2.0f * kPi * damp_hz / static_cast<float>(sample_rate_);
    damp_coeff_ = std::clamp(1.0f - std::exp(-w_damp), 0.0f, 1.0f);
    bw_coeff_   = damp_coeff_;

    const float lfo_rate = 0.05f + mod_ * 1.95f;
    lfo_depth_ = mod2_ * 16.0f;
    lfo_inc_   = 2.0f * kPi * lfo_rate / static_cast<float>(sample_rate_);

    pre_delay_samples_ = std::max(1, static_cast<int>(
        pre_delay_ms_ * static_cast<float>(sample_rate_) / 1000.0f));

    // Moorer early-reflection tap gains: alternating signs, exponential envelope
    for (int k = 0; k < kNumMoorerTaps; ++k)
        moorer_tap_gains_[k] = ((k % 2 == 0) ? 1.0f : -1.0f)
                              * 0.2f * std::pow(0.8f, static_cast<float>(k));
}

// ── regenerate_vn_sequence ────────────────────────────────────────────────────

void ReverbProcessor::regenerate_vn_sequence()
{
    const float sr        = static_cast<float>(sample_rate_);
    const float tail_s    = 0.3f + size_ * 1.7f;
    const float tail_samp = tail_s * sr;
    const float density   = static_cast<float>(kVNDensity) * (0.5f + diffusion_ * 0.5f);
    const float seg_len   = sr / density;
    const int   N         = std::min(kVNMaxPulses, static_cast<int>(tail_samp / seg_len));
    vn_pulse_count_ = N;
    if (N == 0) return;

    const float norm = std::sqrt(3.0f / static_cast<float>(N));

    uint32_t sp = 0x12345678u;
    uint32_t sl = 0xABCDEF01u;
    uint32_t sr2 = 0xDEADBEEFu;

    auto lcg = [](uint32_t& s) -> float {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s >> 8) / static_cast<float>(1u << 24);
    };

    for (int k = 0; k < N; ++k) {
        const int pos = std::max(1, static_cast<int>(
            (static_cast<float>(k) + lcg(sp)) * seg_len));
        vn_pos_[k] = pos;

        // -60 dB envelope at tail_samp
        const float env = std::pow(1e-3f, static_cast<float>(pos) / tail_samp);
        const float amp = env * norm;

        vn_gain_l_[k] = (lcg(sl)  < 0.5f ? 1.0f : -1.0f) * amp;
        vn_gain_r_[k] = (lcg(sr2) < 0.5f ? 1.0f : -1.0f) * amp;
    }
}

// ── update_svf_coeffs ─────────────────────────────────────────────────────────

void ReverbProcessor::update_svf_coeffs()
{
    const float g = std::tan(kPi * filter_cutoff_hz_ / static_cast<float>(sample_rate_));
    svf_k_  = 1.0f / filter_resonance_;
    svf_a1_ = 1.0f / (1.0f + g * (g + svf_k_));
    svf_a2_ = g * svf_a1_;
    svf_a3_ = g * svf_a2_;
}

// ── apply_svf ─────────────────────────────────────────────────────────────────

float ReverbProcessor::apply_svf(float x, float& s1, float& s2) const
{
    const float v1 = svf_a1_ * s1 + svf_a2_ * (x - s2);
    const float v2 = s2 + svf_a2_ * s1 + svf_a3_ * (x - s2);
    s1 = 2.0f * v1 - s1;
    s2 = 2.0f * v2 - s2;
    switch (filter_type_) {
        case FilterType::LP: return v2;
        case FilterType::HP: return x - svf_k_ * v1 - v2;
        case FilterType::BP: return v1;
        default:             return v2;
    }
}

// ── reset ─────────────────────────────────────────────────────────────────────

void ReverbProcessor::reset()
{
    auto clr = [](Buf& b) {
        std::fill(b.data.begin(), b.data.end(), 0.0f);
        b.pos = 0;
    };

    bw_state_ = 0.0f;
    tank_fb_l_ = tank_fb_r_ = 0.0f;
    damp_l_ = damp_r_ = 0.0f;
    lfo_phase_l_ = lfo_phase_r_ = 0.0f;

    clr(pre_delay_buf_);
    for (auto& b : in_ap_) clr(b);
    clr(tank_ap1_l_); clr(tank_ap2_l_); clr(tank_post_l_);
    clr(tank_ap1_r_); clr(tank_ap2_r_); clr(tank_post_r_);

    for (int k = 0; k < kNumCombs;  ++k) {
        clr(sch_comb_l_[k]); clr(sch_comb_r_[k]);
        sch_comb_lp_l_[k] = sch_comb_lp_r_[k] = 0.0f;
    }
    for (int k = 0; k < kNumSchAps; ++k) { clr(sch_ap_l_[k]); clr(sch_ap_r_[k]); }

    for (int k = 0; k < kNumFdn; ++k) { clr(fdn_[k]); fdn_lp_[k] = 0.0f; }

    gard_lp_l_ = gard_lp_r_ = gard_fb_l_ = gard_fb_r_ = 0.0f;
    for (int k = 0; k < kNumGardAps; ++k) { clr(gard_ap_l_[k]); clr(gard_ap_r_[k]); }
    clr(gard_delay_l_); clr(gard_delay_r_);

    clr(moorer_early_buf_);
    for (int k = 0; k < kNumMoorerCombs; ++k) {
        clr(moorer_comb_l_[k]); clr(moorer_comb_r_[k]);
        moorer_comb_lp_l_[k] = moorer_comb_lp_r_[k] = 0.0f;
    }
    for (int k = 0; k < kNumMoorerAps; ++k) { clr(moorer_ap_l_[k]); clr(moorer_ap_r_[k]); }

    vn_lp_l_ = vn_lp_r_ = 0.0f;
    clr(vn_buf_l_); clr(vn_buf_r_);

    shimmer_grain_phase_ = 0.0f;
    clr(shimmer_grain_l_); clr(shimmer_grain_r_);

    svf_s1_l_ = svf_s1_r_ = svf_s2_l_ = svf_s2_r_ = 0.0f;
}

// ── Setters ───────────────────────────────────────────────────────────────────

void ReverbProcessor::set_pre_delay(float ms)
{
    const float c = std::clamp(ms, 0.0f, 200.0f);
    if (c == pre_delay_ms_) return;
    pre_delay_ms_ = c;
    update_coeffs();
}

void ReverbProcessor::set_size(float v)
{
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == size_) return;
    size_ = c;
    update_delays();
    if (algorithm_ == ReverbAlgorithm::VelvetNoise)
        regenerate_vn_sequence();
}

void ReverbProcessor::set_decay(float v)
{
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == decay_) return;
    decay_ = c;
    update_coeffs();
}

void ReverbProcessor::set_damping(float v)
{
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == damping_) return;
    damping_ = c;
    update_coeffs();
}

void ReverbProcessor::set_diffusion(float v)
{
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == diffusion_) return;
    diffusion_ = c;
    update_coeffs();
    if (algorithm_ == ReverbAlgorithm::VelvetNoise)
        regenerate_vn_sequence();
}

void ReverbProcessor::set_mod(float v)
{
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == mod_) return;
    mod_ = c;
    update_coeffs();
}

void ReverbProcessor::set_mod2(float v)
{
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == mod2_) return;
    mod2_ = c;
    update_coeffs();
}

void ReverbProcessor::set_mix(float v)    { mix_ = std::clamp(v, 0.0f, 1.0f); }

void ReverbProcessor::set_output(float db) { output_linear_ = std::pow(10.0f, db / 20.0f); }

void ReverbProcessor::set_algorithm(ReverbAlgorithm a)
{
    if (a != algorithm_) {
        algorithm_ = a;
        if (a == ReverbAlgorithm::VelvetNoise)
            regenerate_vn_sequence();
        reset();
    }
}

void ReverbProcessor::set_filter_enabled(bool enabled)
{
    if (enabled && !filter_enabled_)
        svf_s1_l_ = svf_s1_r_ = svf_s2_l_ = svf_s2_r_ = 0.0f;
    filter_enabled_ = enabled;
}

void ReverbProcessor::set_filter_pos(int pos)
{
    if (pos != filter_pos_)
        svf_s1_l_ = svf_s1_r_ = svf_s2_l_ = svf_s2_r_ = 0.0f;
    filter_pos_ = pos;
}

void ReverbProcessor::set_filter_type(int type)
{
    filter_type_ = static_cast<FilterType>(type);
}

void ReverbProcessor::set_filter_cutoff(float hz)
{
    filter_cutoff_hz_ = std::clamp(hz, 20.0f, 20000.0f);
    update_svf_coeffs();
}

void ReverbProcessor::set_filter_resonance(float q)
{
    filter_resonance_ = std::clamp(q, 0.1f, 20.0f);
    update_svf_coeffs();
}

void ReverbProcessor::set_filter_blend(float blend)
{
    filter_blend_ = std::clamp(blend, 0.0f, 1.0f);
}

// ── Per-algorithm processing ──────────────────────────────────────────────────

void ReverbProcessor::process_dattorro(float in_l, float in_r, float& wet_l, float& wet_r)
{
    pre_delay_buf_.write(0.5f * (in_l + in_r));
    float d = pre_delay_buf_.read(pre_delay_samples_);

    bw_state_ += bw_coeff_ * (d - bw_state_);
    d = bw_state_;
    for (int k = 0; k < 4; ++k)
        d = allpass(in_ap_[k], in_ap_delays_[k], in_ap_coeff_, d);

    const float tank_in_l = d + decay_fb_ * tank_fb_r_;
    const float tank_in_r = d + decay_fb_ * tank_fb_l_;

    float vl = allpass_mod(tank_ap1_l_, static_cast<float>(tank_ap1_delay_l_) +
                           lfo_depth_ * std::sin(lfo_phase_l_), 0.70f, tank_in_l);
    damp_l_ += damp_coeff_ * (vl - damp_l_);
    vl = damp_l_ * decay_fb_;
    vl = allpass(tank_ap2_l_, tank_ap2_delay_l_, decay_ap2_coeff_, vl);
    tank_post_l_.write(vl);
    tank_fb_l_ = tank_post_l_.read(tank_post_delay_l_);

    float vr = allpass_mod(tank_ap1_r_, static_cast<float>(tank_ap1_delay_r_) +
                           lfo_depth_ * std::sin(lfo_phase_r_ + kPi * 0.5f), 0.70f, tank_in_r);
    damp_r_ += damp_coeff_ * (vr - damp_r_);
    vr = damp_r_ * decay_fb_;
    vr = allpass(tank_ap2_r_, tank_ap2_delay_r_, decay_ap2_coeff_, vr);
    tank_post_r_.write(vr);
    tank_fb_r_ = tank_post_r_.read(tank_post_delay_r_);

    if (std::fabsf(tank_fb_l_) < 1e-15f) tank_fb_l_ = 0.0f;
    if (std::fabsf(tank_fb_r_) < 1e-15f) tank_fb_r_ = 0.0f;

    wet_l = 0.6f * tank_fb_r_ + 0.4f * tank_post_l_.read(tank_post_delay_l_ / 3 + 1);
    wet_r = 0.6f * tank_fb_l_ + 0.4f * tank_post_r_.read(tank_post_delay_r_ / 3 + 1);
}

void ReverbProcessor::process_schroeder(float in_l, float in_r, float& wet_l, float& wet_r)
{
    float sum_l = 0.0f, sum_r = 0.0f;
    for (int k = 0; k < kNumCombs; ++k) {
        sum_l += comb(sch_comb_l_[k], sch_comb_delays_l_[k], decay_fb_, sch_comb_lp_l_[k], damp_coeff_, in_l);
        sum_r += comb(sch_comb_r_[k], sch_comb_delays_r_[k], decay_fb_, sch_comb_lp_r_[k], damp_coeff_, in_r);
    }
    float yl = sum_l * 0.25f;
    float yr = sum_r * 0.25f;
    for (int k = 0; k < kNumSchAps; ++k) {
        const float ll = lfo_depth_ * std::sin(lfo_phase_l_ + static_cast<float>(k) * kPi * 0.5f);
        const float lr = lfo_depth_ * std::sin(lfo_phase_r_ + static_cast<float>(k) * kPi * 0.5f);
        yl = allpass_mod(sch_ap_l_[k], static_cast<float>(sch_ap_delays_[k]) + ll, in_ap_coeff_, yl);
        yr = allpass_mod(sch_ap_r_[k], static_cast<float>(sch_ap_delays_[k]) + lr, in_ap_coeff_, yr);
    }
    wet_l = yl;
    wet_r = yr;
}

void ReverbProcessor::process_fdn(float in_l, float in_r, float& wet_l, float& wet_r)
{
    const float mono = 0.5f * (in_l + in_r);
    const float fdn_lfo[kNumFdn] = {
        lfo_depth_ * std::sin(lfo_phase_l_),
        lfo_depth_ * std::sin(lfo_phase_r_),
        lfo_depth_ * std::sin(lfo_phase_l_ + kPi * 0.5f),
        lfo_depth_ * std::sin(lfo_phase_r_ + kPi * 0.5f),
    };
    float s[kNumFdn];
    for (int k = 0; k < kNumFdn; ++k) {
        const float d = fdn_[k].read_h(static_cast<float>(fdn_delays_[k]) + fdn_lfo[k]);
        fdn_lp_[k] += damp_coeff_ * (d - fdn_lp_[k]);
        s[k] = fdn_lp_[k] * decay_fb_;
    }
    const float t0 = 0.5f * (s[0] + s[1] + s[2] + s[3]);
    const float t1 = 0.5f * (s[0] - s[1] + s[2] - s[3]);
    const float t2 = 0.5f * (s[0] + s[1] - s[2] - s[3]);
    const float t3 = 0.5f * (s[0] - s[1] - s[2] + s[3]);
    fdn_[0].write(t0 + mono);
    fdn_[1].write(t1 + mono);
    fdn_[2].write(t2 + mono);
    fdn_[3].write(t3 + mono);
    if (std::fabsf(s[0]) < 1e-15f) { for (int k = 0; k < kNumFdn; ++k) fdn_lp_[k] = 0.0f; }
    wet_l = (s[0] + s[2]) * 0.5f;
    wet_r = (s[1] + s[3]) * 0.5f;
}

void ReverbProcessor::process_gardner(float in_l, float in_r, float& wet_l, float& wet_r)
{
    float yl = in_l + decay_fb_ * gard_fb_l_;
    yl = allpass_mod(gard_ap_l_[0], static_cast<float>(gard_ap_delays_l_[0]) + lfo_depth_ * std::sin(lfo_phase_l_),                in_ap_coeff_,     yl);
    yl = allpass_mod(gard_ap_l_[1], static_cast<float>(gard_ap_delays_l_[1]) + lfo_depth_ * std::sin(lfo_phase_l_ + kPi * 0.33f), in_ap_coeff_,     yl);
    yl = allpass_mod(gard_ap_l_[2], static_cast<float>(gard_ap_delays_l_[2]) + lfo_depth_ * std::sin(lfo_phase_l_ + kPi * 0.67f), decay_ap2_coeff_, yl);
    gard_lp_l_ += damp_coeff_ * (yl - gard_lp_l_);
    gard_delay_l_.write(gard_lp_l_);
    gard_fb_l_ = gard_delay_l_.read(gard_delay_l_len_);
    if (std::fabsf(gard_fb_l_) < 1e-15f) gard_fb_l_ = 0.0f;

    float yr = in_r + decay_fb_ * gard_fb_r_;
    yr = allpass_mod(gard_ap_r_[0], static_cast<float>(gard_ap_delays_r_[0]) + lfo_depth_ * std::sin(lfo_phase_r_),                in_ap_coeff_,     yr);
    yr = allpass_mod(gard_ap_r_[1], static_cast<float>(gard_ap_delays_r_[1]) + lfo_depth_ * std::sin(lfo_phase_r_ + kPi * 0.33f), in_ap_coeff_,     yr);
    yr = allpass_mod(gard_ap_r_[2], static_cast<float>(gard_ap_delays_r_[2]) + lfo_depth_ * std::sin(lfo_phase_r_ + kPi * 0.67f), decay_ap2_coeff_, yr);
    gard_lp_r_ += damp_coeff_ * (yr - gard_lp_r_);
    gard_delay_r_.write(gard_lp_r_);
    gard_fb_r_ = gard_delay_r_.read(gard_delay_r_len_);
    if (std::fabsf(gard_fb_r_) < 1e-15f) gard_fb_r_ = 0.0f;

    wet_l = gard_fb_l_;
    wet_r = gard_fb_r_;
}

void ReverbProcessor::process_moorer(float in_l, float in_r, float& wet_l, float& wet_r)
{
    // Mono tapped delay line for early reflections
    moorer_early_buf_.write(0.5f * (in_l + in_r));
    float early = 0.0f;
    for (int k = 0; k < kNumMoorerTaps; ++k)
        early += moorer_tap_gains_[k] * moorer_early_buf_.read(moorer_tap_delays_[k]);

    // Schroeder-style late tail: 4 parallel combs + 2 series allpasses per channel
    float csum_l = 0.0f, csum_r = 0.0f;
    for (int k = 0; k < kNumMoorerCombs; ++k) {
        csum_l += comb(moorer_comb_l_[k], moorer_comb_dl_[k], decay_fb_, moorer_comb_lp_l_[k], damp_coeff_, early);
        csum_r += comb(moorer_comb_r_[k], moorer_comb_dr_[k], decay_fb_, moorer_comb_lp_r_[k], damp_coeff_, early);
    }
    float yl = csum_l * 0.25f;
    float yr = csum_r * 0.25f;
    for (int k = 0; k < kNumMoorerAps; ++k) {
        const float ll = lfo_depth_ * std::sin(lfo_phase_l_ + static_cast<float>(k) * kPi * 0.5f);
        const float lr = lfo_depth_ * std::sin(lfo_phase_r_ + static_cast<float>(k) * kPi * 0.5f);
        yl = allpass_mod(moorer_ap_l_[k], static_cast<float>(moorer_ap_del_[k]) + ll, in_ap_coeff_, yl);
        yr = allpass_mod(moorer_ap_r_[k], static_cast<float>(moorer_ap_del_[k]) + lr, in_ap_coeff_, yr);
    }
    wet_l = yl;
    wet_r = yr;
}

void ReverbProcessor::process_velvet_noise(float in_l, float in_r, float& wet_l, float& wet_r)
{
    vn_buf_l_.write(in_l);
    vn_buf_r_.write(in_r);

    float yl = 0.0f, yr = 0.0f;
    for (int k = 0; k < vn_pulse_count_; ++k) {
        yl += vn_gain_l_[k] * vn_buf_l_.read(vn_pos_[k]);
        yr += vn_gain_r_[k] * vn_buf_r_.read(vn_pos_[k]);
    }

    // LP damping on output
    vn_lp_l_ += damp_coeff_ * (yl - vn_lp_l_);
    vn_lp_r_ += damp_coeff_ * (yr - vn_lp_r_);
    wet_l = vn_lp_l_;
    wet_r = vn_lp_r_;
}

void ReverbProcessor::process_shimmer(float in_l, float in_r, float& wet_l, float& wet_r)
{
    // mod_ = shimmer mix 0-1; mod2_ = pitch shift 0 (+0 st) to 1 (+12 st = octave up)
    const float pitch_factor = std::pow(2.0f, mod2_);
    const float shimmer_mix  = mod_;
    const float gs           = static_cast<float>(shimmer_grain_size_);

    // Advance grain phase; two grains 180 deg apart give continuous crossfade
    shimmer_grain_phase_ += 1.0f;
    if (shimmer_grain_phase_ >= gs) shimmer_grain_phase_ -= gs;

    const float pa = shimmer_grain_phase_;
    float pb = pa + gs * 0.5f;
    if (pb >= gs) pb -= gs;

    // Triangular crossfade windows
    const float ea = 1.0f - std::fabsf(pa / gs * 2.0f - 1.0f);
    const float eb = 1.0f - std::fabsf(pb / gs * 2.0f - 1.0f);

    // Read at pitch_factor * (gs - phase): playback speed = pitch_factor, pitch = +12*log2(pf) st
    const float da = std::min(pitch_factor * (gs - pa) + 1.0f,
                              static_cast<float>(shimmer_grain_l_.size - 2));
    const float db = std::min(pitch_factor * (gs - pb) + 1.0f,
                              static_cast<float>(shimmer_grain_l_.size - 2));

    const float pitched_l = ea * shimmer_grain_l_.read_h(da) + eb * shimmer_grain_l_.read_h(db);
    const float pitched_r = ea * shimmer_grain_r_.read_h(da) + eb * shimmer_grain_r_.read_h(db);

    // Blend pitched signal into Dattorro cross-feedback
    const float fb_l = tank_fb_l_ + shimmer_mix * (pitched_l - tank_fb_l_);
    const float fb_r = tank_fb_r_ + shimmer_mix * (pitched_r - tank_fb_r_);

    // Dattorro tank with modified feedback
    pre_delay_buf_.write(0.5f * (in_l + in_r));
    float d = pre_delay_buf_.read(pre_delay_samples_);
    bw_state_ += bw_coeff_ * (d - bw_state_);
    d = bw_state_;
    for (int k = 0; k < 4; ++k)
        d = allpass(in_ap_[k], in_ap_delays_[k], in_ap_coeff_, d);

    const float tank_in_l = d + decay_fb_ * fb_r;
    const float tank_in_r = d + decay_fb_ * fb_l;

    float vl = allpass_mod(tank_ap1_l_, static_cast<float>(tank_ap1_delay_l_) +
                           lfo_depth_ * std::sin(lfo_phase_l_), 0.70f, tank_in_l);
    damp_l_ += damp_coeff_ * (vl - damp_l_);
    vl = damp_l_ * decay_fb_;
    vl = allpass(tank_ap2_l_, tank_ap2_delay_l_, decay_ap2_coeff_, vl);
    tank_post_l_.write(vl);
    tank_fb_l_ = tank_post_l_.read(tank_post_delay_l_);

    float vr = allpass_mod(tank_ap1_r_, static_cast<float>(tank_ap1_delay_r_) +
                           lfo_depth_ * std::sin(lfo_phase_r_ + kPi * 0.5f), 0.70f, tank_in_r);
    damp_r_ += damp_coeff_ * (vr - damp_r_);
    vr = damp_r_ * decay_fb_;
    vr = allpass(tank_ap2_r_, tank_ap2_delay_r_, decay_ap2_coeff_, vr);
    tank_post_r_.write(vr);
    tank_fb_r_ = tank_post_r_.read(tank_post_delay_r_);

    if (std::fabsf(tank_fb_l_) < 1e-15f) tank_fb_l_ = 0.0f;
    if (std::fabsf(tank_fb_r_) < 1e-15f) tank_fb_r_ = 0.0f;

    // Feed updated tank feedback into grain buffers for next sample
    shimmer_grain_l_.write(tank_fb_l_);
    shimmer_grain_r_.write(tank_fb_r_);

    wet_l = 0.6f * tank_fb_r_ + 0.4f * tank_post_l_.read(tank_post_delay_l_ / 3 + 1);
    wet_r = 0.6f * tank_fb_l_ + 0.4f * tank_post_r_.read(tank_post_delay_r_ / 3 + 1);
}

// ── process ───────────────────────────────────────────────────────────────────

void ReverbProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        float in_l = left[i], in_r = right[i];

        if (filter_enabled_ && filter_pos_ == 0) {
            in_l += filter_blend_ * (apply_svf(in_l, svf_s1_l_, svf_s2_l_) - in_l);
            in_r += filter_blend_ * (apply_svf(in_r, svf_s1_r_, svf_s2_r_) - in_r);
        }

        lfo_phase_l_ += lfo_inc_;
        if (lfo_phase_l_ >= 2.0f * kPi) lfo_phase_l_ -= 2.0f * kPi;
        lfo_phase_r_ += lfo_inc_ * 1.07f;
        if (lfo_phase_r_ >= 2.0f * kPi) lfo_phase_r_ -= 2.0f * kPi;

        float wet_l = 0.0f, wet_r = 0.0f;
        switch (algorithm_) {
            case ReverbAlgorithm::Dattorro:    process_dattorro(in_l, in_r, wet_l, wet_r);     break;
            case ReverbAlgorithm::Schroeder:   process_schroeder(in_l, in_r, wet_l, wet_r);    break;
            case ReverbAlgorithm::FDN:         process_fdn(in_l, in_r, wet_l, wet_r);          break;
            case ReverbAlgorithm::Gardner:     process_gardner(in_l, in_r, wet_l, wet_r);      break;
            case ReverbAlgorithm::Moorer:      process_moorer(in_l, in_r, wet_l, wet_r);       break;
            case ReverbAlgorithm::VelvetNoise: process_velvet_noise(in_l, in_r, wet_l, wet_r); break;
            case ReverbAlgorithm::Shimmer:     process_shimmer(in_l, in_r, wet_l, wet_r);      break;
            default: break;
        }

        float out_l = in_l + mix_ * (wet_l - in_l);
        float out_r = in_r + mix_ * (wet_r - in_r);

        if (filter_enabled_ && filter_pos_ == 1) {
            out_l += filter_blend_ * (apply_svf(out_l, svf_s1_l_, svf_s2_l_) - out_l);
            out_r += filter_blend_ * (apply_svf(out_r, svf_s1_r_, svf_s2_r_) - out_r);
        }

        left[i]  = out_l * output_linear_;
        right[i] = out_r * output_linear_;
    }
}

} // namespace kaos_engine
