#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include <vector>

namespace kaos_engine {

enum class FilterMode : int {
    LP12     = 0,   // 2-pole lowpass  (-12 dB/oct)
    LP24     = 1,   // 4-pole lowpass  (-24 dB/oct)
    HP12     = 2,   // 2-pole highpass
    HP24     = 3,   // 4-pole highpass
    BandPass = 4,   // 2-pole bandpass
    Notch    = 5,   // 2-pole band-reject
    AllPass  = 6,   // 2-pole allpass  (phase shift, flat magnitude)
    Peak     = 7,   // peaking bell EQ (GAIN active)
    LowShelf = 8,   // low shelf EQ    (GAIN active)
    HiShelf  = 9,   // high shelf EQ   (GAIN active)
    Comb     = 10,  // feedback comb   (RESONANCE = feedback gain 0..1)
    Ladder   = 11,  // 4-pole Moog-style LP (high RESONANCE → self-oscillation)
    kCount   = 12
};

class FilterProcessor {
public:
    void prepare(double sample_rate, int block_size);
    void reset();
    void process(float* left, float* right, int num_samples);

    void set_mode     (FilterMode m) { mode_ = m; update_coeffs(); }
    void set_cutoff   (float hz)     { cutoff_hz_ = std::clamp(hz, 20.0f, 20000.0f); update_coeffs(); }
    void set_resonance(float q)      { resonance_ = std::max(0.05f, q); update_coeffs(); }
    void set_gain_db  (float db)     { gain_db_ = db; update_coeffs(); }
    void set_drive    (float d)      { drive_ = std::clamp(d, 0.0f, 1.0f); }
    void set_output   (float db)     { output_lin_ = std::pow(10.0f, db * 0.05f); }
    void set_mix      (float v)      { mix_ = std::clamp(v, 0.0f, 1.0f); }

    FilterMode get_mode()      const { return mode_; }
    float      get_cutoff()    const { return cutoff_hz_; }
    float      get_resonance() const { return resonance_; }
    float      get_gain_db()   const { return gain_db_; }

private:
    // SVF sample (Simper topology). Returns the selected output based on mode_.
    inline float svf_sample(float x, float& s1, float& s2) const;

    // Direct Form II transposed biquad sample.
    inline float bq_sample(float x, float& w1, float& w2) const;

    // One ladder sample.
    inline float ladder_sample(float x, float* s) const;

    void update_coeffs();

    double     sample_rate_ = 44100.0;
    FilterMode mode_        = FilterMode::LP12;
    float      cutoff_hz_   = 1000.0f;
    float      resonance_   = 0.7071f;  // Q; 0.707 = Butterworth
    float      gain_db_     = 0.0f;
    float      drive_       = 0.0f;
    float      output_lin_  = 1.0f;
    float      mix_         = 1.0f;

    // ── SVF (Simper) coefficients ─────────────────────────────────────────────
    float svf_g_  = 0.0f;
    float svf_k_  = 0.0f;
    float svf_a1_ = 0.0f;
    float svf_a2_ = 0.0f;
    float svf_a3_ = 0.0f;

    // Two cascaded SVF stages (LP24 / HP24 use both; others use stage A only).
    float s1a_l_ = 0, s1a_r_ = 0;
    float s2a_l_ = 0, s2a_r_ = 0;
    float s1b_l_ = 0, s1b_r_ = 0;
    float s2b_l_ = 0, s2b_r_ = 0;

    // ── RBJ biquad coefficients (Peak / LowShelf / HiShelf) ──────────────────
    float bq_b0_ = 1, bq_b1_ = 0, bq_b2_ = 0;
    float bq_a1_ = 0, bq_a2_ = 0;

    // Biquad state — Direct Form II transposed, stereo.
    float bq_w1_l_ = 0, bq_w1_r_ = 0;
    float bq_w2_l_ = 0, bq_w2_r_ = 0;

    // ── Ladder state ──────────────────────────────────────────────────────────
    float lad_f_    = 0.0f;  // one-pole coefficient 2*sin(π*fc/fs)
    float lad_k_    = 0.0f;  // resonance feedback 0..4
    float lad_l_[4] = {};
    float lad_r_[4] = {};

    // ── Feedback comb ─────────────────────────────────────────────────────────
    std::vector<float> comb_l_, comb_r_;
    int   comb_write_  = 0;
    int   comb_delay_  = 44;
    float comb_fb_     = 0.0f;  // feedback gain derived from resonance_
    float comb_damp_   = 0.0f;  // one-pole LP coeff in feedback path (from drive_)
    float comb_damp_l_ = 0.0f;  // LP state, left channel
    float comb_damp_r_ = 0.0f;  // LP state, right channel
};

} // namespace kaos_engine
