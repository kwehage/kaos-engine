#include "reverb_plugin.h"
#include "reverb_editor.h"

namespace kaos_engine {

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static constexpr auto kParamPreDelay     = "pre_delay";
static constexpr auto kParamAlgorithm   = "algorithm";
static constexpr auto kParamSize         = "size";
static constexpr auto kParamDecay        = "decay";
static constexpr auto kParamDamping      = "damping";
static constexpr auto kParamDiffusion    = "diffusion";
static constexpr auto kParamMod          = "mod";
static constexpr auto kParamMod2         = "mod2";
static constexpr auto kParamOutput       = "output";
static constexpr auto kParamMix          = "mix";
static constexpr auto kParamFilterOn     = "filter_on";
static constexpr auto kParamFilterPos    = "filter_pos";
static constexpr auto kParamFilterType   = "filter_type";
static constexpr auto kParamFilterCutoff = "filter_cutoff";
static constexpr auto kParamFilterRes    = "filter_res";
static constexpr auto kParamFilterBlend  = "filter_blend";

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout ReverbPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    {
        NormalisableRange<float> pd_range(0.0f, 200.0f, 0.1f, 0.4f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamPreDelay, 1}, "Pre-Delay",
            pd_range, 20.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamAlgorithm, 1}, "Algorithm",
        StringArray{"Dattorro", "Schroeder", "FDN", "Gardner", "Moorer", "Velvet Noise", "Shimmer"}, 0));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamSize, 1}, "Size",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDecay, 1}, "Decay",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDamping, 1}, "Damping",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDiffusion, 1}, "Diffusion",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMod, 1}, "Mod 1",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMod2, 1}, "Mod 2",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamOutput, 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));

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
ReverbPlugin::ReverbPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_REVERB", make_params())
{
    p_pre_delay_      = apvts_.getRawParameterValue(kParamPreDelay);
    p_algorithm_      = apvts_.getRawParameterValue(kParamAlgorithm);
    p_size_           = apvts_.getRawParameterValue(kParamSize);
    p_decay_          = apvts_.getRawParameterValue(kParamDecay);
    p_damping_        = apvts_.getRawParameterValue(kParamDamping);
    p_diffusion_      = apvts_.getRawParameterValue(kParamDiffusion);
    p_mod_            = apvts_.getRawParameterValue(kParamMod);
    p_mod2_           = apvts_.getRawParameterValue(kParamMod2);
    p_output_         = apvts_.getRawParameterValue(kParamOutput);
    p_mix_            = apvts_.getRawParameterValue(kParamMix);
    p_filter_on_      = apvts_.getRawParameterValue(kParamFilterOn);
    p_filter_pos_     = apvts_.getRawParameterValue(kParamFilterPos);
    p_filter_type_    = apvts_.getRawParameterValue(kParamFilterType);
    p_filter_cutoff_  = apvts_.getRawParameterValue(kParamFilterCutoff);
    p_filter_res_     = apvts_.getRawParameterValue(kParamFilterRes);
    p_filter_blend_   = apvts_.getRawParameterValue(kParamFilterBlend);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void ReverbPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void ReverbPlugin::releaseResources()
{
    dsp_.reset();
}

// ── Audio processing ───────────────────────────────────────────────────────────
void ReverbPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_pre_delay  (p_pre_delay_->load());
    dsp_.set_algorithm  (static_cast<ReverbAlgorithm>(static_cast<int>(p_algorithm_->load())));
    dsp_.set_size       (p_size_->load());
    dsp_.set_decay      (p_decay_->load());
    dsp_.set_damping    (p_damping_->load());
    dsp_.set_diffusion  (p_diffusion_->load());
    dsp_.set_mod        (p_mod_->load());
    dsp_.set_mod2       (p_mod2_->load());
    dsp_.set_output     (p_output_->load());
    dsp_.set_mix        (p_mix_->load());
    dsp_.set_filter_enabled   (p_filter_on_->load() > 0.5f);
    dsp_.set_filter_pos       (static_cast<int>(p_filter_pos_->load()));
    dsp_.set_filter_type      (static_cast<int>(p_filter_type_->load()));
    dsp_.set_filter_cutoff    (p_filter_cutoff_->load());
    dsp_.set_filter_resonance (p_filter_res_->load());
    dsp_.set_filter_blend     (p_filter_blend_->load());

    const int num_samples = buffer.getNumSamples();
    const int num_ch      = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, num_samples);

    if (num_ch >= 2) {
        dsp_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), num_samples);
    } else if (num_ch == 1) {
        float* ch0 = buffer.getWritePointer(0);
        juce::HeapBlock<float> tmp(static_cast<size_t>(num_samples));
        std::copy(ch0, ch0 + num_samples, tmp.getData());
        dsp_.process(ch0, tmp.getData(), num_samples);
    }
}

// ── State ──────────────────────────────────────────────────────────────────────
void ReverbPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void ReverbPlugin::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Editor ─────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* ReverbPlugin::createEditor()
{
    return new ReverbEditor(*this);
}

} // namespace kaos_engine

// ── JUCE plugin factory ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::ReverbPlugin();
}
