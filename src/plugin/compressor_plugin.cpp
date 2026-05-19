#include "compressor_plugin.h"
#include "compressor_editor.h"

namespace kaos_engine {

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static constexpr auto kAlgo      = "algorithm";
static constexpr auto kThreshold = "threshold";
static constexpr auto kRatio     = "ratio";
static constexpr auto kKnee      = "knee";
static constexpr auto kAttack    = "attack";
static constexpr auto kRelease   = "release";
static constexpr auto kMakeup    = "makeup";
static constexpr auto kOutput    = "output";
static constexpr auto kMix       = "mix";

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout CompressorPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kAlgo, 1}, "Algorithm",
        StringArray{"VCA", "Optical", "FET"}, 0));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kThreshold, 1}, "Threshold",
        NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -18.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    {   // Ratio: 1:1 to 20:1, skewed for better control in the 1-8 range
        NormalisableRange<float> r(1.0f, 20.0f, 0.1f);
        r.setSkewForCentre(4.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRatio, 1}, "Ratio", r, 4.0f));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kKnee, 1}, "Knee",
        NormalisableRange<float>(0.0f, 20.0f, 0.1f), 2.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    {   // Attack: 0.1 to 1000 ms, log skew
        NormalisableRange<float> r(0.1f, 1000.0f, 0.1f);
        r.setSkewForCentre(10.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kAttack, 1}, "Attack", r, 10.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    {   // Release: 10 to 5000 ms, log skew
        NormalisableRange<float> r(10.0f, 5000.0f, 1.0f);
        r.setSkewForCentre(100.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRelease, 1}, "Release", r, 100.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kMakeup, 1}, "Makeup",
        NormalisableRange<float>(-20.0f, 20.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kOutput, 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────
CompressorPlugin::CompressorPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_COMP", make_params())
{
    p_algorithm_ = apvts_.getRawParameterValue(kAlgo);
    p_threshold_ = apvts_.getRawParameterValue(kThreshold);
    p_ratio_     = apvts_.getRawParameterValue(kRatio);
    p_knee_      = apvts_.getRawParameterValue(kKnee);
    p_attack_    = apvts_.getRawParameterValue(kAttack);
    p_release_   = apvts_.getRawParameterValue(kRelease);
    p_makeup_    = apvts_.getRawParameterValue(kMakeup);
    p_output_    = apvts_.getRawParameterValue(kOutput);
    p_mix_       = apvts_.getRawParameterValue(kMix);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void CompressorPlugin::prepareToPlay(double sr, int bs) { dsp_.prepare(sr, bs); }
void CompressorPlugin::releaseResources()               { dsp_.reset(); }

// ── Audio processing ───────────────────────────────────────────────────────────
void CompressorPlugin::processBlock(juce::AudioBuffer<float>& buf,
                                     juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_algorithm(static_cast<CompressorAlgorithm>(
                           juce::roundToInt(p_algorithm_->load())));
    dsp_.set_threshold(p_threshold_->load());
    dsp_.set_ratio    (p_ratio_->load());
    dsp_.set_knee     (p_knee_->load());
    dsp_.set_attack   (p_attack_->load());
    dsp_.set_release  (p_release_->load());
    dsp_.set_makeup   (p_makeup_->load());
    dsp_.set_output   (p_output_->load());
    dsp_.set_mix      (p_mix_->load());

    const int ns  = buf.getNumSamples();
    const int nch = buf.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buf.clear(ch, 0, ns);

    if (nch >= 2)
        dsp_.process(buf.getWritePointer(0), buf.getWritePointer(1), ns);
    else if (nch == 1) {
        juce::HeapBlock<float> tmp(static_cast<size_t>(ns));
        std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + ns, tmp.getData());
        dsp_.process(buf.getWritePointer(0), tmp.getData(), ns);
    }
}

// ── State ──────────────────────────────────────────────────────────────────────
void CompressorPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void CompressorPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* CompressorPlugin::createEditor()
{
    return new CompressorEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::CompressorPlugin();
}
