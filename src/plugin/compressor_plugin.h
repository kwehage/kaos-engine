#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "compressor_processor.h"

namespace kaos_engine {

class CompressorPlugin final : public juce::AudioProcessor {
public:
    CompressorPlugin();
    ~CompressorPlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::compressor"; }
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
    float get_gain_reduction_db() const { return dsp_.get_gain_reduction_db(); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    CompressorProcessor dsp_;

    std::atomic<float>* p_algorithm_  = nullptr;
    std::atomic<float>* p_threshold_  = nullptr;
    std::atomic<float>* p_ratio_      = nullptr;
    std::atomic<float>* p_knee_       = nullptr;
    std::atomic<float>* p_attack_     = nullptr;
    std::atomic<float>* p_release_    = nullptr;
    std::atomic<float>* p_makeup_     = nullptr;
    std::atomic<float>* p_output_     = nullptr;
    std::atomic<float>* p_mix_        = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorPlugin)
};

} // namespace kaos_engine
