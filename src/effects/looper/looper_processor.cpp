#include "looper_processor.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace kaos_engine {

void LooperProcessor::prepare(double sr, int max_sec)
{
    sample_rate_ = sr;
    max_samples_ = int(sr * max_sec);
    buf_l_.assign(max_samples_, 0.0f);
    buf_r_.assign(max_samples_, 0.0f);
    reset();
}

void LooperProcessor::reset()
{
    enter_state(LooperState::Idle);
    rec_pos_    = 0;
    bounce_dir_ = 1;
    bars_rec_   = 0;
    bar_cnt_samples_ = 0;
    pending_cmd_record_ = false;
    pending_cmd_stop_   = false;
    pending_cmd_clear_  = false;
    play_pos_.store(0, std::memory_order_relaxed);
    loop_length_.store(0, std::memory_order_relaxed);
    rec_pos_atomic_.store(0, std::memory_order_relaxed);
    std::fill(peaks_l_, peaks_l_ + kPeakBins, 0.0f);
    std::fill(peaks_r_, peaks_r_ + kPeakBins, 0.0f);
    waveform_dirty_.store(true, std::memory_order_relaxed);
}

void LooperProcessor::enter_state(LooperState s)
{
    state_.store(static_cast<int>(s), std::memory_order_relaxed);
}

// ── Commands ──────────────────────────────────────────────────────────────────

void LooperProcessor::cmd_record() { pending_cmd_record_ = true; }
void LooperProcessor::cmd_stop()   { pending_cmd_stop_   = true; }
void LooperProcessor::cmd_clear()  { pending_cmd_clear_  = true; }

// ── Helpers ───────────────────────────────────────────────────────────────────

void LooperProcessor::update_peak(int pos)
{
    if (max_samples_ <= 0) return;
    const int bin = int(int64_t(pos) * kPeakBins / max_samples_);
    if (bin >= 0 && bin < kPeakBins) {
        peaks_l_[bin] = std::max(peaks_l_[bin], std::abs(buf_l_[pos]));
        peaks_r_[bin] = std::max(peaks_r_[bin], std::abs(buf_r_[pos]));
    }
}

void LooperProcessor::rebuild_peaks()
{
    std::fill(peaks_l_, peaks_l_ + kPeakBins, 0.0f);
    std::fill(peaks_r_, peaks_r_ + kPeakBins, 0.0f);
    const int len = loop_length_.load(std::memory_order_relaxed);
    if (len <= 0) return;
    for (int i = 0; i < len; ++i) {
        const int bin = int(int64_t(i) * kPeakBins / len);
        if (bin < kPeakBins) {
            peaks_l_[bin] = std::max(peaks_l_[bin], std::abs(buf_l_[i]));
            peaks_r_[bin] = std::max(peaks_r_[bin], std::abs(buf_r_[i]));
        }
    }
}

// Reverse the buffer and scale all samples by feedback_ — called once per loop pass
// for AccumReverse mode.  The new input from this pass is already baked in before
// we call this, so it gets the same feedback reduction as the rest of the buffer.
void LooperProcessor::flip_and_reduce(int len)
{
    const float fb = feedback_;
    for (int j = 0; j < len; ++j) {
        buf_l_[j] *= fb;
        buf_r_[j] *= fb;
    }
    for (int j = 0, k = len - 1; j < k; ++j, --k) {
        std::swap(buf_l_[j], buf_l_[k]);
        std::swap(buf_r_[j], buf_r_[k]);
    }
}

// Finalise a recording and begin playback.  AccumReverse immediately flips the
// freshly-recorded buffer so the very first playback pass hears the reversal.
void LooperProcessor::begin_playback(int len)
{
    loop_length_.store(len, std::memory_order_relaxed);
    if (playback_ == LooperPlayback::AccumReverse)
        flip_and_reduce(len);
    rebuild_peaks();
    waveform_dirty_.store(true, std::memory_order_relaxed);
    play_pos_.store(0, std::memory_order_relaxed);
    bounce_dir_ = 1;
    enter_state(LooperState::Playing);
}

// ── Per-block processing ─────────────────────────────────────────────────────

