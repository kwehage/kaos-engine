#include "compressor_processor.h"
#include <cmath>

namespace kaos_engine {

// ── Setup ─────────────────────────────────────────────────────────────────────

void CompressorProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    update_coeffs();
    reset();
}

void CompressorProcessor::reset()
{
    gain_smooth_ = 0.0f;
    out_l_prev_  = 0.0f;
    out_r_prev_  = 0.0f;
    gr_meter_.store(0.0f);
}

// ── Setters ───────────────────────────────────────────────────────────────────

void CompressorProcessor::set_algorithm(CompressorAlgorithm a) { algo_ = a; update_coeffs(); reset(); }
void CompressorProcessor::set_threshold(float db)  { threshold_db_ = db; }
void CompressorProcessor::set_ratio    (float r)   { ratio_ = std::max(1.0f, r); update_coeffs(); }
void CompressorProcessor::set_knee     (float db)  { knee_db_ = std::max(0.0f, db); }
void CompressorProcessor::set_makeup   (float db)  { makeup_db_ = db; makeup_lin_ = std::pow(10.0f, db / 20.0f); }
void CompressorProcessor::set_output   (float db)  { output_db_ = db; output_lin_ = std::pow(10.0f, db / 20.0f); }
void CompressorProcessor::set_mix      (float v)   { mix_ = std::clamp(v, 0.0f, 1.0f); }

void CompressorProcessor::set_attack(float ms)
{
    attack_ms_ = ms;
    update_coeffs();
}

void CompressorProcessor::set_release(float ms)
{
    release_ms_ = ms;
    update_coeffs();
}

void CompressorProcessor::update_coeffs()
{
    const float fs = float(sample_rate_);
    auto alpha = [&](float ms) {
        return std::exp(-1.0f / std::max(0.1f, ms * 0.001f * fs));
    };

    alpha_a_   = alpha(attack_ms_);
    alpha_r_   = alpha(release_ms_);

    // Optical: fast release = user time; slow tail = 10× user time
    opt_r_fast_ = alpha(release_ms_);
    opt_r_slow_ = alpha(release_ms_ * 10.0f);

    // FET: attack speed scales with ratio — higher ratio → faster attack.
    // Clamp so α_a never goes below 0.5 (prevents clicks at extreme settings).
    const float fet_attack_effective = attack_ms_ / std::max(1.0f, ratio_ * 0.5f);
    fet_alpha_a_ = std::max(0.5f, alpha(fet_attack_effective));
}

// ── Gain computer (Giannoulis 2012, hard or soft knee) ────────────────────────

float CompressorProcessor::gain_computer(float level_db) const
{
    const float x = level_db - threshold_db_;
    const float W = knee_db_;

    if (2.0f * x < -W) {
        return 0.0f;  // below knee: no reduction
    }
    if (2.0f * std::abs(x) <= W) {
        // Parabolic soft-knee transition
        const float t = (x + W * 0.5f);
        return -(t * t) / (2.0f * W) * (1.0f - 1.0f / ratio_);
    }
    // Above full threshold: fixed ratio
    return x * (1.0f / ratio_ - 1.0f);
}

// ── Processing ────────────────────────────────────────────────────────────────

void CompressorProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // ── Level detection ────────────────────────────────────────────────────
        float level_db;
        if (algo_ == CompressorAlgorithm::FET) {
            // Feed-back: detect from previous output (post-VCA signal)
            const float lsq = out_l_prev_ * out_l_prev_ + out_r_prev_ * out_r_prev_;
            level_db = 10.0f * std::log10(std::max(lsq * 0.5f, 1e-20f));
        } else {
            // Feed-forward: detect from dry input (stereo-linked RMS of instant sample)
            const float lsq = dry_l * dry_l + dry_r * dry_r;
            level_db = 10.0f * std::log10(std::max(lsq * 0.5f, 1e-20f));
        }

        // ── Gain computer ──────────────────────────────────────────────────────
        const float gc = gain_computer(level_db);

        // ── Attack / release smoother ──────────────────────────────────────────
        if (algo_ == CompressorAlgorithm::Optical) {
            if (gc < gain_smooth_) {
                // Compression deepening → attack
                gain_smooth_ = alpha_a_ * gain_smooth_ + (1.0f - alpha_a_) * gc;
            } else {
                // Release: blend fast/slow based on current GR depth.
                // Deep compression → slow tail; light compression → snappy release.
                const float depth  = std::min(1.0f, std::abs(gain_smooth_) / 12.0f);
                const float alpha  = opt_r_fast_ + depth * (opt_r_slow_ - opt_r_fast_);
                gain_smooth_ = alpha * gain_smooth_ + (1.0f - alpha) * gc;
            }
        } else if (algo_ == CompressorAlgorithm::FET) {
            // FET uses ratio-scaled attack
            const float alpha_a = (gc < gain_smooth_) ? fet_alpha_a_ : alpha_r_;
            gain_smooth_ = alpha_a * gain_smooth_ + (1.0f - alpha_a) * gc;
        } else {
            // VCA: standard program-independent attack/release
            const float alpha = (gc < gain_smooth_) ? alpha_a_ : alpha_r_;
            gain_smooth_ = alpha * gain_smooth_ + (1.0f - alpha) * gc;
        }

        // ── Apply gain ─────────────────────────────────────────────────────────
        const float gain_lin = makeup_lin_ * std::pow(10.0f, gain_smooth_ / 20.0f);
        const float wet_l    = dry_l * gain_lin;
        const float wet_r    = dry_r * gain_lin;

        left[i]  = output_lin_ * (dry_l + mix_ * (wet_l - dry_l));
        right[i] = output_lin_ * (dry_r + mix_ * (wet_r - dry_r));

        // Save for FET feedback path
        out_l_prev_ = left[i];
        out_r_prev_ = right[i];
    }

    // Update meter (last sample's GR, ≤ 0 dB)
    gr_meter_.store(gain_smooth_);
}

} // namespace kaos_engine
