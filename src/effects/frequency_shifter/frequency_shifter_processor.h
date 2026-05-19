#pragma once
#include <vector>
#include <cmath>

namespace kaos_engine {

enum class FreqShiftDirection    { Up = 0, Down = 1, Both = 2 };
enum class FreqShiftFeedbackMode { Single = 0, PingPong = 1, Tape = 2 };

class FrequencyShifterProcessor {
public:
    FrequencyShifterProcessor();

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_shift    (float hz);
    void set_direction(FreqShiftDirection d);
    void set_feedback (float fb);
    void set_delay_time(float ms);
    void set_lfo_rate (float hz);
    void set_lfo_depth(float hz);
    void set_feedback_mode(FreqShiftFeedbackMode mode);
    void set_tone     (float normalised);  // 0..1  → LP cutoff 500 Hz..20 kHz
    void set_drive    (float amount);      // 0..1  → tanh saturation in feedback path
    void set_diffusion(float coeff);       // 0..1  → allpass coefficient 0..0.7
    void set_output   (float db);
    void set_mix      (float mix);

private:
    // ── Hilbert IIR: 2nd-order allpass section ────────────────────────────────
    // H(z) = (a² − z⁻²) / (1 − a²·z⁻²)
    // y[n] = a²·(x[n] + y[n−2]) − x[n−2]
    struct AllpassSection {
        float xz1 = 0.0f, xz2 = 0.0f;
        float yz1 = 0.0f, yz2 = 0.0f;

        float process(float x, float a2) noexcept {
            const float y = a2 * (x + yz2) - xz2;
            xz2 = xz1; xz1 = x;
            yz2 = yz1; yz1 = y;
            return y;
        }
        void reset() noexcept { xz1 = xz2 = yz1 = yz2 = 0.0f; }
    };

    // ── Hilbert state (one instance per channel) ──────────────────────────────
    // Implements Yehar's polyphase quadrature IIR filter.
    // Path 1 (+ 1-sample delay) → Q component.
    // Path 2                    → I component.
    struct HilbertState {
        AllpassSection path1[4];
        AllpassSection path2[4];
        float          delay1 = 0.0f;

        void  reset() noexcept;
        void  process(float x, float& i_out, float& q_out) noexcept;
    };

    // ── Schroeder allpass diffusor ────────────────────────────────────────────
    // v[n] = x[n] + a*v[n-N];  y[n] = -a*v[n] + v[n-N]
    // Flat magnitude; delays / smears without amplitude coloration.
    struct AllpassDiffusor {
        std::vector<float> buf;
        int pos   = 0;
        int mask  = 0;
        int delay = 0;

        void  prepare(int delay_samples);
        float process(float x, float a) noexcept;
        void  reset() noexcept;
    };

    // ── Circular delay buffer ─────────────────────────────────────────────────
    struct DelayLine {
        std::vector<float> buf;
        int pos  = 0;
        int mask = 0;

        void  prepare(int min_size);
        void  write(float x) noexcept;
        float read_h(float d) const noexcept;   // 4-point Hermite interpolation
        void  reset() noexcept;
    };

    double sample_rate_ = 44100.0;

    HilbertState    hilbert_l_, hilbert_r_;
    DelayLine       delay_l_,   delay_r_;
    AllpassDiffusor ap1_l_, ap1_r_;   // stage 1 (~5 ms)
    AllpassDiffusor ap2_l_, ap2_r_;   // stage 2 (~8 ms)

    FreqShiftDirection    direction_       = FreqShiftDirection::Up;
    FreqShiftFeedbackMode feedback_mode_  = FreqShiftFeedbackMode::Single;
    float shift_hz_      = 10.0f;
    float feedback_      = 0.0f;
    float delay_ms_      = 100.0f;
    float lfo_rate_hz_   = 0.0f;
    float lfo_depth_hz_  = 0.0f;
    float tone_coeff_    = 1.0f;    // LP coefficient (derived from tone_norm_)
    float tone_norm_     = 1.0f;
    float fb_tone_l_     = 0.0f;    // LP filter state in feedback chain
    float fb_tone_r_     = 0.0f;
    float drive_         = 0.0f;
    float diffusion_     = 0.0f;
    float output_linear_ = 1.0f;
    float mix_           = 1.0f;

    float phasor_phase_       = 0.0f;
    float lfo_phase_          = 0.0f;
    float lfo_phase_inc_      = 0.0f;
    float delay_samples_      = 4410.0f;
    // Tape flutter: fixed 1.5 Hz LFO on the delay read position.
    float tape_flutter_phase_ = 0.0f;
    float tape_flutter_inc_   = 0.0f;

    static constexpr float kPi = 3.14159265358979323846f;
};

} // namespace kaos_engine
