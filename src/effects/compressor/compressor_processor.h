#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>

namespace kaos_engine {

enum class CompressorAlgorithm {
    VCA     = 0,   // feed-forward, program-independent attack/release
    Optical = 1,   // feed-forward, level-dependent release (LA-2A style)
    FET     = 2,   // feed-back topology, peak detection (1176 style)
    kCount  = 3
};

class CompressorProcessor {
public:
    CompressorProcessor() = default;

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_algorithm(CompressorAlgorithm a);
    void set_threshold(float db);        // -60..0 dBFS
    void set_ratio    (float ratio);     // 1..20  (use 20 for limiter feel)
    void set_knee     (float db);        // 0 = hard, up to 20 dB soft
    void set_attack   (float ms);        // 0.1..1000 ms
    void set_release  (float ms);        // 10..5000 ms
    void set_makeup   (float db);        // -20..+20 dB
    void set_output   (float db);        // -20..+6 dB
    void set_mix      (float v);         // 0..1

    // Gain computer: given a detected level, return gain reduction dB (<=0).
    // Called from both processor and editor (for transfer-curve drawing).
    float gain_computer(float level_db) const;

    // Current gain reduction for meter (safe to call from UI thread).
    float get_gain_reduction_db() const { return gr_meter_.load(); }

private:
    void update_coeffs();

    double sample_rate_ = 44100.0;
    CompressorAlgorithm algo_ = CompressorAlgorithm::VCA;

    // Parameters
    float threshold_db_ = -18.0f;
    float ratio_        =   4.0f;
    float knee_db_      =   2.0f;
    float attack_ms_    =  10.0f;
    float release_ms_   = 100.0f;
    float makeup_db_    =   0.0f;
    float output_db_    =   0.0f;
    float mix_          =   1.0f;

    // Derived coefficients (updated by update_coeffs)
    float alpha_a_   = 0.9f;   // attack smoother coefficient
    float alpha_r_   = 0.99f;  // release smoother coefficient
    // Optical: fast and slow release components (release_ms and 10× release_ms)
    float opt_r_fast_ = 0.95f;
    float opt_r_slow_ = 0.9999f;
    // FET: ratio-scaled attack (faster at higher ratios)
    float fet_alpha_a_ = 0.9f;
    float makeup_lin_  = 1.0f;
    float output_lin_  = 1.0f;

    // Per-sample state
    float gain_smooth_  = 0.0f;  // smoothed gain reduction in dB (≤ 0)
    float out_l_prev_   = 0.0f;  // FET feedback: previous output sample
    float out_r_prev_   = 0.0f;

    std::atomic<float> gr_meter_ { 0.0f };
};

} // namespace kaos_engine
