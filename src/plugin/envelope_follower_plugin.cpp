#include "envelope_follower_plugin.h"
#include "envelope_follower_editor.h"

namespace kaos_engine {

static constexpr auto kDetector  = "detector";
static constexpr auto kOutShape  = "output_shape";
static constexpr auto kAttack    = "attack";
static constexpr auto kRelease   = "release";
static constexpr auto kGain      = "gain";
static constexpr auto kDepth     = "depth";
static constexpr auto kOutMode   = "output_mode";
static constexpr auto kCcNumber  = "cc_number";
static constexpr auto kCcChannel = "cc_channel";

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
EnvelopeFollowerPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kDetector, 1}, "Detector",
        StringArray{"Peak", "RMS"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kOutShape, 1}, "Output Shape",
        StringArray{"Follow", "Duck", "Rise", "Fall", "Release"}, 0));

    {
        NormalisableRange<float> r(0.1f, 500.0f, 0.1f);
        r.setSkewForCentre(10.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kAttack, 1}, "Attack", r, 10.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }
    {
        NormalisableRange<float> r(1.0f, 5000.0f, 1.0f);
        r.setSkewForCentre(200.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRelease, 1}, "Release", r, 200.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    // Gain: 0.1x to 20x, skew centre at 1x (unity)
    {
        NormalisableRange<float> r(0.1f, 20.0f, 0.01f);
        r.setSkewForCentre(1.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kGain, 1}, "Gain", r, 1.0f));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kDepth, 1}, "Depth",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kOutMode, 1}, "Output Mode",
        StringArray{"MIDI CC", "Audio CV", "MIDI CC + CV"}, 2));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcNumber, 1}, "CC Number",
        NormalisableRange<float>(0.0f, 127.0f, 1.0f), 1.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcChannel, 1}, "CC Channel",
        NormalisableRange<float>(1.0f, 16.0f, 1.0f), 1.0f));

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────

EnvelopeFollowerPlugin::EnvelopeFollowerPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",    juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain",juce::AudioChannelSet::stereo(), false)
          .withOutput("Main",     juce::AudioChannelSet::stereo(), true)
          .withOutput("CV Out",   juce::AudioChannelSet::mono(),   false)),
      apvts_(*this, nullptr, "KE_ENV", make_params())
{
    p_detector_   = apvts_.getRawParameterValue(kDetector);
    p_out_shape_  = apvts_.getRawParameterValue(kOutShape);
    p_attack_     = apvts_.getRawParameterValue(kAttack);
    p_release_    = apvts_.getRawParameterValue(kRelease);
    p_gain_       = apvts_.getRawParameterValue(kGain);
    p_depth_      = apvts_.getRawParameterValue(kDepth);
    p_out_mode_   = apvts_.getRawParameterValue(kOutMode);
    p_cc_number_  = apvts_.getRawParameterValue(kCcNumber);
    p_cc_channel_ = apvts_.getRawParameterValue(kCcChannel);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool EnvelopeFollowerPlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto& sc = layouts.getChannelSet(true,  1);
    const auto& cv = layouts.getChannelSet(false, 1);
    return (sc.isDisabled() || sc == juce::AudioChannelSet::stereo()
                             || sc == juce::AudioChannelSet::mono())
        && (cv.isDisabled() || cv == juce::AudioChannelSet::mono());
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void EnvelopeFollowerPlugin::prepareToPlay(double sr, int bs)
{
    dsp_.prepare(sr, bs);
    last_cc_val_ = -1;
}

void EnvelopeFollowerPlugin::releaseResources() { dsp_.reset(); }

// ── Processing ────────────────────────────────────────────────────────────────

void EnvelopeFollowerPlugin::processBlock(juce::AudioBuffer<float>& buf,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals ndn;

    dsp_.set_detector    (static_cast<EnvelopeDetector>   (juce::roundToInt(p_detector_->load())));
    dsp_.set_output_shape(static_cast<EnvelopeOutputShape>(juce::roundToInt(p_out_shape_->load())));
    dsp_.set_attack_ms   (p_attack_ ->load());
    dsp_.set_release_ms(p_release_->load());
    dsp_.set_gain      (p_gain_   ->load());
    dsp_.set_depth     (p_depth_  ->load());

    const int mode    = juce::roundToInt(p_out_mode_  ->load());
    const int cc_num  = juce::roundToInt(p_cc_number_ ->load());
    const int cc_chan = juce::roundToInt(p_cc_channel_->load());
    const int ns      = buf.getNumSamples();

    // Prefer the sidechain bus when connected; fall back to main input.
    auto sc_buf  = getBusBuffer(buf, true, 1);
    auto main_in = getBusBuffer(buf, true, 0);
    const bool has_sc = (sc_buf.getNumChannels() >= 1);

    const float* det_l = has_sc ? sc_buf .getReadPointer(0) : main_in.getReadPointer(0);
    const float* det_r = has_sc ? (sc_buf.getNumChannels() >= 2 ? sc_buf.getReadPointer(1)
                                                                 : sc_buf.getReadPointer(0))
                                : main_in.getReadPointer(1);

    const bool cv_on  = (mode == 1 || mode == 2);
    const bool mid_on = (mode == 0 || mode == 2);

    auto cv_buf = getBusBuffer(buf, false, 1);
    float* cv_ptr = (cv_on && cv_buf.getNumChannels() >= 1) ? cv_buf.getWritePointer(0)
                                                             : nullptr;

    float last_val = 0.0f;
    for (int i = 0; i < ns; ++i) {
        const float v = dsp_.process_sample(det_l[i], det_r[i]);
        last_val = v;
        if (cv_ptr) cv_ptr[i] = v;
    }

    if (mid_on) {
        const int cc_val = std::clamp(juce::roundToInt(last_val * 127.0f), 0, 127);
        if (cc_val != last_cc_val_) {
            midi.addEvent(juce::MidiMessage::controllerEvent(cc_chan, cc_num, cc_val), ns - 1);
            last_cc_val_ = cc_val;
        }
    }
}

// ── State ──────────────────────────────────────────────────────────────────────

void EnvelopeFollowerPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void EnvelopeFollowerPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* EnvelopeFollowerPlugin::createEditor()
{
    return new EnvelopeFollowerEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::EnvelopeFollowerPlugin();
}
