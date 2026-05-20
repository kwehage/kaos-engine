#pragma once
#include <vector>
#include <atomic>
#include <cstdint>

namespace kaos_engine {

enum class LooperState : int {
    Idle       = 0,
    WaitRecord = 1,
    Recording  = 2,
    WaitStop   = 3,
    Playing    = 4,
    Stopped    = 5,
};

enum class LooperPlayback : int {
    Forward       = 0,
    Backward      = 1,
    Bounce        = 2,
    Accumulate    = 3,
    AccumReverse  = 4,
};

class LooperProcessor {
public:
    static constexpr int kPeakBins   = 700;
    static constexpr int kMaxSeconds = 30;

    void prepare(double sample_rate, int max_seconds = kMaxSeconds);
    void reset();

    // Commands — must be called on the audio thread
    void cmd_record();
    void cmd_stop();
    void cmd_clear();

    // Per-block process
    void process(float* L, float* R, int num_samples, bool at_bar_boundary);

    void set_playback       (LooperPlayback m) { playback_   = m; }
    void set_sync           (bool s)           { sync_       = s; }
    void set_bars           (int b)            { bars_       = b; }
    void set_bar_len_samples(int len)          { bar_len_    = len; }
    void set_feedback       (float f)          { feedback_   = f; }
    void set_input_gain     (float g)          { input_gain_ = g; }
    void set_output_gain    (float g)          { out_gain_   = g; }
    void set_mix            (float m)          { mix_        = m; }

    // Thread-safe reads for the editor
    LooperState get_state()       const { return static_cast<LooperState>(state_.load(std::memory_order_relaxed)); }
    int   get_play_pos()          const { return play_pos_.load(std::memory_order_relaxed); }
    int   get_loop_length()       const { return loop_length_.load(std::memory_order_relaxed); }
    int   get_rec_pos()           const { return rec_pos_atomic_.load(std::memory_order_relaxed); }
    const float* peaks_l()        const { return peaks_l_; }
    const float* peaks_r()        const { return peaks_r_; }
    bool  take_waveform_dirty()         { return waveform_dirty_.exchange(false, std::memory_order_acq_rel); }

private:
    void update_peak(int pos);
    void rebuild_peaks();
    void enter_state(LooperState s);
    void flip_and_reduce(int len);
    void begin_playback(int len);

    double sample_rate_ = 48000.0;
    int    max_samples_ = 0;

    std::vector<float> buf_l_, buf_r_;
    float peaks_l_[kPeakBins] {};
    float peaks_r_[kPeakBins] {};

    std::atomic<int>  state_       { 0 };
    std::atomic<int>  play_pos_    { 0 };
    std::atomic<int>  loop_length_ { 0 };
    std::atomic<int>  rec_pos_atomic_ { 0 };
    std::atomic<bool> waveform_dirty_ { false };

    // Audio-thread-only
    int  rec_pos_    = 0;
    int  bounce_dir_ = 1;
    int  bars_rec_   = 0;   // bars elapsed since recording started
    int  bar_cnt_samples_ = 0; // sample counter within current bar (for fallback)

    LooperPlayback playback_  = LooperPlayback::Forward;
    bool           sync_      = false;
    int            bars_      = 1;
    int            bar_len_   = 0;    // samples per bar (0 = unknown)
    float          feedback_  = 0.75f;
    float          input_gain_= 1.0f;
    float          out_gain_  = 1.0f;
    float          mix_       = 1.0f;

    bool pending_cmd_record_ = false;
    bool pending_cmd_stop_   = false;
    bool pending_cmd_clear_  = false;
};

} // namespace kaos_engine
