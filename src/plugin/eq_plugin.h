#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "eq_processor.h"

namespace kaos_engine {

class EqPlugin final : public juce::AudioProcessor {
public:
    EqPlugin();
    ~EqPlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::eq"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }

    // ── Spectrum analyzer interface ───────────────────────────────────────────
    static constexpr int kFftOrder    = 11;
    static constexpr int kFftSize     = 1 << kFftOrder;
    static constexpr int kFifoCapacity= kFftSize * 4;

    bool pull_fft_block(float* fft_out);

    static constexpr int kNumBands = EqProcessor::kNumBands;  // 7

private:
    juce::AudioProcessorValueTreeState apvts_;
    EqProcessor dsp_;

    juce::AbstractFifo fifo_     { kFifoCapacity };
    float              fifo_buf_ [kFifoCapacity] {};

    void push_to_fifo(float sample);

    // Per-band parameter pointers (7 bands × 4 params each)
    std::atomic<float>* p_freq_[kNumBands] {};
    std::atomic<float>* p_gain_[kNumBands] {};
    std::atomic<float>* p_q_   [kNumBands] {};
    std::atomic<float>* p_type_[kNumBands] {};

    std::atomic<float>* p_output_ = nullptr;
    std::atomic<float>* p_mix_    = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqPlugin)
};

} // namespace kaos_engine
