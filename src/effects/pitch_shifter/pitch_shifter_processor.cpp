#include "pitch_shifter_processor.h"
#include <algorithm>
#include <cmath>

namespace kaos_engine {

// ── Buf ───────────────────────────────────────────────────────────────────────

void PitchShifterProcessor::Buf::prepare(int min_size)
{
    int s = 1;
    while (s < min_size) s <<= 1;
    if (s == size) return;
    data.assign(static_cast<size_t>(s), 0.0f);
    size = s;
    mask = s - 1;
    pos  = 0;
}

void PitchShifterProcessor::Buf::write(float x)
{
    data[static_cast<size_t>(pos)] = x;
    pos = (pos + 1) & mask;
}

float PitchShifterProcessor::Buf::read(int d) const
{
    return data[static_cast<size_t>((pos - d + size) & mask)];
}

float PitchShifterProcessor::Buf::read_h(float d) const
{
    const int   di  = static_cast<int>(d);
    const float fr  = d - static_cast<float>(di);
    const float xm1 = data[static_cast<size_t>((pos - di - 1 + size) & mask)];
    const float x0  = data[static_cast<size_t>((pos - di     + size) & mask)];
    const float x1  = data[static_cast<size_t>((pos - di + 1 + size) & mask)];
    const float x2  = data[static_cast<size_t>((pos - di + 2 + size) & mask)];
    const float c0  = x0;
    const float c1  = 0.5f * (x1 - xm1);
    const float c2  = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3  = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * fr + c2) * fr + c1) * fr + c0;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PitchShifterProcessor::PitchShifterProcessor() {}

// ── prepare ───────────────────────────────────────────────────────────────────

void PitchShifterProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    const float sr_ratio = static_cast<float>(sample_rate) / 44100.0f;

    grain_tape_ = std::max(64, static_cast<int>(8820 * sr_ratio));  // ~200 ms

    // Buffer must hold max grain × max pitch_factor + chaos headroom (×0.35 extra)
    const int buf_size = static_cast<int>((kMaxPf + 0.5f) * static_cast<float>(grain_tape_)) + 16;
    write_l_.prepare(buf_size);
    write_r_.prepare(buf_size);

    for (int vi = 0; vi < kNumVoices; ++vi) {
        update_voice_pitch(vi);
        update_voice_mod(vi);
    }
    reset();
}

// ── reset ─────────────────────────────────────────────────────────────────────

void PitchShifterProcessor::reset()
{
    auto clr = [](Buf& b) {
        std::fill(b.data.begin(), b.data.end(), 0.0f);
        b.pos = 0;
    };
    clr(write_l_);
    clr(write_r_);

    for (int vi = 0; vi < kNumVoices; ++vi) {
        VoiceState& v = voices_[vi];
        v.grain_phase_  = 0.0f;
        v.chaos_a_      = 0.0f;
        v.lcg_          = 0x12345678u + static_cast<uint32_t>(vi) * 0x9e3779b9u;
        const float nom = static_cast<float>(grain_tape_) * v.pitch_factor_;
        v.tape_delay_   = std::max(2.0f, nom);
        v.tape_prev_    = v.tape_delay_;
        v.tape_fade_    = 1.0f;
        v.flutter_phase_= 0.0f;
    }
}

// ── Setters ───────────────────────────────────────────────────────────────────

void PitchShifterProcessor::set_algorithm(PitchShifterAlgorithm a)
{
    if (a == algorithm_) return;
    algorithm_ = a;
    for (int vi = 0; vi < kNumVoices; ++vi)
        update_voice_mod(vi);
    reset();
}

void PitchShifterProcessor::set_voice_pitch(int voice, float semitones)
{
    if (voice < 0 || voice >= kNumVoices) return;
    const float c = std::clamp(semitones, -24.0f, 24.0f);
    if (c == voice_semitones_[voice]) return;
    voice_semitones_[voice] = c;
    update_voice_pitch(voice);
}

void PitchShifterProcessor::set_voice_detune(int voice, float cents)
{
    if (voice < 0 || voice >= kNumVoices) return;
    const float c = std::clamp(cents, -50.0f, 50.0f);
    if (c == voice_cents_[voice]) return;
    voice_cents_[voice] = c;
    update_voice_pitch(voice);
}

void PitchShifterProcessor::set_voice_gain(int voice, float gain)
{
    if (voice < 0 || voice >= kNumVoices) return;
    voice_gains_[voice] = std::clamp(gain, 0.0f, 1.0f);
}

