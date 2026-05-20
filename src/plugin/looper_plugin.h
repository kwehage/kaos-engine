#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "looper_processor.h"

namespace kaos_engine {

class LooperPlugin final : public juce::AudioProcessor {
public:
    LooperPlugin();
    ~LooperPlugin() override = default;

    void prepareToPlay(double sample_rate, int block_size) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int size) override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }
    LooperProcessor& get_dsp() { return dsp_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    LooperProcessor dsp_;
    juce::AudioProcessorValueTreeState apvts_;

    std::atomic<float>* p_sync_mode_  = nullptr;
    std::atomic<float>* p_bars_       = nullptr;
    std::atomic<float>* p_playback_   = nullptr;
    std::atomic<float>* p_feedback_   = nullptr;
    std::atomic<float>* p_input_gain_ = nullptr;
    std::atomic<float>* p_out_gain_   = nullptr;
    std::atomic<float>* p_mix_        = nullptr;
    std::atomic<float>* p_cc_record_  = nullptr;
    std::atomic<float>* p_cc_stop_    = nullptr;
    std::atomic<float>* p_cc_clear_   = nullptr;
    std::atomic<float>* p_cc_channel_ = nullptr;

    double sample_rate_ = 48000.0;
    // Track last CC values to avoid re-triggering on hold
    int last_cc_rec_val_  = -1;
    int last_cc_stop_val_ = -1;
    int last_cc_clr_val_  = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperPlugin)
};

} // namespace kaos_engine
