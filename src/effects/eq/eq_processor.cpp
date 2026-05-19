#include "eq_processor.h"
#include <cmath>
#include <algorithm>

namespace kaos_engine {

static constexpr double kPi = 3.14159265358979323846;

// ── BiquadCoeffs factories (RBJ Cookbook) ──────────────────────────────────────

BiquadCoeffs BiquadCoeffs::identity() { return {}; }

BiquadCoeffs BiquadCoeffs::high_pass(double fs, double f, double q)
{
    const double w0    = 2.0 * kPi * f / fs;
    const double cosw  = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0    = 1.0 + alpha;
    BiquadCoeffs c;
    c.b0 =  (1.0 + cosw) / (2.0 * a0);
    c.b1 = -(1.0 + cosw) / a0;
    c.b2 =  (1.0 + cosw) / (2.0 * a0);
    c.a1 = (-2.0 * cosw) / a0;
    c.a2 =  (1.0 - alpha) / a0;
    return c;
}

BiquadCoeffs BiquadCoeffs::low_pass(double fs, double f, double q)
{
    const double w0    = 2.0 * kPi * f / fs;
    const double cosw  = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0    = 1.0 + alpha;
    BiquadCoeffs c;
    c.b0 =  (1.0 - cosw) / (2.0 * a0);
    c.b1 =  (1.0 - cosw) / a0;
    c.b2 =  (1.0 - cosw) / (2.0 * a0);
    c.a1 = (-2.0 * cosw) / a0;
    c.a2 =  (1.0 - alpha) / a0;
    return c;
}

BiquadCoeffs BiquadCoeffs::low_shelf(double fs, double f, double q, double gain_db)
{
    const double A    = std::pow(10.0, gain_db / 40.0);
    const double w0   = 2.0 * kPi * f / fs;
    const double cosw = std::cos(w0);
    const double sinw = std::sin(w0);
    const double alpha= sinw / (2.0 * q);
    const double sqA  = 2.0 * std::sqrt(A) * alpha;
    const double a0   = (A + 1.0) + (A - 1.0) * cosw + sqA;
    BiquadCoeffs c;
    c.b0 =  A * ((A + 1.0) - (A - 1.0) * cosw + sqA) / a0;
    c.b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cosw) / a0;
    c.b2 =  A * ((A + 1.0) - (A - 1.0) * cosw - sqA) / a0;
    c.a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosw) / a0;
    c.a2 =  ((A + 1.0) + (A - 1.0) * cosw - sqA) / a0;
    return c;
}

BiquadCoeffs BiquadCoeffs::high_shelf(double fs, double f, double q, double gain_db)
{
    const double A    = std::pow(10.0, gain_db / 40.0);
    const double w0   = 2.0 * kPi * f / fs;
    const double cosw = std::cos(w0);
    const double sinw = std::sin(w0);
    const double alpha= sinw / (2.0 * q);
    const double sqA  = 2.0 * std::sqrt(A) * alpha;
    const double a0   = (A + 1.0) - (A - 1.0) * cosw + sqA;
    BiquadCoeffs c;
    c.b0 =  A * ((A + 1.0) + (A - 1.0) * cosw + sqA) / a0;
    c.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw) / a0;
    c.b2 =  A * ((A + 1.0) + (A - 1.0) * cosw - sqA) / a0;
    c.a1 =  2.0 * ((A - 1.0) - (A + 1.0) * cosw) / a0;
    c.a2 =  ((A + 1.0) - (A - 1.0) * cosw - sqA) / a0;
    return c;
}

BiquadCoeffs BiquadCoeffs::peak(double fs, double f, double q, double gain_db)
{
    const double A    = std::pow(10.0, gain_db / 40.0);
    const double w0   = 2.0 * kPi * f / fs;
    const double cosw = std::cos(w0);
    const double alpha= std::sin(w0) / (2.0 * q);
    const double a0   = 1.0 + alpha / A;
    BiquadCoeffs c;
    c.b0 =  (1.0 + alpha * A) / a0;
    c.b1 = (-2.0 * cosw)      / a0;
    c.b2 =  (1.0 - alpha * A) / a0;
    c.a1 = (-2.0 * cosw)      / a0;
    c.a2 =  (1.0 - alpha / A) / a0;
    return c;
}

BiquadCoeffs BiquadCoeffs::notch(double fs, double f, double q)
{
    const double w0    = 2.0 * kPi * f / fs;
    const double cosw  = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);
    const double a0    = 1.0 + alpha;
    BiquadCoeffs c;
    c.b0 =  1.0          / a0;
    c.b1 = (-2.0 * cosw) / a0;
    c.b2 =  1.0          / a0;
    c.a1 = (-2.0 * cosw) / a0;
    c.a2 =  (1.0 - alpha)/ a0;
    return c;
}

