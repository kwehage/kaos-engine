#include "distortion_processor.h"
#include <algorithm>
#include <cmath>

namespace kaos_engine {

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

DistortionProcessor::DistortionProcessor() = default;

void DistortionProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    update_tone_coeff();
    update_svf_coeffs();
    reset();
}

void DistortionProcessor::reset()
{
    tone_l_   = tone_r_   = 0.0f;
    dc_xl_    = dc_xr_    = 0.0f;
    dc_yl_    = dc_yr_    = 0.0f;
    sr_cnt_l_ = sr_cnt_r_ = 0;
    sr_hld_l_ = sr_hld_r_ = 0.0f;
    prev_l_   = prev_r_   = 0.0f;
    svf_s1_l_ = svf_s1_r_ = svf_s2_l_ = svf_s2_r_ = 0.0f;
}

void DistortionProcessor::set_drive(float db)
{
    drive_linear_ = std::pow(10.0f, db / 20.0f);
}

void DistortionProcessor::set_mode(DistortionMode mode)
{
    if (mode_ != mode) {
        mode_ = mode;
        reset();
    }
}

void DistortionProcessor::set_tone(float normalised)
{
    tone_norm_ = std::clamp(normalised, 0.0f, 1.0f);
    update_tone_coeff();
}

void DistortionProcessor::set_feedback(float alpha)
{
    feedback_ = std::clamp(alpha, 0.0f, 1.0f);
}

void DistortionProcessor::set_bias(float bias)
{
    bias_ = std::clamp(bias, 0.0f, 1.0f);
}

void DistortionProcessor::set_output(float db)
{
    output_linear_ = std::pow(10.0f, db / 20.0f);
}

void DistortionProcessor::set_mix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

void DistortionProcessor::set_filter_enabled(bool enabled)
{
    if (enabled && !filter_enabled_)
        svf_s1_l_ = svf_s1_r_ = svf_s2_l_ = svf_s2_r_ = 0.0f;
    filter_enabled_ = enabled;
}

void DistortionProcessor::set_filter_pos(int pos)
{
    if (pos != filter_pos_)
        svf_s1_l_ = svf_s1_r_ = svf_s2_l_ = svf_s2_r_ = 0.0f;
    filter_pos_ = pos;
}

void DistortionProcessor::set_filter_type(int type)
{
    filter_type_ = static_cast<FilterType>(type);
}

void DistortionProcessor::set_filter_cutoff(float hz)
{
    filter_cutoff_hz_ = std::clamp(hz, 20.0f, 20000.0f);
    update_svf_coeffs();
}

void DistortionProcessor::set_filter_resonance(float q)
{
    filter_resonance_ = std::clamp(q, 0.1f, 20.0f);
    update_svf_coeffs();
}

void DistortionProcessor::set_filter_blend(float blend)
{
    filter_blend_ = std::clamp(blend, 0.0f, 1.0f);
}

void DistortionProcessor::update_svf_coeffs()
{
    const float g = std::tan(kPi * filter_cutoff_hz_ / (float)sample_rate_);
    svf_k_  = 1.0f / filter_resonance_;
    svf_a1_ = 1.0f / (1.0f + g * (g + svf_k_));
    svf_a2_ = g * svf_a1_;
    svf_a3_ = g * svf_a2_;
}

float DistortionProcessor::apply_svf(float x, float& s1, float& s2) const
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

void DistortionProcessor::update_tone_coeff()
{
    const float min_hz = 500.0f;
    const float max_hz = 20000.0f;
    const float freq   = min_hz * std::pow(max_hz / min_hz, tone_norm_);
    const float w      = 2.0f * kPi * freq / static_cast<float>(sample_rate_);
    tone_coeff_ = std::clamp(1.0f - std::exp(-w), 0.0f, 1.0f);
}

// ── Transfer functions ─────────────────────────────────────────────────────────

float DistortionProcessor::soft_clip(float x)
{
    return std::tanh(x);
}

float DistortionProcessor::hard_clip(float x)
{
    return std::clamp(x, -1.0f, 1.0f);
}

float DistortionProcessor::foldback(float x)
{
    float v = x;
    while (v > 1.0f || v < -1.0f) {
        if (v >  1.0f) v =  2.0f - v;
        if (v < -1.0f) v = -2.0f - v;
    }
    return v;
}

float DistortionProcessor::tube_clip(float x) const
{
    // Asymmetric tanh: shift operating point by b, generating even harmonics.
    const float b = bias_ * 0.7f;
    return std::tanh(x + b) - std::tanh(b);
}

float DistortionProcessor::arctan_clip(float x, float g)
{
    // x is already pre-driven (= dry * g). Normalise by atan(g) so the
    // output is bounded to ±1. Do NOT multiply by g again — that would
    // square the effective drive and saturate even the noise floor.
    const float denom = std::atan(g);
    if (denom < 1e-6f) return x;
    return std::atan(x) / denom;
}

float DistortionProcessor::log_clip(float x, float g)
{
    // x is already pre-driven (= dry * g). Same rationale as arctan_clip:
    // use |x| directly so the effective parameter is g, not g².
    const float denom = std::log(1.0f + g);
    if (denom < 1e-6f) return x;
    return std::copysign(std::log(1.0f + std::abs(x)) / denom, x);
}

float DistortionProcessor::sine_fold(float x)
{
    // Sine wavefolder: smooth at low drive, complex FM-like folds at high drive.
    return std::sin(x);
}

