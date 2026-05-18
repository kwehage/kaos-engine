#pragma once
#include <cmath>
#include <cstring>

namespace kaos_engine {

enum class DistortionMode {
    Soft       = 0,   // tanh waveshaper
    Hard       = 1,   // hard clip
    Foldback   = 2,   // foldback reflect
    Tube       = 3,   // asymmetric tanh  (uses bias b)
    Arctan     = 4,   // arctan normalised
    Log        = 5,   // logarithmic companding
    SineFold   = 6,   // sine-based wavefolder
    Diode      = 7,   // exponential diode approximation
    HalfWave   = 8,   // half-wave rectifier
    FullWave   = 9,   // full-wave rectifier (octave up)
    Chebyshev  = 10,  // Chebyshev T3 (pure 3rd harmonic)
    Bitcrusher = 11,  // bit-depth reduction  (uses bias b = bit depth)
    SampleRate = 12,  // sample-rate reduction  (uses bias b = rate factor)
    kNumModes
};

inline bool mode_uses_bias(DistortionMode m)
{
    return m == DistortionMode::Tube       ||
           m == DistortionMode::Bitcrusher ||
           m == DistortionMode::SampleRate;
}

enum class FilterType { LP = 0, HP = 1, BP = 2 };

class DistortionProcessor {
public:
    DistortionProcessor();

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_drive(float db);
    void set_mode(DistortionMode mode);
    void set_feedback(float alpha);
    void set_tone(float normalised);
    void set_bias(float bias);
    void set_output(float db);
    void set_mix(float mix);

    // ── Optional pre/post filter ───────────────────────────────────────────
    void set_filter_enabled(bool enabled);
    void set_filter_pos(int pos);        // 0 = pre, 1 = post
    void set_filter_type(int type);      // 0 = LP, 1 = HP, 2 = BP
    void set_filter_cutoff(float hz);    // 20..20000
    void set_filter_resonance(float q);  // 0.1..10
    void set_filter_blend(float blend);  // 0..1

private:
    float process_sample(float x,
                         float& tone_state,
                         float& dc_x_prev, float& dc_y_prev,
                         int& sr_count,    float& sr_held,
                         float& prev_dist);

    // ── Transfer functions ─────────────────────────────────────────────────
    static float soft_clip(float x);
    static float hard_clip(float x);
    static float foldback(float x);
    float        tube_clip(float x) const;
    static float arctan_clip(float x, float g);
    static float log_clip(float x, float g);
    static float sine_fold(float x);
    static float diode_clip(float x);
    static float half_wave(float x);
    static float full_wave(float x);
    static float chebyshev_t3(float x);
    float        bitcrush(float x) const;
    float        sample_rate_reduce(float x, int& count, float& held) const;

    // ── Tone and DC filters ────────────────────────────────────────────────
    float       apply_tone(float x, float& state) const;
    static float apply_dc_block(float x, float& x_prev, float& y_prev);
    void        update_tone_coeff();

    // ── State variable filter (SVF, Simper topology) ───────────────────────
    void  update_svf_coeffs();
    float apply_svf(float x, float& s1, float& s2) const;

    double sample_rate_  = 44100.0;

    float drive_linear_  = 1.0f;
    float output_linear_ = 1.0f;
    float mix_           = 1.0f;
    float bias_          = 0.0f;
    float feedback_      = 0.0f;
    DistortionMode mode_ = DistortionMode::Soft;

    float tone_coeff_    = 0.5f;
    float tone_norm_     = 0.5f;

    // Per-channel waveshaper state
    float tone_l_   = 0.0f, tone_r_   = 0.0f;
    float dc_xl_    = 0.0f, dc_xr_    = 0.0f;
    float dc_yl_    = 0.0f, dc_yr_    = 0.0f;
    int   sr_cnt_l_ = 0,    sr_cnt_r_ = 0;
    float sr_hld_l_ = 0.0f, sr_hld_r_ = 0.0f;
    float prev_l_   = 0.0f, prev_r_   = 0.0f;

    // Per-channel SVF state
    float svf_s1_l_ = 0.0f, svf_s1_r_ = 0.0f;
    float svf_s2_l_ = 0.0f, svf_s2_r_ = 0.0f;

    // SVF coefficients (recomputed when cutoff or resonance changes)
    float svf_a1_   = 1.0f;
    float svf_a2_   = 0.0f;
    float svf_a3_   = 0.0f;
    float svf_k_    = 1.414f;

    // Filter parameters
    bool       filter_enabled_   = false;
    int        filter_pos_       = 0;       // 0=pre, 1=post
    FilterType filter_type_      = FilterType::LP;
    float      filter_cutoff_hz_ = 5000.0f;
    float      filter_resonance_ = 0.707f;
    float      filter_blend_     = 1.0f;

    static constexpr float kDcR = 0.9975f;
};

} // namespace kaos_engine
