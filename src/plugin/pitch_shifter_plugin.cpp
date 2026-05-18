#include "pitch_shifter_plugin.h"
#include "pitch_shifter_editor.h"

namespace kaos_engine {

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static constexpr auto kParamAlgo    = "algorithm";
static constexpr auto kParamPitch1  = "pitch1";
static constexpr auto kParamPitch2  = "pitch2";
static constexpr auto kParamPitch3  = "pitch3";
static constexpr auto kParamDetune1 = "detune1";
static constexpr auto kParamDetune2 = "detune2";
static constexpr auto kParamDetune3 = "detune3";
static constexpr auto kParamGain1   = "gain1";
static constexpr auto kParamGain2   = "gain2";
static constexpr auto kParamGain3   = "gain3";
static constexpr auto kParamMod1_1  = "mod1_1";
static constexpr auto kParamMod1_2  = "mod1_2";
static constexpr auto kParamMod1_3  = "mod1_3";
static constexpr auto kParamMod2_1  = "mod2_1";
static constexpr auto kParamMod2_2  = "mod2_2";
static constexpr auto kParamMod2_3  = "mod2_3";
static constexpr auto kParamMix     = "mix";
static constexpr auto kParamOutput  = "output";

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout PitchShifterPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamAlgo, 1}, "Algorithm",
        StringArray{"Granular", "Smooth", "Tape"}, 0));

    // Per-voice pitch (semitones, integer steps)
    for (auto [id, name] : { std::pair{kParamPitch1,"Pitch 1"}, {kParamPitch2,"Pitch 2"}, {kParamPitch3,"Pitch 3"} })
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id, 1}, name,
            NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withLabel("st")));

    // Per-voice detune (cents, fine)
    for (auto [id, name] : { std::pair{kParamDetune1,"Detune 1"}, {kParamDetune2,"Detune 2"}, {kParamDetune3,"Detune 3"} })
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id, 1}, name,
            NormalisableRange<float>(-50.0f, 50.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel("ct")));

    // Per-voice gain (0..1, V1 default=1, V2/V3 default=0)
    const float gain_defaults[3] = { 1.0f, 0.0f, 0.0f };
    int gi = 0;
    for (auto [id, name] : { std::pair{kParamGain1,"Gain 1"}, {kParamGain2,"Gain 2"}, {kParamGain3,"Gain 3"} })
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id, 1}, name,
            NormalisableRange<float>(0.0f, 1.0f, 0.001f), gain_defaults[gi++]));

    // Per-voice MOD 1 (grain size / flutter rate), default 0.3
    for (auto [id, name] : { std::pair{kParamMod1_1,"Mod 1 V1"}, {kParamMod1_2,"Mod 1 V2"}, {kParamMod1_3,"Mod 1 V3"} })
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id, 1}, name,
            NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));

    // Per-voice MOD 2 (chaos / flutter depth), default 0
    for (auto [id, name] : { std::pair{kParamMod2_1,"Mod 2 V1"}, {kParamMod2_2,"Mod 2 V2"}, {kParamMod2_3,"Mod 2 V3"} })
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id, 1}, name,
            NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamOutput, 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    return {params.begin(), params.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────
PitchShifterPlugin::PitchShifterPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_PITCH_SHIFTER", make_params())
{
    p_algorithm_ = apvts_.getRawParameterValue(kParamAlgo);
    p_pitch_[0]  = apvts_.getRawParameterValue(kParamPitch1);
    p_pitch_[1]  = apvts_.getRawParameterValue(kParamPitch2);
    p_pitch_[2]  = apvts_.getRawParameterValue(kParamPitch3);
    p_detune_[0] = apvts_.getRawParameterValue(kParamDetune1);
    p_detune_[1] = apvts_.getRawParameterValue(kParamDetune2);
    p_detune_[2] = apvts_.getRawParameterValue(kParamDetune3);
    p_gain_[0]   = apvts_.getRawParameterValue(kParamGain1);
    p_gain_[1]   = apvts_.getRawParameterValue(kParamGain2);
    p_gain_[2]   = apvts_.getRawParameterValue(kParamGain3);
    p_mod1_[0]   = apvts_.getRawParameterValue(kParamMod1_1);
    p_mod1_[1]   = apvts_.getRawParameterValue(kParamMod1_2);
    p_mod1_[2]   = apvts_.getRawParameterValue(kParamMod1_3);
    p_mod2_[0]   = apvts_.getRawParameterValue(kParamMod2_1);
    p_mod2_[1]   = apvts_.getRawParameterValue(kParamMod2_2);
    p_mod2_[2]   = apvts_.getRawParameterValue(kParamMod2_3);
    p_mix_       = apvts_.getRawParameterValue(kParamMix);
    p_output_    = apvts_.getRawParameterValue(kParamOutput);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void PitchShifterPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void PitchShifterPlugin::releaseResources() { dsp_.reset(); }

// ── Audio processing ───────────────────────────────────────────────────────────
void PitchShifterPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_algorithm(static_cast<PitchShifterAlgorithm>(static_cast<int>(p_algorithm_->load())));
    for (int vi = 0; vi < PitchShifterProcessor::kNumVoices; ++vi) {
        dsp_.set_voice_pitch (vi, p_pitch_[vi]->load());
        dsp_.set_voice_detune(vi, p_detune_[vi]->load());
        dsp_.set_voice_gain  (vi, p_gain_[vi]->load());
        dsp_.set_voice_mod1  (vi, p_mod1_[vi]->load());
        dsp_.set_voice_mod2  (vi, p_mod2_[vi]->load());
    }
    dsp_.set_mix   (p_mix_->load());
    dsp_.set_output(p_output_->load());

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
void PitchShifterPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void PitchShifterPlugin::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Editor ─────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* PitchShifterPlugin::createEditor()
{
    return new PitchShifterEditor(*this);
}

} // namespace kaos_engine

// ── JUCE plugin factory ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::PitchShifterPlugin();
}