void PitchShifterProcessor::set_voice_mod1(int voice, float v)
{
    if (voice < 0 || voice >= kNumVoices) return;
    const float c = std::clamp(v, 0.0f, 1.0f);
    if (c == voice_mod1_[voice]) return;
    voice_mod1_[voice] = c;
    update_voice_mod(voice);
}

void PitchShifterProcessor::set_voice_mod2(int voice, float v)
{
    if (voice < 0 || voice >= kNumVoices) return;
    voice_mod2_[voice] = std::clamp(v, 0.0f, 1.0f);
    // chaos range and flutter depth are computed inline — no derived state to update
}

void PitchShifterProcessor::set_mix   (float v)  { mix_ = std::clamp(v, 0.0f, 1.0f); }
void PitchShifterProcessor::set_output(float db) { output_linear_ = std::pow(10.0f, db / 20.0f); }

void PitchShifterProcessor::update_voice_pitch(int vi)
{
    const float combined = voice_semitones_[vi] + voice_cents_[vi] / 100.0f;
    voices_[vi].pitch_factor_ = std::pow(2.0f, combined / 12.0f);
    // Re-anchor tape delay to new nominal position
    const float nom = static_cast<float>(grain_tape_) * voices_[vi].pitch_factor_;
    voices_[vi].tape_prev_  = voices_[vi].tape_delay_;
    voices_[vi].tape_delay_ = std::max(2.0f, nom);
    voices_[vi].tape_fade_  = 0.0f;
}

void PitchShifterProcessor::update_voice_mod(int vi)
{
    // Compute per-voice grain size from MOD 1
    // Granular: 20..200 ms; Smooth: 80..300 ms; Tape: flutter uses MOD inline
    const float sr = static_cast<float>(sample_rate_);
    float ms;
    switch (algorithm_) {
        case PitchShifterAlgorithm::Smooth:
            ms = 80.0f + voice_mod1_[vi] * 220.0f;
            break;
        case PitchShifterAlgorithm::Tape:
            ms = 200.0f;  // tape grain size is fixed; MOD1 is flutter rate
            break;
        default:  // Granular
            ms = 20.0f + voice_mod1_[vi] * 180.0f;
            break;
    }
    voices_[vi].grain_v_ = std::max(64, static_cast<int>(ms * sr / 1000.0f));
}

// ── Granular: dual-grain, triangular window, per-voice grain size + chaos ─────

void PitchShifterProcessor::process_granular(int vi, float& out_l, float& out_r)
{
    VoiceState& v = voices_[vi];
    const float pf    = v.pitch_factor_;
    const float gs    = static_cast<float>(v.grain_v_);
    const float max_d = static_cast<float>(write_l_.size - 2);

    v.grain_phase_ += 1.0f;
    if (v.grain_phase_ >= gs) {
        v.grain_phase_ -= gs;
        // New grain period — draw a fresh chaos offset from the per-voice LCG
        v.lcg_ = v.lcg_ * 1664525u + 1013904223u;
        const float r          = static_cast<float>(v.lcg_ >> 8) / static_cast<float>(1u << 24);
        const float chaos_range = voice_mod2_[vi] * gs * 0.3f;  // up to ±30% of grain
        v.chaos_a_ = (r * 2.0f - 1.0f) * chaos_range;
    }

    const float pa = v.grain_phase_;
    float pb = pa + gs * 0.5f;
    if (pb >= gs) pb -= gs;

    const float ea = 1.0f - std::fabsf(pa / gs * 2.0f - 1.0f);
    const float eb = 1.0f - std::fabsf(pb / gs * 2.0f - 1.0f);

    const float da = std::clamp(pf * (gs - pa) + v.chaos_a_ + 1.0f, 1.0f, max_d);
    const float db = std::clamp(pf * (gs - pb) + v.chaos_a_ + 1.0f, 1.0f, max_d);

    out_l = ea * write_l_.read_h(da) + eb * write_l_.read_h(db);
    out_r = ea * write_r_.read_h(da) + eb * write_r_.read_h(db);
}

// ── Smooth: dual-grain, Hann window, per-voice grain size + chaos ─────────────

