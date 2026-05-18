#pragma once
#include <vector>
#include <cmath>

namespace kaos_engine {

enum class DelayMode {
    Standard    = 0,   // LP-filtered feedback delay
    Slapback    = 1,   // single short echo, no feedback
    PingPong    = 2,   // stereo L/R cross-bounce
    Tape        = 3,   // modulated delay + saturation in feedback
    Diffusion   = 4,   // allpass chain diffusor
    Reverse     = 5,   // buffer read backwards
    Comb        = 6,   // resonant comb filter
    MultiTap    = 7,   // 3 read heads at 1x / 1.5x / 2x time
    Shimmer     = 8,   // pitch-shifted feedback (+1 octave, 2-head crossfade)
    Haas        = 9,   // L/R offset for stereo widening (< 40 ms)
    BBD         = 10,  // bucket-brigade device: clock-modulated delay + output LP
    kNumModes
};

// MOD controls LP cutoff for Standard/Comb (no other MOD use in those modes)
inline bool delay_mode_mod_is_lp(DelayMode m)
{
    return m == DelayMode::Standard || m == DelayMode::Comb;
}

// MOD2 controls LP cutoff for modes where MOD is already used for something else
inline bool delay_mode_uses_mod2(DelayMode m)
{
    return m == DelayMode::PingPong  ||
           m == DelayMode::Tape      ||
           m == DelayMode::Diffusion ||
           m == DelayMode::MultiTap  ||
           m == DelayMode::Shimmer;
}

inline bool delay_mode_uses_mod(DelayMode m)
{
    return delay_mode_mod_is_lp(m)  ||
           m == DelayMode::Tape      ||
           m == DelayMode::Diffusion ||
           m == DelayMode::PingPong  ||
           m == DelayMode::MultiTap  ||
           m == DelayMode::Shimmer   ||
           m == DelayMode::BBD;
}

inline bool delay_mode_uses_feedback(DelayMode m)
{
    return m != DelayMode::Slapback && m != DelayMode::Haas;
}

class DelayProcessor {
public:
    DelayProcessor();

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_time_ms(float ms);
    void set_feedback(float feedback);
    void set_tone(float normalised);
    void set_mod(float normalised);
    void set_mod2(float normalised);
    void set_output(float db);       // -20..+6 dB post-processing gain
    void set_mix(float mix);
    void set_mode(DelayMode mode);

private:
    static constexpr int kBufBits = 18;
    static constexpr int kBufSize = 1 << kBufBits;
    static constexpr int kBufMask = kBufSize - 1;

    std::vector<float> buf_l_, buf_r_;
    int write_pos_ = 0;

    float read_hermite(const std::vector<float>& buf, float delay_samples) const;

    // ── Allpass diffusion ──────────────────────────────────────────────────────
    struct AllpassStage {
        std::vector<float> buf_l, buf_r;
        int pos = 0, delay = 0;
        float coeff = 0.5f;
        float process_l(float x);
        float process_r(float x);
    };
    AllpassStage ap_[4];
    static constexpr float kApMs[4] = { 5.0f, 17.0f, 29.0f, 43.0f };

    // ── Shimmer pitch shift (2-head sawtooth crossfade) ────────────────────────
    float shimmer_phase_ = 0.0f;

    // ── Reverse ────────────────────────────────────────────────────────────────
    int rev_phase_ = 0;

    // ── LFO (Tape / BBD) ──────────────────────────────────────────────────────
    float lfo_phase_ = 0.0f;
    float lfo_inc_   = 0.0f;

    // ── Feedback filters ───────────────────────────────────────────────────────
    float tone_l_ = 0.0f, tone_r_ = 0.0f;
    float dc_xl_  = 0.0f, dc_yl_ = 0.0f;
    float dc_xr_  = 0.0f, dc_yr_ = 0.0f;
    float bbd_lp_l_ = 0.0f, bbd_lp_r_ = 0.0f;
    float bbd_lp_coeff_ = 0.5f;

    // ── Parameters ────────────────────────────────────────────────────────────
    double sample_rate_    = 44100.0;
    float  delay_samples_  = 22050.0f;
    float  feedback_       = 0.4f;
    float  output_linear_  = 1.0f;
    float  mod_            = 0.3f;
    float  mod2_           = 0.75f;
    float  mix_            = 0.5f;
    DelayMode mode_        = DelayMode::Standard;

    float  mod_lp_coeff_   = 0.5f;
    float  mod2_lp_coeff_  = 0.5f;

    void update_lfo_inc();
    void update_mod_lp_coeff();
    void update_mod2_lp_coeff();
    void update_bbd_lp();
    void rebuild_allpass_buffers();

    static float apply_dc_block(float x, float& x_prev, float& y_prev);
    static float apply_tone(float x, float& state, float coeff);
    static float apply_lp(float x, float& state, float coeff);
    static float soft_clip(float x);

    static constexpr float kDcR   = 0.9975f;
    static constexpr float kPi    = 3.14159265358979323846f;
    static constexpr float kMaxMs = 2000.0f;
    static constexpr float kMinMs = 1.0f;

    float lp_coeff_from_norm(float n) const;
};

} // namespace kaos_engine
