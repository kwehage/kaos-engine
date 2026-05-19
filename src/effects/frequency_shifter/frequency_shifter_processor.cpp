#include "frequency_shifter_processor.h"
#include <algorithm>
#include <cmath>

namespace kaos_engine {

// ── Yehar quadrature IIR Hilbert coefficients (a² precomputed) ────────────────
// Source: https://yehar.com/blog/?p=368
// Path 1 includes a 1-sample delay (→ Q component).
// Path 2 has no extra delay (→ I component).
namespace {
    constexpr float kPath1A2[4] = {
        0.6923878f        * 0.6923878f,
        0.9360654322959f  * 0.9360654322959f,
        0.9882295226860f  * 0.9882295226860f,
        0.9987488452737f  * 0.9987488452737f,
    };
    constexpr float kPath2A2[4] = {
        0.4021921162426f  * 0.4021921162426f,
        0.8561710882420f  * 0.8561710882420f,
        0.9722909545651f  * 0.9722909545651f,
        0.9952884791278f  * 0.9952884791278f,
    };
} // namespace

// ── HilbertState ──────────────────────────────────────────────────────────────

void FrequencyShifterProcessor::HilbertState::reset() noexcept
{
    for (auto& s : path1) s.reset();
    for (auto& s : path2) s.reset();
    delay1 = 0.0f;
}

void FrequencyShifterProcessor::HilbertState::process(
    float x, float& i_out, float& q_out) noexcept
{
    // Path 1: 4 allpass sections + 1-sample delay → Q
    float p1 = x;
    for (int k = 0; k < 4; ++k)
        p1 = path1[k].process(p1, kPath1A2[k]);
    q_out  = delay1;
    delay1 = p1;

    // Path 2: 4 allpass sections → I
    float p2 = x;
    for (int k = 0; k < 4; ++k)
        p2 = path2[k].process(p2, kPath2A2[k]);
    i_out = p2;
}

// ── DelayLine ─────────────────────────────────────────────────────────────────

void FrequencyShifterProcessor::DelayLine::prepare(int min_size)
{
    int sz = 1;
    while (sz < min_size) sz <<= 1;
    buf.assign(sz, 0.0f);
    mask = sz - 1;
    pos  = 0;
}

void FrequencyShifterProcessor::DelayLine::write(float x) noexcept
{
    buf[pos & mask] = x;
    pos = (pos + 1) & mask;
}

float FrequencyShifterProcessor::DelayLine::read_h(float d) const noexcept
{
    // 4-point Hermite: frac=0 → di samples ago, frac→1 → di+1 samples ago.
    const int   di   = static_cast<int>(d);
    const float frac = d - static_cast<float>(di);
    const float y0   = buf[(pos - di + 1) & mask];  // di-1 samples ago
    const float y1   = buf[(pos - di    ) & mask];  // di   samples ago
    const float y2   = buf[(pos - di - 1) & mask];  // di+1 samples ago
    const float y3   = buf[(pos - di - 2) & mask];  // di+2 samples ago
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void FrequencyShifterProcessor::DelayLine::reset() noexcept
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    pos = 0;
}

// ── AllpassDiffusor ───────────────────────────────────────────────────────────

void FrequencyShifterProcessor::AllpassDiffusor::prepare(int delay_samples)
{
    int sz = 1;
    while (sz <= delay_samples) sz <<= 1;
    buf.assign(sz, 0.0f);
    mask  = sz - 1;
    delay = delay_samples;
    pos   = 0;
}

float FrequencyShifterProcessor::AllpassDiffusor::process(float x, float a) noexcept
{
    // v[n] = x[n] + a*v[n-N];  y[n] = -a*v[n] + v[n-N]
    const float v_delayed = buf[(pos - delay) & mask];
    const float v         = x + a * v_delayed;
    buf[pos & mask]       = v;
    pos                   = (pos + 1) & mask;
    return -a * v + v_delayed;
}

void FrequencyShifterProcessor::AllpassDiffusor::reset() noexcept
{
    std::fill(buf.begin(), buf.end(), 0.0f);
    pos = 0;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

FrequencyShifterProcessor::FrequencyShifterProcessor() = default;

void FrequencyShifterProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    // Allocate enough for 2 seconds + headroom for Hermite neighbours.
    const int max_samples = static_cast<int>(2.1 * sample_rate) + 8;
    delay_l_.prepare(max_samples);
    delay_r_.prepare(max_samples);
    // Allpass diffusor stages: ~5 ms and ~8 ms (prime-ish to avoid beating).
    const int ap1_d = static_cast<int>(0.005 * sample_rate);
    const int ap2_d = static_cast<int>(0.008 * sample_rate);
    ap1_l_.prepare(ap1_d); ap1_r_.prepare(ap1_d);
    ap2_l_.prepare(ap2_d); ap2_r_.prepare(ap2_d);
    set_delay_time(delay_ms_);
    set_tone(tone_norm_);
    lfo_phase_inc_      = 2.0f * kPi * lfo_rate_hz_ / static_cast<float>(sample_rate_);
    tape_flutter_inc_   = 2.0f * kPi * 1.5f        / static_cast<float>(sample_rate_);
    reset();
}

void FrequencyShifterProcessor::reset()
{
    hilbert_l_.reset();
    hilbert_r_.reset();
    delay_l_.reset();
    delay_r_.reset();
    ap1_l_.reset(); ap1_r_.reset();
    ap2_l_.reset(); ap2_r_.reset();
    fb_tone_l_ = fb_tone_r_ = 0.0f;
    phasor_phase_       = 0.0f;
    lfo_phase_          = 0.0f;
    tape_flutter_phase_ = 0.0f;
}

// ── Setters ───────────────────────────────────────────────────────────────────

void FrequencyShifterProcessor::set_shift(float hz)
{
    shift_hz_ = std::clamp(hz, 0.0f, 5000.0f);
}

void FrequencyShifterProcessor::set_direction(FreqShiftDirection d)
{
    direction_ = d;
}

void FrequencyShifterProcessor::set_feedback(float fb)
{
    feedback_ = std::clamp(fb, 0.0f, 0.99f);
}

void FrequencyShifterProcessor::set_delay_time(float ms)
{
    delay_ms_ = ms;
    const float max_d = delay_l_.buf.empty()
        ? 88200.0f
        : static_cast<float>(delay_l_.buf.size() - 8);
    delay_samples_ = std::clamp(
        ms * 0.001f * static_cast<float>(sample_rate_),
        2.0f, max_d);
}

void FrequencyShifterProcessor::set_lfo_rate(float hz)
{
    lfo_rate_hz_   = hz;
    lfo_phase_inc_ = 2.0f * kPi * hz / static_cast<float>(sample_rate_);
}

void FrequencyShifterProcessor::set_lfo_depth(float hz)
{
    lfo_depth_hz_ = hz;
}

void FrequencyShifterProcessor::set_feedback_mode(FreqShiftFeedbackMode mode)
{
    feedback_mode_ = mode;
}

void FrequencyShifterProcessor::set_tone(float normalised)
{
    tone_norm_ = std::clamp(normalised, 0.0f, 1.0f);
    const float freq = 500.0f * std::pow(40.0f, tone_norm_);   // 500 Hz..20 kHz
    const float w    = 2.0f * kPi * freq / static_cast<float>(sample_rate_);
    tone_coeff_ = std::clamp(1.0f - std::exp(-w), 0.0f, 1.0f);
}

void FrequencyShifterProcessor::set_drive(float amount)
{
    drive_ = std::clamp(amount, 0.0f, 1.0f);
}

void FrequencyShifterProcessor::set_diffusion(float coeff)
{
    diffusion_ = std::clamp(coeff, 0.0f, 1.0f);
}

void FrequencyShifterProcessor::set_output(float db)
{
    output_linear_ = std::pow(10.0f, db / 20.0f);
}

void FrequencyShifterProcessor::set_mix(float mix)
{
    mix_ = std::clamp(mix, 0.0f, 1.0f);
}

// ── Block processing ──────────────────────────────────────────────────────────

void FrequencyShifterProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];

        // Tape mode: advance flutter LFO and offset the delay read position.
        float eff_delay = delay_samples_;
        if (feedback_mode_ == FreqShiftFeedbackMode::Tape) {
            tape_flutter_phase_ += tape_flutter_inc_;
            if (tape_flutter_phase_ > 2.0f * kPi) tape_flutter_phase_ -= 2.0f * kPi;
            // Depth = 0.3% of delay time — subtle pitch wobble on each echo.
            eff_delay += delay_samples_ * 0.003f * std::sin(tape_flutter_phase_);
            eff_delay  = std::max(2.0f, eff_delay);
        }

        // Read feedback echo. Ping-Pong swaps L/R so each echo crosses to the other side.
        float fb_l, fb_r;
        if (feedback_mode_ == FreqShiftFeedbackMode::PingPong) {
            fb_l = delay_r_.read_h(eff_delay);
            fb_r = delay_l_.read_h(eff_delay);
        } else {
            fb_l = delay_l_.read_h(eff_delay);
            fb_r = delay_r_.read_h(eff_delay);
        }

        // Mix input with feedback before shifting so each echo adds another D Hz.
        const float in_l = dry_l + feedback_ * fb_l;
        const float in_r = dry_r + feedback_ * fb_r;

        // Instantaneous phasor with optional LFO sweep
        const float eff_shift = shift_hz_
                              + lfo_depth_hz_ * std::sin(lfo_phase_);
        const float cos_t = std::cos(phasor_phase_);
        const float sin_t = std::sin(phasor_phase_);

        phasor_phase_ += 2.0f * kPi * eff_shift / static_cast<float>(sample_rate_);
        if (phasor_phase_ >  kPi) phasor_phase_ -= 2.0f * kPi;
        if (phasor_phase_ < -kPi) phasor_phase_ += 2.0f * kPi;

        lfo_phase_ += lfo_phase_inc_;
        if (lfo_phase_ > 2.0f * kPi) lfo_phase_ -= 2.0f * kPi;

        // Form analytic signal via Hilbert IIR
        float i_l, q_l, i_r, q_r;
        hilbert_l_.process(in_l, i_l, q_l);
        hilbert_r_.process(in_r, i_r, q_r);

        // SSB complex multiply
        float wet_l, wet_r;
        switch (direction_) {
            case FreqShiftDirection::Up:
                wet_l = i_l * cos_t - q_l * sin_t;
                wet_r = i_r * cos_t - q_r * sin_t;
                break;
            case FreqShiftDirection::Down:
                wet_l = i_l * cos_t + q_l * sin_t;
                wet_r = i_r * cos_t + q_r * sin_t;
                break;
            case FreqShiftDirection::Both:
                // Up + Down sum; Q terms cancel → I·cos(θ)
                wet_l = i_l * cos_t;
                wet_r = i_r * cos_t;
                break;
            default:
                wet_l = i_l * cos_t - q_l * sin_t;
                wet_r = i_r * cos_t - q_r * sin_t;
        }

        // ── Feedback colour chain ─────────────────────────────────────────────
        // Applied to the signal written into the delay, not to the direct output.
        // Each Risset echo arrives progressively darker, more saturated, and smeared.
        float col_l = wet_l;
        float col_r = wet_r;

        // Tone: single-pole LP darkens high-frequency content of each echo.
        fb_tone_l_ += tone_coeff_ * (col_l - fb_tone_l_);  col_l = fb_tone_l_;
        fb_tone_r_ += tone_coeff_ * (col_r - fb_tone_r_);  col_r = fb_tone_r_;

        // Drive: tanh saturation limits runaway and adds warmth.
        if (drive_ > 0.001f) {
            const float k = drive_ * 4.0f;
            col_l = std::tanh(k * col_l);
            col_r = std::tanh(k * col_r);
        }

        // Diffusion: 2-stage Schroeder allpass smears each echo into a dense tail.
        if (diffusion_ > 0.001f) {
            const float a = diffusion_ * 0.7f;
            col_l = ap1_l_.process(col_l, a);
            col_r = ap1_r_.process(col_r, a);
            col_l = ap2_l_.process(col_l, a);
            col_r = ap2_r_.process(col_r, a);
        }

        // Write coloured signal to delay (DC offset prevents denormal accumulation).
        delay_l_.write(col_l + 1e-18f);
        delay_r_.write(col_r + 1e-18f);

        // Output uses the direct (uncoloured) shifted signal.
        const float out_l = wet_l * output_linear_;
        const float out_r = wet_r * output_linear_;
        left[i]  = dry_l + mix_ * (out_l - dry_l);
        right[i] = dry_r + mix_ * (out_r - dry_r);
    }
}

} // namespace kaos_engine
