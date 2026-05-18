#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "delay_processor.h"

namespace kaos_engine {

class DelayPlugin final : public juce::AudioProcessor {
public:
    DelayPlugin();
    ~DelayPlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::delay"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }

private:
    juce::AudioProcessorValueTreeState apvts_;
    DelayProcessor dsp_;

    std::atomic<float>* p_time_     = nullptr;
    std::atomic<float>* p_mode_     = nullptr;
    std::atomic<float>* p_feedback_ = nullptr;
    std::atomic<float>* p_tone_     = nullptr;
    std::atomic<float>* p_mod_      = nullptr;
    std::atomic<float>* p_mod2_     = nullptr;
    std::atomic<float>* p_output_   = nullptr;
    std::atomic<float>* p_mix_      = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayPlugin)
};

} // namespace kaos_engine
