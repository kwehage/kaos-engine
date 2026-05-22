#include "noise_processor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {

void NoiseProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    ch_l_.seed(0xdeadbeef);
    ch_r_.seed(0xfeedface);
    ch_prev_.seed(0xcafebabe);
    update_grain_params();
    update_gate_coeffs();
    modal_dirty_ = true;
    reset();
}

void NoiseProcessor::update_gate_coeffs()
{
    const float fs = float(sample_rate_);
    if (fs <= 0.0f) return;
    gate_alpha_a_ = 1.0f - std::exp(-1.0f / (attack_ms_  * 0.001f * fs));
    gate_alpha_r_ = 1.0f - std::exp(-1.0f / (release_ms_ * 0.001f * fs));
}

void NoiseProcessor::reset()
{
    for (auto& g : grains_) g = {};
    spawn_phase_      = 0.0f;
    gate_env_         = 0.0f;
    gate_smooth_      = 0.0f;
    infrasonic_phase_ = 0.0f;
    roughness_phase_  = 0.0f;
    sr_phase_         = 0.0f;
    sr_hold_l_        = 0.0f;
    sr_hold_r_        = 0.0f;
}

void NoiseProcessor::update_grain_params()
{
    const float fs = float(sample_rate_);
    grain_size_samples_ = grain_size_ms_ * 0.001f * fs;
    // spawn_inc_: fraction of full-rate at which we attempt new grains
    // density 0..1 maps to 0..30 grains/second average
    spawn_inc_ = (density_ * 30.0f) / fs;
    // Simplex evolution rate: 0.5 Hz at SIZE=5ms, 50 Hz at SIZE=500ms (log scale)
    const float log_t = (grain_size_ms_ - 5.0f) / 495.0f;
    simplex_step_ = 0.5f * std::pow(100.0f, log_t) / fs;
}

void NoiseProcessor::try_spawn_grain()
{
    for (auto& g : grains_) {
        if (!g.active) {
            g.phase  = 0.0f;
            g.inc    = 1.0f / grain_size_samples_;
            g.active = true;
            return;
        }
    }
}

// Returns one noise sample for the given channel according to current type.
float NoiseProcessor::noise_sample(NoiseChannel& ch) const
{
    switch (type_) {
        case NoiseType::Pink:   return const_cast<NoiseChannel&>(ch).pink();
        case NoiseType::Brown:  return const_cast<NoiseChannel&>(ch).brown();
        case NoiseType::Lorenz: {
            const float dt  = grain_size_ms_ * 0.0002f;          // 5ms->0.001, 50ms->0.01, 500ms->0.1
            const float rho = 24.0f + density_ * 11.0f;          // 24 (ordered) to 35 (chaotic)
            return const_cast<NoiseChannel&>(ch).lorenz(dt, rho);
        }
        case NoiseType::FeedbackComb: {
            const int delay = std::max(1, std::min(int(grain_size_samples_),
                                                   NoiseChannel::kCombDelayCap - 1));
            return const_cast<NoiseChannel&>(ch).comb(density_ * 0.98f, delay);
        }
        case NoiseType::Simplex: {
            const int   octaves     = 1 + int(density_ * 7.0f);
            const float persistence = 0.5f + mod_ * 0.45f;
            return const_cast<NoiseChannel&>(ch).simplex(simplex_step_, octaves, persistence);
        }
        case NoiseType::Gendyn: {
            const int   n_breaks = 2 + int(density_ * 14.0f);
            const float base_dur = grain_size_samples_ / float(n_breaks);
            const float min_dur  = std::max(1.0f, base_dur * 0.1f);
            return const_cast<NoiseChannel&>(ch).gendyn(
                mod_ * 0.3f, mod_ * base_dur * 0.3f, base_dur, min_dur, n_breaks);
        }
        case NoiseType::Duffing: {
            const float dt    = grain_size_ms_ * 0.0002f;   // 5ms->0.001, 50ms->0.01, 500ms->0.1
            const float gamma = 0.3f + density_ * 0.9f;     // 0.3=near-periodic, 1.2=strongly chaotic
            const float omega = 0.9f + mod_ * 0.6f;         // 0.9-1.5 forcing frequency
            return const_cast<NoiseChannel&>(ch).duffing(dt, gamma, omega);
        }
        case NoiseType::DomainWarp: {
            const int   octaves = 1 + int(density_ * 7.0f);
            const float warp    = mod_ * 8.0f;  // 0=regular fBm, 8=heavy turbulent warp
            return const_cast<NoiseChannel&>(ch).domain_warp(simplex_step_, octaves, warp);
        }
        case NoiseType::Velvet: {
            const float density_hz = 200.0f + density_ * 5800.0f;  // 200-6000 pulses/sec
            return const_cast<NoiseChannel&>(ch).velvet(density_hz, float(sample_rate_));
        }
        case NoiseType::MissingFund: {
            // SIZE maps grain_size_ms_ (5-500ms) to f0 (15-80 Hz)
            const float f0      = 15.0f + (grain_size_ms_ - 5.0f) / 495.0f * 65.0f;
            const float rolloff = 0.30f + density_ * 0.65f;  // 0.30=sparse harmonics, 0.95=rich
            return const_cast<NoiseChannel&>(ch).missing_fund(f0, float(sample_rate_), rolloff, mod_);
        }
        case NoiseType::Blue:
            return const_cast<NoiseChannel&>(ch).blue();
        case NoiseType::Chua: {
            const float dt    = grain_size_ms_ * 0.0002f;   // 5ms->0.001, 500ms->0.1
            const float alpha = 9.0f + density_ * 3.0f;     // 9-12: all values in chaotic regime
            return const_cast<NoiseChannel&>(ch).chua(dt, alpha);
        }
        case NoiseType::HarshWall: {
            // scale 1-15: SIZE 5ms->1 (bright ~6kHz center), SIZE 500ms->15 (dark ~200Hz center)
            const int   scale = 1 + int((grain_size_ms_ - 5.0f) / 495.0f * 14.0f);
            const float f     = 0.50f + density_ * 0.45f;  // 0.50-0.95: subtle coloring to strong wall
            const float drive = 1.0f  + mod_     * 14.0f;  // 1-15: warm/soft to harsh/hard-clip
            return const_cast<NoiseChannel&>(ch).harsh_wall(scale, f, drive);
        }
        default: return const_cast<NoiseChannel&>(ch).randbi();
    }
}

