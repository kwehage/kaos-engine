#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

namespace kaos_engine {

enum class NoiseType : int {
    // ── Coloured noise (always-on, independent) ──────────────────────────────
    White    = 0,   // flat spectrum
    Pink     = 1,   // -3 dB/oct (1/f, Kellett algorithm)
    Blue     = 2,   // +3 dB/oct (differentiated white noise); no active parameters
    Brown    = 3,   // -6 dB/oct (1/f^2, Brownian)
    // ── Structured noise (always-on, independent) ────────────────────────────
    Granular     = 4,   // Hann-windowed grain bursts; SIZE and DENSITY active
    FeedbackComb = 5,   // white noise through resonant comb filter; SIZE = delay, DENSITY = feedback
    Simplex      = 6,   // fBm simplex noise; SIZE = speed, DENSITY = octaves, MOD = persistence
    Lorenz       = 7,   // Lorenz attractor RK4; SIZE = dt (pitch), DENSITY = rho (order->chaos)
    Duffing      = 8,   // forced double-well oscillator; SIZE = dt, DENSITY = gamma (chaos), MOD = omega
    Gendyn       = 9,   // Xenakis stochastic; N breakpoints mutate each cycle; SIZE = period, DENSITY = N, MOD = mutation rate
    HarshWall    = 10,  // 4 parallel linear comb filters; SIZE = pitch center, DENSITY = feedback, MOD = saturation depth
    Chua         = 11,  // Chua's circuit double-scroll attractor RK4; SIZE = dt, DENSITY = alpha (5-12)
    // ── Input-reactive (derived from input signal) ───────────────────────────
    // (12-16 defined below)
    Residual     = 12,  // high-pass residual of input: n = x - LP(x); SIZE = LP cutoff
    Coupled      = 13,  // logistic chaos driven by input energy; DENSITY = base chaos level
    Diffuse      = 14,  // Schroeder allpass diffusion of input; DENSITY = allpass coeff
    Modal        = 15,  // inharmonic modal resonator bank excited by input; SIZE = f1, DENSITY = B, MOD = T60
    SimplexDriven = 16, // 2D simplex; input level navigates y-axis; SIZE = speed, DENSITY = octaves, MOD = y-depth
    // ── More always-on ───────────────────────────────────────────────────────
    Velvet      = 17,   // sparse +1/0/-1 impulse sequence; DENSITY = pulse rate (200-6000 Hz)
    MissingFund = 18,   // harmonics 2f-5f of a sub-bass tone; SIZE = f0 (15-80 Hz), DENSITY = harmonic rolloff, MOD = phase noise
    DomainWarp  = 19,   // 2-layer domain-warped fBm; SIZE = speed, DENSITY = octaves, MOD = warp depth
    // ── Input-reactive additions ─────────────────────────────────────────────
    FrictionScrape = 20, // stick-slip friction excites modal resonators; SIZE = f1, DENSITY = B+threshold, MOD = T60
    GendynDriven   = 21, // Gendyn mutation rate scales with input level; SIZE = period, DENSITY = N, MOD = base rate
    KarplusStrong  = 22, // waveguide physical model; SIZE = pitch (2kHz-50Hz), DENSITY = LP damping, MOD = stiffness
};

enum class NoiseMode : int {
    Follow   = 0,  // noise amplitude tracks input envelope proportionally
    Gated    = 1,  // noise gates on/off when input crosses THRESHOLD (binary, smoothed)
    AlwaysOn = 2,  // noise runs continuously at fixed gain
};

enum class NoiseBlend : int {
    Add         = 0,  // y = (1-mix)*x + mix*n
    AM          = 1,  // y = x * (1 + mix*n)
    Saturate    = 2,  // y = lerp(x, tanh(x + mod*n), mix)
    Spectral    = 3,  // OLA: |X_k|' = |X_k| * (1 + mod*n_k) per bin
    PhaseRandom  = 4,  // OLA: bin phases randomized by mod depth; magnitudes preserved
    RingMod      = 5,  // y = (1-mix)*x + mix*(x*n); suppressed-carrier AM
    InfrasonicAM = 6,  // sub-20Hz LFO modulates signal amplitude; MOD = freq (0.1-19 Hz), MIX = depth
    Roughness    = 7,  // ring mod at sub-200Hz carrier; MOD = freq (20-200 Hz), MIX = depth
    SampleRate   = 8,  // ZOH sample-rate reduction; MOD = decimation factor (1-32x), MIX = depth
    SpectralEnv  = 9,  // OLA: replace bin magnitudes with target noise slope, preserve phases; MOD = slope (0=flat/white, 1=brown)
};

// Self-contained per-channel noise generator.
struct NoiseChannel {
    uint32_t rng = 12345u;