void PitchShifterProcessor::process_smooth(int vi, float& out_l, float& out_r)
{
    VoiceState& v = voices_[vi];
    const float pf    = v.pitch_factor_;
    const float gs    = static_cast<float>(v.grain_v_);
    const float max_d = static_cast<float>(write_l_.size - 2);

    v.grain_phase_ += 1.0f;
    if (v.grain_phase_ >= gs) {
        v.grain_phase_ -= gs;
        v.lcg_ = v.lcg_ * 1664525u + 1013904223u;
        const float r          = static_cast<float>(v.lcg_ >> 8) / static_cast<float>(1u << 24);
        const float chaos_range = voice_mod2_[vi] * gs * 0.3f;
        v.chaos_a_ = (r * 2.0f - 1.0f) * chaos_range;
    }

    const float pa = v.grain_phase_;
    float pb = pa + gs * 0.5f;
    if (pb >= gs) pb -= gs;

    // Hann windows — ea + eb = 1 everywhere at 50% overlap
    const float ea = 0.5f - 0.5f * std::cos(2.0f * kPi * pa / gs);
    const float eb = 0.5f - 0.5f * std::cos(2.0f * kPi * pb / gs);

    const float da = std::clamp(pf * (gs - pa) + v.chaos_a_ + 1.0f, 1.0f, max_d);
    const float db = std::clamp(pf * (gs - pb) + v.chaos_a_ + 1.0f, 1.0f, max_d);

    out_l = ea * write_l_.read_h(da) + eb * write_l_.read_h(db);
    out_r = ea * write_r_.read_h(da) + eb * write_r_.read_h(db);
}

// ── Tape: moving read pointer + flutter LFO ────────────────────────────────────
//
// MOD 1 → flutter rate 0..8 Hz (how fast the tape speed wavers)
// MOD 2 → flutter depth 0..±16 samples (amplitude of the speed variation)

void PitchShifterProcessor::process_tape(int vi, float& out_l, float& out_r)
{
    VoiceState& v = voices_[vi];
    const float pf      = v.pitch_factor_;
    const float advance = 1.0f - pf;
    const float gs      = static_cast<float>(grain_tape_);
    const float max_d   = static_cast<float>(write_l_.size - 2);

    // Flutter LFO: MOD1 = rate, MOD2 = depth
    const float flutter_hz    = voice_mod1_[vi] * 8.0f;                           // 0..8 Hz
    const float flutter_depth = voice_mod2_[vi] * 16.0f;                          // 0..±16 smp
    const float flutter_inc   = 2.0f * kPi * flutter_hz / static_cast<float>(sample_rate_);
    const float flutter       = flutter_depth * std::sin(v.flutter_phase_);
    v.flutter_phase_ += flutter_inc;
    if (v.flutter_phase_ >= 2.0f * kPi) v.flutter_phase_ -= 2.0f * kPi;

    v.tape_delay_ += advance;
    v.tape_prev_  += advance;

    const float safe_cur = std::clamp(v.tape_delay_ + flutter, 1.0f, max_d);
    const float safe_prv = std::clamp(v.tape_prev_  + flutter, 1.0f, max_d);

    out_l = v.tape_fade_ * write_l_.read_h(safe_cur)
          + (1.0f - v.tape_fade_) * write_l_.read_h(safe_prv);
    out_r = v.tape_fade_ * write_r_.read_h(safe_cur)
          + (1.0f - v.tape_fade_) * write_r_.read_h(safe_prv);

    const float fade_inc = 1.0f / std::max(2.0f, gs * 0.05f);
    v.tape_fade_ = std::min(1.0f, v.tape_fade_ + fade_inc);

    if (v.tape_delay_ < 2.0f || v.tape_delay_ > gs * kMaxPf) {
        v.tape_prev_  = v.tape_delay_;
        v.tape_delay_ = std::max(2.0f, gs * std::max(0.25f, pf));
        v.tape_fade_  = 0.0f;
    }
}

// ── process ───────────────────────────────────────────────────────────────────

void PitchShifterProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        const float in_l = left[i];
        const float in_r = right[i];

        write_l_.write(in_l);
        write_r_.write(in_r);

        float sum_l = 0.0f, sum_r = 0.0f;
        for (int vi = 0; vi < kNumVoices; ++vi) {
            if (voice_gains_[vi] < 0.001f) continue;
            float vl = 0.0f, vr = 0.0f;
            switch (algorithm_) {
                case PitchShifterAlgorithm::Granular: process_granular(vi, vl, vr); break;
                case PitchShifterAlgorithm::Smooth:   process_smooth  (vi, vl, vr); break;
                case PitchShifterAlgorithm::Tape:     process_tape    (vi, vl, vr); break;
                default: break;
            }
            sum_l += voice_gains_[vi] * vl;
            sum_r += voice_gains_[vi] * vr;
        }

        left[i]  = (in_l + mix_ * (sum_l - in_l)) * output_linear_;
        right[i] = (in_r + mix_ * (sum_r - in_r)) * output_linear_;
    }
}

} // namespace kaos_engine
