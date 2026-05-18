#include "delay_processor.h"
#include <algorithm>
#include <cstdlib>

namespace kaos_engine {

DelayProcessor::DelayProcessor()
{
    buf_l_.assign(kBufSize, 0.0f);
    buf_r_.assign(kBufSize, 0.0f);
}

void DelayProcessor::prepare(double sample_rate, int /*block_size*/)
{
    sample_rate_ = sample_rate;
    update_lfo_inc();
    update_mod_lp_coeff();
    update_mod2_lp_coeff();
    update_bbd_lp();
    rebuild_allpass_buffers();
    reset();
}

void DelayProcessor::reset()
{
    std::fill(buf_l_.begin(), buf_l_.end(), 0.0f);
    std::fill(buf_r_.begin(), buf_r_.end(), 0.0f);
    write_pos_      = 0;
    rev_phase_      = 0;
    shimmer_phase_  = 0.0f;
    lfo_phase_      = 0.0f;
    tone_l_ = tone_r_ = bbd_lp_l_ = bbd_lp_r_ = 0.0f;
    dc_xl_ = dc_yl_ = dc_xr_ = dc_yr_ = 0.0f;

    for (auto& ap : ap_) {
        std::fill(ap.buf_l.begin(), ap.buf_l.end(), 0.0f);
        std::fill(ap.buf_r.begin(), ap.buf_r.end(), 0.0f);
        ap.pos = 0;
    }
}

// ── Setters ────────────────────────────────────────────────────────────────────

void DelayProcessor::set_time_ms(float ms)
{
    delay_samples_ = std::clamp((float)(ms * 0.001 * sample_rate_),
                                1.0f, (float)(kBufSize - 4));
}

void DelayProcessor::set_feedback(float fb) { feedback_ = std::clamp(fb, 0.0f, 0.99f); }

void DelayProcessor::set_tone(float /*n*/) {}   // LP now controlled by MOD / MOD2

void DelayProcessor::set_mod(float m)
{
    mod_ = std::clamp(m, 0.0f, 1.0f);
    update_lfo_inc();
    update_mod_lp_coeff();
}

void DelayProcessor::set_mod2(float m)
{
    mod2_ = std::clamp(m, 0.0f, 1.0f);
    update_mod2_lp_coeff();
}

void DelayProcessor::set_output(float db)
{
    output_linear_ = std::pow(10.0f, db / 20.0f);
}

void DelayProcessor::set_mix(float mix)   { mix_ = std::clamp(mix, 0.0f, 1.0f); }

void DelayProcessor::set_mode(DelayMode mode)
{
    if (mode_ != mode) { mode_ = mode; reset(); }
}

// ── Helpers ────────────────────────────────────────────────────────────────────

float DelayProcessor::lp_coeff_from_norm(float n) const
{
    const float freq = 500.0f * std::pow(40.0f, n);   // 500–20000 Hz
    const float w    = 2.0f * kPi * freq / (float)sample_rate_;
    return std::clamp(1.0f - std::exp(-w), 0.0f, 1.0f);
}

void DelayProcessor::update_mod_lp_coeff()
{
    mod_lp_coeff_ = lp_coeff_from_norm(mod_);
}

void DelayProcessor::update_mod2_lp_coeff()
{
    mod2_lp_coeff_ = lp_coeff_from_norm(mod2_);
}

void DelayProcessor::update_lfo_inc()
{
    // mod_ 0..1 -> LFO 0.05..5 Hz (log)
    lfo_inc_ = 2.0f * kPi * 0.05f * std::pow(100.0f, mod_) / (float)sample_rate_;
}

void DelayProcessor::update_bbd_lp()
{
    // Fixed ~3 kHz LP to model BBD clock-artefact filter
    const float w = 2.0f * kPi * 3000.0f / (float)sample_rate_;
    bbd_lp_coeff_ = std::clamp(1.0f - std::exp(-w), 0.0f, 1.0f);
}

void DelayProcessor::rebuild_allpass_buffers()
{
    for (int i = 0; i < 4; ++i) {
        const int samps = std::max(1, (int)(kApMs[i] * 0.001f * sample_rate_));
        int sz = 1;
        while (sz < samps + 2) sz <<= 1;
        ap_[i].delay = samps;
        ap_[i].buf_l.assign(sz, 0.0f);
        ap_[i].buf_r.assign(sz, 0.0f);
        ap_[i].pos   = 0;
    }
}

float DelayProcessor::AllpassStage::process_l(float x)
{
    const int mask = (int)buf_l.size() - 1;
    const float del = buf_l[(pos - delay) & mask];
    const float v   = x + coeff * del;
    buf_l[pos & mask] = v;
    pos = (pos + 1) & mask;
    return -coeff * v + del;
}

float DelayProcessor::AllpassStage::process_r(float x)
{
    const int mask = (int)buf_r.size() - 1;
    const float del = buf_r[(pos - delay - 1) & mask];
    const float v   = x + coeff * del;
    buf_r[pos & mask] = v;
    return -coeff * v + del;
}

float DelayProcessor::read_hermite(const std::vector<float>& buf, float d) const
{
    const int   di = (int)d;
    const float fr = d - di;
    const float x0 = buf[(write_pos_ - di + 1) & kBufMask];
    const float x1 = buf[(write_pos_ - di    ) & kBufMask];
    const float x2 = buf[(write_pos_ - di - 1) & kBufMask];
    const float x3 = buf[(write_pos_ - di - 2) & kBufMask];
    const float c0 = x1;
    const float c1 = 0.5f * (x2 - x0);
    const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
    const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * fr + c2) * fr + c1) * fr + c0;
}