    void seed(uint32_t s) { rng = s ? s : 1u; }

    float randbi() {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return float(int(rng)) * 4.656612873077393e-10f;  // / 2^31
    }

    // Paul Kellett 7-variable pink noise approximation (-3 dB/oct)
    float pink() {
        const float w = randbi();
        b[0] = 0.99886f*b[0] + w*0.0555179f;
        b[1] = 0.99332f*b[1] + w*0.0750759f;
        b[2] = 0.96900f*b[2] + w*0.1538520f;
        b[3] = 0.86650f*b[3] + w*0.3104856f;
        b[4] = 0.55000f*b[4] + w*0.5329522f;
        b[5] = -0.7616f*b[5] - w*0.0168980f;
        const float out = b[0]+b[1]+b[2]+b[3]+b[4]+b[5]+b[6] + w*0.5362f;
        b[6] = w * 0.115926f;
        return out * 0.11f;
    }

    // Brownian (leaky integrator of white noise, normalised to match pink RMS)
    float brown() {
        brn_ = std::clamp(brn_ * 0.998f + randbi() * 0.002f, -1.0f, 1.0f);
        return std::clamp(brn_ * 8.0f, -1.0f, 1.0f);
    }

    float b[7]  = {};
    float brn_  = 0.0f;

    // ── Input-derived source state ─────────────────────────────────────────────
    float lp_       = 0.0f;    // Residual: one-pole LP state
    float chaos_    = 0.373f;  // Coupled: logistic map state (must stay in (0,1))
    float coupled_dc_= 0.0f;   // Coupled: slow DC tracker for period-doubling removal

    // Diffuse: three-stage Schroeder allpass cascade
    static constexpr int kAp1Cap = 128, kAp1Del = 113;
    static constexpr int kAp2Cap = 256, kAp2Del = 179;
    static constexpr int kAp3Cap = 512, kAp3Del = 277;
    float ap1_[kAp1Cap] {}, ap2_[kAp2Cap] {}, ap3_[kAp3Cap] {};
    int   ap1_pos_ = 0, ap2_pos_ = 0, ap3_pos_ = 0;

    // HP residual of input: n = x - LP(x)
    float residual(float x, float alpha) {
        lp_ = lp_ * alpha + x * (1.0f - alpha);
        return x - lp_;
    }

    // Logistic chaos gated by input energy, DC-removed
    float coupled(float x, float density) {
        const float r = std::clamp(3.5f + density * (0.5f + std::abs(x) * 0.5f),
                                   3.0f, 4.0f);
        chaos_ = std::clamp(r * chaos_ * (1.0f - chaos_), 1e-6f, 1.0f - 1e-6f);
        const float raw = 2.0f * chaos_ - 1.0f;
        // Remove DC that accumulates in period-doubling regimes
        coupled_dc_ = coupled_dc_ * 0.9998f + raw * 0.0002f;
        return raw - coupled_dc_;
    }

    // Schroeder allpass diffusion of input
    float diffuse(float x, float g) {
        float y = ap(x, g,        ap1_, ap1_pos_, kAp1Del, kAp1Cap);
        y       = ap(y, g * 0.8f, ap2_, ap2_pos_, kAp2Del, kAp2Cap);
        y       = ap(y, g * 0.6f, ap3_, ap3_pos_, kAp3Del, kAp3Cap);
        return y;
    }

