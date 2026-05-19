#pragma once
#include <cmath>
#include <algorithm>
#include <random>

namespace kaos_engine {

enum class StochasticMode : int {
    SampleHold  = 0,  // stepped random; SHAPE = output range
    SampleGlide = 1,  // S&H with portamento; SHAPE = glide fraction
    Smooth      = 2,  // cubic interpolation between targets; SHAPE = curve
    Brownian    = 3,  // random walk; SHAPE = mean reversion strength
    Lorenz      = 4,  // Lorenz strange attractor; SHAPE = rho (chaos density)
    Logistic    = 5,  // logistic map; SHAPE = r (periodic -> fully chaotic)
    kCount      = 6
};

class StochasticProcessor {
public:
    void  prepare(double sample_rate, int block_size);
    void  reset();
    void  seed(uint32_t s);

    float next_sample();
    void  process(float* out, int num_samples);

    void set_mode   (StochasticMode m) { mode_     = m; }
    void set_rate_hz(float hz)         { rate_hz_  = std::max(hz, 0.001f); }
    void set_depth  (float d)          { depth_    = std::clamp(d, 0.0f, 1.0f); }
    void set_shape  (float s)          { shape_    = std::clamp(s, 0.0f, 1.0f); }
    void set_offset (float o)          { offset_   = o; }
    void set_running(bool r)           { running_  = r; }

    StochasticMode get_mode()    const { return mode_; }
    bool           is_running()  const { return running_; }

    // Lorenz state exposed for phase-portrait drawing in the editor.
    float get_output() const { return last_output_; }
    float get_lx()     const { return lx_; }
    float get_lz()     const { return lz_; }

private:
    float sample_sh();
    float sample_sg();
    float sample_smooth();
    float sample_brownian();
    float sample_lorenz();
    float sample_logistic();

    inline float rand01()  { return udist_(rng_); }
    inline float randbi()  { return rand01() * 2.0f - 1.0f; }

    double         sample_rate_ = 44100.0;
    StochasticMode mode_        = StochasticMode::SampleHold;
    float          rate_hz_     = 1.0f;
    float          depth_       = 1.0f;
    float          shape_       = 0.5f;
    float          offset_      = 0.0f;
    bool           running_     = false;

    // Phase accumulator (S&H, S&G, Smooth, Logistic)
    float phase_    = 0.0f;
    float current_  = 0.0f;
    float previous_ = 0.0f;

    // Brownian state
    float brown_ = 0.0f;

    float last_output_ = 0.0f;

    // Lorenz state (RK4)
    float lx_ =  0.1f, ly_ = 0.0f, lz_ = 0.0f;

    // Logistic state
    float logistic_x_ = 0.5001f;

    std::mt19937 rng_ { 42 };
    std::uniform_real_distribution<float> udist_ { 0.0f, 1.0f };
};

} // namespace kaos_engine
