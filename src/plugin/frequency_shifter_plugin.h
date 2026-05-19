#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "frequency_shifter_processor.h"

namespace kaos_engine {

class FrequencyShifterPlugin final : public juce::AudioProcessor {
public:
    FrequencyShifterPlugin();
    ~FrequencyShifterPlugin() override = default;

    // ── AudioProcessor ────────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::frequency-shifter"; }
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
    FrequencyShifterProcessor dsp_;

    std::atomic<float>* p_shift_     = nullptr;
    std::atomic<float>* p_direction_     = nullptr;
    std::atomic<float>* p_feedback_mode_ = nullptr;
    std::atomic<float>* p_feedback_  = nullptr;
    std::atomic<float>* p_delay_     = nullptr;
    std::atomic<float>* p_lfo_rate_  = nullptr;
    std::atomic<float>* p_lfo_depth_ = nullptr;
    std::atomic<float>* p_tone_      = nullptr;
    std::atomic<float>* p_drive_     = nullptr;
    std::atomic<float>* p_diffusion_ = nullptr;
    std::atomic<float>* p_output_    = nullptr;
    std::atomic<float>* p_mix_       = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyShifterPlugin)
};

} // namespace kaos_engine