    // Lorenz attractor (sigma=10, beta=8/3) via RK4. dt controls pitch/speed;
    // rho controls order (near 24.74 = bifurcation) vs chaos (28+ = butterfly).
    float lorenz(float dt, float rho) {
        const float s = 10.0f, b = 8.0f / 3.0f;
        const float k1x = s*(lrz_y_-lrz_x_), k1y = lrz_x_*(rho-lrz_z_)-lrz_y_, k1z = lrz_x_*lrz_y_-b*lrz_z_;
        const float x2 = lrz_x_+0.5f*dt*k1x, y2 = lrz_y_+0.5f*dt*k1y, z2 = lrz_z_+0.5f*dt*k1z;
        const float k2x = s*(y2-x2), k2y = x2*(rho-z2)-y2, k2z = x2*y2-b*z2;
        const float x3 = lrz_x_+0.5f*dt*k2x, y3 = lrz_y_+0.5f*dt*k2y, z3 = lrz_z_+0.5f*dt*k2z;
        const float k3x = s*(y3-x3), k3y = x3*(rho-z3)-y3, k3z = x3*y3-b*z3;
        const float x4 = lrz_x_+dt*k3x, y4 = lrz_y_+dt*k3y, z4 = lrz_z_+dt*k3z;
        const float k4x = s*(y4-x4), k4y = x4*(rho-z4)-y4, k4z = x4*y4-b*z4;
        lrz_x_ += (dt/6.0f)*(k1x+2*k2x+2*k3x+k4x);
        lrz_y_ += (dt/6.0f)*(k1y+2*k2y+2*k3y+k4y);
        lrz_z_ += (dt/6.0f)*(k1z+2*k2z+2*k3z+k4z);
        return lrz_x_ * 0.05f;  // x ranges ~+-20, normalise to +-1
    }

    // Lorenz state (initialised off-origin so attractor engages immediately)
    float lrz_x_ = 0.1f, lrz_y_ = 0.0f, lrz_z_ = 0.0f;

    // Duffing double-well oscillator: dx/dt = v, dv/dt = -0.3v + x - x^3 + gamma*cos(omega*t)
    float duf_x_ = 0.1f, duf_v_ = 0.0f, duf_t_ = 0.0f;

    float duffing(float dt, float gamma, float omega) {
        const float k1x = duf_v_;
        const float k1v = -0.3f*duf_v_ + duf_x_ - duf_x_*duf_x_*duf_x_ + gamma*std::cos(omega*duf_t_);
        const float x2  = duf_x_+0.5f*dt*k1x, v2 = duf_v_+0.5f*dt*k1v;
        const float k2x = v2;
        const float k2v = -0.3f*v2 + x2 - x2*x2*x2 + gamma*std::cos(omega*(duf_t_+0.5f*dt));
        const float x3  = duf_x_+0.5f*dt*k2x, v3 = duf_v_+0.5f*dt*k2v;
        const float k3x = v3;
        const float k3v = -0.3f*v3 + x3 - x3*x3*x3 + gamma*std::cos(omega*(duf_t_+0.5f*dt));
        const float x4  = duf_x_+dt*k3x, v4 = duf_v_+dt*k3v;
        const float k4x = v4;
        const float k4v = -0.3f*v4 + x4 - x4*x4*x4 + gamma*std::cos(omega*(duf_t_+dt));
        duf_x_ += (dt/6.0f)*(k1x+2*k2x+2*k3x+k4x);
        duf_v_ += (dt/6.0f)*(k1v+2*k2v+2*k3v+k4v);
        duf_t_ += dt;
        return duf_x_ * (1.0f/1.5f);
    }

    // Modal resonator biquad state (8 modes max, shared across Modal type)
    static constexpr int kModalCap = 8;
    float modal_y1_[kModalCap] {};
    float modal_y2_[kModalCap] {};

    // ── Feedback comb filter ──────────────────────────────────────────────────
    static constexpr int kCombDelayCap = 22100;  // 500 ms at 44.1 kHz
    float comb_buf_[kCombDelayCap] {};
    int   comb_pos_ = 0;

    // White noise -> resonant comb: y[n] = x[n] + fb * y[n-M]; normalised by (1-fb).
    float comb(float feedback, int delay) {
        delay = std::max(1, std::min(delay, kCombDelayCap - 1));
        const int rpos = (comb_pos_ - delay + kCombDelayCap) % kCombDelayCap;
        const float y = randbi() + feedback * comb_buf_[rpos];
        comb_buf_[comb_pos_] = std::tanh(y);
        comb_pos_ = (comb_pos_ + 1) % kCombDelayCap;
        return y * (1.0f - feedback);
    }

