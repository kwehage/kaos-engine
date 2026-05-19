#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>
#include <cstdint>

namespace kaos_engine {

enum class GateAlgorithm {
    Gate     = 0,   // binary open/close with hold and hysteresis
    Expander = 1,   // ratio-based attenuation below threshold
    Ducker   = 2,   // attenuates when ABOVE threshold (sidechain ducking)
    kCount   = 3
};

// Display state readable by the UI thread.
enum class GateDisplayState : int {
    Closed  = 0,
    Release = 1,
    Hold    = 2,
    Open    = 3
};

class GateProcessor {
public:
    GateProcessor() = default;

    void prepare(double sample_rate, int block_size);
    // key_l / key_r: optional sidechain detection signal (Ducker, keyed gate/expander).
    // Pass nullptr to use the main signal for self-detection.
    void process(float* left, float* right, int num_samples,
                 const float* key_l = nullptr, const float* key_r = nullptr);
    void reset();

    void set_algorithm  (GateAlgorithm a);
    void set_threshold  (float db);        // -80..0 dBFS trigger point
    void set_range      (float db);        // -80..0 dB minimum gain when closed
    void set_ratio      (float ratio);     // 1..100 (Expander/Ducker only)
    void set_attack     (float ms);        // 0.1..500 ms
    void set_hold       (float ms);        // 0..2000 ms
    void set_release    (float ms);        // 10..5000 ms
    void set_hysteresis (float db);        // 0..20 dB: close threshold = threshold - hysteresis
    void set_output     (float db);
    void set_mix        (float v);

    // Gain computer: returns target gain in dB (always <= 0).
    // Exposed so the editor can draw the transfer function analytically.
    float gain_computer(float level_db) const;

    // Meter values (UI thread safe)
    float             get_gr_db()       const { return gr_meter_.load(); }
    float             get_level_db()    const { return level_meter_.load(); }
    GateDisplayState  get_disp_state()  const {
        return static_cast<GateDisplayState>(state_meter_.load());
    }

private:
    void update_coeffs();

    double sample_rate_  = 44100.0;
    GateAlgorithm algo_  = GateAlgorithm::Gate;

    // Parameters
    float threshold_db_  = -40.0f;
    float range_db_      = -60.0f;
    float ratio_         =  10.0f;
    float attack_ms_     =   2.0f;
    float hold_ms_       =  50.0f;
    float release_ms_    = 200.0f;
    float hysteresis_db_ =   6.0f;
    float output_db_     =   0.0f;
    float mix_           =   1.0f;

    // Derived
    float alpha_a_     = 0.99f;
    float alpha_r_     = 0.999f;
    int   hold_samples_= 2205;
    float range_lin_   = 0.0f;
    float output_lin_  = 1.0f;

    // Per-sample state
    float gain_smooth_  = -1000.0f;  // dB; starts effectively closed
    int   hold_counter_ = 0;
    float hpf_l_        = 0.0f;      // 100 Hz HPF state (prevents bass triggering gate)
    float hpf_r_        = 0.0f;

    std::atomic<float> gr_meter_    { -60.0f };
    std::atomic<float> level_meter_ { -100.0f };
    std::atomic<int>   state_meter_ { int(GateDisplayState::Closed) };
};

} // namespace kaos_engine