// Input-derived noise types: produce a sample correlated with x.
float NoiseProcessor::derived_noise_sample(NoiseChannel& ch, float x) const
{
    auto& c = const_cast<NoiseChannel&>(ch);
    switch (type_) {
        case NoiseType::Residual: {
            const float fs    = float(sample_rate_);
            const float fc    = 1000.0f / grain_size_ms_;
            const float alpha = std::exp(-2.0f * float(M_PI) * fc / fs);
            return std::tanh(c.residual(x, alpha) * 4.0f);
        }
        case NoiseType::Coupled:
            return c.coupled(x, density_);
        case NoiseType::Diffuse: {
            const float g = 0.1f + density_ * 0.8f;
            return std::tanh(c.diffuse(x, g) * 4.0f);
        }
        case NoiseType::Modal: {
            // Drive the pre-computed resonator bank with the input sample.
            // modal_b0_/a1_/a2_ are populated by update_modal_coeffs() before
            // this call (checked at the top of process()).
            float out = 0.0f;
            for (int k = 0; k < modal_n_active_; ++k) {
                const float y = modal_b0_[k] * x
                              - modal_a1_[k] * ch.modal_y1_[k]
                              - modal_a2_[k] * ch.modal_y2_[k];
                ch.modal_y2_[k] = ch.modal_y1_[k];
                ch.modal_y1_[k] = y;
                out += y / float(k + 1);   // decreasing per-mode weight
            }
            return std::tanh(out * 4.0f);
        }
        case NoiseType::SimplexDriven: {
            const float env_alpha = 0.999f;
            const int   octaves   = 1 + int(density_ * 7.0f);
            return const_cast<NoiseChannel&>(ch).simplex_driven(
                x, simplex_step_, env_alpha, mod_, octaves);
        }
        case NoiseType::FrictionScrape: {
            // Stick-slip bow model: smooth proportional force while sticking,
            // saturated + noisy force while slipping. Threshold scales with DENSITY
            // (which also sets modal inharmonicity B): low DENSITY = slips easily,
            // high DENSITY = stiffer bow, harder to slip.
            c.friction_vel_ = c.friction_vel_ * 0.997f + std::abs(x) * 0.003f;
            const float vel   = c.friction_vel_;
            const float thresh = 0.02f + density_ * 0.08f;
            const float excite = (vel < thresh)
                ? (vel / thresh)
                : (std::tanh((vel - thresh) / thresh * 8.0f) + c.randbi() * 0.25f);
            float out = 0.0f;
            for (int k = 0; k < modal_n_active_; ++k) {
                const float y = modal_b0_[k] * excite
                              - modal_a1_[k] * c.modal_y1_[k]
                              - modal_a2_[k] * c.modal_y2_[k];
                c.modal_y2_[k] = c.modal_y1_[k];
                c.modal_y1_[k] = y;
                out += y / float(k + 1);
            }
            return std::tanh(out * 4.0f);
        }
        case NoiseType::GendynDriven: {
            // Input level adds to mutation rate: quiet = near-frozen waveform,
            // loud playing = rapid stochastic mutation toward noise.
            const float driven_mod = std::clamp(mod_ + std::abs(x) * 2.0f, 0.0f, 1.0f);
            const int   n_breaks   = 2 + int(density_ * 14.0f);
            const float base_dur   = grain_size_samples_ / float(n_breaks);
            const float min_dur    = std::max(1.0f, base_dur * 0.1f);
            return c.gendyn(driven_mod * 0.3f, driven_mod * base_dur * 0.3f,
                            base_dur, min_dur, n_breaks);
        }
        case NoiseType::KarplusStrong: {
            // SIZE 5ms→2000 Hz (metallic high bell), SIZE 500ms→50 Hz (deep plate).
            const float freq = 2000.0f * std::pow(0.025f, (grain_size_ms_ - 5.0f) / 495.0f);
            const int   D    = std::max(2, std::min(int(sample_rate_ / freq),
                                                    NoiseChannel::kKsCap - 1));
            const float lp    = 0.99f - density_ * 0.09f;  // 0.99=bright/long, 0.90=dark/short
            const float stiff = mod_ * 0.97f;               // 0=harmonic, 0.97=inharmonic
            return c.ks(x, D, lp, stiff);
        }
        default: return 0.0f;
    }
}

