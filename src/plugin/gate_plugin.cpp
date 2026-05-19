#include "gate_plugin.h"
#include "gate_editor.h"

namespace kaos_engine {

static constexpr auto kAlgo       = "algorithm";
static constexpr auto kThreshold  = "threshold";
static constexpr auto kRange      = "range";
static constexpr auto kRatio      = "ratio";
static constexpr auto kAttack     = "attack";
static constexpr auto kHold       = "hold";
static constexpr auto kRelease    = "release";
static constexpr auto kHysteresis = "hysteresis";
static constexpr auto kOutput     = "output";
static constexpr auto kMix        = "mix";

juce::AudioProcessorValueTreeState::ParameterLayout GatePlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kAlgo, 1}, "Algorithm",
        StringArray{"Gate", "Expander", "Ducker"}, 0));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kThreshold, 1}, "Threshold",
        NormalisableRange<float>(-80.0f, 0.0f, 0.1f), -40.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kRange, 1}, "Range",
        NormalisableRange<float>(-80.0f, -0.1f, 0.1f), -60.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    {
        NormalisableRange<float> r(1.0f, 100.0f, 0.1f);
        r.setSkewForCentre(10.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRatio, 1}, "Ratio", r, 10.0f));
    }

    {
        NormalisableRange<float> r(0.1f, 500.0f, 0.1f);
        r.setSkewForCentre(5.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kAttack, 1}, "Attack", r, 2.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    {
        NormalisableRange<float> r(0.0f, 2000.0f, 1.0f);
        r.setSkewForCentre(100.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kHold, 1}, "Hold", r, 50.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    {
        NormalisableRange<float> r(10.0f, 5000.0f, 1.0f);
        r.setSkewForCentre(200.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRelease, 1}, "Release", r, 200.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kHysteresis, 1}, "Hysteresis",
        NormalisableRange<float>(0.0f, 20.0f, 0.1f), 6.0f,
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

GatePlugin::GatePlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",   juce::AudioChannelSet::stereo(), true)
          .withInput ("Key In",  juce::AudioChannelSet::stereo(), false)
          .withOutput("Output",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Gate CV", juce::AudioChannelSet::mono(),   false)),
      apvts_(*this, nullptr, "KE_GATE", make_params())
{
    p_algorithm_  = apvts_.getRawParameterValue(kAlgo);
    p_threshold_  = apvts_.getRawParameterValue(kThreshold);
    p_range_      = apvts_.getRawParameterValue(kRange);
    p_ratio_      = apvts_.getRawParameterValue(kRatio);
    p_attack_     = apvts_.getRawParameterValue(kAttack);
    p_hold_       = apvts_.getRawParameterValue(kHold);
    p_release_    = apvts_.getRawParameterValue(kRelease);
    p_hysteresis_ = apvts_.getRawParameterValue(kHysteresis);
    p_output_     = apvts_.getRawParameterValue(kOutput);
    p_mix_        = apvts_.getRawParameterValue(kMix);
}

bool GatePlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    const auto& cv  = layouts.getChannelSet(false, 1);
    const auto& key = layouts.getChannelSet(true,  1);
    return (cv.isDisabled()  || cv  == juce::AudioChannelSet::mono())
        && (key.isDisabled() || key == juce::AudioChannelSet::mono()
                             || key == juce::AudioChannelSet::stereo());
}

void GatePlugin::prepareToPlay(double sr, int bs) { dsp_.prepare(sr, bs); }
void GatePlugin::releaseResources()               { dsp_.reset(); }

void GatePlugin::processBlock(juce::AudioBuffer<float>& buf,
                               juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_algorithm  (static_cast<GateAlgorithm>(juce::roundToInt(p_algorithm_->load())));
    dsp_.set_threshold  (p_threshold_->load());
    dsp_.set_range      (p_range_->load());
    dsp_.set_ratio      (p_ratio_->load());
    dsp_.set_attack     (p_attack_->load());
    dsp_.set_hold       (p_hold_->load());
    dsp_.set_release    (p_release_->load());
    dsp_.set_hysteresis (p_hysteresis_->load());
    dsp_.set_output     (p_output_->load());
    dsp_.set_mix        (p_mix_->load());

    const int ns  = buf.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buf.clear(ch, 0, ns);

    // Key In sidechain bus (input bus 1). Read before any writes to shared buffer slots.
    auto key_buf      = getBusBuffer(buf, true, 1);
    const int key_ch  = key_buf.getNumChannels();
    const float* key_l = (key_ch >= 1) ? key_buf.getReadPointer(0) : nullptr;
    // Fold mono key to both channels; stereo uses its own R channel.
    const float* key_r = (key_ch >= 2) ? key_buf.getReadPointer(1) : key_l;

    auto main_buf = getBusBuffer(buf, true, 0);
    if (main_buf.getNumChannels() >= 2)
        dsp_.process(buf.getWritePointer(0), buf.getWritePointer(1), ns, key_l, key_r);
    else if (main_buf.getNumChannels() == 1) {
        juce::HeapBlock<float> tmp(static_cast<size_t>(ns));
        std::copy(buf.getReadPointer(0), buf.getReadPointer(0) + ns, tmp.getData());
        dsp_.process(buf.getWritePointer(0), tmp.getData(), ns, key_l, key_r);
    }

    // Write gate state to optional CV output bus. 1.0 = open/hold, 0.0 = closed.
    auto cv_buf = getBusBuffer(buf, false, 1);
    if (cv_buf.getNumChannels() >= 1) {
        const float cv = (dsp_.get_disp_state() >= GateDisplayState::Hold) ? 1.0f : 0.0f;
        float* cv_ptr  = cv_buf.getWritePointer(0);
        for (int i = 0; i < ns; ++i) cv_ptr[i] = cv;
    }
}

void GatePlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void GatePlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* GatePlugin::createEditor() { return new GateEditor(*this); }

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::GatePlugin();
}
