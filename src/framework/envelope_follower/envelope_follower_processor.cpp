#include "envelope_follower_processor.h"
#include <cmath>

namespace kaos_engine {

void EnvelopeFollowerProcessor::prepare(double sr, int /*bs*/)
{
    sample_rate_ = sr;
    update_coeffs();
    reset();
}

void EnvelopeFollowerProcessor::reset()
{
    env_          = 0.0f;
    rms_state_    = 0.0f;
    env_prev_     = 0.0f;
    deriv_smooth_ = 0.0f;
    env_peak_     = 0.0f;
    last_output_  = 0.0f;
}

void EnvelopeFollowerProcessor::update_coeffs()
{
    const float fs = float(sample_rate_);
    alpha_a_      = std::clamp(1.0f - std::exp(-1.0f / (attack_ms_  * 0.001f * fs)), 0.0f, 1.0f);
    alpha_r_      = std::clamp(1.0f - std::exp(-1.0f / (release_ms_ * 0.001f * fs)), 0.0f, 1.0f);
    // Peak holder decays over ~2 seconds so each note gets a fresh peak context.
    alpha_peak_r_ = std::exp(-1.0f / (2000.0f * 0.001f * fs));
}

float EnvelopeFollowerProcessor::process_sample(float left, float right)
{
    // Stereo-linked detection with pre-gain
    const float mono    = (left + right) * 0.5f * gain_;
    const float mono_sq = mono * mono;

    float detected;
    if (detector_ == EnvelopeDetector::Peak) {
        detected = std::abs(mono);
    } else {
        rms_state_ = alpha_r_ * mono_sq + (1.0f - alpha_r_) * rms_state_;
        detected   = std::sqrt(std::max(rms_state_, 0.0f));
    }
    detected += 1e-18f;  // denormal prevention

    // Attack / release ballistics
    if (detected > env_)
        env_ = alpha_a_ * detected + (1.0f - alpha_a_) * env_;
    else
        env_ = alpha_r_ * detected + (1.0f - alpha_r_) * env_;

    // Smoothed derivative: positive = rising, negative = falling.
    // Smoothing alpha ≈ 0.99 at 44.1 kHz gives a ~2 ms averaging window --
    // enough to suppress sample-to-sample noise while tracking envelope motion.
    const float raw_deriv = env_ - env_prev_;
    deriv_smooth_ = 0.99f * deriv_smooth_ + 0.01f * raw_deriv;
    env_prev_ = env_;

    // Peak tracker: instant attack, 2-second release.
    // Provides a stable reference for the Release output shape.
    if (env_ > env_peak_)
        env_peak_ = env_;
    else
        env_peak_ = alpha_peak_r_ * env_peak_ + (1.0f - alpha_peak_r_) * env_;

    // ── Output shape ───────────────────────────────────────────────────────────
    float cv;
    switch (shape_) {
    case EnvelopeOutputShape::Follow:
        cv = env_;
        break;

    case EnvelopeOutputShape::Duck:
        cv = 1.0f - env_;
        break;

    case EnvelopeOutputShape::Rise:
        // Active only while envelope is detectably rising.
        // A small threshold avoids false triggers during quiet noise floor.
        cv = (deriv_smooth_ > 1e-5f) ? env_ : 0.0f;
        break;

    case EnvelopeOutputShape::Fall:
        // Active only while envelope is detectably falling.
        cv = (deriv_smooth_ < -1e-5f) ? env_ : 0.0f;
        break;

    case EnvelopeOutputShape::Release: {
        // Rises from 0 -> 1 as the envelope falls from its peak back to silence.
        // cv = 0 while the signal is still near its peak (attack/sustain phase).
        // cv = (peak - env) / peak once the signal has dropped by >10% from peak.
        // This lets the user send a CV that grows throughout the decay/release of a note.
        const float drop_frac = (env_peak_ > 0.001f)
                              ? (env_peak_ - env_) / env_peak_
                              : 0.0f;
        // Suppress output during attack/sustain: require at least 10% drop from peak
        cv = (drop_frac > 0.10f) ? drop_frac : 0.0f;
        break;
    }

    default:
        cv = env_;
        break;
    }

    last_output_ = std::clamp(cv * depth_, 0.0f, 1.0f);
    return last_output_;
}

void EnvelopeFollowerProcessor::process(const float* src_l, const float* src_r,
                                         float* out, int n)
{
    for (int i = 0; i < n; ++i)
        out[i] = process_sample(src_l[i], src_r[i]);
}

} // namespace kaos_engine