double BiquadCoeffs::magnitude_db(double f, double fs) const noexcept
{
    const double phi  = std::pow(std::sin(kPi * f / fs), 2.0);
    const double phi2 = phi * phi;
    const double num  = (b0+b1+b2)*(b0+b1+b2)
                      - 4.0*(b0*b1 + 4.0*b0*b2 + b1*b2)*phi
                      + 16.0*b0*b2*phi2;
    const double den  = (1.0+a1+a2)*(1.0+a1+a2)
                      - 4.0*(a1 + 4.0*a2 + a1*a2)*phi
                      + 16.0*a2*phi2;
    if (den < 1e-30) return 0.0;
    return 10.0 * std::log10(std::max(num / den, 1e-30));
}

// ── EqProcessor ───────────────────────────────────────────────────────────────

void EqProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    for (int b = 0; b < kNumBands; ++b) recompute_band(b);
    reset();
}

void EqProcessor::reset()
{
    for (int b = 0; b < kNumBands; ++b)
        for (int s = 0; s < kMaxStages; ++s) {
            state_l_[b][s].reset();
            state_r_[b][s].reset();
        }
}

void EqProcessor::set_band(int band, const BandConfig& cfg)
{
    if (band < 0 || band >= kNumBands) return;
    config_[band] = cfg;
    recompute_band(band);
}

void EqProcessor::set_output(float db)  { output_linear_ = std::pow(10.0f, db / 20.0f); }
void EqProcessor::set_mix   (float mix) { mix_ = std::clamp(mix, 0.0f, 1.0f); }

void EqProcessor::recompute_band(int b)
{
    using BC = BiquadCoeffs;
    const auto& cfg = config_[b];
    const double fs = sample_rate_;
    const double f  = std::max(double(cfg.freq), 1.0);
    const double q  = std::max(double(cfg.q),   0.01);
    const double g  = double(cfg.gain);

    switch (cfg.type) {
        case BandType::Off:
            coeffs_[b][0] = BC::identity(); n_stages_[b] = 0; break;
        case BandType::Bell:
            coeffs_[b][0] = BC::peak      (fs, f, q, g); n_stages_[b] = 1; break;
        case BandType::Notch:
            coeffs_[b][0] = BC::notch     (fs, f, q);    n_stages_[b] = 1; break;
        case BandType::LowShelf:
            coeffs_[b][0] = BC::low_shelf (fs, f, q, g); n_stages_[b] = 1; break;
        case BandType::HighShelf:
            coeffs_[b][0] = BC::high_shelf(fs, f, q, g); n_stages_[b] = 1; break;
        case BandType::HP12:
            coeffs_[b][0] = BC::high_pass(fs, f, q);     n_stages_[b] = 1; break;
        case BandType::HP24:
            coeffs_[b][0] = BC::high_pass(fs, f, kBW4_Q1);
            coeffs_[b][1] = BC::high_pass(fs, f, kBW4_Q2);
            n_stages_[b]  = 2; break;
        case BandType::LP12:
            coeffs_[b][0] = BC::low_pass(fs, f, q);      n_stages_[b] = 1; break;
        case BandType::LP24:
            coeffs_[b][0] = BC::low_pass(fs, f, kBW4_Q1);
            coeffs_[b][1] = BC::low_pass(fs, f, kBW4_Q2);
            n_stages_[b]  = 2; break;
        default: break;
    }
}

double EqProcessor::magnitude_db(double f) const noexcept
{
    double total = 0.0;
    for (int b = 0; b < kNumBands; ++b) {
        if (config_[b].type == BandType::Off) continue;
        for (int s = 0; s < n_stages_[b]; ++s)
            total += coeffs_[b][s].magnitude_db(f, sample_rate_);
    }
    return total;
}

void EqProcessor::process(float* left, float* right, int num_samples)
{
    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];
        float wet_l = dry_l, wet_r = dry_r;

        for (int b = 0; b < kNumBands; ++b) {
            for (int s = 0; s < n_stages_[b]; ++s) {
                wet_l = state_l_[b][s].process(wet_l, coeffs_[b][s]);
                wet_r = state_r_[b][s].process(wet_r, coeffs_[b][s]);
            }
        }

        wet_l *= output_linear_;
        wet_r *= output_linear_;
        left[i]  = dry_l + mix_ * (wet_l - dry_l);
        right[i] = dry_r + mix_ * (wet_r - dry_r);
    }
}

} // namespace kaos_engine
