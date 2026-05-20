#include "looper_plugin.h"
#include "looper_editor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {

static constexpr auto kSyncMode  = "loop_sync_mode";
static constexpr auto kBars      = "loop_bars";
static constexpr auto kPlayback  = "loop_playback";
static constexpr auto kFeedback  = "loop_feedback";
static constexpr auto kInputGain = "loop_input_gain";
static constexpr auto kOutGain   = "loop_out_gain";
static constexpr auto kMix       = "loop_mix";
static constexpr auto kCcRecord  = "loop_cc_record";
static constexpr auto kCcStop    = "loop_cc_stop";
static constexpr auto kCcClear   = "loop_cc_clear";
static constexpr auto kCcChannel = "loop_cc_channel";

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout LooperPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kSyncMode, 1}, "Sync Mode",
        StringArray{"Freeform", "Time Sync"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kBars, 1}, "Bars",
        StringArray{"1", "2", "4", "8"}, 1));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kPlayback, 1}, "Playback",
        StringArray{"Forward", "Backward", "Bounce", "Accumulate", "Accumulate Reverse"}, 0));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kFeedback, 1}, "Feedback",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    {
        NormalisableRange<float> r(-20.0f, 12.0f, 0.1f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kInputGain, 1}, "Input Gain", r, 0.0f,
            AudioParameterFloatAttributes().withLabel("dB")));
    }

    {
        NormalisableRange<float> r(-20.0f, 6.0f, 0.1f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kOutGain, 1}, "Output Gain", r, 0.0f,
            AudioParameterFloatAttributes().withLabel("dB")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcRecord, 1}, "CC Record",
        NormalisableRange<float>(0.0f, 127.0f, 1.0f), 20.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcStop, 1}, "CC Stop",
        NormalisableRange<float>(0.0f, 127.0f, 1.0f), 21.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcClear, 1}, "CC Clear",
        NormalisableRange<float>(0.0f, 127.0f, 1.0f), 22.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcChannel, 1}, "CC Channel",
        NormalisableRange<float>(0.0f, 16.0f, 1.0f), 0.0f));  // 0 = any channel

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────

LooperPlugin::LooperPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_LOOPER", make_params())
{
    p_sync_mode_  = apvts_.getRawParameterValue(kSyncMode);
    p_bars_       = apvts_.getRawParameterValue(kBars);
    p_playback_   = apvts_.getRawParameterValue(kPlayback);
    p_feedback_   = apvts_.getRawParameterValue(kFeedback);
    p_input_gain_ = apvts_.getRawParameterValue(kInputGain);
    p_out_gain_   = apvts_.getRawParameterValue(kOutGain);
    p_mix_        = apvts_.getRawParameterValue(kMix);
    p_cc_record_  = apvts_.getRawParameterValue(kCcRecord);
    p_cc_stop_    = apvts_.getRawParameterValue(kCcStop);
    p_cc_clear_   = apvts_.getRawParameterValue(kCcClear);
    p_cc_channel_ = apvts_.getRawParameterValue(kCcChannel);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool LooperPlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void LooperPlugin::prepareToPlay(double sr, int /*bs*/)
{
    sample_rate_ = sr;
    dsp_.prepare(sr);
    last_cc_rec_val_  = -1;
    last_cc_stop_val_ = -1;
    last_cc_clr_val_  = -1;
}

void LooperPlugin::releaseResources()
{
    dsp_.reset();
}

// ── Processing ────────────────────────────────────────────────────────────────

void LooperPlugin::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals ndn;

    const bool sync = juce::roundToInt(p_sync_mode_->load()) == 1;

    static const int kBarsValues[] = { 1, 2, 4, 8 };
    const int bars_idx = juce::roundToInt(p_bars_->load());
    const int bars     = kBarsValues[juce::jlimit(0, 3, bars_idx)];

    dsp_.set_playback    (static_cast<LooperPlayback>(juce::roundToInt(p_playback_->load())));
    dsp_.set_sync        (sync);
    dsp_.set_bars        (bars);
    dsp_.set_feedback    (p_feedback_->load());
    dsp_.set_input_gain  (std::pow(10.0f, p_input_gain_->load() * 0.05f));
    dsp_.set_output_gain (std::pow(10.0f, p_out_gain_->load()   * 0.05f));
    dsp_.set_mix         (p_mix_->load());

    // ── Bar boundary detection from DAW playhead ──────────────────────────
    bool at_bar_boundary = false;
    int  bar_len_samples = 0;

    if (auto* ph = getPlayHead()) {
        if (auto pos = ph->getPosition()) {
            double bpm = 120.0;
            if (auto b = pos->getBpm()) bpm = *b;

            int tsn = 4;
            if (auto ts = pos->getTimeSignature()) tsn = ts->numerator;

            const double spb = 60.0 * sample_rate_ / bpm;       // samples per beat
            const double bpb = double(tsn);                       // beats per bar
            bar_len_samples  = int(spb * bpb);

            if (auto ppq = pos->getPpqPosition()) {
                const double block_beats = buf.getNumSamples() / spb;
                const double ppq_end     = *ppq + block_beats;
                const double bar_now     = std::floor(*ppq   / bpb);
                const double bar_end     = std::floor(ppq_end / bpb);
                at_bar_boundary = (bar_end > bar_now);
            }
        }
    } else {
        // Standalone: simulate 120 BPM, 4/4 with a sample counter
        bar_len_samples = int(sample_rate_ * 2.0);  // 2s per bar at 120 BPM
    }

    dsp_.set_bar_len_samples(bar_len_samples);

    // ── MIDI CC scanning ──────────────────────────────────────────────────
    const int cc_rec_num  = juce::roundToInt(p_cc_record_ ->load());
    const int cc_stp_num  = juce::roundToInt(p_cc_stop_   ->load());
    const int cc_clr_num  = juce::roundToInt(p_cc_clear_  ->load());
    const int cc_chan_flt  = juce::roundToInt(p_cc_channel_->load()); // 0 = any

    for (const auto& meta : midi) {
        const auto msg = meta.getMessage();
        if (!msg.isController()) continue;
        const int chan = msg.getChannel();
        const int num  = msg.getControllerNumber();
        const int val  = msg.getControllerValue();
        if (cc_chan_flt != 0 && chan != cc_chan_flt) continue;

        // Trigger on leading edge (value >= 64, previous was < 64)
        if (num == cc_rec_num) {
            if (val >= 64 && last_cc_rec_val_ < 64) dsp_.cmd_record();
            last_cc_rec_val_ = val;
        }
        if (num == cc_stp_num) {
            if (val >= 64 && last_cc_stop_val_ < 64) dsp_.cmd_stop();
            last_cc_stop_val_ = val;
        }
        if (num == cc_clr_num) {
            if (val >= 64 && last_cc_clr_val_ < 64) dsp_.cmd_clear();
            last_cc_clr_val_ = val;
        }
    }

    float* left  = buf.getWritePointer(0);
    float* right = buf.getWritePointer(1);
    dsp_.process(left, right, buf.getNumSamples(), at_bar_boundary);
}

// ── State ──────────────────────────────────────────────────────────────────────

void LooperPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void LooperPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* LooperPlugin::createEditor()
{
    return new LooperEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::LooperPlugin();
}