void NoiseProcessor::update_modal_coeffs()
{
    const float sr = float(sample_rate_);
    if (sr <= 0.0f) return;

    // SIZE maps to fundamental frequency of mode 1 (100 Hz at 5ms, 2000 Hz at 500ms).
    const float f1  = 100.0f * std::pow(20.0f, (grain_size_ms_ - 5.0f) / 495.0f);
    // DENSITY maps to inharmonicity coefficient B (0 = harmonic bar, 0.05 = strongly inharmonic).
    const float B   = density_ * 0.05f;
    // MOD maps to T60 decay time (0.1 s = percussive, 6 s = bell-like ring).
    const float T60 = 0.1f + mod_ * 5.9f;

    modal_n_active_ = 0;
    for (int k = 1; k <= kModalModes; ++k) {
        // Inharmonic bar/plate frequency series: f_k = f1 * k^2 * sqrt(1 + B*k^2)
        const float fk = f1 * float(k) * float(k)
                       * std::sqrt(1.0f + B * float(k) * float(k));
        if (fk >= sr * 0.49f) break;

        // Frequency-dependent decay: higher modes ring shorter (physically realistic).
        const float T60_k = T60 / std::sqrt(float(k));
        const float r     = std::exp(-6.908f / (T60_k * sr));
        const float omega = 2.0f * float(M_PI) * fk / sr;
        const int   i     = k - 1;

        modal_b0_[i] = 1.0f - r;
        modal_a1_[i] = -2.0f * r * std::cos(omega);
        modal_a2_[i] = r * r;
        ++modal_n_active_;
    }
    modal_dirty_ = false;
}

float NoiseProcessor::granular_sample(NoiseChannel& ch)
{
    float out = 0.0f;
    for (auto& g : grains_) {
        if (!g.active) continue;
        // Hann window
        const float window = 0.5f * (1.0f - std::cos(float(2.0 * M_PI) * g.phase));
        out += window * ch.randbi();
        g.phase += g.inc;
        if (g.phase >= 1.0f) g.active = false;
    }
    return out;
}

