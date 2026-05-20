#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../../src/effects/filter/filter_processor.h"

namespace kaos_engine {

class FilterPlugin final : public juce::AudioProcessor {
public:
    FilterPlugin();
    ~FilterPlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::filter"; }
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

    FilterMode get_mode()        const { return dsp_.get_mode(); }
    float      get_cutoff()      const { return dsp_.get_cutoff(); }
    float      get_resonance()   const { return dsp_.get_resonance(); }
    float      get_gain_db()     const { return dsp_.get_gain_db(); }
    double     get_sample_rate() const { return getSampleRate(); }

    // ── Spectrum analyser interface ────────────────────────────────────────────
    static constexpr int kFftOrder     = 11;
    static constexpr int kFftSize      = 1 << kFftOrder;   // 2048
    static constexpr int kFifoCapacity = kFftSize * 4;

    bool pull_fft_block(float* fft_out);

private:
    juce::AbstractFifo fifo_     { kFifoCapacity };
    float              fifo_buf_ [kFifoCapacity] {};

    void push_to_fifo(float sample);
    juce::AudioProcessorValueTreeState apvts_;
    FilterProcessor dsp_;

    std::atomic<float>* p_mode_      = nullptr;
    std::atomic<float>* p_cutoff_    = nullptr;
    std::atomic<float>* p_resonance_ = nullptr;
    std::atomic<float>* p_gain_      = nullptr;
    std::atomic<float>* p_drive_     = nullptr;
    std::atomic<float>* p_output_    = nullptr;
    std::atomic<float>* p_mix_       = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterPlugin)
};

} // namespace kaos_engine
