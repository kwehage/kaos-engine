#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "noise_processor.h"

namespace kaos_engine {

class NoisePlugin final : public juce::AudioProcessor
{
public:
    NoisePlugin();
    ~NoisePlugin() override = default;

    void prepareToPlay (double sample_rate, int block_size) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()              const override { return false; }
    bool   producesMidi()             const override { return false; }
    bool   isMidiEffect()             const override { return false; }
    double getTailLengthSeconds()     const override { return 0.0; }
    int    getNumPrograms()                 override { return 1; }
    int    getCurrentProgram()              override { return 0; }
    void   setCurrentProgram(int)           override {}
    const juce::String getProgramName(int)  override { return {}; }
    void   changeProgramName(int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest)           override;
    void setStateInformation (const void* data, int size_bytes)  override;

    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorValueTreeState& get_apvts() { return apvts_; }

    // For editor waveform preview (mono, no wet/dry, no gate).
    float next_preview_sample() { return dsp_.next_preview_sample(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout make_params();

    NoiseProcessor dsp_;
    juce::AudioProcessorValueTreeState apvts_;

    std::atomic<float>* p_type_       = nullptr;
    std::atomic<float>* p_mode_       = nullptr;
    std::atomic<float>* p_gain_       = nullptr;
    std::atomic<float>* p_grain_size_ = nullptr;
    std::atomic<float>* p_density_    = nullptr;
    std::atomic<float>* p_threshold_  = nullptr;
    std::atomic<float>* p_attack_     = nullptr;
    std::atomic<float>* p_release_    = nullptr;
    std::atomic<float>* p_mix_        = nullptr;
    std::atomic<float>* p_output_     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoisePlugin)
};

} // namespace kaos_engine
