#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

namespace kaos_engine {

enum class NoiseType : int {
    White    = 0,  // flat spectrum
    Pink     = 1,  // -3 dB/oct (1/f, Kellett algorithm)
    Brown    = 2,  // -6 dB/oct (1/f^2, Brownian)
    Granular = 3,  // Hann-windowed grain bursts; SIZE and DENSITY active
};

enum class NoiseMode : int {
    Follow   = 0,  // noise amplitude tracks input envelope proportionally
    Gated    = 1,  // noise gates on/off when input crosses THRESHOLD (binary, smoothed)
    AlwaysOn = 2,  // noise runs continuously at fixed gain
};

// Self-contained per-channel noise generator.
struct NoiseChannel {
    uint32_t rng = 12345u;

    void seed(uint32_t s) { rng = s ? s : 1u; }

    float randbi() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(int(rng)) * 4.656612873077393e-10f;  // / 2^31
    }

    // Paul Kellett 7-variable pink noise approximation (-3 dB/oct)
    float pink() {
        const float w = randbi();
        b[0] = 0.99886f*b[0] + w*0.0555179f;
        b[1] = 0.99332f*b[1] + w*0.0750759f;
        b[2] = 0.96900f*b[2] + w*0.1538520f;
        b[3] = 0.86650f*b[3] + w*0.3104856f;
        b[4] = 0.55000f*b[4] + w*0.5329522f;
        b[5] = -0.7616f*b[5] - w*0.0168980f;
        const float out = b[0]+b[1]+b[2]+b[3]+b[4]+b[5]+b[6] + w*0.5362f;
        b[6] = w * 0.115926f;
        return out * 0.11f;
    }

    // Brownian (leaky integrator of white noise, normalised to match pink RMS)
    float brown() {
        brn_ = std::clamp(brn_ * 0.998f + randbi() * 0.002f, -1.0f, 1.0f);
        return std::clamp(brn_ * 8.0f, -1.0f, 1.0f);
    }

    float b[7]  = {};
    float brn_  = 0.0f;
};

class NoiseProcessor {
public:
    void prepare(double sample_rate, int block_size);
    void reset();

    // Main stereo process -- adds noise to the input in-place.
    void process(float* left, float* right, int num_samples);

    // Returns a single mono noise sample (no dry/wet, no gate).
    // Used by the editor for the waveform preview.
    float next_preview_sample();

    void set_type           (NoiseType t)  { type_         = t; }
    void set_mode           (NoiseMode m)  { mode_         = m; }
    void set_gain           (float g)      { gain_         = std::clamp(g, 0.0f, 1.0f); }
    void set_grain_size_ms  (float ms)     { grain_size_ms_= std::clamp(ms, 5.0f, 500.0f); update_grain_params(); }
    void set_grain_density  (float d)      { density_      = std::clamp(d, 0.0f, 1.0f);  update_grain_params(); }
    void set_threshold_db   (float db)     { threshold_lin_= std::pow(10.0f, db * 0.05f); }
    void set_attack_ms      (float ms)     { attack_ms_    = std::clamp(ms, 0.1f, 500.0f); update_gate_coeffs(); }
    void set_release_ms     (float ms)     { release_ms_   = std::clamp(ms, 1.0f, 5000.0f); update_gate_coeffs(); }
    void set_output         (float db)     { output_lin_   = std::pow(10.0f, db * 0.05f); }
    void set_mix            (float v)      { mix_          = std::clamp(v, 0.0f, 1.0f); }

private:
    float noise_sample(NoiseChannel& ch) const;
    float granular_sample(NoiseChannel& ch);
    void  update_grain_params();
    void  update_gate_coeffs();
    void  try_spawn_grain();

    double    sample_rate_    = 44100.0;
    NoiseType type_           = NoiseType::White;
    NoiseMode mode_           = NoiseMode::AlwaysOn;
    float     gain_           = 0.5f;
    float     grain_size_ms_  = 50.0f;
    float     density_        = 0.5f;
    float     threshold_lin_  = 0.01f;   // default -40 dBFS
    float     attack_ms_      = 10.0f;
    float     release_ms_     = 300.0f;
    float     output_lin_     = 1.0f;
    float     mix_            = 0.5f;

    NoiseChannel ch_l_, ch_r_, ch_prev_;

    // Granular grain pool (shared timing between L and R channels)
    static constexpr int kMaxGrains = 16;
    struct Grain { float phase = 0, inc = 0; bool active = false; };
    std::array<Grain, kMaxGrains> grains_;
    float spawn_phase_ = 0.0f;
    float spawn_inc_   = 0.0f;
    float grain_size_samples_ = 2205.0f;

    // Gated mode: smooth envelope follower (attack/release RC model)
    float gate_env_     = 0.0f;
    float gate_smooth_  = 0.0f;
    float gate_alpha_a_ = 0.002f;
    float gate_alpha_r_ = 0.0002f;
};

} // namespace kaos_engine
