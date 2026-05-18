#pragma once
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace kaos_engine {

enum class FilterType { LP = 0, HP = 1, BP = 2 };

enum class ReverbAlgorithm {
    Dattorro    = 0,
    Schroeder   = 1,
    FDN         = 2,
    Gardner     = 3,
    Moorer      = 4,
    VelvetNoise = 5,
    Shimmer     = 6,
    kNumAlgorithms
};

class ReverbProcessor {
public:
    ReverbProcessor();

    void prepare(double sample_rate, int block_size);
    void process(float* left, float* right, int num_samples);
    void reset();

    void set_pre_delay(float ms);
    void set_size(float v);
    void set_decay(float v);
    void set_damping(float v);
    void set_diffusion(float v);
    void set_mod(float v);
    void set_mod2(float v);
    void set_mix(float v);
    void set_output(float db);
    void set_algorithm(ReverbAlgorithm a);

    void set_filter_enabled(bool enabled);
    void set_filter_pos(int pos);
    void set_filter_type(int type);
    void set_filter_cutoff(float hz);
    void set_filter_resonance(float q);
    void set_filter_blend(float blend);

private:
    // ── Circular buffer ───────────────────────────────────────────────────────
    struct Buf {
        std::vector<float> data;
        int pos  = 0;
        int size = 0;
        int mask = 0;

        void prepare(int min_size);
        void write(float x);
        float read(int d) const;
        float read_h(float d) const;
    };

    // ── DSP helpers ───────────────────────────────────────────────────────────
    static float allpass(Buf& buf, int delay, float coeff, float x);
    static float allpass_mod(Buf& buf, float delay, float coeff, float x);
    static float comb(Buf& buf, int delay, float g, float& lp, float lp_coeff, float x);

    // ── Per-algorithm sample processing ──────────────────────────────────────
    void process_dattorro(float in_l, float in_r, float& wet_l, float& wet_r);
    void process_schroeder(float in_l, float in_r, float& wet_l, float& wet_r);
    void process_fdn(float in_l, float in_r, float& wet_l, float& wet_r);
    void process_gardner(float in_l, float in_r, float& wet_l, float& wet_r);
    void process_moorer(float in_l, float in_r, float& wet_l, float& wet_r);
    void process_velvet_noise(float in_l, float in_r, float& wet_l, float& wet_r);
    void process_shimmer(float in_l, float in_r, float& wet_l, float& wet_r);

    // ── Coefficient / delay recomputation ─────────────────────────────────────
    void update_delays();
    void update_coeffs();
    void regenerate_vn_sequence();

    // ── SVF (Simper) filter ────────────────────────────────────────────────────
    void  update_svf_coeffs();
    float apply_svf(float x, float& s1, float& s2) const;

    // ── State ─────────────────────────────────────────────────────────────────
    double sample_rate_ = 44100.0;
    ReverbAlgorithm algorithm_ = ReverbAlgorithm::Dattorro;

    // Parameters
    float pre_delay_ms_  = 20.0f;
    float size_          = 0.5f;
    float decay_         = 0.5f;
    float damping_       = 0.5f;
    float diffusion_     = 0.75f;
    float mod_           = 0.3f;
    float mod2_          = 0.0f;
    float mix_           = 0.3f;
    float output_linear_ = 1.0f;

    // Derived coefficients (shared by all algorithms)
    float decay_fb_        = 0.0f;
    float in_ap_coeff_     = 0.7f;
    float decay_ap2_coeff_ = 0.7f;
    float damp_coeff_      = 0.5f;
    float bw_coeff_        = 0.5f;
    float lfo_inc_         = 0.0f;
    float lfo_depth_       = 0.0f;

    // Shared pre-delay
    Buf   pre_delay_buf_;
    int   pre_delay_samples_ = 0;

    // ── Dattorro state ────────────────────────────────────────────────────────
    float bw_state_    = 0.0f;
    Buf   in_ap_[4];
    float lfo_phase_l_ = 0.0f, lfo_phase_r_ = 0.0f;
    float tank_fb_l_   = 0.0f, tank_fb_r_   = 0.0f;
    Buf   tank_ap1_l_, tank_ap2_l_, tank_post_l_;
    Buf   tank_ap1_r_, tank_ap2_r_, tank_post_r_;
    float damp_l_ = 0.0f, damp_r_ = 0.0f;
    int   in_ap_delays_[4]   = {};
    int   tank_ap1_delay_l_  = 0, tank_ap1_delay_r_  = 0;
    int   tank_ap2_delay_l_  = 0, tank_ap2_delay_r_  = 0;
    int   tank_post_delay_l_ = 0, tank_post_delay_r_ = 0;

    // ── Schroeder state ───────────────────────────────────────────────────────
    static constexpr int kNumCombs  = 4;
    static constexpr int kNumSchAps = 2;
    Buf   sch_comb_l_[kNumCombs],      sch_comb_r_[kNumCombs];
    float sch_comb_lp_l_[kNumCombs],   sch_comb_lp_r_[kNumCombs];
    Buf   sch_ap_l_[kNumSchAps],       sch_ap_r_[kNumSchAps];
    int   sch_comb_delays_l_[kNumCombs], sch_comb_delays_r_[kNumCombs];
    int   sch_ap_delays_[kNumSchAps];

    // ── FDN state (4 delay lines, Hadamard mixing) ────────────────────────────
    static constexpr int kNumFdn = 4;
    Buf   fdn_[kNumFdn];
    float fdn_lp_[kNumFdn];
    int   fdn_delays_[kNumFdn];

    // ── Gardner room state ────────────────────────────────────────────────────
    static constexpr int kNumGardAps = 3;
    Buf   gard_ap_l_[kNumGardAps],       gard_ap_r_[kNumGardAps];
    Buf   gard_delay_l_,                  gard_delay_r_;
    float gard_lp_l_  = 0.0f,            gard_lp_r_  = 0.0f;
    float gard_fb_l_  = 0.0f,            gard_fb_r_  = 0.0f;
    int   gard_ap_delays_l_[kNumGardAps], gard_ap_delays_r_[kNumGardAps];
    int   gard_delay_l_len_ = 0,          gard_delay_r_len_ = 0;

    // ── Moorer state ──────────────────────────────────────────────────────────
    // 8-tap tapped delay line for early reflections + Schroeder-style late tail
    static constexpr int kNumMoorerTaps  = 8;
    static constexpr int kNumMoorerCombs = 4;
    static constexpr int kNumMoorerAps   = 2;
    Buf   moorer_early_buf_;
    int   moorer_tap_delays_[kNumMoorerTaps]  = {};
    float moorer_tap_gains_[kNumMoorerTaps]   = {};
    Buf   moorer_comb_l_[kNumMoorerCombs],    moorer_comb_r_[kNumMoorerCombs];
    float moorer_comb_lp_l_[kNumMoorerCombs], moorer_comb_lp_r_[kNumMoorerCombs];
    Buf   moorer_ap_l_[kNumMoorerAps],        moorer_ap_r_[kNumMoorerAps];
    int   moorer_comb_dl_[kNumMoorerCombs]  = {}, moorer_comb_dr_[kNumMoorerCombs] = {};
    int   moorer_ap_del_[kNumMoorerAps]     = {};

    // ── Velvet noise state ────────────────────────────────────────────────────
    // Sparse FIR reverb: pre-generated sequence of ±1 pulses with exponential decay
    static constexpr int kVNMaxPulses = 2000;
    static constexpr int kVNDensity   = 1000;   // pulses per second at base density
    Buf   vn_buf_l_, vn_buf_r_;
    int   vn_pos_[kVNMaxPulses]    = {};
    float vn_gain_l_[kVNMaxPulses] = {};
    float vn_gain_r_[kVNMaxPulses] = {};
    int   vn_pulse_count_          = 0;
    float vn_lp_l_ = 0.0f,        vn_lp_r_ = 0.0f;

    // ── Shimmer state ─────────────────────────────────────────────────────────
    // Dattorro plate + granular pitch shifter in the feedback loop.
    // MOD 1 (mod_) = shimmer mix amount 0-1; MOD 2 (mod2_) = pitch shift 0-+12 st.
    // Tank buffers are reused from the Dattorro section above.
    static constexpr int kShimmerGrainBase = 2048;
    Buf   shimmer_grain_l_, shimmer_grain_r_;
    float shimmer_grain_phase_ = 0.0f;
    int   shimmer_grain_size_  = 0;

    // SVF state
    float svf_s1_l_ = 0.0f, svf_s1_r_ = 0.0f;
    float svf_s2_l_ = 0.0f, svf_s2_r_ = 0.0f;
    float svf_a1_   = 1.0f, svf_a2_   = 0.0f, svf_a3_ = 0.0f, svf_k_ = 1.414f;

    // Filter parameters
    bool       filter_enabled_   = false;
    int        filter_pos_       = 0;
    FilterType filter_type_      = FilterType::LP;
    float      filter_cutoff_hz_ = 5000.0f;
    float      filter_resonance_ = 0.707f;
    float      filter_blend_     = 1.0f;

    static constexpr float kPi = 3.14159265358979323846f;

    // Dattorro base delays at 44100 Hz
    static constexpr int kBaseInAp[4] = { 210, 158, 561, 410 };
    static constexpr int kBaseAp1L    = 996;
    static constexpr int kBaseAp1R    = 1345;
    static constexpr int kBaseAp2L    = 2666;
    static constexpr int kBaseAp2R    = 3935;
    static constexpr int kBasePostL   = 444;
    static constexpr int kBasePostR   = 740;

    // Schroeder base delays at 44100 Hz (mutually prime)
    static constexpr int kSchCombBase[kNumCombs] = { 1307, 1637, 1871, 2273 };
    static constexpr int kSchApBase[kNumSchAps]  = { 221, 74 };

    // FDN base delays at 44100 Hz (mutually prime)
    static constexpr int kFdnBase[kNumFdn] = { 1051, 1307, 1559, 2003 };

    // Gardner base delays at 44100 Hz
    static constexpr int kGardApBase[kNumGardAps] = { 221, 74, 453 };
    static constexpr int kGardDelayBase            = 2205;

    // Moorer base delays at 44100 Hz (geometrically spaced 4–55 ms)
    static constexpr int kMoorerTapBase[kNumMoorerTaps]   = { 190, 353, 523, 747, 1024, 1370, 1823, 2400 };
    static constexpr int kMoorerCombBase[kNumMoorerCombs] = { 1731, 2269, 2801, 3559 };
    static constexpr int kMoorerApBase[kNumMoorerAps]     = { 317, 107 };
};

} // namespace kaos_engine