float DistortionProcessor::diode_clip(float x)
{
    // Exponential diode approximation: sign(x)*(1 - exp(-|x|))
    return std::copysign(1.0f - std::exp(-std::abs(x)), x);
}

float DistortionProcessor::half_wave(float x)
{
    // Zeroes negative half-cycles → DC + even harmonics.
    return std::max(0.0f, x);
}

float DistortionProcessor::full_wave(float x)
{
    // Flips negative cycles → output frequency doubles (octave up).
    return std::abs(x);
}

float DistortionProcessor::chebyshev_t3(float x)
{
    // T3(x) = 4x^3 - 3x: generates only the 3rd harmonic from a unit sine.
    // Clamp to [-1,1] to stay in the valid Chebyshev range.
    const float v = std::clamp(x, -1.0f, 1.0f);
    return 4.0f * v * v * v - 3.0f * v;
}

float DistortionProcessor::bitcrush(float x) const
{
    // bias_ 0→1 maps bit depth 16→1 (more bias = more crushing).
    const int   bits  = std::max(1, static_cast<int>(std::round(16.0f - bias_ * 15.0f)));
    const float steps = std::pow(2.0f, static_cast<float>(bits - 1));
    return std::round(x * steps) / steps;
}

float DistortionProcessor::sample_rate_reduce(float x, int& count, float& held) const
{
    // bias_ 0→1 maps downsampling factor 1→32 (more bias = lower effective rate).
    const int factor = std::max(1, static_cast<int>(std::round(1.0f + bias_ * 31.0f)));
    if (factor <= 1) return x;
    if (count == 0) held = x;
    count = (count + 1) % factor;
    return held;
}

// ── Filters ───────────────────────────────────────────────────────────────────

float DistortionProcessor::apply_tone(float x, float& state) const
{
    state += tone_coeff_ * (x - state);
    return state;
}

float DistortionProcessor::apply_dc_block(float x, float& x_prev, float& y_prev)
{
    const float y = x - x_prev + kDcR * y_prev;
    x_prev = x;
    y_prev = y;
    return y;
}

// ── Per-sample processing ──────────────────────────────────────────────────────

float DistortionProcessor::process_sample(float x,
                                          float& tone_state,
                                          float& dc_x_prev, float& dc_y_prev,
                                          int& sr_count,    float& sr_held,
                                          float& prev_dist)
{
    float v = x * drive_linear_;

    switch (mode_) {
        case DistortionMode::Soft:
            v = soft_clip(v);
            break;
        case DistortionMode::Hard:
            v = hard_clip(v);
            break;
        case DistortionMode::Foldback:
            v = foldback(v);
            break;
        case DistortionMode::Tube:
            v = tube_clip(v);
            v = apply_dc_block(v, dc_x_prev, dc_y_prev);
            break;
        case DistortionMode::Arctan:
            v = arctan_clip(v, drive_linear_);
            break;
        case DistortionMode::Log:
            v = log_clip(v, drive_linear_);
            break;
        case DistortionMode::SineFold:
            v = sine_fold(v);
            break;
        case DistortionMode::Diode:
            v = diode_clip(v);
            break;
        case DistortionMode::HalfWave:
            v = half_wave(v);
            break;
        case DistortionMode::FullWave:
            v = full_wave(v);
            break;
        case DistortionMode::Chebyshev:
            v = chebyshev_t3(v);
            break;
        case DistortionMode::Bitcrusher:
            v = hard_clip(v);         // clamp before quantisation
            v = bitcrush(v);
            break;
        case DistortionMode::SampleRate:
            v = sample_rate_reduce(v, sr_count, sr_held);
            break;
        default:
            break;
    }

    // y[n] = f(x[n]) + alpha * f(x[n-1])
    const float f_xn = v;
    v = f_xn + feedback_ * prev_dist;
    prev_dist = f_xn;

    v = apply_tone(v, tone_state);
    return v * output_linear_;
}

// ── Block processing ───────────────────────────────────────────────────────────

void DistortionProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // Optional pre-filter: blend between dry and filtered input
        float in_l = dry_l, in_r = dry_r;
        if (filter_enabled_ && filter_pos_ == 0) {
            in_l = dry_l + filter_blend_ * (apply_svf(dry_l, svf_s1_l_, svf_s2_l_) - dry_l);
            in_r = dry_r + filter_blend_ * (apply_svf(dry_r, svf_s1_r_, svf_s2_r_) - dry_r);
        }

        float wet_l = process_sample(in_l, tone_l_, dc_xl_, dc_yl_, sr_cnt_l_, sr_hld_l_, prev_l_);
        float wet_r = process_sample(in_r, tone_r_, dc_xr_, dc_yr_, sr_cnt_r_, sr_hld_r_, prev_r_);

        // Optional post-filter: blend between unfiltered and filtered output
        if (filter_enabled_ && filter_pos_ == 1) {
            wet_l = wet_l + filter_blend_ * (apply_svf(wet_l, svf_s1_l_, svf_s2_l_) - wet_l);
            wet_r = wet_r + filter_blend_ * (apply_svf(wet_r, svf_s1_r_, svf_s2_r_) - wet_r);
        }

        left[i]  = dry_l + mix_ * (wet_l - dry_l);
        right[i] = dry_r + mix_ * (wet_r - dry_r);
    }
}

} // namespace kaos_engine
