#include "lfo_plugin.h"
#include "lfo_editor.h"
#include <cmath>

namespace kaos_engine {

// Beat values for each sync-division choice (in quarter-note beats)
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

static constexpr auto kWaveform   = "waveform";
static constexpr auto kRate       = "rate";
static constexpr auto kDepth      = "depth";
static constexpr auto kPhaseOff   = "phase_off";
static constexpr auto kSync       = "sync";
static constexpr auto kSyncDiv    = "sync_div";
static constexpr auto kShape      = "shape";
static constexpr auto kOutputMode = "output_mode";
static constexpr auto kCcNumber   = "cc_number";
static constexpr auto kCcChannel  = "cc_channel";
static constexpr auto kOffset      = "offset";
static constexpr auto kTriggerMode = "trigger_mode";

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout LfoPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kWaveform, 1}, "Waveform",
        StringArray{"Sine", "Triangle", "Square", "Sawtooth", "Rev. Saw",
                    "Half Sine", "Exp Ramp", "Log Ramp", "Pulse",
                    "Staircase Up", "Staircase Down"}, 0));

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

    // Shape: waveform-specific modifier.
    // Pulse → duty cycle (0=narrow, 0.5=square, 1=wide)
    // Staircase → step count (0=2 steps, 1=16 steps)
    // ExpRamp / LogRamp → curve steepness (0=near-linear, 1=very steep)
    // All others → no effect
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kShape, 1}, "Shape",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kPhaseOff, 1}, "Phase",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kSync, 1}, "Tempo Sync",
        StringArray{"Free", "Sync"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kSyncDiv, 1}, "Sync Division",
        StringArray{"Whole", "Half", "Dotted 1/4", "1/4",
                    "Dotted 1/8", "1/8", "1/8 Triplet", "1/16"}, 3));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kOutputMode, 1}, "Output Mode",
        StringArray{"MIDI CC", "Audio CV", "MIDI CC + CV"}, 2));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcNumber, 1}, "CC Number",
        NormalisableRange<float>(0.0f, 127.0f, 1.0f), 1.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kCcChannel, 1}, "CC Channel",
        NormalisableRange<float>(1.0f, 16.0f, 1.0f), 1.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kOffset, 1}, "Offset",
        NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kTriggerMode, 1}, "Trigger Mode",
        StringArray{"Free", "Note Retrigger", "Transport", "Sidechain"}, 0));

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────

LfoPlugin::LfoPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",      juce::AudioChannelSet::stereo(), true)
          .withInput ("Trigger In", juce::AudioChannelSet::mono(),   false)
          .withOutput("Main",       juce::AudioChannelSet::stereo(), true)
          .withOutput("CV Out",     juce::AudioChannelSet::mono(),   false)),
      apvts_(*this, nullptr, "KE_LFO", make_params())
{
    p_waveform_    = apvts_.getRawParameterValue(kWaveform);
    p_rate_        = apvts_.getRawParameterValue(kRate);
    p_depth_       = apvts_.getRawParameterValue(kDepth);
    p_shape_       = apvts_.getRawParameterValue(kShape);
    p_phase_off_   = apvts_.getRawParameterValue(kPhaseOff);
    p_sync_        = apvts_.getRawParameterValue(kSync);
    p_sync_div_    = apvts_.getRawParameterValue(kSyncDiv);
    p_output_mode_ = apvts_.getRawParameterValue(kOutputMode);
    p_cc_number_   = apvts_.getRawParameterValue(kCcNumber);
    p_cc_channel_  = apvts_.getRawParameterValue(kCcChannel);
    p_offset_       = apvts_.getRawParameterValue(kOffset);
    p_trigger_mode_ = apvts_.getRawParameterValue(kTriggerMode);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool LfoPlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo()) return false;
    const auto& cv_out  = layouts.getChannelSet(false, 1);
    const auto& trig_in = layouts.getChannelSet(true,  1);
    return (cv_out.isDisabled()  || cv_out  == juce::AudioChannelSet::mono())
        && (trig_in.isDisabled() || trig_in == juce::AudioChannelSet::mono());
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void LfoPlugin::prepareToPlay(double sr, int bs)
{
    lfo_.prepare(sr, bs);
    last_cc_val_ = -1;
}

void LfoPlugin::releaseResources() { lfo_.reset(); }

// ── Processing ────────────────────────────────────────────────────────────────

