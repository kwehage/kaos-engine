#include "filter_plugin.h"
#include "filter_editor.h"

namespace kaos_engine {

static constexpr auto kMode      = "mode";
static constexpr auto kCutoff    = "cutoff";
static constexpr auto kResonance = "resonance";
static constexpr auto kGain      = "gain";
static constexpr auto kDrive     = "drive";
static constexpr auto kOutput    = "output";
static constexpr auto kMix       = "mix";

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout FilterPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kMode, 1}, "Mode",
        StringArray{"LP 12", "LP 24", "HP 12", "HP 24",
                    "Band Pass", "Notch", "All Pass",
                    "Peak", "Low Shelf", "Hi Shelf",
                    "Comb", "Ladder"}, 0));

    {
        NormalisableRange<float> r(20.0f, 20000.0f, 0.1f);
        r.setSkewForCentre(1000.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kCutoff, 1}, "Cutoff", r, 1000.0f,
            AudioParameterFloatAttributes().withLabel("Hz")));
    }

    {
        NormalisableRange<float> r(0.05f, 20.0f, 0.01f);
        r.setSkewForCentre(1.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kResonance, 1}, "Resonance", r, 0.7071f));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kGain, 1}, "Gain",
        NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kDrive, 1}, "Drive",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

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

FilterPlugin::FilterPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_FILTER", make_params())
{
    p_mode_      = apvts_.getRawParameterValue(kMode);
    p_cutoff_    = apvts_.getRawParameterValue(kCutoff);
    p_resonance_ = apvts_.getRawParameterValue(kResonance);
    p_gain_      = apvts_.getRawParameterValue(kGain);
    p_drive_     = apvts_.getRawParameterValue(kDrive);
    p_output_    = apvts_.getRawParameterValue(kOutput);
    p_mix_       = apvts_.getRawParameterValue(kMix);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool FilterPlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void FilterPlugin::prepareToPlay(double sr, int bs) { dsp_.prepare(sr, bs); }
void FilterPlugin::releaseResources()               { dsp_.reset(); }

// ── Processing ────────────────────────────────────────────────────────────────

void FilterPlugin::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals ndn;

    dsp_.set_mode     (static_cast<FilterMode>(juce::roundToInt(p_mode_->load())));
    dsp_.set_cutoff   (p_cutoff_->load());
    dsp_.set_resonance(p_resonance_->load());
    dsp_.set_gain_db  (p_gain_->load());
    dsp_.set_drive    (p_drive_->load());
    dsp_.set_output   (p_output_->load());
    dsp_.set_mix      (p_mix_->load());

    const int ns  = buf.getNumSamples();
    const int nch = buf.getNumChannels();

    if (nch >= 2)
        dsp_.process(buf.getWritePointer(0), buf.getWritePointer(1), ns);
    else if (nch == 1) {
        juce::HeapBlock<float> tmp(static_cast<size_t>(ns));
        std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + ns, tmp.getData());
        dsp_.process(buf.getWritePointer(0), tmp.getData(), ns);
    }
}

// ── State ──────────────────────────────────────────────────────────────────────

void FilterPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void FilterPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* FilterPlugin::createEditor() { return new FilterEditor(*this); }

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::FilterPlugin();
}
