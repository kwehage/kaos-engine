#include "stochastic_plugin.h"
#include "stochastic_editor.h"

namespace kaos_engine {

static constexpr auto kMode       = "mode";
static constexpr auto kRate       = "rate";
static constexpr auto kDepth      = "depth";
static constexpr auto kShape      = "shape";
static constexpr auto kOffset     = "offset";
static constexpr auto kOutputMode = "output_mode";
static constexpr auto kCcNumber   = "cc_number";
static constexpr auto kCcChannel  = "cc_channel";
static constexpr auto kTrigger    = "trigger_mode";
static constexpr auto kSync       = "sync";
static constexpr auto kSyncDiv    = "sync_div";

// Beat values for each sync division choice (in quarter-note beats)
static const float kSyncBeats[] = {
    4.0f,           // Whole
    2.0f,           // Half
    1.5f,           // Dotted 1/4
    1.0f,           // 1/4
    0.75f,          // Dotted 1/8
    0.5f,           // 1/8
    1.0f / 3.0f,    // 1/8 Triplet
    0.25f,          // 1/16
};

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout StochasticPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kMode, 1}, "Mode",
        StringArray{"S+H", "S+Glide", "Smooth", "Brownian", "Lorenz", "Logistic"}, 0));

    {
        NormalisableRange<float> r(0.01f, 100.0f, 0.001f);
        r.setSkewForCentre(1.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRate, 1}, "Rate", r, 1.0f,
            AudioParameterFloatAttributes().withLabel("Hz")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kDepth, 1}, "Depth",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kShape, 1}, "Shape",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kOffset, 1}, "Offset",
        NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kOutputMode, 1}, "Output Mode",
        StringArray{"MIDI CC", "Audio CV", "MIDI CC + CV"}, 2));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcNumber, 1}, "CC Number",
        NormalisableRange<float>(0.0f, 127.0f, 1.0f), 1.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcChannel, 1}, "CC Channel",
        NormalisableRange<float>(1.0f, 16.0f, 1.0f), 1.0f));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kTrigger, 1}, "Trigger Mode",
        StringArray{"Free", "Note Retrigger", "Transport"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kSync, 1}, "Tempo Sync",
        StringArray{"Free", "Sync"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kSyncDiv, 1}, "Sync Division",
        StringArray{"Whole", "Half", "Dotted 1/4", "1/4",
                    "Dotted 1/8", "1/8", "1/8 Triplet", "1/16"}, 3));

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────

StochasticPlugin::StochasticPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",   juce::AudioChannelSet::stereo(), true)
          .withOutput("Main",    juce::AudioChannelSet::stereo(), true)
          .withOutput("CV Out",  juce::AudioChannelSet::mono(),   false)),
      apvts_(*this, nullptr, "KE_STOCH", make_params())
{
    p_mode_         = apvts_.getRawParameterValue(kMode);
    p_rate_         = apvts_.getRawParameterValue(kRate);
    p_depth_        = apvts_.getRawParameterValue(kDepth);
    p_shape_        = apvts_.getRawParameterValue(kShape);
    p_offset_       = apvts_.getRawParameterValue(kOffset);
    p_output_mode_  = apvts_.getRawParameterValue(kOutputMode);
    p_cc_number_    = apvts_.getRawParameterValue(kCcNumber);
    p_cc_channel_   = apvts_.getRawParameterValue(kCcChannel);
    p_trigger_mode_ = apvts_.getRawParameterValue(kTrigger);
    p_sync_         = apvts_.getRawParameterValue(kSync);
    p_sync_div_     = apvts_.getRawParameterValue(kSyncDiv);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool StochasticPlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    const auto& cv = layouts.getChannelSet(false, 1);
    return cv.isDisabled() || cv == juce::AudioChannelSet::mono();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void StochasticPlugin::prepareToPlay(double sr, int bs)
{
    gen_.prepare(sr, bs);
    last_cc_val_ = -1;
}

void StochasticPlugin::releaseResources() { gen_.reset(); }

// ── Processing ────────────────────────────────────────────────────────────────

void StochasticPlugin::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals ndn;

    gen_.set_mode  (static_cast<StochasticMode>(juce::roundToInt(p_mode_->load())));
    gen_.set_depth (p_depth_->load());

    const bool synced = juce::roundToInt(p_sync_->load()) == 1;
    if (synced) {
        const int   div_idx     = juce::roundToInt(p_sync_div_->load());
        const float cycle_beats = kSyncBeats[div_idx];
        float bpm = 120.0f;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = float(*b);
        gen_.set_rate_hz(bpm / 60.0f / cycle_beats);
    } else {
        gen_.set_rate_hz(p_rate_->load());
    }
    gen_.set_shape  (p_shape_->load());
    gen_.set_offset (p_offset_->load());

    const int mode      = juce::roundToInt(p_output_mode_->load());
    const int cc_num    = juce::roundToInt(p_cc_number_->load());
    const int cc_chan   = juce::roundToInt(p_cc_channel_->load());
    const int trig_mode = juce::roundToInt(p_trigger_mode_->load());
    const int ns        = buf.getNumSamples();

    // Stop immediately when switching away from Free
    if (trig_mode != last_trigger_mode_) {
        if (trig_mode != int(StochasticTriggerMode::Free))
            gen_.set_running(false);
        last_trigger_mode_ = trig_mode;
    }

    // Trigger logic
    if (trig_mode == int(StochasticTriggerMode::Free)) {
        gen_.set_running(true);
    } else if (trig_mode == int(StochasticTriggerMode::NoteRetrigger)) {
        for (const auto& meta : midi) {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn())  { gen_.reset(); gen_.set_running(true);  }
            if (msg.isNoteOff()) {               gen_.set_running(false); }
        }
    } else if (trig_mode == int(StochasticTriggerMode::Transport)) {
        bool now_playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                now_playing = pos->getIsPlaying();
        if (now_playing && !was_playing_)  { gen_.reset(); gen_.set_running(true);  }
        if (!now_playing && was_playing_)  {               gen_.set_running(false); }
        was_playing_ = now_playing;
    }

    const bool cv_on  = (mode == 1 || mode == 2);
    const bool mid_on = (mode == 0 || mode == 2);
    const bool has_cv = (buf.getNumChannels() >= 3);
    float* cv_ptr = (cv_on && has_cv) ? buf.getWritePointer(2) : nullptr;

    float last_val = 0.0f;
    for (int i = 0; i < ns; ++i) {
        const float v = gen_.next_sample();
        last_val = v;
        if (cv_ptr) cv_ptr[i] = v;
    }

    if (mid_on) {
        const int cc_val = std::clamp(juce::roundToInt((last_val * 0.5f + 0.5f) * 127.0f), 0, 127);
        if (cc_val != last_cc_val_) {
            midi.addEvent(juce::MidiMessage::controllerEvent(cc_chan, cc_num, cc_val), ns - 1);
            last_cc_val_ = cc_val;
        }
    }
}

// ── State ──────────────────────────────────────────────────────────────────────

void StochasticPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void StochasticPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* StochasticPlugin::createEditor()
{
    return new StochasticEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::StochasticPlugin();
}