    // ── Simplex / fBm noise ───────────────────────────────────────────────────
    float simplex_x_ = 0.0f;  // 1D phase accumulator
    float spx2_x_    = 0.0f;  // 2D x-axis (time)
    float spx2_env_  = 0.0f;  // 2D y-axis envelope (smoothed input level)

    static uint32_t spx_hash(int32_t n) {
        uint32_t x = uint32_t(n);
        x = ((x >> 16) ^ x) * 0x45d9f3bu;
        x = ((x >> 16) ^ x) * 0x45d9f3bu;
        return (x >> 16) ^ x;
    }

    static float simplex1d(float px) {
        const int32_t i0 = (px >= 0.0f) ? int32_t(px) : int32_t(px) - 1;
        const int32_t i1 = i0 + 1;
        const float t0 = px - float(i0), t1 = t0 - 1.0f;
        float n0 = 0.5f - t0*t0, n1 = 0.5f - t1*t1;
        const float g0 = (spx_hash(i0) & 1u) ? 1.0f : -1.0f;
        const float g1 = (spx_hash(i1) & 1u) ? 1.0f : -1.0f;
        n0 = (n0 < 0.0f) ? 0.0f : n0*n0*n0*n0 * g0 * t0;
        n1 = (n1 < 0.0f) ? 0.0f : n1*n1*n1*n1 * g1 * t1;
        return 110.0f * (n0 + n1);  // corrected: 2.756 is the 2D constant; 1D max is ~0.009
    }

    static float fbm1d(float px, int octaves, float persistence) {
        float val = 0.0f, amp = 1.0f, sum_amp = 0.0f, freq = 1.0f;
        for (int k = 0; k < octaves; ++k) {
            val += amp * simplex1d(px * freq);
            sum_amp += amp;
            amp *= persistence;
            freq *= 2.0f;
        }
        return val / sum_amp;
    }

    float simplex(float step, int octaves, float persistence) {
        const float val = fbm1d(simplex_x_, octaves, persistence);
        simplex_x_ += step;
        return val;
    }

    static float grad2d(int32_t hx, int32_t hy, float gx, float gy) {
        static const float GX[8] = { 1,-1, 0, 0, 1,-1, 1,-1 };
        static const float GY[8] = { 0, 0, 1,-1, 1,-1,-1, 1 };
        const int h = int(spx_hash(hx ^ (hy * 1619))) & 7;
        return GX[h]*gx + GY[h]*gy;
    }

    static float simplex2d(float px, float py) {
        const float F2 = 0.366025403784f, G2 = 0.211324865405f;
        const float s = (px + py) * F2;
        const float xs = px + s, ys = py + s;
        const int32_t ix = (xs >= 0.0f) ? int32_t(xs) : int32_t(xs) - 1;
        const int32_t iy = (ys >= 0.0f) ? int32_t(ys) : int32_t(ys) - 1;
        const float t  = float(ix + iy) * G2;
        const float x0 = px - (float(ix) - t), y0 = py - (float(iy) - t);
        const int32_t i1 = (x0 > y0) ? 1 : 0, j1 = (x0 > y0) ? 0 : 1;
        const float x1 = x0 - float(i1) + G2,  y1 = y0 - float(j1) + G2;
        const float x2 = x0 - 1.0f + 2.0f*G2,  y2 = y0 - 1.0f + 2.0f*G2;
        float t0 = 0.5f - x0*x0 - y0*y0;
        float t1 = 0.5f - x1*x1 - y1*y1;
        float t2 = 0.5f - x2*x2 - y2*y2;
        const float n0 = (t0 < 0) ? 0.0f : t0*t0*t0*t0 * grad2d(ix,    iy,    x0, y0);
        const float n1 = (t1 < 0) ? 0.0f : t1*t1*t1*t1 * grad2d(ix+i1, iy+j1, x1, y1);
        const float n2 = (t2 < 0) ? 0.0f : t2*t2*t2*t2 * grad2d(ix+1,  iy+1,  x2, y2);
        return 70.0f * (n0 + n1 + n2);
    }

