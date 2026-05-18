#pragma once
#include <cmath>

namespace kaos_engine {

enum class ModulatorMode {
    Tremolo = 0,  // amplitude flutter via unipolar LFO
    AM      = 1,  // amplitude modulation at audio rate (sidebands + original)
    RingMod = 2,  // ring modulation — carrier suppressed, sidebands only
    kNumModes
};

enum class ModulatorWaveform {
    Sine     = 0,
    Triangle = 1,
    Square   = 2,
    Saw      = 3,
    kNumWaveforms
};

// BIAS and DEPTH are only meaningful for certain modes; callers should grey
// the corresponding UI controls when these return false.
inline bool modulator_uses_bias (ModulatorMode m) { return m == ModulatorMode::AM;     }
inline bool modulator_uses_depth(ModulatorMode m) { return m != ModulatorMode::RingMod; }

class ModulatorProcessor {
public:
    ModulatorProcessor();

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_rate(float hz);
    void set_mode(ModulatorMode mode);
    void set_waveform(ModulatorWaveform waveform);
    void set_depth(float depth);        // 0..1
    void set_bias(float bias);          // 0..1  (AM only: 0 = ring-mod character, 1 = full AM)
    void set_phase_offset(float norm);  // 0..1  representing 0..180 degrees of stereo offset
    void set_output(float db);
    void set_mix(float mix);

private:
    float generate_oscillator(float phase) const;

    double          sample_rate_    = 44100.0;
    ModulatorMode   mode_           = ModulatorMode::Tremolo;
    ModulatorWaveform waveform_     = ModulatorWaveform::Sine;

    float rate_hz_      = 4.0f;
    float depth_        = 0.5f;
    float bias_         = 1.0f;
    float phase_offset_ = 0.0f;   // stored as 0..0.5 (half-cycle = 180 degrees)
    float output_linear_= 1.0f;
    float mix_          = 1.0f;

    float phase_l_   = 0.0f;
    float phase_inc_ = 0.0f;

    static constexpr float kPi = 3.14159265358979323846f;
};

} // namespace kaos_engine