void LfoPlugin::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals ndn;

    // Read params
    lfo_.set_waveform    (static_cast<LfoWaveform>(juce::roundToInt(p_waveform_->load())));
    lfo_.set_depth       (p_depth_->load());
    lfo_.set_shape       (p_shape_->load());
    lfo_.set_offset      (p_offset_->load());
    lfo_.set_phase_offset(p_phase_off_->load());

    const bool synced = juce::roundToInt(p_sync_->load()) == 1;
    lfo_.set_tempo_sync(synced);
    if (synced) {
        const int div_idx   = juce::roundToInt(p_sync_div_->load());
        const float cycle_beats = kSyncBeats[div_idx];
        lfo_.set_sync_beats(cycle_beats);

        if (auto* ph = getPlayHead()) {
            if (auto pos = ph->getPosition()) {
                if (auto bpm = pos->getBpm())
                    lfo_.set_bpm(*bpm);

                // Lock LFO phase to DAW transport position so seeking,
                // looping, and Stop/Play all snap the LFO to the correct
                // beat-aligned phase rather than running free.
                if (auto ppq = pos->getPpqPosition()) {
                    const float phase = float(std::fmod(*ppq / cycle_beats, 1.0));
                    lfo_.set_phase_direct(phase);
                }
            }
        }
    } else {
        lfo_.set_rate_hz(p_rate_->load());
    }

    const int mode      = juce::roundToInt(p_output_mode_->load());
    const int cc_num    = juce::roundToInt(p_cc_number_->load());
    const int cc_chan   = juce::roundToInt(p_cc_channel_->load());
    const int trig_mode = juce::roundToInt(p_trigger_mode_->load());
    const int ns        = buf.getNumSamples();

    // ── Trigger modes ─────────────────────────────────────────────────────────
    // Sidechain input: the Trigger In bus is channel 2 of the input when active.
    // It shares the buffer slot with the CV output — read it before writing CV.
    const bool has_sidechain = (buf.getNumChannels() >= 3);
    const float* trig_ptr    = has_sidechain ? buf.getReadPointer(2) : nullptr;

    // When switching away from Free mode, stop immediately so the indicator
    // doesn't keep moving until the new trigger fires.
    if (trig_mode != last_trigger_mode_) {
        if (trig_mode != int(LfoTriggerMode::Free))
            lfo_.set_running(false);
        last_trigger_mode_ = trig_mode;
    }

    if (trig_mode == int(LfoTriggerMode::Free)) {
        lfo_.set_running(true);

    } else if (trig_mode == int(LfoTriggerMode::NoteRetrigger)) {
        for (const auto& meta : midi) {
            const auto msg = meta.getMessage();
            if (msg.isNoteOn())  { lfo_.reset(); lfo_.set_running(true); }
            if (msg.isNoteOff()) { lfo_.set_running(false); }
        }

    } else if (trig_mode == int(LfoTriggerMode::Transport)) {
        bool now_playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                now_playing = pos->getIsPlaying();
        if (now_playing && !was_playing_)  { lfo_.reset(); lfo_.set_running(true); }
        if (!now_playing && was_playing_)    lfo_.set_running(false);
        was_playing_ = now_playing;
    }
    // Sidechain mode is handled per-sample in the generation loop below.

    // CV output bus: channel 2 when enabled.
    // In Sidechain mode channel 2 is read as input first, then overwritten as output.
    const bool cv_on  = (mode == 1 || mode == 2) && has_sidechain;
    const bool mid_on = (mode == 0 || mode == 2);
    float* cv_ptr = cv_on ? buf.getWritePointer(2) : nullptr;

    // ── Sample generation loop ────────────────────────────────────────────────
    float last_val = 0.0f;
    for (int i = 0; i < ns; ++i) {
        // Sidechain trigger: Schmitt trigger at 0.6/0.4 to avoid chattering.
        if (trig_mode == int(LfoTriggerMode::Sidechain) && trig_ptr) {
            const float sc = trig_ptr[i];
            if (!sc_gate_open_ && sc > 0.6f) { lfo_.reset(); lfo_.set_running(true);  sc_gate_open_ = true; }
            if ( sc_gate_open_ && sc < 0.4f) {                lfo_.set_running(false); sc_gate_open_ = false; }
        }

        const float v = lfo_.next_sample();
        last_val = v;
        if (cv_ptr) cv_ptr[i] = v;
    }

    // Emit one MIDI CC per block. Signal is in [-1,+1]; map to [0,127].
    if (mid_on) {
        const float norm  = last_val * 0.5f + 0.5f;
        const int   cc_val = std::clamp(juce::roundToInt(norm * 127.0f), 0, 127);
        if (cc_val != last_cc_val_) {
            midi.addEvent(juce::MidiMessage::controllerEvent(cc_chan, cc_num, cc_val), ns - 1);
            last_cc_val_ = cc_val;
        }
    }
}

// ── State ──────────────────────────────────────────────────────────────────────

void LfoPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void LfoPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* LfoPlugin::createEditor() { return new LfoEditor(*this); }

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::LfoPlugin();
}
