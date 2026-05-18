#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "modulator_processor.h"

namespace kaos_engine {

class ModulatorPlugin final : public juce::AudioProcessor {
public:
    ModulatorPlugin();
    ~ModulatorPlugin() override = default;

    // ── AudioProcessor ────────────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::modulator"; }
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

private:
    juce::AudioProcessorValueTreeState apvts_;
    ModulatorProcessor dsp_;

    std::atomic<float>* p_rate_     = nullptr;
    std::atomic<float>* p_mode_     = nullptr;
    std::atomic<float>* p_waveform_ = nullptr;
    std::atomic<float>* p_depth_    = nullptr;
    std::atomic<float>* p_bias_     = nullptr;
    std::atomic<float>* p_phase_    = nullptr;
    std::atomic<float>* p_output_   = nullptr;
    std::atomic<float>* p_mix_      = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatorPlugin)
};

} // namespace kaos_engine
