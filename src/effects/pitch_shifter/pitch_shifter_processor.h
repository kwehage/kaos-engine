#pragma once
#include <vector>
#include <cmath>
#include <cstdint>

namespace kaos_engine {

enum class PitchShifterAlgorithm {
    Granular    = 0,
    Smooth      = 1,
    Tape        = 2,
    PhaseVocoder= 3,
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
    void set_voice_pitch (int voice, float semitones);
    void set_voice_detune(int voice, float cents);
    void set_voice_gain  (int voice, float gain);
    void set_voice_mod1  (int voice, float v);
    void set_voice_mod2  (int voice, float v);
    void set_mix  (float v);
    void set_output(float db);

private:
    // ── Circular buffer ───────────────────────────────────────────────────────
    struct Buf {
        std::vector<float> data;
        int pos  = 0;
        int size = 0;
        int mask = 0;

        void  prepare(int min_size);
        void  write(float x);
        float read(int d)   const;
        float read_h(float d) const;
    };

    // ── Granular / Tape per-voice state ───────────────────────────────────────
    struct VoiceState {
        float    grain_phase_ = 0.0f;
        float    chaos_a_     = 0.0f;
        uint32_t lcg_         = 0x12345678u;
        int      grain_v_     = 2205;

        float tape_delay_    = 200.0f;
        float tape_prev_     = 200.0f;
        float tape_fade_     = 1.0f;
        float flutter_phase_ = 0.0f;

        float pitch_factor_  = 1.0f;
    };

    // ── Phase Vocoder state ───────────────────────────────────────────────────
    static constexpr int kPvN    = 1024;
    static constexpr int kPvHop  = kPvN / 4;       // 256 — 75% overlap
    static constexpr int kPvBins = kPvN / 2 + 1;   // 513
    // OLA normalisation for Hann² at 75% overlap = 1.5
    static constexpr float kPvNorm = 1.5f;

    struct PvChan {
        float out_buf   [kPvN * 2] {};  // overlap-add ring (2N for headroom)
        float last_phase[kPvBins]  {};
        float synth_phase[kPvBins] {};
        int   out_write = 0;
        int   out_read  = 0;
    };

    struct PvVoice {
        PvChan L, R;
        float  work    [kPvN * 2] {};  // FFT scratch (interleaved real/imag)
        float  window  [kPvN]     {};  // precomputed Hann
        float  mag_out [kPvBins]  {};  // per-frame output magnitudes
        float  freq_out[kPvBins]  {};  // per-frame true output frequencies (bins)
        int    fill = 0;               // samples since last analysis hop
    };

    // ── Per-algorithm voice processing ────────────────────────────────────────
    void process_granular (int vi, float& out_l, float& out_r);
    void process_smooth   (int vi, float& out_l, float& out_r);
    void process_tape     (int vi, float& out_l, float& out_r);
    void process_pv       (int vi, float& out_l, float& out_r);

    void process_pv_chan  (PvVoice& pv, PvChan& ch, bool is_left, float pf);

    void update_voice_pitch(int vi);
    void update_voice_mod  (int vi);

    static void  mini_fft  (float* cx, int N, bool inverse);
    static float wrap_pi   (float x);

    // ── State ─────────────────────────────────────────────────────────────────
    double sample_rate_ = 44100.0;

    Buf write_l_, write_r_;

    int grain_tape_ = 8820;

    VoiceState voices_[kNumVoices];
    PvVoice    pv_voices_[kNumVoices];

    PitchShifterAlgorithm algorithm_ = PitchShifterAlgorithm::Granular;
    float mix_          = 0.5f;
    float output_linear_= 1.0f;

    float voice_semitones_[kNumVoices] = {};
    float voice_cents_[kNumVoices]     = {};
    float voice_gains_[kNumVoices]     = { 1.0f, 0.0f, 0.0f };
    float voice_mod1_[kNumVoices]      = { 0.3f, 0.3f, 0.3f };
    float voice_mod2_[kNumVoices]      = { 0.0f, 0.0f, 0.0f };

    static constexpr float kPi    = 3.14159265358979323846f;
    static constexpr float kMaxPf = 4.0f;
};

} // namespace kaos_engine
