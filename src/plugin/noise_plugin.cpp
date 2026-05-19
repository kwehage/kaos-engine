#include "noise_plugin.h"
#include "noise_editor.h"

namespace kaos_engine {

static constexpr auto kType      = "noise_type";
static constexpr auto kMode      = "noise_mode";
static constexpr auto kGain      = "gain";
static constexpr auto kGrainSize = "grain_size";
static constexpr auto kDensity   = "density";
static constexpr auto kThreshold = "threshold";
static constexpr auto kAttack    = "attack";
static constexpr auto kRelease   = "release";
static constexpr auto kMix       = "mix";
static constexpr auto kOutput    = "output";

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
NoisePlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kType, 1}, "Noise Type",
        StringArray{"White", "Pink", "Brown", "Granular"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kMode, 1}, "Mode",
        StringArray{"Follow", "Gated", "Always On"}, 0));

    {
        NormalisableRange<float> r(0.0f, 1.0f, 0.001f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kGain, 1}, "Gain", r, 0.5f));
    }

    {
        NormalisableRange<float> r(5.0f, 500.0f, 1.0f);
        r.setSkewForCentre(50.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kGrainSize, 1}, "Grain Size", r, 50.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kDensity, 1}, "Density",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    {
        NormalisableRange<float> r(-60.0f, 0.0f, 0.1f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kThreshold, 1}, "Threshold", r, -40.0f,
            AudioParameterFloatAttributes().withLabel("dB")));
    }

    {
        NormalisableRange<float> r(0.1f, 500.0f, 0.1f);
        r.setSkewForCentre(10.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kAttack, 1}, "Attack", r, 10.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }
    {
        NormalisableRange<float> r(1.0f, 5000.0f, 1.0f);
        r.setSkewForCentre(300.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRelease, 1}, "Release", r, 300.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    {
        NormalisableRange<float> r(-20.0f, 6.0f, 0.1f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kOutput, 1}, "Output", r, 0.0f,
            AudioParameterFloatAttributes().withLabel("dB")));
    }

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────

NoisePlugin::NoisePlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_NOISE", make_params())
{
    p_type_       = apvts_.getRawParameterValue(kType);
    p_mode_       = apvts_.getRawParameterValue(kMode);
    p_gain_       = apvts_.getRawParameterValue(kGain);
    p_grain_size_ = apvts_.getRawParameterValue(kGrainSize);
    p_density_    = apvts_.getRawParameterValue(kDensity);
    p_threshold_  = apvts_.getRawParameterValue(kThreshold);
    p_attack_     = apvts_.getRawParameterValue(kAttack);
    p_release_    = apvts_.getRawParameterValue(kRelease);
    p_mix_        = apvts_.getRawParameterValue(kMix);
    p_output_     = apvts_.getRawParameterValue(kOutput);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool NoisePlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void NoisePlugin::prepareToPlay(double sr, int bs)  { dsp_.prepare(sr, bs); }
void NoisePlugin::releaseResources()                { dsp_.reset(); }

// ── Processing ────────────────────────────────────────────────────────────────

void NoisePlugin::processBlock(juce::AudioBuffer<float>& buf,
                                juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals ndn;

    dsp_.set_type       (static_cast<NoiseType>(juce::roundToInt(p_type_->load())));
    dsp_.set_mode       (static_cast<NoiseMode>(juce::roundToInt(p_mode_->load())));
    dsp_.set_gain       (p_gain_      ->load());
    dsp_.set_grain_size_ms(p_grain_size_->load());
    dsp_.set_grain_density(p_density_  ->load());
    dsp_.set_threshold_db (p_threshold_->load());
    dsp_.set_attack_ms    (p_attack_   ->load());
    dsp_.set_release_ms   (p_release_  ->load());
    dsp_.set_mix        (p_mix_       ->load());
    dsp_.set_output     (p_output_    ->load());

    float* left  = buf.getWritePointer(0);
    float* right = buf.getWritePointer(1);
    dsp_.process(left, right, buf.getNumSamples());
}

// ── State ──────────────────────────────────────────────────────────────────────

void NoisePlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void NoisePlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* NoisePlugin::createEditor()
{
    return new NoiseEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::NoisePlugin();
}
