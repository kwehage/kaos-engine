#include "modulator_processor.h"
#include <algorithm>
#include <cmath>

namespace kaos_engine {

ModulatorProcessor::ModulatorProcessor() = default;

void ModulatorProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    phase_inc_   = rate_hz_ / static_cast<float>(sample_rate_);
    reset();
}

void ModulatorProcessor::reset()
{
    phase_l_ = 0.0f;
}

void ModulatorProcessor::set_rate(float hz)
{
    rate_hz_   = hz;
    phase_inc_ = rate_hz_ / static_cast<float>(sample_rate_);
}

void ModulatorProcessor::set_mode(ModulatorMode mode)
{
    mode_ = mode;
}

void ModulatorProcessor::set_waveform(ModulatorWaveform waveform)
{
    waveform_ = waveform;
}

void ModulatorProcessor::set_depth(float depth)
{
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

void ModulatorProcessor::set_bias(float bias)
{
    bias_ = std::clamp(bias, 0.0f, 1.0f);
}

void ModulatorProcessor::set_phase_offset(float norm)
{
    // norm 0..1 → 0..180 degrees → 0..0.5 of the oscillator period
    phase_offset_ = std::clamp(norm, 0.0f, 1.0f) * 0.5f;
}

void ModulatorProcessor::set_output(float db)
{
    output_linear_ = std::pow(10.0f, db / 20.0f);
}

void ModulatorProcessor::set_mix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

float ModulatorProcessor::generate_oscillator(float phase) const
{
    switch (waveform_) {
        case ModulatorWaveform::Sine:
            return std::sin(phase * 2.0f * kPi);
        case ModulatorWaveform::Triangle:
            return 1.0f - 4.0f * std::abs(phase - 0.5f);
        case ModulatorWaveform::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case ModulatorWaveform::Saw:
            return 2.0f * phase - 1.0f;
        default:
            return std::sin(phase * 2.0f * kPi);
    }
}

void ModulatorProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // Right channel is the same oscillator at a fixed phase offset.
        float phase_r = phase_l_ + phase_offset_;
        if (phase_r >= 1.0f) phase_r -= 1.0f;

        const float osc_l = generate_oscillator(phase_l_);
        const float osc_r = generate_oscillator(phase_r);

        phase_l_ += phase_inc_;
        if (phase_l_ >= 1.0f) phase_l_ -= 1.0f;

        float mod_l, mod_r;
        switch (mode_) {
            case ModulatorMode::Tremolo: {
                // Unipolar envelope: pos ∈ [0, 1], m ∈ [(1 - depth), 1]
                const float pos_l = 0.5f * osc_l + 0.5f;
                const float pos_r = 0.5f * osc_r + 0.5f;
                mod_l = (1.0f - depth_) + depth_ * pos_l;
                mod_r = (1.0f - depth_) + depth_ * pos_r;
                break;
            }
            case ModulatorMode::AM:
                // m = bias + depth * carrier; bias=1 preserves original, bias=0 is ring-mod like
                mod_l = bias_ + depth_ * osc_l;
                mod_r = bias_ + depth_ * osc_r;
                break;
            case ModulatorMode::RingMod:
                // Pure multiply — carrier suppressed, sidebands only
                mod_l = osc_l;
                mod_r = osc_r;
                break;
            default:
                mod_l = mod_r = 1.0f;
        }

        const float wet_l = dry_l * mod_l * output_linear_;
        const float wet_r = dry_r * mod_r * output_linear_;

        left[i]  = dry_l + mix_ * (wet_l - dry_l);
        right[i] = dry_r + mix_ * (wet_r - dry_r);
    }
}

} // namespace kaos_engine
