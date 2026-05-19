#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "gate_processor.h"

namespace kaos_engine {

class GatePlugin final : public juce::AudioProcessor {
public:
    GatePlugin();
    ~GatePlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::gate"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }

    float            get_gr_db()      const { return dsp_.get_gr_db(); }
    float            get_level_db()   const { return dsp_.get_level_db(); }
    GateDisplayState get_disp_state() const { return dsp_.get_disp_state(); }

    // For transfer-curve drawing in the editor
    float gain_computer(float level_db) const { return dsp_.gain_computer(level_db); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    GateProcessor dsp_;

    std::atomic<float>* p_algorithm_  = nullptr;
    std::atomic<float>* p_threshold_  = nullptr;
    std::atomic<float>* p_range_      = nullptr;
    std::atomic<float>* p_ratio_      = nullptr;
    std::atomic<float>* p_attack_     = nullptr;
    std::atomic<float>* p_hold_       = nullptr;
    std::atomic<float>* p_release_    = nullptr;
    std::atomic<float>* p_hysteresis_ = nullptr;
    std::atomic<float>* p_output_     = nullptr;
    std::atomic<float>* p_mix_        = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GatePlugin)
};

} // namespace kaos_engine
