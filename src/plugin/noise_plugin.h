#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "noise_processor.h"

namespace kaos_engine {

class NoisePlugin final : public juce::AudioProcessor
{
public:
    NoisePlugin();
    ~NoisePlugin() override = default;

    void prepareToPlay (double sample_rate, int block_size) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()              const override { return false; }
    bool   producesMidi()             const override { return false; }
    bool   isMidiEffect()             const override { return false; }
    double getTailLengthSeconds()     const override { return 0.0; }
    int    getNumPrograms()                 override { return 1; }
    int    getCurrentProgram()              override { return 0; }
    void   setCurrentProgram(int)           override {}
    const juce::String getProgramName(int)  override { return {}; }
    void   changeProgramName(int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest)           override;
    void setStateInformation (const void* data, int size_bytes)  override;

    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }

    float next_preview_sample() { return dsp_.next_preview_sample(); }

    // Most-recent processed output (wet) and raw input (dry) samples, updated
    // each processBlock and read by the editor strip chart at 30 Hz.
    float get_output_sample() const { return output_sample_.load(std::memory_order_relaxed); }
    float get_dry_sample()    const { return dry_sample_   .load(std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    NoiseProcessor dsp_;
    juce::AudioProcessorValueTreeState apvts_;

    std::atomic<float>* p_type_       = nullptr;
    std::atomic<float>* p_mode_       = nullptr;
    std::atomic<float>* p_blend_      = nullptr;
    std::atomic<float>* p_mod_        = nullptr;
    std::atomic<float>* p_gain_       = nullptr;
    std::atomic<float>* p_grain_size_ = nullptr;
    std::atomic<float>* p_density_    = nullptr;
    std::atomic<float>* p_threshold_  = nullptr;
    std::atomic<float>* p_attack_     = nullptr;
    std::atomic<float>* p_release_    = nullptr;
    std::atomic<float>* p_mix_        = nullptr;
    std::atomic<float>* p_output_     = nullptr;

    // ── Spectral OLA state ────────────────────────────────────────────────────
    static constexpr int kSpectOrder = 10;
    static constexpr int kSpectSize  = 1 << kSpectOrder;  // 1024 bins
    static constexpr int kSpectHop   = kSpectSize / 2;    // 512 samples (~11.6 ms)

    // Per-channel OLA state + gate envelope for spectral mode.
    struct OlaChannel {
        float in_buf_   [kSpectSize]     {};  // input ring buffer
        float out_buf_  [kSpectSize * 2] {};  // OLA accumulator (cleared as read)
        float fft_buf_  [kSpectSize * 2] {};  // FFT working buffer
        float dry_buf_  [kSpectHop]      {};  // dry delay line (kSpectHop samples)
        int   in_pos_    = 0;
        int   hop_count_ = 0;
        int   out_read_  = 0;
        int   out_write_ = kSpectHop;   // starts kSpectHop ahead → latency = kSpectHop
        int   dry_pos_   = 0;
        // Per-channel gate envelope
        float gate_env_  = 0.0f;
        float gate_smooth_ = 0.0f;
    };

    juce::dsp::FFT                      spect_fft_  { kSpectOrder };
    juce::dsp::WindowingFunction<float> spect_win_  {
        size_t(kSpectSize),
        juce::dsp::WindowingFunction<float>::hann };

    float      hann_win_[kSpectSize] {};  // pre-extracted Hann window
    OlaChannel ola_l_, ola_r_;
    NoiseChannel spect_noise_l_, spect_noise_r_;  // per-bin noise sources

    void reset_ola();
    float process_ola_sample(OlaChannel& ch, NoiseChannel& noise_ch,
                              float in, float mod, float mix,
                              float threshold_lin,
                              float gate_alpha_a, float gate_alpha_r,
                              NoiseMode mode, float output_lin);
    void  process_ola_frame (OlaChannel& ch, NoiseChannel& noise_ch, float mod);

    std::atomic<float> output_sample_ {0.0f};
    std::atomic<float> dry_sample_    {0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoisePlugin)
};

} // namespace kaos_engine
