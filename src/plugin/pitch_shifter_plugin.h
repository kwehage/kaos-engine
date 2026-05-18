#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "pitch_shifter_processor.h"

namespace kaos_engine {

class PitchShifterPlugin final : public juce::AudioProcessor {
public:
    PitchShifterPlugin();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms()    override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    juce::AudioProcessorValueTreeState apvts_;
    PitchShifterProcessor dsp_;

    std::atomic<float>* p_algorithm_  = nullptr;
    std::atomic<float>* p_pitch_[PitchShifterProcessor::kNumVoices]  = {};
    std::atomic<float>* p_detune_[PitchShifterProcessor::kNumVoices] = {};
    std::atomic<float>* p_gain_[PitchShifterProcessor::kNumVoices]   = {};
    std::atomic<float>* p_mod1_[PitchShifterProcessor::kNumVoices]   = {};
    std::atomic<float>* p_mod2_[PitchShifterProcessor::kNumVoices]   = {};
    std::atomic<float>* p_mix_     = nullptr;
    std::atomic<float>* p_output_  = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchShifterPlugin)
};

} // namespace kaos_engine
