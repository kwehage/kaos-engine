#pragma once
#include <vector>
#include <cmath>
#include <cstdint>

namespace kaos_engine {

enum class PitchShifterAlgorithm {
    Granular = 0,
    Smooth   = 1,
    Tape     = 2,
    kNumAlgorithms
};

class PitchShifterProcessor {
public:
    static constexpr int kNumVoices = 3;

    PitchShifterProcessor();

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_algorithm   (PitchShifterAlgorithm a);
    void set_voice_pitch (int voice, float semitones);  // -24..+24 st
    void set_voice_detune(int voice, float cents);       // -50..+50 cents
    void set_voice_gain  (int voice, float gain);        // 0..1
    void set_voice_mod1  (int voice, float v);           // 0..1
    void set_voice_mod2  (int voice, float v);           // 0..1
    void set_mix  (float v);
    void set_output(float db);

private:
    // ── Circular buffer ───────────────────────────────────────────────────────
    struct Buf {
        std::vector<float> data;
        int pos  = 0;
        int size = 0;
        int mask = 0;

        void prepare(int min_size);
        void write(float x);
        float read(int d) const;
        float read_h(float d) const;
    };

    // ── Per-voice state ───────────────────────────────────────────────────────
    struct VoiceState {
        // Granular / Smooth
        float    grain_phase_ = 0.0f;
        float    chaos_a_     = 0.0f;        // chaos read-position offset, updated per grain
        uint32_t lcg_         = 0x12345678u; // per-voice LCG for chaos randomisation
        int      grain_v_     = 2205;        // per-voice grain size (samples), from MOD 1

        // Tape
        float tape_delay_    = 200.0f;
        float tape_prev_     = 200.0f;
        float tape_fade_     = 1.0f;
        float flutter_phase_ = 0.0f;  // flutter LFO phase (radians)

        float pitch_factor_  = 1.0f;
    };

    // ── Per-algorithm voice processing ────────────────────────────────────────
    void process_granular(int vi, float& out_l, float& out_r);
    void process_smooth  (int vi, float& out_l, float& out_r);
    void process_tape    (int vi, float& out_l, float& out_r);

    void update_voice_pitch(int vi);
    void update_voice_mod  (int vi);   // recomputes grain_v_ from mod1 + algorithm

    // ── State ─────────────────────────────────────────────────────────────────
    double sample_rate_ = 44100.0;

    // Shared input buffers — all voices read from the same written signal
    Buf write_l_, write_r_;

    // Global grain sizes used as reference / buffer-sizing anchors (samples)
    int grain_tape_ = 8820;  // ~200 ms at 44.1 kHz — tape and buffer-size reference

    VoiceState voices_[kNumVoices];

    PitchShifterAlgorithm algorithm_ = PitchShifterAlgorithm::Granular;
    float mix_          = 0.5f;
    float output_linear_= 1.0f;

    float voice_semitones_[kNumVoices] = {};
    float voice_cents_[kNumVoices]     = {};
    float voice_gains_[kNumVoices]     = { 1.0f, 0.0f, 0.0f };
    float voice_mod1_[kNumVoices]      = { 0.3f, 0.3f, 0.3f }; // grain size / flutter rate
    float voice_mod2_[kNumVoices]      = { 0.0f, 0.0f, 0.0f }; // chaos / flutter depth

    static constexpr float kPi    = 3.14159265358979323846f;
    static constexpr float kMaxPf = 4.0f;  // 2^(24/12) — max pitch factor for +-24 st
};

} // namespace kaos_engine
