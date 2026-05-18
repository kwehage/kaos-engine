#include "distortion_plugin.h"
#include "distortion_editor.h"

namespace kaos_engine {

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static constexpr auto kParamDrive         = "drive";
static constexpr auto kParamMode          = "mode";
static constexpr auto kParamFeedback      = "feedback";
static constexpr auto kParamTone          = "tone";
static constexpr auto kParamBias          = "bias";
static constexpr auto kParamOutput        = "output";
static constexpr auto kParamMix           = "mix";
static constexpr auto kParamFilterOn      = "filter_on";
static constexpr auto kParamFilterPos     = "filter_pos";
static constexpr auto kParamFilterType    = "filter_type";
static constexpr auto kParamFilterCutoff  = "filter_cutoff";
static constexpr auto kParamFilterRes     = "filter_res";
static constexpr auto kParamFilterBlend   = "filter_blend";

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout DistortionPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDrive, 1}, "Drive",
        NormalisableRange<float>(0.0f, 40.0f, 0.1f), 12.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamMode, 1}, "Mode",
        StringArray{
            "Soft", "Hard", "Foldback", "Tube",
            "Arctan", "Log", "Sine Fold", "Diode",
            "Half-wave", "Full-wave", "Chebyshev",
            "Bitcrusher", "Sample Rate"
        }, 0));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamFeedback, 1}, "Feedback",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamTone, 1}, "Tone",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamBias, 1}, "Bias",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamOutput, 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{kParamFilterOn, 1}, "Filter On", false));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamFilterPos, 1}, "Filter Position",
        StringArray{"Pre", "Post"}, 0));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamFilterType, 1}, "Filter Type",
        StringArray{"LP", "HP", "BP"}, 0));

    {
        NormalisableRange<float> cutoff_range(20.0f, 20000.0f, 1.0f);
        cutoff_range.setSkewForCentre(1000.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamFilterCutoff, 1}, "Filter Cutoff",
            cutoff_range, 5000.0f,
            AudioParameterFloatAttributes().withLabel("Hz")));
    }

    {
        NormalisableRange<float> res_range(0.1f, 10.0f, 0.01f);
        res_range.setSkewForCentre(1.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamFilterRes, 1}, "Filter Resonance",
            res_range, 0.707f));
    }

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamFilterBlend, 1}, "Filter Blend",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return {params.begin(), params.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────
DistortionPlugin::DistortionPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_DISTORTION", make_params())
{
    p_drive_         = apvts_.getRawParameterValue(kParamDrive);
    p_mode_          = apvts_.getRawParameterValue(kParamMode);
    p_feedback_      = apvts_.getRawParameterValue(kParamFeedback);
    p_tone_          = apvts_.getRawParameterValue(kParamTone);
    p_bias_          = apvts_.getRawParameterValue(kParamBias);
    p_output_        = apvts_.getRawParameterValue(kParamOutput);
    p_mix_           = apvts_.getRawParameterValue(kParamMix);
    p_filter_on_     = apvts_.getRawParameterValue(kParamFilterOn);
    p_filter_pos_    = apvts_.getRawParameterValue(kParamFilterPos);
    p_filter_type_   = apvts_.getRawParameterValue(kParamFilterType);
    p_filter_cutoff_ = apvts_.getRawParameterValue(kParamFilterCutoff);
    p_filter_res_    = apvts_.getRawParameterValue(kParamFilterRes);
    p_filter_blend_  = apvts_.getRawParameterValue(kParamFilterBlend);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void DistortionPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void DistortionPlugin::releaseResources()
{
    dsp_.reset();
}

// ── Audio processing ───────────────────────────────────────────────────────────
void DistortionPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    // Push current parameter values into the DSP
    dsp_.set_drive(p_drive_->load());
    dsp_.set_mode(static_cast<DistortionMode>(static_cast<int>(p_mode_->load())));
    dsp_.set_feedback(p_feedback_->load());
    dsp_.set_tone(p_tone_->load());
    dsp_.set_bias(p_bias_->load());
    dsp_.set_output(p_output_->load());
    dsp_.set_mix(p_mix_->load());
    dsp_.set_filter_enabled(p_filter_on_->load() > 0.5f);
    dsp_.set_filter_pos(static_cast<int>(p_filter_pos_->load()));
    dsp_.set_filter_type(static_cast<int>(p_filter_type_->load()));
    dsp_.set_filter_cutoff(p_filter_cutoff_->load());
    dsp_.set_filter_resonance(p_filter_res_->load());
    dsp_.set_filter_blend(p_filter_blend_->load());

    const int num_samples = buffer.getNumSamples();
    const int num_ch      = buffer.getNumChannels();

    // Clear any extra output channels
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, num_samples);

    if (num_ch >= 2) {
        dsp_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), num_samples);
    } else if (num_ch == 1) {
        // Mono: process as left, copy to right (if output has >1 ch)
        float* ch0 = buffer.getWritePointer(0);
        // Use a temporary for right so we don't double-process
        juce::HeapBlock<float> tmp(static_cast<size_t>(num_samples));
        std::copy(ch0, ch0 + num_samples, tmp.getData());
        dsp_.process(ch0, tmp.getData(), num_samples);
    }
}

// ── State ──────────────────────────────────────────────────────────────────────
void DistortionPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void DistortionPlugin::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Editor ─────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* DistortionPlugin::createEditor()
{
    return new DistortionEditor(*this);
}

} // namespace kaos_engine

// ── JUCE plugin factory ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::DistortionPlugin();
}