    static float fbm2d(float px, float py, int octaves) {
        float val = 0.0f, amp = 1.0f, sum_amp = 0.0f, freq = 1.0f;
        for (int k = 0; k < octaves; ++k) {
            val += amp * simplex2d(px * freq, py * freq);
            sum_amp += amp;
            amp  *= 0.5f;
            freq *= 2.0f;
        }
        return val / sum_amp;
    }

    // Input level smoothly navigates the y-axis of a 2D simplex field.
    float simplex_driven(float x_in, float step, float env_alpha, float mod_depth, int octaves) {
        spx2_env_ = spx2_env_ * env_alpha + std::abs(x_in) * (1.0f - env_alpha);
        const float py  = spx2_env_ * mod_depth * 8.0f;
        const float val = fbm2d(spx2_x_, py, octaves);
        spx2_x_ += step;
        return val;
    }

    // Domain-warped fBm: evaluate fBm, use result as offset coordinate for a second fBm pass.
    // Creates swirling turbulent texture unlike regular simplex.
    float dw_x_ = 0.0f;

    float domain_warp(float step, int octaves, float warp) {
        const float d1  = fbm1d(dw_x_,            octaves, 0.5f);
        const float val = fbm1d(dw_x_ + warp * d1, octaves, 0.5f);
        dw_x_ += step;
        return val;
    }

    // Friction scraping: stick-slip model drives the modal resonator bank.
    // friction_vel_ is the smoothed input envelope ("bow velocity").
    float friction_vel_ = 0.0f;

    // Inharmonic Karplus-Strong: delay line + LP damping + stiffness allpass.
    // Input continuously excites the waveguide, producing bell/string resonance
    // that responds to playing dynamics.
    static constexpr int kKsCap  = 1024;
    static constexpr int kKsMask = 1023;
    float ks_buf_[kKsCap] {};
    int   ks_pos_  = 0;
    float ks_lp_   = 0.0f;
    float ks_ap_x_ = 0.0f, ks_ap_y_ = 0.0f;

    float ks(float x, int D, float lp_coeff, float stiffness) {
        const int rpos = (ks_pos_ - D + kKsCap) & kKsMask;
        // LP (damping): lp_coeff near 0.99 = bright/long, near 0.90 = dark/short
        ks_lp_ = lp_coeff * ks_lp_ + (1.0f - lp_coeff) * ks_buf_[rpos];
        // Stiffness allpass H(z) = (a + z^-1)/(1 + a*z^-1): disperses harmonics upward
        const float ap_out = stiffness * (ks_lp_ - ks_ap_y_) + ks_ap_x_;
        ks_ap_x_ = ks_lp_;
        ks_ap_y_ = ap_out;
        ks_buf_[ks_pos_] = ap_out + x * 0.3f;  // feedback + continuous excitation
        ks_pos_ = (ks_pos_ + 1) & kKsMask;
        return ap_out;
    }

    // Blue noise: differentiate white noise (+3 dB/oct)
    float blue_prev_ = 0.0f;
    float blue() {
        const float w = randbi();
        const float b = (w - blue_prev_) * 0.7071f;  // normalise by sqrt(2)
        blue_prev_ = w;
        return b;
    }

    // Chua's circuit: 3-D piecewise-linear chaos (double-scroll attractor).
    // dx/dt = alpha*(y - x - f(x)),  dy/dt = x - y + z,  dz/dt = -beta*y
    // f(x) = m1*x + 0.5*(m0-m1)*(|x+1| - |x-1|)
    float chua_x_ = 0.6f, chua_y_ = 0.0f, chua_z_ = 0.0f;

    static float chua_nl(float x) {
        // Correct double-scroll parameters (Matsumoto 1985):
        // m0 = inner-region slope (-8/7), m1 = outer-region slope (-5/7).
        // Both negative; inner more so. Gives unstable saddle-focus at origin
        // and non-trivial equilibria at x = +-1.5 -- the double-scroll geometry.
        // The document's m0=-1/7, m1=2/7 is wrong: those values make the origin
        // stable so all trajectories collapse to zero (silent output).
        constexpr float m0 = -8.0f/7.0f, m1 = -5.0f/7.0f;
        return m1*x + 0.5f*(m0-m1)*(std::abs(x+1.0f) - std::abs(x-1.0f));
    }