float DelayProcessor::apply_dc_block(float x, float& x_prev, float& y_prev)
{
    const float y = x - x_prev + kDcR * y_prev;
    x_prev = x; y_prev = y;
    return y;
}

float DelayProcessor::apply_tone(float x, float& state, float coeff)
{
    state += coeff * (x - state);
    return state;
}

float DelayProcessor::apply_lp(float x, float& state, float coeff)
{
    state += coeff * (x - state);
    return state;
}

float DelayProcessor::soft_clip(float x) { return std::tanh(x); }

// ── Block processing ───────────────────────────────────────────────────────────

void DelayProcessor::process(float* left, float* right, int num_samples)
{
    const int   delay_int  = std::max(1, (int)delay_samples_);
    const float ap_coeff   = 0.1f + mod_ * 0.8f;  // diffusion
    const float cross_fb   = mod_ * 0.5f;          // ping-pong cross-feed
    const float lfo_depth  = delay_samples_ * 0.10f * mod_; // tape/BBD depth

    // Shimmer grain size: 150 ms, capped to half the delay
    const float grain_size = std::min(0.15f * (float)sample_rate_, delay_samples_ * 0.5f);

    for (auto& ap : ap_) ap.coeff = ap_coeff;

    for (int i = 0; i < num_samples; ++i) {
        const float dry_l = left[i];
        const float dry_r = right[i];
        float wet_l = 0.0f, wet_r = 0.0f;

        // LFO tick (Tape / BBD)
        const float lfo_val = std::sin(lfo_phase_);
        lfo_phase_ += lfo_inc_;
        if (lfo_phase_ >= 2.0f * kPi) lfo_phase_ -= 2.0f * kPi;

        switch (mode_) {

        // ── Standard ──────────────────────────────────────────────────────────
        // MOD controls LP cutoff in feedback path
        case DelayMode::Standard: {
            wet_l = read_hermite(buf_l_, delay_samples_);
            wet_r = read_hermite(buf_r_, delay_samples_);
            const float fb_l = apply_dc_block(apply_tone(wet_l * feedback_, tone_l_, mod_lp_coeff_), dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(apply_tone(wet_r * feedback_, tone_r_, mod_lp_coeff_), dc_xr_, dc_yr_);
            buf_l_[write_pos_] = dry_l + fb_l;
            buf_r_[write_pos_] = dry_r + fb_r;
            break;
        }

        // ── Slapback ──────────────────────────────────────────────────────────
        case DelayMode::Slapback: {
            const float t = std::min(delay_samples_, (float)(sample_rate_ * 0.2));
            wet_l = read_hermite(buf_l_, t);
            wet_r = read_hermite(buf_r_, t);
            buf_l_[write_pos_] = dry_l;
            buf_r_[write_pos_] = dry_r;
            break;
        }

        // ── Ping-Pong ─────────────────────────────────────────────────────────
        // MOD = cross-feed amount, MOD2 controls LP cutoff
        case DelayMode::PingPong: {
            wet_l = read_hermite(buf_r_, delay_samples_);
            wet_r = read_hermite(buf_l_, delay_samples_);
            const float fb_l = apply_dc_block(apply_tone((wet_l * feedback_ + wet_r * cross_fb), tone_l_, mod2_lp_coeff_), dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(apply_tone((wet_r * feedback_ + wet_l * cross_fb), tone_r_, mod2_lp_coeff_), dc_xr_, dc_yr_);
            buf_l_[write_pos_] = dry_l + fb_l;
            buf_r_[write_pos_] = dry_r + fb_r;
            break;
        }

        // ── Tape ──────────────────────────────────────────────────────────────
        // MOD = LFO rate, MOD2 controls LP cutoff
        case DelayMode::Tape: {
            const float mod_d = delay_samples_ + lfo_val * lfo_depth;
            wet_l = read_hermite(buf_l_, mod_d);
            wet_r = read_hermite(buf_r_, mod_d);
            const float fb_l = apply_dc_block(apply_tone(soft_clip(wet_l * feedback_), tone_l_, mod2_lp_coeff_), dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(apply_tone(soft_clip(wet_r * feedback_), tone_r_, mod2_lp_coeff_), dc_xr_, dc_yr_);
            buf_l_[write_pos_] = dry_l + fb_l;
            buf_r_[write_pos_] = dry_r + fb_r;
            break;
        }

        // ── Diffusion ─────────────────────────────────────────────────────────
        // MOD = allpass coefficient, MOD2 controls LP cutoff
        case DelayMode::Diffusion: {
            float diff_l = dry_l, diff_r = dry_r;
            for (auto& ap : ap_) { diff_l = ap.process_l(diff_l); diff_r = ap.process_r(diff_r); }
            wet_l = read_hermite(buf_l_, delay_samples_);
            wet_r = read_hermite(buf_r_, delay_samples_);
            const float fb_l = apply_dc_block(apply_tone(wet_l * feedback_, tone_l_, mod2_lp_coeff_), dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(apply_tone(wet_r * feedback_, tone_r_, mod2_lp_coeff_), dc_xr_, dc_yr_);
            buf_l_[write_pos_] = diff_l + fb_l;
            buf_r_[write_pos_] = diff_r + fb_r;
            break;
        }

        // ── Reverse ───────────────────────────────────────────────────────────
        case DelayMode::Reverse: {
            const int rev_read = (write_pos_ - rev_phase_) & kBufMask;
            wet_l = buf_l_[rev_read];
            wet_r = buf_r_[rev_read];
            rev_phase_++;
            if (rev_phase_ >= delay_int) rev_phase_ = 0;
            buf_l_[write_pos_] = dry_l + apply_dc_block(wet_l * feedback_, dc_xl_, dc_yl_);
            buf_r_[write_pos_] = dry_r + apply_dc_block(wet_r * feedback_, dc_xr_, dc_yr_);
            break;
        }

        // ── Comb filter ───────────────────────────────────────────────────────
        // MOD controls LP cutoff; left-of-center = dark resonance, right = bright metallic
        case DelayMode::Comb: {
            wet_l = read_hermite(buf_l_, delay_samples_);
            wet_r = read_hermite(buf_r_, delay_samples_);
            const float fb_l = wet_l * feedback_;
            const float fb_r = wet_r * feedback_;
            buf_l_[write_pos_] = dry_l + apply_tone(fb_l, tone_l_, mod_lp_coeff_);
            buf_r_[write_pos_] = dry_r + apply_tone(fb_r, tone_r_, mod_lp_coeff_);
            break;
        }

        // ── Multi-tap (3 taps at 1x / 1.5x / 2x time) ────────────────────────
        // MOD = tap level balance, MOD2 controls LP cutoff
        case DelayMode::MultiTap: {
            const float t1 = delay_samples_;
            const float t2 = delay_samples_ * 1.5f;
            const float t3 = delay_samples_ * 2.0f;
            const float w1 = 1.0f;
            const float w2 = 0.4f + mod_ * 0.4f;
            const float w3 = mod_ * 0.5f;
            const float norm = 1.0f / (w1 + w2 + w3 + 0.001f);
            wet_l = (read_hermite(buf_l_, t1) * w1 +
                     read_hermite(buf_l_, t2) * w2 +
                     read_hermite(buf_l_, std::min(t3, (float)(kBufSize - 4))) * w3) * norm;
            wet_r = (read_hermite(buf_r_, t1) * w1 +
                     read_hermite(buf_r_, t2) * w2 +
                     read_hermite(buf_r_, std::min(t3, (float)(kBufSize - 4))) * w3) * norm;
            const float fb_l = apply_dc_block(apply_tone(wet_l * feedback_, tone_l_, mod2_lp_coeff_), dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(apply_tone(wet_r * feedback_, tone_r_, mod2_lp_coeff_), dc_xr_, dc_yr_);
            buf_l_[write_pos_] = dry_l + fb_l;
            buf_r_[write_pos_] = dry_r + fb_r;
            break;
        }

        // ── Shimmer (+1 octave pitch-shifted feedback, 2-head crossfade) ──────
        // MOD = shimmer blend, MOD2 controls LP cutoff
        case DelayMode::Shimmer: {
            shimmer_phase_ += 1.0f;
            if (shimmer_phase_ >= grain_size) shimmer_phase_ -= grain_size;

            const float ph2 = shimmer_phase_ + grain_size * 0.5f >= grain_size
                              ? shimmer_phase_ + grain_size * 0.5f - grain_size
                              : shimmer_phase_ + grain_size * 0.5f;

            const float w1 = 1.0f - std::abs(shimmer_phase_ / grain_size - 0.5f) * 2.0f;
            const float w2 = 1.0f - w1;

            const float d1 = delay_samples_ - shimmer_phase_;
            const float d2 = delay_samples_ - ph2;

            const auto safe = [&](float d) {
                return std::clamp(d, 1.0f, (float)(kBufSize - 4));
            };

            const float pitched_l = w1 * read_hermite(buf_l_, safe(d1))
                                  + w2 * read_hermite(buf_l_, safe(d2));
            const float pitched_r = w1 * read_hermite(buf_r_, safe(d1))
                                  + w2 * read_hermite(buf_r_, safe(d2));

            const float shimmer_mix = mod_;
            const float unshifted_l = read_hermite(buf_l_, delay_samples_);
            const float unshifted_r = read_hermite(buf_r_, delay_samples_);
            wet_l = unshifted_l * (1.0f - shimmer_mix) + pitched_l * shimmer_mix;
            wet_r = unshifted_r * (1.0f - shimmer_mix) + pitched_r * shimmer_mix;

            const float fb_l = apply_dc_block(apply_tone(wet_l * feedback_, tone_l_, mod2_lp_coeff_), dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(apply_tone(wet_r * feedback_, tone_r_, mod2_lp_coeff_), dc_xr_, dc_yr_);
            buf_l_[write_pos_] = dry_l + fb_l;
            buf_r_[write_pos_] = dry_r + fb_r;
            break;
        }

        // ── Haas effect (L/R offset for stereo widening) ─────────────────────
        case DelayMode::Haas: {
            const float offset = std::min(delay_samples_,
                                          (float)(sample_rate_ * 0.04)); // cap 40 ms
            wet_l = dry_l;
            wet_r = read_hermite(buf_r_, offset);
            buf_l_[write_pos_] = dry_l;
            buf_r_[write_pos_] = dry_r;
            left[i]  = dry_l * output_linear_;
            right[i] = (dry_r + mix_ * (wet_r - dry_r)) * output_linear_;
            write_pos_ = (write_pos_ + 1) & kBufMask;
            continue;
        }

        // ── BBD (bucket-brigade device emulation) ─────────────────────────────
        // MOD = LFO rate; fixed 3 kHz output LP (no apply_tone)
        case DelayMode::BBD: {
            const float mod_d = delay_samples_ + lfo_val * lfo_depth;
            wet_l = apply_lp(read_hermite(buf_l_, mod_d), bbd_lp_l_, bbd_lp_coeff_);
            wet_r = apply_lp(read_hermite(buf_r_, mod_d), bbd_lp_r_, bbd_lp_coeff_);
            const float fb_l = apply_dc_block(wet_l * feedback_, dc_xl_, dc_yl_);
            const float fb_r = apply_dc_block(wet_r * feedback_, dc_xr_, dc_yr_);
            buf_l_[write_pos_] = dry_l + fb_l;
            buf_r_[write_pos_] = dry_r + fb_r;
            break;
        }

        default: break;
        }

        write_pos_ = (write_pos_ + 1) & kBufMask;
        left[i]  = (dry_l + mix_ * (wet_l - dry_l)) * output_linear_;
        right[i] = (dry_r + mix_ * (wet_r - dry_r)) * output_linear_;
    }
}

} // namespace kaos_engine
