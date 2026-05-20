#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "spectrogram_processor.h"

namespace kaos_engine {

class SpectrogramPlugin final : public juce::AudioProcessor
{
public:
    SpectrogramPlugin();
    ~SpectrogramPlugin() override = default;

    void prepareToPlay (double sample_rate, int block_size) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()          const override { return false; }
    bool   producesMidi()         const override { return false; }
    bool   isMidiEffect()         const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int    getNumPrograms()             override { return 1; }
    int    getCurrentProgram()          override { return 0; }
    void   setCurrentProgram(int)       override {}
    const juce::String getProgramName(int) override { return {}; }
    void   changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest)          override;
    void setStateInformation(const void* data, int size_bytes) override;

    // ── Spectrum feed ─────────────────────────────────────────────────────────
    static constexpr int kFftOrder     = SpectrogramDefs::kFftOrder;
    static constexpr int kFftSize      = SpectrogramDefs::kFftSize;
    static constexpr int kFifoCapacity = SpectrogramDefs::kFifoCapacity;

    bool pull_fft_block(float* fft_out);
    double get_sample_rate() const { return getSampleRate(); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    juce::AbstractFifo fifo_     { kFifoCapacity };
    float              fifo_buf_ [kFifoCapacity] {};

    void push_to_fifo(float sample);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramPlugin)
};

} // namespace kaos_engine