    float chua(float dt, float alpha) {
        constexpr float beta = 100.0f/7.0f;
        const float k1x = alpha*(chua_y_-chua_x_-chua_nl(chua_x_));
        const float k1y = chua_x_-chua_y_+chua_z_;
        const float k1z = -beta*chua_y_;
        const float x2=chua_x_+0.5f*dt*k1x, y2=chua_y_+0.5f*dt*k1y, z2=chua_z_+0.5f*dt*k1z;
        const float k2x = alpha*(y2-x2-chua_nl(x2));
        const float k2y = x2-y2+z2;
        const float k2z = -beta*y2;
        const float x3=chua_x_+0.5f*dt*k2x, y3=chua_y_+0.5f*dt*k2y, z3=chua_z_+0.5f*dt*k2z;
        const float k3x = alpha*(y3-x3-chua_nl(x3));
        const float k3y = x3-y3+z3;
        const float k3z = -beta*y3;
        const float x4=chua_x_+dt*k3x, y4=chua_y_+dt*k3y, z4=chua_z_+dt*k3z;
        const float k4x = alpha*(y4-x4-chua_nl(x4));
        const float k4y = x4-y4+z4;
        const float k4z = -beta*y4;
        chua_x_ += (dt/6.0f)*(k1x+2*k2x+2*k3x+k4x);
        chua_y_ += (dt/6.0f)*(k1y+2*k2y+2*k3y+k4y);
        chua_z_ += (dt/6.0f)*(k1z+2*k2z+2*k3z+k4z);
        return chua_x_ * 0.4f;  // x ranges ~+-2.5, normalise to ~+-1
    }

    // Velvet noise: one +-1 impulse per window of M samples (no multiplications needed).
    int   vn_pos_       = 0;
    int   vn_pulse_pos_ = 0;

    // density_hz: pulses per second (200-6000). M = fs/density_hz.
    float velvet(float density_hz, float sample_rate) {
        const int M = std::max(2, int(sample_rate / density_hz));
        const float out = (vn_pos_ == vn_pulse_pos_) ?
                          (randbi() > 0.0f ? 1.0f : -1.0f) : 0.0f;
        if (++vn_pos_ >= M) {
            vn_pos_ = 0;
            vn_pulse_pos_ = int((randbi() * 0.5f + 0.5f) * float(M - 1) + 0.5f);
            vn_pulse_pos_ = std::clamp(vn_pulse_pos_, 0, M - 1);
        }
        return out;
    }

    // Missing fundamental: harmonics 2f, 3f, 4f, 5f without the fundamental.
    // The auditory system infers f0 even though it is absent -- implies sub-bass.
    float mf_phases_[4] = {};

    float missing_fund(float f0, float sample_rate, float rolloff, float phase_noise) {
        constexpr float kTwoPi  = 6.28318530717958647692f;
        constexpr float kWeights[4] = {1.0f, 0.70f, 0.50f, 0.40f};
        float out = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const float freq = f0 * float(k + 2);  // 2f, 3f, 4f, 5f
            mf_phases_[k] += freq / sample_rate + randbi() * phase_noise * 0.0003f;
            if (mf_phases_[k] >= 1.0f) mf_phases_[k] -= 1.0f;
            if (mf_phases_[k] <  0.0f) mf_phases_[k] += 1.0f;
            out += kWeights[k] * std::pow(rolloff, float(k))
                 * std::sin(kTwoPi * mf_phases_[k]);
        }
        return out * 0.4f;  // normalise to ~+-1
    }

    // HNW: 4 parallel LINEAR comb filters (no in-loop saturation).
    // Saturation inside the loop collapses all settings to the same +-1 output, destroying
    // spectral variety. Keep the combs linear so SIZE (delay length) and DENSITY (feedback)
    // actually shape the spectrum, then apply output tanh controlled by MOD.
    static constexpr int kHnwCap  = 512;
    static constexpr int kHnwMask = 511;
    float hnw_buf_[4][kHnwCap] {};
    int   hnw_pos_[4] = {};
    float hnw_dc_     = 0.0f;

