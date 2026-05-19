#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../src/framework/lfo/lfo_processor.h"

namespace kaos_engine {

enum class LfoTriggerMode : int {
    Free          = 0,   // always running
    NoteRetrigger = 1,   // MIDI note-on resets, note-off holds
    Transport     = 2,   // play → reset+run, stop → hold
    Sidechain     = 3,   // audio bus: high → reset+run, low → hold
};

class LfoPlugin final : public juce::AudioProcessor {
public:
    LfoPlugin();
    ~LfoPlugin() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::lfo"; }
    bool acceptsMidi()  const override { return true; }  // for Note Retrigger mode
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
    float get_phase() const { return lfo_.get_phase(); }

private:
    juce::AudioProcessorValueTreeState apvts_;
    LfoProcessor lfo_;

    std::atomic<float>* p_waveform_    = nullptr;
    std::atomic<float>* p_rate_        = nullptr;
    std::atomic<float>* p_depth_       = nullptr;
    std::atomic<float>* p_shape_       = nullptr;
    std::atomic<float>* p_phase_off_   = nullptr;
    std::atomic<float>* p_sync_        = nullptr;
    std::atomic<float>* p_sync_div_    = nullptr;
    std::atomic<float>* p_output_mode_ = nullptr;
    std::atomic<float>* p_cc_number_   = nullptr;
    std::atomic<float>* p_cc_channel_  = nullptr;
    std::atomic<float>* p_offset_       = nullptr;
    std::atomic<float>* p_trigger_mode_ = nullptr;

    int  last_cc_val_       = -1;
    int  last_trigger_mode_ = 0;   // tracks mode changes to gate set_running(false)
    bool was_playing_       = false;   // transport state, previous block
    bool sc_gate_open_      = false;   // sidechain gate state, previous sample

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoPlugin)
};

} // namespace kaos_engine