void LooperProcessor::process(float* L, float* R, int ns, bool at_bar_boundary)
{
    // Clear always takes effect immediately
    if (pending_cmd_clear_) {
        pending_cmd_clear_ = false;
        pending_cmd_record_ = false;
        pending_cmd_stop_   = false;
        rec_pos_ = 0;
        bars_rec_ = 0;
        std::fill(peaks_l_, peaks_l_ + kPeakBins, 0.0f);
        std::fill(peaks_r_, peaks_r_ + kPeakBins, 0.0f);
        loop_length_.store(0, std::memory_order_relaxed);
        play_pos_.store(0, std::memory_order_relaxed);
        rec_pos_atomic_.store(0, std::memory_order_relaxed);
        waveform_dirty_.store(true, std::memory_order_relaxed);
        enter_state(LooperState::Idle);
    }

    const LooperState st = get_state();

    // ── State transitions at block boundary ────────────────────────────────
    if (pending_cmd_record_) {
        pending_cmd_record_ = false;
        if (st == LooperState::Idle || st == LooperState::Stopped) {
            rec_pos_ = 0;
            bars_rec_ = 0;
            std::fill(peaks_l_, peaks_l_ + kPeakBins, 0.0f);
            std::fill(peaks_r_, peaks_r_ + kPeakBins, 0.0f);
            loop_length_.store(0, std::memory_order_relaxed);
            waveform_dirty_.store(true, std::memory_order_relaxed);
            if (sync_ && bar_len_ > 0)
                enter_state(LooperState::WaitRecord);
            else
                enter_state(LooperState::Recording);
        }
    }

    if (pending_cmd_stop_) {
        pending_cmd_stop_ = false;
        const LooperState st2 = get_state();
        if (st2 == LooperState::Recording) {
            if (sync_ && bar_len_ > 0)
                enter_state(LooperState::WaitStop);
            else
                begin_playback(rec_pos_);
        } else if (st2 == LooperState::WaitRecord) {
            enter_state(LooperState::Idle);
        } else if (st2 == LooperState::Playing) {
            enter_state(LooperState::Stopped);
        }
    }

    // ── Bar boundary transitions ────────────────────────────────────────────
    if (at_bar_boundary) {
        const LooperState st3 = get_state();
        if (st3 == LooperState::WaitRecord) {
            rec_pos_ = 0;
            bars_rec_ = 0;
            enter_state(LooperState::Recording);
        } else if (st3 == LooperState::Recording) {
            ++bars_rec_;
            if (bars_ > 0 && bars_rec_ >= bars_)
                begin_playback(rec_pos_);
        } else if (st3 == LooperState::WaitStop) {
            begin_playback(rec_pos_);
        }
    }

    // ── Per-sample processing ───────────────────────────────────────────────
    const LooperState cur = get_state();
    const float ig = input_gain_;
    const float og = out_gain_;
    const float mx = mix_;
    const float fb = feedback_;
    const int   ll = loop_length_.load(std::memory_order_relaxed);
    int         pp = play_pos_.load(std::memory_order_relaxed);

    for (int i = 0; i < ns; ++i) {
        const float in_l = ig * L[i];
        const float in_r = ig * R[i];
        float out_l, out_r;

        switch (cur) {
            case LooperState::Recording:
            case LooperState::WaitStop: {
                if (rec_pos_ < max_samples_) {
                    buf_l_[rec_pos_] = in_l;
                    buf_r_[rec_pos_] = in_r;
                    update_peak(rec_pos_);
                    ++rec_pos_;
                    rec_pos_atomic_.store(rec_pos_, std::memory_order_relaxed);
                    if (rec_pos_ >= max_samples_)
                        begin_playback(rec_pos_);
                }
                out_l = og * L[i];
                out_r = og * R[i];
                break;
            }

            case LooperState::Playing: {
                if (ll <= 0) { out_l = og * L[i]; out_r = og * R[i]; break; }

                const float wet_l = buf_l_[pp];
                const float wet_r = buf_r_[pp];

                // Overdub for accumulate modes
                const bool do_overdub = (playback_ == LooperPlayback::Accumulate ||
                                         playback_ == LooperPlayback::AccumReverse);
                if (do_overdub) {
                    buf_l_[pp] += in_l;
                    buf_r_[pp] += in_r;
                    const int bin = int(int64_t(pp) * kPeakBins / ll);
                    if (bin >= 0 && bin < kPeakBins) {
                        peaks_l_[bin] = std::max(peaks_l_[bin], std::abs(buf_l_[pp]));
                        peaks_r_[bin] = std::max(peaks_r_[bin], std::abs(buf_r_[pp]));
                    }
                }

                // Advance playhead; detect loop boundary
                bool boundary = false;
                if (playback_ == LooperPlayback::Backward) {
                    --pp;
                    if (pp < 0) { boundary = true; pp = ll - 1; }
                } else if (playback_ == LooperPlayback::Bounce) {
                    pp += bounce_dir_;
                    if (bounce_dir_ == 1 && pp >= ll) {
                        pp = ll - 2; bounce_dir_ = -1;
                    } else if (bounce_dir_ == -1 && pp < 0) {
                        boundary = true; pp = 1; bounce_dir_ = 1;
                    }
                } else {
                    // Forward, Accumulate, AccumReverse all advance forward
                    ++pp;
                    if (pp >= ll) { boundary = true; pp = 0; }
                }

                // At loop boundary: apply feedback to the whole buffer, then
                // reverse it for AccumReverse.  All modes participate so that
                // FEEDBACK controls the decay rate regardless of playback style.
                if (boundary) {
                    for (int j = 0; j < ll; ++j) {
                        buf_l_[j] *= fb;
                        buf_r_[j] *= fb;
                    }
                    if (playback_ == LooperPlayback::AccumReverse) {
                        for (int j = 0, k = ll - 1; j < k; ++j, --k) {
                            std::swap(buf_l_[j], buf_l_[k]);
                            std::swap(buf_r_[j], buf_r_[k]);
                        }
                    }
                    rebuild_peaks();
                    waveform_dirty_.store(true, std::memory_order_relaxed);
                }

                out_l = og * ((1.0f - mx) * L[i] + mx * wet_l);
                out_r = og * ((1.0f - mx) * R[i] + mx * wet_r);
                break;
            }

            default:
                out_l = og * L[i];
                out_r = og * R[i];
                break;
        }

        L[i] = out_l;
        R[i] = out_r;
    }

    if (cur == LooperState::Playing)
        play_pos_.store(pp, std::memory_order_relaxed);
}

} // namespace kaos_engine
