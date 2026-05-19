#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>

namespace kaos_engine {

// ── RBJ Cookbook biquad coefficients (normalised, a0 = 1) ─────────────────────
struct BiquadCoeffs {
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    static BiquadCoeffs identity   ();
    static BiquadCoeffs high_pass  (double fs, double f, double q);
    static BiquadCoeffs low_pass   (double fs, double f, double q);
    static BiquadCoeffs low_shelf  (double fs, double f, double q, double gain_db);
    static BiquadCoeffs high_shelf (double fs, double f, double q, double gain_db);
    static BiquadCoeffs peak       (double fs, double f, double q, double gain_db);
    static BiquadCoeffs notch      (double fs, double f, double q);

    // Analytical magnitude at f Hz via the phi trick (UI thread only).
    double magnitude_db(double f, double fs) const noexcept;
};

// ── Per-channel TDFII state ────────────────────────────────────────────────────
struct BiquadState {
    double z1 = 0.0, z2 = 0.0;
    float process(float x, const BiquadCoeffs& c) noexcept {
        const double out = c.b0 * x + z1;
        z1 = c.b1 * x + z2 - c.a1 * out;
        z2 = c.b2 * x      - c.a2 * out;
        return static_cast<float>(out);
    }
    void reset() noexcept { z1 = z2 = 0.0; }
};

// ── Band type ──────────────────────────────────────────────────────────────────
enum class BandType : int {
    Off       = 0,
    Bell      = 1,
    Notch     = 2,
    LowShelf  = 3,
    HighShelf = 4,
    HP12      = 5,   // 12 dB/oct (single 2nd-order biquad)
    HP24      = 6,   // 24 dB/oct (two cascaded Butterworth biquads)
    LP12      = 7,
    LP24      = 8,
    kCount    = 9
};

inline bool band_has_gain(BandType t) {
    return t == BandType::Bell || t == BandType::LowShelf || t == BandType::HighShelf;
}

// Butterworth 4th-order cascade Q factors (used for HP24/LP24).
static constexpr double kBW4_Q1 = 0.54119610;
static constexpr double kBW4_Q2 = 1.30656296;

// ── Per-band configuration ─────────────────────────────────────────────────────
struct BandConfig {
    BandType type = BandType::Off;
    float    freq = 1000.0f;
    float    gain = 0.0f;
    float    q    = 1.0f;
};

// ── 7-band parametric EQ processor ────────────────────────────────────────────
class EqProcessor {
public:
    static constexpr int kNumBands  = 7;
    static constexpr int kMaxStages = 2;   // HP24/LP24 use two cascaded biquads

    EqProcessor() = default;

    void prepare(double sample_rate, int block_size);
    void reset();
    void process(float* left, float* right, int num_samples);

    void set_band  (int band, const BandConfig& cfg);
    void set_output(float db);
    void set_mix   (float mix);

    // Combined analytical magnitude at f Hz. Called on UI thread; benign data race.
    double magnitude_db(double f) const noexcept;

private:
    double sample_rate_   = 44100.0;
    float  output_linear_ = 1.0f;
    float  mix_           = 1.0f;

    BandConfig   config_  [kNumBands]             {};
    BiquadCoeffs coeffs_  [kNumBands][kMaxStages] {};
    int          n_stages_[kNumBands]             {};
    BiquadState  state_l_ [kNumBands][kMaxStages] {};
    BiquadState  state_r_ [kNumBands][kMaxStages] {};

    void recompute_band(int band);
};

} // namespace kaos_engine
