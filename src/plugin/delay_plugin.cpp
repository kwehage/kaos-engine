#include "delay_plugin.h"
#include "delay_editor.h"

namespace kaos_engine {

static constexpr auto kParamTime     = "time";
static constexpr auto kParamMode     = "mode";
static constexpr auto kParamFeedback = "feedback";
static constexpr auto kParamTone     = "tone";
static constexpr auto kParamMod      = "mod";
static constexpr auto kParamMod2     = "mod2";
static constexpr auto kParamOutput   = "output";
static constexpr auto kParamMix      = "mix";

juce::AudioProcessorValueTreeState::ParameterLayout DelayPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamTime, 1}, "Time",
        NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.4f), 500.0f,
        AudioParameterFloatAttributes().withLabel("ms")));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamMode, 1}, "Mode",
        StringArray{
            "Standard", "Slapback", "Ping-Pong", "Tape", "Diffusion", "Reverse",
            "Comb", "Multi-Tap", "Shimmer", "Haas", "BBD"
        }, 0));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamFeedback, 1}, "Feedback",
        NormalisableRange<float>(0.0f, 0.99f, 0.001f), 0.4f));

    // Tone kept as a parameter for preset compatibility but is no longer used in DSP
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamTone, 1}, "Tone",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMod, 1}, "Mod",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMod2, 1}, "Mod 2",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamOutput, 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    return {params.begin(), params.end()};
}

DelayPlugin::DelayPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_DELAY", make_params())
{
    p_time_     = apvts_.getRawParameterValue(kParamTime);
    p_mode_     = apvts_.getRawParameterValue(kParamMode);
    p_feedback_ = apvts_.getRawParameterValue(kParamFeedback);
    p_tone_     = apvts_.getRawParameterValue(kParamTone);
    p_mod_      = apvts_.getRawParameterValue(kParamMod);
    p_mod2_     = apvts_.getRawParameterValue(kParamMod2);
    p_output_   = apvts_.getRawParameterValue(kParamOutput);
    p_mix_      = apvts_.getRawParameterValue(kParamMix);
}

void DelayPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void DelayPlugin::releaseResources()
{
    dsp_.reset();
}

void DelayPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                               juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_time_ms  (p_time_->load());
    dsp_.set_mode     (static_cast<DelayMode>(static_cast<int>(p_mode_->load())));
    dsp_.set_feedback (p_feedback_->load());
    dsp_.set_mod      (p_mod_->load());
    dsp_.set_mod2     (p_mod2_->load());
    dsp_.set_output   (p_output_->load());
    dsp_.set_mix      (p_mix_->load());

    const int num_samples = buffer.getNumSamples();
    const int num_ch      = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, num_samples);

    if (num_ch >= 2) {
        dsp_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), num_samples);
    } else if (num_ch == 1) {
        juce::HeapBlock<float> tmp(static_cast<size_t>(num_samples));
        float* ch0 = buffer.getWritePointer(0);
        std::copy(ch0, ch0 + num_samples, tmp.getData());
        dsp_.process(ch0, tmp.getData(), num_samples);
    }
}

void DelayPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void DelayPlugin::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* DelayPlugin::createEditor()
{
    return new DelayEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::DelayPlugin();
}
