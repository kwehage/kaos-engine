#include "modulator_plugin.h"
#include "modulator_editor.h"

namespace kaos_engine {

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static constexpr auto kParamRate     = "rate";
static constexpr auto kParamMode     = "mode";
static constexpr auto kParamWaveform = "waveform";
static constexpr auto kParamDepth    = "depth";
static constexpr auto kParamBias     = "bias";
static constexpr auto kParamPhase    = "phase";
static constexpr auto kParamOutput   = "output";
static constexpr auto kParamMix      = "mix";

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout ModulatorPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    {
        NormalisableRange<float> rate_range(0.05f, 10000.0f);
        rate_range.setSkewForCentre(100.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamRate, 1}, "Rate",
            rate_range, 4.0f,
            AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction([](float v, int) {
                    return juce::String(v, 2);
                })));
    }

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamMode, 1}, "Mode",
        StringArray{"Tremolo", "AM", "Ring Mod"}, 0));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamWaveform, 1}, "Waveform",
        StringArray{"Sine", "Triangle", "Square", "Saw"}, 0));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDepth, 1}, "Depth",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamBias, 1}, "Bias",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamPhase, 1}, "Phase",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f,
        AudioParameterFloatAttributes().withLabel("deg")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamOutput, 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return {params.begin(), params.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────
ModulatorPlugin::ModulatorPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_MODULATOR", make_params())
{
    p_rate_     = apvts_.getRawParameterValue(kParamRate);
    p_mode_     = apvts_.getRawParameterValue(kParamMode);
    p_waveform_ = apvts_.getRawParameterValue(kParamWaveform);
    p_depth_    = apvts_.getRawParameterValue(kParamDepth);
    p_bias_     = apvts_.getRawParameterValue(kParamBias);
    p_phase_    = apvts_.getRawParameterValue(kParamPhase);
    p_output_   = apvts_.getRawParameterValue(kParamOutput);
    p_mix_      = apvts_.getRawParameterValue(kParamMix);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void ModulatorPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void ModulatorPlugin::releaseResources()
{
    dsp_.reset();
}

// ── Audio processing ───────────────────────────────────────────────────────────
void ModulatorPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_rate(p_rate_->load());
    dsp_.set_mode(static_cast<ModulatorMode>(static_cast<int>(p_mode_->load())));
    dsp_.set_waveform(static_cast<ModulatorWaveform>(static_cast<int>(p_waveform_->load())));
    dsp_.set_depth(p_depth_->load());
    dsp_.set_bias(p_bias_->load());
    dsp_.set_phase_offset(p_phase_->load());
    dsp_.set_output(p_output_->load());
    dsp_.set_mix(p_mix_->load());

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
void ModulatorPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void ModulatorPlugin::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Editor ─────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* ModulatorPlugin::createEditor()
{
    return new ModulatorEditor(*this);
}

} // namespace kaos_engine

// ── JUCE plugin factory ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::ModulatorPlugin();
}