void NoiseProcessor::process(float* left, float* right, int num_samples)
{
    if (modal_dirty_)
        update_modal_coeffs();

    for (int i = 0; i < num_samples; ++i) {
        // Derive gate/envelope modulator from input
        if (mode_ == NoiseMode::AlwaysOn) {
            gate_smooth_ = 1.0f;
        } else {
            const float level = std::max(std::abs(left[i]), std::abs(right[i]));
            float target;
            if (mode_ == NoiseMode::Follow) {
                // Proportional: target tracks the actual signal level above threshold.
                // Below threshold the target falls to 0, so noise fades out.
                target = (level > threshold_lin_) ? level : 0.0f;
            } else {
                // Gated: binary 0/1 target, smoothed by attack/release.
                target = (level > threshold_lin_) ? 1.0f : 0.0f;
            }
            const float alpha = (target > gate_env_) ? gate_alpha_a_ : gate_alpha_r_;
            gate_env_ += alpha * (target - gate_env_);
            gate_smooth_ = gate_env_;
        }

        float nl, nr;
        if (type_ == NoiseType::Granular) {
            spawn_phase_ += spawn_inc_;
            if (spawn_phase_ >= 1.0f) {
                spawn_phase_ -= 1.0f;
                try_spawn_grain();
            }
            nl = granular_sample(ch_l_);
            nr = granular_sample(ch_r_);
        } else if (type_ == NoiseType::Residual       ||
                   type_ == NoiseType::Coupled        ||
                   type_ == NoiseType::Diffuse         ||
                   type_ == NoiseType::Modal           ||
                   type_ == NoiseType::SimplexDriven   ||
                   type_ == NoiseType::FrictionScrape  ||
                   type_ == NoiseType::GendynDriven    ||
                   type_ == NoiseType::KarplusStrong) {
            nl = derived_noise_sample(ch_l_, left [i]);
            nr = derived_noise_sample(ch_r_, right[i]);
        } else {
            nl = noise_sample(ch_l_);
            nr = noise_sample(ch_r_);
        }

        const float noise_l = nl * gain_ * gate_smooth_;
        const float noise_r = nr * gain_ * gate_smooth_;

        float out_l, out_r;
        switch (blend_) {
            case NoiseBlend::AM:
                out_l = left [i] * (1.0f + mix_ * noise_l);
                out_r = right[i] * (1.0f + mix_ * noise_r);
                break;
            case NoiseBlend::Saturate:
                out_l = (1.0f - mix_) * left [i] + mix_ * std::tanh(left [i] + mod_ * noise_l);
                out_r = (1.0f - mix_) * right[i] + mix_ * std::tanh(right[i] + mod_ * noise_r);
                break;
            case NoiseBlend::RingMod:
                // Suppressed-carrier AM: y = (1-mix)*x + mix*(x*n).
                // At mix=1, only sidebands remain (no dry signal).
                out_l = (1.0f - mix_) * left [i] + mix_ * left [i] * noise_l;
                out_r = (1.0f - mix_) * right[i] + mix_ * right[i] * noise_r;
                break;
            case NoiseBlend::SampleRate: {
                const float factor = 1.0f + mod_ * 31.0f;  // 1-32x decimation
                sr_phase_ += 1.0f;
                if (sr_phase_ >= factor) {
                    sr_phase_ -= factor;
                    sr_hold_l_ = left [i];
                    sr_hold_r_ = right[i];
                }
                out_l = (1.0f - mix_) * left [i] + mix_ * sr_hold_l_;
                out_r = (1.0f - mix_) * right[i] + mix_ * sr_hold_r_;
                break;
            }
            case NoiseBlend::Roughness: {
                const float fc        = 20.0f + mod_ * 180.0f;  // 20-200 Hz carrier
                const float phase_inc = fc / float(sample_rate_);
                const float carrier   = std::sin(2.0f * float(M_PI) * roughness_phase_);
                roughness_phase_ += phase_inc;
                if (roughness_phase_ >= 1.0f) roughness_phase_ -= 1.0f;
                out_l = (1.0f - mix_) * left [i] + mix_ * left [i] * carrier;
                out_r = (1.0f - mix_) * right[i] + mix_ * right[i] * carrier;
                break;
            }
            case NoiseBlend::InfrasonicAM: {
                const float freq      = 0.1f + mod_ * 18.9f;   // 0.1-19 Hz
                const float phase_inc = freq / float(sample_rate_);
                const float lfo = std::sin(2.0f * float(M_PI) * infrasonic_phase_);
                infrasonic_phase_ += phase_inc;
                if (infrasonic_phase_ >= 1.0f) infrasonic_phase_ -= 1.0f;
                out_l = left [i] * (1.0f + mix_ * lfo);
                out_r = right[i] * (1.0f + mix_ * lfo);
                break;
            }
            default: { // Add
                const float dry = 1.0f - mix_;
                out_l = dry * left [i] + mix_ * noise_l;
                out_r = dry * right[i] + mix_ * noise_r;
                break;
            }
        }
        left [i] = output_lin_ * out_l;
        right[i] = output_lin_ * out_r;
    }
}

float NoiseProcessor::next_preview_sample()
{
    if (type_ == NoiseType::Granular) {
        spawn_phase_ += spawn_inc_;
        if (spawn_phase_ >= 1.0f) {
            spawn_phase_ -= 1.0f;
            try_spawn_grain();
        }
        return granular_sample(ch_prev_) * gain_;
    }
    // Input-derived types have no signal in preview context.
    if (type_ == NoiseType::Residual       ||
        type_ == NoiseType::Coupled        ||
        type_ == NoiseType::Diffuse         ||
        type_ == NoiseType::Modal           ||
        type_ == NoiseType::SimplexDriven   ||
        type_ == NoiseType::FrictionScrape  ||
        type_ == NoiseType::GendynDriven    ||
        type_ == NoiseType::KarplusStrong)
        return 0.0f;
    return noise_sample(ch_prev_) * gain_;
}

} // namespace kaos_engine
