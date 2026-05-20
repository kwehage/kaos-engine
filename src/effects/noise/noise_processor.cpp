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
    spawn_phase_ = 0.0f;
    gate_env_    = 0.0f;
    gate_smooth_ = 0.0f;
}

void NoiseProcessor::update_grain_params()
{
    const float fs = float(sample_rate_);
    grain_size_samples_ = grain_size_ms_ * 0.001f * fs;
    // spawn_inc_: fraction of full-rate at which we attempt new grains
    // density 0..1 maps to 0..30 grains/second average
    spawn_inc_ = (density_ * 30.0f) / fs;
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
        case NoiseType::Pink:  return const_cast<NoiseChannel&>(ch).pink();
        case NoiseType::Brown: return const_cast<NoiseChannel&>(ch).brown();
        default:               return const_cast<NoiseChannel&>(ch).randbi();
    }
}

// Input-derived noise types: produce a sample correlated with x.
float NoiseProcessor::derived_noise_sample(NoiseChannel& ch, float x) const
{
    auto& c = const_cast<NoiseChannel&>(ch);
    switch (type_) {
        case NoiseType::Residual: {
            // LP cutoff: fc = 1000 / grain_size_ms_ Hz
            const float fs    = float(sample_rate_);
            const float fc    = 1000.0f / grain_size_ms_;
            const float alpha = std::exp(-2.0f * float(M_PI) * fc / fs);
            // Normalize: residual amplitude <= |x| (signal-dependent).
            // tanh(n * 4) brings it to roughly ±1 range for typical signals,
            // giving similar modulation depth to the independent noise types.
            return std::tanh(c.residual(x, alpha) * 4.0f);
        }
        case NoiseType::Coupled:
            // Already ±1 range after DC removal; no further normalization needed.
            return c.coupled(x, density_);
        case NoiseType::Diffuse: {
            const float g = 0.1f + density_ * 0.8f;
            // Allpass preserves amplitude exactly — normalize same as Residual.
            return std::tanh(c.diffuse(x, g) * 4.0f);
        }
        default: return 0.0f;
    }
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
        } else if (type_ == NoiseType::Residual ||
                   type_ == NoiseType::Coupled  ||
                   type_ == NoiseType::Diffuse) {
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
                // Amplitude modulation: noise multiplies the signal.
                // y = x * (1 + mix*n)
                out_l = left [i] * (1.0f + mix_ * noise_l);
                out_r = right[i] * (1.0f + mix_ * noise_r);
                break;
            case NoiseBlend::Saturate:
                // Noise injected before a tanh soft clipper.
                // y = lerp(x, tanh(x + mod*n), mix)
                out_l = (1.0f - mix_) * left [i] + mix_ * std::tanh(left [i] + mod_ * noise_l);
                out_r = (1.0f - mix_) * right[i] + mix_ * std::tanh(right[i] + mod_ * noise_r);
                break;
            default: { // Add: standard dry/wet crossfade
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
    if (type_ == NoiseType::Residual ||
        type_ == NoiseType::Coupled  ||
        type_ == NoiseType::Diffuse)
        return 0.0f;
    return noise_sample(ch_prev_) * gain_;
}

} // namespace kaos_engine
