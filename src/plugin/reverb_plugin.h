#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "reverb_processor.h"

namespace kaos_engine {

class ReverbPlugin final : public juce::AudioProcessor {
public:
    ReverbPlugin();
    ~ReverbPlugin() override = default;

    // ── AudioProcessor ────────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "kaos-engine::reverb"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

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
    ReverbProcessor dsp_;

    std::atomic<float>* p_pre_delay_      = nullptr;
    std::atomic<float>* p_algorithm_      = nullptr;
    std::atomic<float>* p_size_           = nullptr;
    std::atomic<float>* p_decay_          = nullptr;
    std::atomic<float>* p_damping_        = nullptr;
    std::atomic<float>* p_diffusion_      = nullptr;
    std::atomic<float>* p_mod_            = nullptr;
    std::atomic<float>* p_mod2_           = nullptr;
    std::atomic<float>* p_output_         = nullptr;
    std::atomic<float>* p_mix_            = nullptr;
    std::atomic<float>* p_filter_on_      = nullptr;
    std::atomic<float>* p_filter_pos_     = nullptr;
    std::atomic<float>* p_filter_type_    = nullptr;
    std::atomic<float>* p_filter_cutoff_  = nullptr;
    std::atomic<float>* p_filter_res_     = nullptr;
    std::atomic<float>* p_filter_blend_   = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbPlugin)
};

} // namespace kaos_engine
