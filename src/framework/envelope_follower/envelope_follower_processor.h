#pragma once
#include <cmath>
#include <algorithm>
#include <atomic>

namespace kaos_engine {

enum class EnvelopeDetector : int {
    Peak = 0,   // abs(x) -> attack/release ballistics
    RMS  = 1,   // sqrt(avg(x^2)) -> attack/release ballistics
};

enum class EnvelopeOutputShape : int {
    Follow  = 0, // cv = env  (louder signal -> higher CV)
    Duck    = 1, // cv = 1-env (louder signal -> lower CV; classic sidechaining)
    Rise    = 2, // cv = env while envelope is rising, 0 while falling (onset detector)
    Fall    = 3, // cv = env while envelope is falling, 0 while rising (decay detector)
    Release = 4, // cv rises 0->1 as envelope falls from peak back to silence
};

class EnvelopeFollowerProcessor {
public:
    void  prepare(double sample_rate, int block_size);
    void  reset();

    // Process one stereo pair; returns the CV output value in [0, 1].
    float process_sample(float left, float right);
    void  process(const float* src_l, const float* src_r, float* out, int n);

    void set_detector    (EnvelopeDetector   d) { detector_    = d; }
    void set_output_shape(EnvelopeOutputShape s){ shape_       = s; }
    void set_attack_ms   (float ms)             { attack_ms_   = std::max(ms, 0.1f);  update_coeffs(); }
    void set_release_ms  (float ms)             { release_ms_  = std::max(ms, 1.0f);  update_coeffs(); }
    void set_gain        (float g)              { gain_        = std::max(g, 0.0f); }
    void set_depth       (float d)              { depth_       = std::clamp(d, 0.0f, 1.0f); }

    // Both values are needed for the dual-line strip chart in the editor.
    float get_envelope() const { return env_; }       // raw ballistics output [0,1]
    float get_output()   const { return last_output_; } // shaped CV output [0,1]

private:
    void update_coeffs();

    double               sample_rate_   = 44100.0;
    EnvelopeDetector     detector_      = EnvelopeDetector::Peak;
    EnvelopeOutputShape  shape_         = EnvelopeOutputShape::Follow;
    float                attack_ms_     = 10.0f;
    float                release_ms_    = 100.0f;
    float                gain_          = 1.0f;
    float                depth_         = 1.0f;

    float alpha_a_       = 0.0f;   // attack coefficient
    float alpha_r_       = 0.0f;   // release coefficient
    float alpha_peak_r_  = 0.0f;   // slow peak-holder release coefficient (~2 s)
    float env_           = 0.0f;   // ballistics state
    float rms_state_     = 0.0f;   // RMS squared accumulator

    // Derivative tracking for Rise / Fall modes
    float env_prev_      = 0.0f;
    float deriv_smooth_  = 0.0f;   // smoothed first derivative (amplitude/sample)

    // Peak tracker for Release mode
    float env_peak_      = 0.0f;

    float last_output_   = 0.0f;
};

} // namespace kaos_engine
