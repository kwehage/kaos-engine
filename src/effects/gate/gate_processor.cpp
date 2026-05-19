#include "gate_processor.h"
#include <cmath>

namespace kaos_engine {

// ── Setup ─────────────────────────────────────────────────────────────────────

void GateProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    update_coeffs();
    reset();
}

void GateProcessor::reset()
{
    gain_smooth_  = range_db_;
    hold_counter_ = 0;
    hpf_l_ = hpf_r_ = 0.0f;
    gr_meter_.store(range_db_);
    level_meter_.store(-100.0f);
    state_meter_.store(int(GateDisplayState::Closed));
}

// ── Setters ───────────────────────────────────────────────────────────────────

void GateProcessor::set_algorithm (GateAlgorithm a)  { algo_ = a; }
void GateProcessor::set_threshold (float db)          { threshold_db_  = db; }
void GateProcessor::set_range     (float db)          { range_db_ = std::min(-0.1f, db); update_coeffs(); }
void GateProcessor::set_ratio     (float r)           { ratio_ = std::max(1.0f, r); }
void GateProcessor::set_hysteresis(float db)          { hysteresis_db_ = std::max(0.0f, db); }
void GateProcessor::set_output    (float db)          { output_db_ = db; output_lin_ = std::pow(10.0f, db / 20.0f); }
void GateProcessor::set_mix       (float v)           { mix_ = std::clamp(v, 0.0f, 1.0f); }

void GateProcessor::set_attack(float ms)
{
    attack_ms_ = ms;
    update_coeffs();
}

void GateProcessor::set_hold(float ms)
{
    hold_ms_ = ms;
    update_coeffs();
}

void GateProcessor::set_release(float ms)
{
    release_ms_ = ms;
    update_coeffs();
}

void GateProcessor::update_coeffs()
{
    const float fs = float(sample_rate_);
    auto alpha = [&](float ms) {
        return std::exp(-1.0f / std::max(0.1f, ms * 0.001f * fs));
    };
    alpha_a_      = alpha(attack_ms_);
    alpha_r_      = alpha(release_ms_);
    hold_samples_ = std::max(0, int(hold_ms_ * 0.001f * fs));
    range_lin_    = std::pow(10.0f, range_db_ / 20.0f);
}

// ── Gain computer ─────────────────────────────────────────────────────────────
// Returns target gain in dB (0 = fully open, range_db_ = fully closed).

float GateProcessor::gain_computer(float level_db) const
{
    switch (algo_) {

    case GateAlgorithm::Gate: {
        // Binary: 0 dB when open, range_db_ when closed.
        // Hysteresis handled in the process loop; here just use threshold.
        return (level_db >= threshold_db_) ? 0.0f : range_db_;
    }

    case GateAlgorithm::Expander: {
        if (level_db >= threshold_db_) return 0.0f;
        const float excess = threshold_db_ - level_db;
        const float gr     = -excess * (1.0f - 1.0f / ratio_);
        return std::max(gr, range_db_);
    }

    case GateAlgorithm::Ducker: {
        // Attenuate when ABOVE threshold (opposite of gate).
        if (level_db <= threshold_db_) return 0.0f;
        const float excess = level_db - threshold_db_;
        const float gr     = -excess * (1.0f - 1.0f / ratio_);
        return std::max(gr, range_db_);
    }

    default:
        return 0.0f;
    }
}

// ── Processing ────────────────────────────────────────────────────────────────

void GateProcessor::process(float* left, float* right, int num_samples,
                             const float* key_l, const float* key_r)
{
    // 100 Hz 1-pole HPF coefficient (prevents subsonic content from triggering gate)
    const float hpf_alpha = std::exp(-2.0f * 3.14159265f * 100.0f / float(sample_rate_));

    // Close threshold = open threshold - hysteresis (for Gate/Expander).
    // Ducker uses the same threshold in both directions.
    const float thr_open  = threshold_db_;
    const float thr_close = threshold_db_ - hysteresis_db_;

    GateDisplayState disp = GateDisplayState::Closed;

    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // Use sidechain key when provided; fall back to self-detection.
        const float src_l = key_l ? key_l[i] : dry_l;
        const float src_r = key_r ? key_r[i] : dry_r;

        // ── 100 Hz HPF on detection signal (prevents bass from holding gate open) ──
        hpf_l_ = hpf_alpha * hpf_l_ + (1.0f - hpf_alpha) * src_l;
        hpf_r_ = hpf_alpha * hpf_r_ + (1.0f - hpf_alpha) * src_r;
        const float det_l = src_l - hpf_l_;
        const float det_r = src_r - hpf_r_;

        // ── Level detection: stereo-linked RMS of current sample pair ─────────────
        const float lsq      = det_l * det_l + det_r * det_r;
        const float level_db = 10.0f * std::log10(std::max(lsq * 0.5f, 1e-20f));

        // ── Gain target ───────────────────────────────────────────────────────────
        // For Gate: use hysteresis (two thresholds). For Expander/Ducker: single threshold.
        float target;
        if (algo_ == GateAlgorithm::Gate) {
            // Hysteresis: open only when level > thr_open; close only when < thr_close.
            if (level_db >= thr_open)
                target = 0.0f;
            else if (level_db < thr_close)
                target = range_db_;
            else
                // In the hysteresis band: keep previous state
                target = (gain_smooth_ > (range_db_ + 0.1f)) ? 0.0f : range_db_;
        } else {
            target = gain_computer(level_db);
        }

        // ── Attack / hold / release state machine ─────────────────────────────────
        if (target > gain_smooth_) {
            // Opening → attack phase
            hold_counter_ = hold_samples_;  // reset hold when opening
            gain_smooth_  = alpha_a_ * gain_smooth_ + (1.0f - alpha_a_) * target;
        } else if (target < gain_smooth_) {
            // Closing → check hold timer
            if (hold_counter_ > 0) {
                --hold_counter_;
                // During hold: maintain current gain (don't release yet)
            } else {
                gain_smooth_ = alpha_r_ * gain_smooth_ + (1.0f - alpha_r_) * target;
            }
        }

        // ── Apply gain ─────────────────────────────────────────────────────────────
        const float gain_lin = std::pow(10.0f, gain_smooth_ / 20.0f);
        const float wet_l    = dry_l * gain_lin;
        const float wet_r    = dry_r * gain_lin;

        left[i]  = output_lin_ * (dry_l + mix_ * (wet_l - dry_l));
        right[i] = output_lin_ * (dry_r + mix_ * (wet_r - dry_r));

        // Track display state (last sample wins; 30 Hz update rate is fast enough)
        if (gain_smooth_ > -1.0f)
            disp = GateDisplayState::Open;
        else if (hold_counter_ > 0)
            disp = GateDisplayState::Hold;
        else if (gain_smooth_ > range_db_ + 1.0f)
            disp = GateDisplayState::Release;
        else
            disp = GateDisplayState::Closed;

        level_meter_.store(level_db);
    }

    gr_meter_   .store(gain_smooth_);
    state_meter_.store(int(disp));
}

} // namespace kaos_engine
