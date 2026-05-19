#include "filter_processor.h"
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void FilterProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    // Allocate comb delay buffer — max delay at 20 Hz fundamental.
    const int max_delay = int(std::ceil(sample_rate / 20.0)) + 4;
    comb_l_.assign(max_delay, 0.0f);
    comb_r_.assign(max_delay, 0.0f);
    update_coeffs();
    reset();
}

void FilterProcessor::reset()
{
    s1a_l_ = s1a_r_ = s2a_l_ = s2a_r_ = 0.0f;
    s1b_l_ = s1b_r_ = s2b_l_ = s2b_r_ = 0.0f;
    bq_w1_l_ = bq_w1_r_ = bq_w2_l_ = bq_w2_r_ = 0.0f;
    std::fill(lad_l_, lad_l_ + 4, 0.0f);
    std::fill(lad_r_, lad_r_ + 4, 0.0f);
    if (!comb_l_.empty()) std::fill(comb_l_.begin(), comb_l_.end(), 0.0f);
    if (!comb_r_.empty()) std::fill(comb_r_.begin(), comb_r_.end(), 0.0f);
    comb_write_ = 0;
}

// ── Coefficient update ────────────────────────────────────────────────────────

void FilterProcessor::update_coeffs()
{
    const float fs  = float(sample_rate_);
    const float fc  = std::min(cutoff_hz_, fs * 0.499f);

    switch (mode_) {

    case FilterMode::LP12: case FilterMode::LP24:
    case FilterMode::HP12: case FilterMode::HP24:
    case FilterMode::BandPass: case FilterMode::Notch: case FilterMode::AllPass: {
        // Simper SVF
        svf_g_  = std::tan(float(M_PI) * fc / fs);
        svf_k_  = 1.0f / resonance_;
        svf_a1_ = 1.0f / (1.0f + svf_g_ * (svf_g_ + svf_k_));
        svf_a2_ = svf_g_ * svf_a1_;
        svf_a3_ = svf_g_ * svf_a2_;
        break;
    }

    case FilterMode::Peak: {
        // RBJ peaking EQ
        const float A  = std::pow(10.0f, gain_db_ / 40.0f);
        const float w0 = 2.0f * float(M_PI) * fc / fs;
        const float s  = std::sin(w0);
        const float c  = std::cos(w0);
        const float al = s / (2.0f * resonance_);
        const float a0 = 1.0f + al / A;
        bq_b0_ = (1.0f + al * A) / a0;
        bq_b1_ = (-2.0f * c)     / a0;
        bq_b2_ = (1.0f - al * A) / a0;
        bq_a1_ = (-2.0f * c)     / a0;
        bq_a2_ = (1.0f - al / A) / a0;
        break;
    }

    case FilterMode::LowShelf: {
        const float A  = std::pow(10.0f, gain_db_ / 40.0f);
        const float w0 = 2.0f * float(M_PI) * fc / fs;
        const float s  = std::sin(w0);
        const float c  = std::cos(w0);
        const float al = s / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 1.0f - 1.0f) + 2.0f);
        // Simplified: slope=1 shelf
        const float sqA = std::sqrt(A);
        const float a0  = (A+1.0f) + (A-1.0f)*c + 2.0f*sqA*al;
        bq_b0_ = (A * ((A+1.0f) - (A-1.0f)*c + 2.0f*sqA*al)) / a0;
        bq_b1_ = (2.0f * A * ((A-1.0f) - (A+1.0f)*c))         / a0;
        bq_b2_ = (A * ((A+1.0f) - (A-1.0f)*c - 2.0f*sqA*al))  / a0;
        bq_a1_ = (-2.0f * ((A-1.0f) + (A+1.0f)*c))             / a0;
        bq_a2_ = ((A+1.0f) + (A-1.0f)*c - 2.0f*sqA*al)         / a0;
        break;
    }

    case FilterMode::HiShelf: {
        const float A  = std::pow(10.0f, gain_db_ / 40.0f);
        const float w0 = 2.0f * float(M_PI) * fc / fs;
        const float s  = std::sin(w0);
        const float c  = std::cos(w0);
        const float sqA = std::sqrt(A);
        const float al   = s / 2.0f;
        const float a0   = (A+1.0f) - (A-1.0f)*c + 2.0f*sqA*al;
        bq_b0_ = (A * ((A+1.0f) + (A-1.0f)*c + 2.0f*sqA*al)) / a0;
        bq_b1_ = (-2.0f * A * ((A-1.0f) + (A+1.0f)*c))        / a0;
        bq_b2_ = (A * ((A+1.0f) + (A-1.0f)*c - 2.0f*sqA*al))  / a0;
        bq_a1_ = (2.0f * ((A-1.0f) - (A+1.0f)*c))              / a0;
        bq_a2_ = ((A+1.0f) - (A-1.0f)*c - 2.0f*sqA*al)         / a0;
        break;
    }

    case FilterMode::Comb: {
        // feedback gain: resonance_ maps [0.05..20] → [0.0..0.98]
        comb_fb_ = std::min(resonance_ / 21.0f, 0.98f) * 20.0f;
        comb_fb_ = std::clamp(comb_fb_, 0.0f, 0.98f);
        const int max_d = int(comb_l_.size()) - 1;
        comb_delay_ = std::clamp(int(std::round(float(sample_rate_) / fc)), 1, max_d);
        // Damping LP in feedback path: drive_ 0→1 maps to full-bright→heavily-damped.
        // LP cutoff ranges from ~20 kHz (no effect) down to ~200 Hz (kills most harmonics).
        {
            const float damp_fc = 20000.0f * std::pow(0.01f, drive_);  // 20kHz..200Hz
            const float w = float(M_PI) * damp_fc / float(sample_rate_);
            comb_damp_ = std::exp(-w);  // one-pole LP alpha ≈ e^(-2πfc/fs)
        }
        break;
    }

    case FilterMode::Ladder: {
        lad_f_ = std::min(2.0f * std::sin(float(M_PI) * fc / fs), 0.98f);
        // resonance_ [0.05..20] → k [0..4]; k=4 = self-oscillation
        lad_k_ = std::clamp(resonance_ / 5.0f, 0.0f, 3.99f);
        break;
    }

    default: break;
    }
}