    // scale: delay multiplier (1-15) -- sets spectral centroid from ~6kHz to ~200Hz
    // f:     comb feedback 0.50-0.95 -- 0.50=subtle coloring, 0.95=strong resonant peaks
    // drive: output tanh gain 1-15  -- 1=warm/soft, 15=hard clip/harsh
    float harsh_wall(int scale, float f, float drive) {
        const int base[4] = {7, 11, 17, 23};
        float sum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const int   d    = std::max(1, std::min(base[k] * scale, kHnwCap - 1));
            const int   rpos = (hnw_pos_[k] - d) & kHnwMask;
            const float yn   = randbi() + f * hnw_buf_[k][rpos];
            // Clamp buffer to prevent overflow at high f; linear below +-16
            hnw_buf_[k][hnw_pos_[k]] = std::clamp(yn, -16.0f, 16.0f);
            hnw_pos_[k] = (hnw_pos_[k] + 1) & kHnwMask;
            sum += yn;
        }
        // RMS normalisation: comb power = sigma_in^2/(1-f^2), divide by sqrt(1-f^2)
        // to keep consistent output level as DENSITY sweeps. Power(raw) = 0.25*sigma_in^2.
        const float norm = std::sqrt(1.0f - f * f);
        const float raw  = sum * 0.25f * norm;
        // Output saturation: drive=1 leaves spectral peaks audible; drive=15 hard-clips them
        const float out  = std::tanh(drive * raw);
        hnw_dc_ = hnw_dc_ * 0.999f + out * 0.001f;
        return out - hnw_dc_;
    }

    // ── GENDYN stochastic synthesis (Xenakis) ────────────────────────────────
    // N breakpoints connected by linear interpolation. Each breakpoint undergoes
    // an independent random walk in both amplitude and duration each time it is
    // traversed. Low MOD = quasi-periodic tone; high MOD = evolving noise.
    static constexpr int kGendynBreaks = 16;
    float gendyn_amp_[kGendynBreaks] {};
    float gendyn_dur_[kGendynBreaks] {};
    int   gendyn_pos_   = 0;
    float gendyn_phase_ = 0.0f;
    bool  gendyn_init_  = false;

    float gendyn(float amp_step, float dur_step, float base_dur, float min_dur, int n_breaks) {
        if (!gendyn_init_) {
            for (int k = 0; k < kGendynBreaks; ++k) {
                gendyn_amp_[k] = randbi();
                // Initial durations scattered around base_dur in [50%, 100%] range
                gendyn_dur_[k] = base_dur * (0.75f + randbi() * 0.25f);
            }
            gendyn_pos_   = 0;
            gendyn_phase_ = 0.0f;
            gendyn_init_  = true;
        }
        const int next = (gendyn_pos_ + 1) % n_breaks;
        const float out = gendyn_amp_[gendyn_pos_] * (1.0f - gendyn_phase_)
                        + gendyn_amp_[next]         * gendyn_phase_;
        gendyn_phase_ += 1.0f / std::max(1.0f, gendyn_dur_[gendyn_pos_]);
        if (gendyn_phase_ >= 1.0f) {
            gendyn_phase_ -= 1.0f;
            gendyn_amp_[gendyn_pos_] = std::clamp(
                gendyn_amp_[gendyn_pos_] + amp_step * randbi(), -1.0f, 1.0f);
            gendyn_dur_[gendyn_pos_] = std::clamp(
                gendyn_dur_[gendyn_pos_] + dur_step * randbi(), min_dur, base_dur * 4.0f);
            gendyn_pos_ = next;
        }
        return out;
    }

private:
    static float ap(float x, float g, float* buf, int& pos, int delay, int cap) {
        const int   rpos = (pos - delay + cap) % cap;
        const float vD   = buf[rpos];
        const float v    = x + g * vD;
        buf[pos]         = v;
        pos              = (pos + 1) % cap;
        return vD - g * v;
    }
};

class NoiseProcessor {
public:
    void prepare(double sample_rate, int block_size);
    void reset();

