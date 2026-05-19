#pragma once
#include <cmath>
#include <algorithm>

namespace kaos_engine {

enum class LfoWaveform : int {
    Sine       = 0,
    Triangle   = 1,
    Square     = 2,
    Sawtooth   = 3,   // rising ramp
    ReverseSaw = 4,   // falling ramp
    HalfSine   = 5,   // abs(sin) — two bumps per cycle, always ≥ 0
    ExpRamp    = 6,   // exponential rise -1→+1 (slow start, fast finish)
    LogRamp    = 7,   // logarithmic rise -1→+1 (fast start, slow finish)
    Pulse      = 8,   // square with variable duty cycle (SHAPE = duty 0..1)
    Staircase     = 9,    // quantised rising ramp   (SHAPE → step count 2..16)
    StaircaseDown = 10,   // quantised falling ramp  (SHAPE → step count 2..16)
    kCount        = 11
};

class LfoProcessor {
public:
    void  prepare(double sample_rate, int block_size);
    void  reset();

    float next_sample();
    void  process(float* out, int num_samples);

    void set_waveform    (LfoWaveform w);
    void set_rate_hz     (float hz);
    void set_depth       (float d);       // 0..1
    void set_offset      (float o);       // -1..1  DC shift
    void set_shape       (float s);       // 0..1   waveform-specific modifier
    void set_phase_offset(float p);       // 0..1
    void set_tempo_sync  (bool on);
    void set_bpm         (double bpm);
    void set_sync_beats  (float beats);

    float get_phase()  const { return phase_; }
    bool  is_running() const { return running_; }

    // Lock phase to DAW transport position (ppqPosition) when tempo sync is active.
    void set_phase_direct(float p) { phase_ = p - std::floor(p); }

    // Pause or resume phase advancement. When paused the LFO holds its current
    // output value — used by trigger modes to implement hold-on-gate-close.
    void set_running(bool r) { running_ = r; }

private:
    void  update_phase_inc();
    float raw_wave(float p) const;

    double      sample_rate_ = 44100.0;
    LfoWaveform waveform_    = LfoWaveform::Sine;
    float       rate_hz_     = 1.0f;
    float       depth_       = 1.0f;
    float       offset_      = 0.0f;
    float       shape_       = 0.5f;   // 0..1, default 0.5
    float       phase_off_   = 0.0f;
    float       phase_       = 0.0f;
    float       phase_inc_   = 0.0f;
    bool        tempo_sync_  = false;
    bool        running_     = false;  // false = hold at current phase; Free mode sets true each block
    double      bpm_         = 120.0;
    float       sync_beats_  = 1.0f;
};

} // namespace kaos_engine
