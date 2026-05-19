#include "lfo_processor.h"
#include <cmath>

namespace kaos_engine {

static constexpr float kTwoPi = 6.28318530717959f;
static constexpr float kE     = 2.71828182845905f;

void LfoProcessor::prepare(double sr, int /*block_size*/)
{
    sample_rate_ = sr;
    update_phase_inc();
    reset();
}

void LfoProcessor::reset() { phase_ = phase_off_; }

// ── Setters ───────────────────────────────────────────────────────────────────

void LfoProcessor::set_waveform    (LfoWaveform w) { waveform_ = w; }
void LfoProcessor::set_depth       (float d)       { depth_  = std::clamp(d,  0.0f, 1.0f); }
void LfoProcessor::set_offset      (float o)       { offset_ = std::clamp(o, -1.0f, 1.0f); }
void LfoProcessor::set_shape       (float s)       { shape_  = std::clamp(s,  0.0f, 1.0f); }
void LfoProcessor::set_phase_offset(float p)       { phase_off_ = p; }

void LfoProcessor::set_rate_hz(float hz)
{
    rate_hz_ = hz;
    if (!tempo_sync_) update_phase_inc();
}

void LfoProcessor::set_tempo_sync(bool on)  { tempo_sync_ = on;  update_phase_inc(); }
void LfoProcessor::set_bpm(double bpm)      { bpm_ = bpm;        if (tempo_sync_) update_phase_inc(); }
void LfoProcessor::set_sync_beats(float b)  { sync_beats_ = b;   if (tempo_sync_) update_phase_inc(); }

void LfoProcessor::update_phase_inc()
{
    float eff_hz = rate_hz_;
    if (tempo_sync_) {
        const float beat_hz = float(bpm_) / 60.0f;
        eff_hz = beat_hz / std::max(0.001f, sync_beats_);
    }
    phase_inc_ = eff_hz / float(sample_rate_);
}

// ── Waveform evaluation ───────────────────────────────────────────────────────

float LfoProcessor::raw_wave(float p) const
{
    switch (waveform_) {

        case LfoWaveform::Sine:
            return std::sin(kTwoPi * p);

        case LfoWaveform::Triangle:
            return 1.0f - 4.0f * std::abs(p - 0.5f);

        case LfoWaveform::Square:
            return p < 0.5f ? 1.0f : -1.0f;

        case LfoWaveform::Sawtooth:
            return 2.0f * p - 1.0f;

        case LfoWaveform::ReverseSaw:
            return 1.0f - 2.0f * p;

        case LfoWaveform::HalfSine:
            // abs(sin): always in [0,1]. Two bumps per cycle.
            // With default depth=1 and offset=0 the output is unipolar [0,1].
            return std::abs(std::sin(kTwoPi * p));

        case LfoWaveform::ExpRamp: {
            // Exponential rise from -1 to +1.  Slow start, fast finish.
            // shape_ ∈ [0,1] controls curve steepness: 0 ≈ linear, 1 = very steep.
            const float k = 1.0f + shape_ * 7.0f;          // k ∈ [1, 8]
            return -1.0f + 2.0f * (std::exp(k * p) - 1.0f) / (std::exp(k) - 1.0f);
        }

        case LfoWaveform::LogRamp: {
            // Logarithmic rise from -1 to +1.  Fast start, slow finish.
            // shape_ controls steepness identically to ExpRamp.
            const float k = 1.0f + shape_ * 7.0f;
            return -1.0f + 2.0f * std::log(1.0f + p * (std::exp(k) - 1.0f)) / k;
        }

        case LfoWaveform::Pulse:
            // Variable duty cycle. shape_ = fraction of cycle spent at +1.
            // Default shape=0.5 → identical to Square.
            return p < shape_ ? 1.0f : -1.0f;

        case LfoWaveform::Staircase: {
            const int   n    = 2 + int(shape_ * 14.0f + 0.5f);
            const float step = std::min(std::floor(p * float(n)), float(n - 1));
            return step / float(n - 1) * 2.0f - 1.0f;
        }

        case LfoWaveform::StaircaseDown: {
            const int   n    = 2 + int(shape_ * 14.0f + 0.5f);
            const float step = std::min(std::floor(p * float(n)), float(n - 1));
            return 1.0f - step / float(n - 1) * 2.0f;
        }

        default:
            return 0.0f;
    }
}

// ── Sample generation ─────────────────────────────────────────────────────────

float LfoProcessor::next_sample()
{
    const float p = phase_;
    if (running_) {
        phase_ += phase_inc_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;
    }
    return std::clamp(raw_wave(p) * depth_ + offset_, -1.0f, 1.0f);
}

void LfoProcessor::process(float* buf, int n)
{
    for (int i = 0; i < n; ++i)
        buf[i] = next_sample();
}

} // namespace kaos_engine
