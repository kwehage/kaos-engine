#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../src/framework/envelope_follower/envelope_follower_processor.h"

namespace kaos_engine {

class EnvelopeFollowerPlugin final : public juce::AudioProcessor {
public:
    EnvelopeFollowerPlugin();
    ~EnvelopeFollowerPlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::envelope-follower"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return true; }
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
    float get_envelope() const { return dsp_.get_envelope(); }
    float get_output()   const { return dsp_.get_output(); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    EnvelopeFollowerProcessor dsp_;

    std::atomic<float>* p_detector_   = nullptr;
    std::atomic<float>* p_out_shape_  = nullptr;
    std::atomic<float>* p_attack_     = nullptr;
    std::atomic<float>* p_release_    = nullptr;
    std::atomic<float>* p_gain_       = nullptr;
    std::atomic<float>* p_depth_      = nullptr;
    std::atomic<float>* p_out_mode_   = nullptr;
    std::atomic<float>* p_cc_number_  = nullptr;
    std::atomic<float>* p_cc_channel_ = nullptr;

    int last_cc_val_ = -1;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeFollowerPlugin)
};

} // namespace kaos_engine
