#include "stochastic_processor.h"
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void StochasticProcessor::prepare(double sr, int /*bs*/)
{
    sample_rate_ = sr;
    reset();
}

void StochasticProcessor::reset()
{
    phase_      = 0.0f;
    current_    = randbi();
    previous_   = randbi();
    brown_      = 0.0f;
    lx_ = 0.1f; ly_ = 0.0f; lz_ = 0.0f;
    logistic_x_ = 0.5001f + randbi() * 0.001f;
}

void StochasticProcessor::seed(uint32_t s)
{
    rng_.seed(s);
    reset();
}

// ── Output ────────────────────────────────────────────────────────────────────

float StochasticProcessor::next_sample()
{
    if (!running_) {
        last_output_ = std::clamp(current_ * depth_ + offset_, -1.0f, 1.0f);
        return last_output_;
    }

    float raw;
    switch (mode_) {
    case StochasticMode::SampleHold:  raw = sample_sh();       break;
    case StochasticMode::SampleGlide: raw = sample_sg();       break;
    case StochasticMode::Smooth:      raw = sample_smooth();   break;
    case StochasticMode::Brownian:    raw = sample_brownian(); break;
    case StochasticMode::Lorenz:      raw = sample_lorenz();   break;
    case StochasticMode::Logistic:    raw = sample_logistic(); break;
    default:                          raw = 0.0f;              break;
    }
    last_output_ = std::clamp(raw * depth_ + offset_, -1.0f, 1.0f);
    return last_output_;
}

void StochasticProcessor::process(float* out, int n)
{
    for (int i = 0; i < n; ++i)
        out[i] = next_sample();
}

// ── Mode implementations ───────────────────────────────────────────────────────

// SHAPE = output range: 0 -> narrow [-0.1, +0.1], 1 -> full [-1, +1]
float StochasticProcessor::sample_sh()
{
    phase_ += rate_hz_ / float(sample_rate_);
    if (phase_ >= 1.0f) {
        phase_ -= std::floor(phase_);
        previous_ = current_;
        const float range = 0.1f + shape_ * 0.9f;
        current_ = randbi() * range;
    }
    return current_;
}

// SHAPE = glide fraction: 0 -> instant jump, 1 -> always gliding
float StochasticProcessor::sample_sg()
{
    phase_ += rate_hz_ / float(sample_rate_);
    if (phase_ >= 1.0f) {
        phase_ -= std::floor(phase_);
        previous_ = current_;
        current_ = randbi();
    }
    const float gf = std::max(shape_, 0.01f);
    if (phase_ < gf) {
        const float t = phase_ / gf;
        const float s = t * t * (3.0f - 2.0f * t);  // smoothstep
        return previous_ + s * (current_ - previous_);
    }
    return current_;
}

// SHAPE = interpolation curve: 0 -> linear, 1 -> smoothstep
float StochasticProcessor::sample_smooth()
{
    phase_ += rate_hz_ / float(sample_rate_);
    if (phase_ >= 1.0f) {
        phase_ -= std::floor(phase_);
        previous_ = current_;
        current_ = randbi();
    }
    const float t_lin    = phase_;
    const float t_smooth = t_lin * t_lin * (3.0f - 2.0f * t_lin);
    const float t        = t_lin + (t_smooth - t_lin) * shape_;
    return previous_ + t * (current_ - previous_);
}

// SHAPE = mean reversion: 0 -> free drift, 1 -> strong pull to zero
float StochasticProcessor::sample_brownian()
{
    // Wiener process step. At rate_hz_=1, brown_ reaches ~1.4 std dev after one
    // second, which tanh maps to roughly +-0.9 -- good display range.
    const float scale = std::sqrt(2.0f * rate_hz_ / float(sample_rate_));
    brown_ += randbi() * scale;

    // Ornstein-Uhlenbeck mean reversion.
    // Small baseline (0.02) prevents indefinite runaway at SHAPE=0.
    const float theta = 0.02f + shape_ * 5.0f;
    brown_ -= theta * brown_ * (rate_hz_ / float(sample_rate_));

    // Hard bound -- tanh(3)=0.995 so values beyond +-3 contribute nothing.
    brown_ = std::clamp(brown_, -3.0f, 3.0f);

    // tanh maps the accumulated state to [-1,+1] for output ONLY.
    // brown_ itself is NOT overwritten -- that was the original bug: storing
    // tanh(brown_) back squashed the accumulator toward zero every step,
    // preventing any drift regardless of the SHAPE setting.
    return std::tanh(brown_);
}

// RK4 integration of the Lorenz system.
// SHAPE -> rho (24 = simple, 28 = classic butterfly, 36 = wilder)
float StochasticProcessor::sample_lorenz()
{
    const float sigma = 10.0f;
    const float beta  = 8.0f / 3.0f;
    const float rho   = 24.0f + shape_ * 12.0f;

    // dt scaled so rate_hz_=1 gives roughly one orbit per second.
    // The Lorenz attractor has a natural period of ~6 time units, so
    // advancing 6 units/second gives ~1 orbit/second.
    const float dt = std::min(rate_hz_ * 6.0f / float(sample_rate_), 0.05f);

    // RK4 step
    float k1x = sigma*(ly_-lx_),          k1y = lx_*(rho-lz_)-ly_,    k1z = lx_*ly_-beta*lz_;
    float ax = lx_+k1x*dt*0.5f, ay = ly_+k1y*dt*0.5f, az = lz_+k1z*dt*0.5f;
    float k2x = sigma*(ay-ax),             k2y = ax*(rho-az)-ay,        k2z = ax*ay-beta*az;
    float bx = lx_+k2x*dt*0.5f, by = ly_+k2y*dt*0.5f, bz = lz_+k2z*dt*0.5f;
    float k3x = sigma*(by-bx),             k3y = bx*(rho-bz)-by,        k3z = bx*by-beta*bz;
    float cx = lx_+k3x*dt,      cy = ly_+k3y*dt,      cz = lz_+k3z*dt;
    float k4x = sigma*(cy-cx),             k4y = cx*(rho-cz)-cy,        k4z = cx*cy-beta*cz;

    lx_ += (dt / 6.0f) * (k1x + 2*k2x + 2*k3x + k4x);
    ly_ += (dt / 6.0f) * (k1y + 2*k2y + 2*k3y + k4y);
    lz_ += (dt / 6.0f) * (k1z + 2*k2z + 2*k3z + k4z);

    // x component normalized to [-1, +1] (range ~+-20 at classic parameters)
    return std::clamp(lx_ / 20.0f, -1.0f, 1.0f);
}

// SHAPE -> r: 0 = period-2 (r=3.5), 1 = fully chaotic (r=4.0)
float StochasticProcessor::sample_logistic()
{
    phase_ += rate_hz_ / float(sample_rate_);
    if (phase_ >= 1.0f) {
        phase_ -= std::floor(phase_);
        const float r = 3.5f + shape_ * 0.5f;
        logistic_x_ = r * logistic_x_ * (1.0f - logistic_x_);
        // Guard against degenerate fixed points
        if (logistic_x_ < 1e-6f || logistic_x_ > 1.0f - 1e-6f || std::isnan(logistic_x_))
            logistic_x_ = 0.5001f + randbi() * 0.001f;
    }
    return logistic_x_ * 2.0f - 1.0f;  // [0,1] -> [-1,+1]
}

} // namespace kaos_engine