    // Main stereo process -- adds noise to the input in-place.
    void process(float* left, float* right, int num_samples);

    // Returns a single mono noise sample (no dry/wet, no gate).
    // Used by the editor for the waveform preview.
    float next_preview_sample();

    void set_type           (NoiseType t)  { type_         = t; }
    void set_mode           (NoiseMode m)  { mode_         = m; }
    void set_blend          (NoiseBlend b) { blend_        = b; }
    void set_mod            (float v)      { const float c = std::clamp(v, 0.0f, 1.0f);   if (c != mod_)          { mod_          = c;  modal_dirty_ = true; } }
    void set_gain           (float g)      { gain_         = std::clamp(g, 0.0f, 1.0f); }
    void set_grain_size_ms  (float ms)     { const float c = std::clamp(ms, 5.0f, 500.0f); if (c != grain_size_ms_){ grain_size_ms_ = c; modal_dirty_ = true; } update_grain_params(); }
    void set_grain_density  (float d)      { const float c = std::clamp(d, 0.0f, 1.0f);   if (c != density_)      { density_      = c;  modal_dirty_ = true; } update_grain_params(); }
    void set_threshold_db   (float db)     { threshold_lin_= std::pow(10.0f, db * 0.05f); }
    void set_attack_ms      (float ms)     { attack_ms_    = std::clamp(ms, 0.1f, 500.0f); update_gate_coeffs(); }
    void set_release_ms     (float ms)     { release_ms_   = std::clamp(ms, 1.0f, 5000.0f); update_gate_coeffs(); }
    void set_output         (float db)     { output_lin_   = std::pow(10.0f, db * 0.05f); }
    void set_mix            (float v)      { mix_          = std::clamp(v, 0.0f, 1.0f); }

private:
    float noise_sample(NoiseChannel& ch) const;
    float derived_noise_sample(NoiseChannel& ch, float x) const;
    float granular_sample(NoiseChannel& ch);
    void  update_grain_params();
    void  update_gate_coeffs();
    void  update_modal_coeffs();
    void  try_spawn_grain();

    double     sample_rate_   = 44100.0;
    NoiseType  type_          = NoiseType::White;
    NoiseMode  mode_          = NoiseMode::AlwaysOn;
    NoiseBlend blend_         = NoiseBlend::Add;
    float      mod_           = 0.03f;  // Saturate injection depth
    float     gain_           = 0.5f;
    float     grain_size_ms_  = 50.0f;
    float     density_        = 0.5f;
    float     threshold_lin_  = 0.01f;   // default -40 dBFS
    float     attack_ms_      = 10.0f;
    float     release_ms_     = 300.0f;
    float     output_lin_     = 1.0f;
    float     mix_            = 0.5f;

    NoiseChannel ch_l_, ch_r_, ch_prev_;

    // Granular grain pool (shared timing between L and R channels)
    static constexpr int kMaxGrains = 16;
    struct Grain { float phase = 0, inc = 0; bool active = false; };
    std::array<Grain, kMaxGrains> grains_;
    float spawn_phase_ = 0.0f;
    float spawn_inc_   = 0.0f;
    float grain_size_samples_ = 2205.0f;
    float simplex_step_       = 0.0001f;  // cached from grain_size_ms_ via update_grain_params()

    // Gated mode: smooth envelope follower (attack/release RC model)
    float gate_env_     = 0.0f;
    float gate_smooth_  = 0.0f;
    float gate_alpha_a_ = 0.002f;
    float gate_alpha_r_ = 0.0002f;

    float infrasonic_phase_ = 0.0f;
    float roughness_phase_  = 0.0f;
    float sr_phase_         = 0.0f;  // SampleRate blend: position within decimation window
    float sr_hold_l_        = 0.0f;
    float sr_hold_r_        = 0.0f;

    // Modal resonator coefficients (shared L/R; recomputed lazily when params change)
    static constexpr int kModalModes = 8;
    float modal_b0_[kModalModes] {};
    float modal_a1_[kModalModes] {};
    float modal_a2_[kModalModes] {};
    int   modal_n_active_  = 0;
    bool  modal_dirty_     = true;
};

} // namespace kaos_engine