// ── Per-sample helpers ────────────────────────────────────────────────────────

inline float FilterProcessor::svf_sample(float x, float& s1, float& s2) const
{
    const float v1 = svf_a1_ * s1 + svf_a2_ * (x - s2);
    const float lp = s2 + svf_a2_ * s1 + svf_a3_ * (x - s2);
    s1 = 2.0f * v1 - s1;
    s2 = 2.0f * lp - s2;

    switch (mode_) {
    case FilterMode::LP12: case FilterMode::LP24: return lp;
    case FilterMode::HP12: case FilterMode::HP24: return x - svf_k_ * v1 - lp;
    case FilterMode::BandPass:                    return v1;
    case FilterMode::Notch:                       return x - svf_k_ * v1;
    case FilterMode::AllPass:                     return x - 2.0f * svf_k_ * v1;
    default:                                      return lp;
    }
}

inline float FilterProcessor::bq_sample(float x, float& w1, float& w2) const
{
    const float y = bq_b0_ * x + w1;
    w1 = bq_b1_ * x - bq_a1_ * y + w2;
    w2 = bq_b2_ * x - bq_a2_ * y;
    return y;
}

inline float FilterProcessor::ladder_sample(float x, float* s) const
{
    const float u = std::tanh(x - lad_k_ * s[3]);
    s[0] += lad_f_ * (u    - s[0]);
    s[1] += lad_f_ * (s[0] - s[1]);
    s[2] += lad_f_ * (s[1] - s[2]);
    s[3] += lad_f_ * (s[2] - s[3]);
    return s[3];
}

// ── Block processing ──────────────────────────────────────────────────────────

void FilterProcessor::process(float* left, float* right, int n)
{
    const bool use_svf     = (mode_ <= FilterMode::AllPass);
    const bool use_svf_24  = (mode_ == FilterMode::LP24 || mode_ == FilterMode::HP24);
    const bool use_bq      = (mode_ == FilterMode::Peak || mode_ == FilterMode::LowShelf
                               || mode_ == FilterMode::HiShelf);
    const bool use_comb    = (mode_ == FilterMode::Comb);
    (void)(mode_ == FilterMode::Ladder);  // ladder handled in else branch

    const int  mask        = int(comb_l_.size()) - 1;
    const bool do_drive    = (drive_ > 0.001f);
    const float drive_g    = 1.0f + drive_ * 4.0f;  // gain before tanh

    for (int i = 0; i < n; ++i) {
        float l = left[i];
        float r = right[i];

        // Comb repurposes drive as feedback damping — no pre-filter saturation.
        if (do_drive && !use_comb) {
            l = std::tanh(drive_g * l) / drive_g;
            r = std::tanh(drive_g * r) / drive_g;
        }

        float wl, wr;

        if (use_svf) {
            wl = svf_sample(l, s1a_l_, s2a_l_);
            wr = svf_sample(r, s1a_r_, s2a_r_);
            if (use_svf_24) {
                wl = svf_sample(wl, s1b_l_, s2b_l_);
                wr = svf_sample(wr, s1b_r_, s2b_r_);
            }
        } else if (use_bq) {
            wl = bq_sample(l, bq_w1_l_, bq_w2_l_);
            wr = bq_sample(r, bq_w1_r_, bq_w2_r_);
        } else if (use_comb) {
            const int rd = (comb_write_ - comb_delay_ + int(comb_l_.size())) & mask;
            // One-pole LP on the delayed signal before adding back (damping).
            // When drive_=0: comb_damp_≈1, LP passes everything (full brightness).
            // When drive_=1: comb_damp_ cuts most harmonics, leaving only the fundamental.
            comb_damp_l_ = comb_damp_ * comb_damp_l_ + (1.0f - comb_damp_) * comb_l_[rd];
            comb_damp_r_ = comb_damp_ * comb_damp_r_ + (1.0f - comb_damp_) * comb_r_[rd];
            wl = l + comb_fb_ * comb_damp_l_;
            wr = r + comb_fb_ * comb_damp_r_;
            comb_l_[comb_write_ & mask] = wl;
            comb_r_[comb_write_ & mask] = wr;
            ++comb_write_;
        } else { // Ladder
            wl = ladder_sample(l, lad_l_);
            wr = ladder_sample(r, lad_r_);
        }

        left[i]  = output_lin_ * (left[i]  + mix_ * (wl - left[i]));
        right[i] = output_lin_ * (right[i] + mix_ * (wr - right[i]));
    }
}

} // namespace kaos_engine
