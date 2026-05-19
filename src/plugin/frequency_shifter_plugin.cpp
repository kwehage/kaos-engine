#include "frequency_shifter_plugin.h"
#include "frequency_shifter_editor.h"

namespace kaos_engine {

// ── Parameter IDs ──────────────────────────────────────────────────────────────
static constexpr auto kParamShift     = "shift";
static constexpr auto kParamDirection    = "direction";
static constexpr auto kParamFeedbackMode = "feedback_mode";
static constexpr auto kParamFeedback  = "feedback";
static constexpr auto kParamDelay     = "delay";
static constexpr auto kParamLfoRate   = "lfo_rate";
static constexpr auto kParamLfoDepth  = "lfo_depth";
static constexpr auto kParamTone      = "tone";
static constexpr auto kParamDrive     = "drive";
static constexpr auto kParamDiffusion = "diffusion";
static constexpr auto kParamOutput    = "output";
static constexpr auto kParamMix       = "mix";

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout FrequencyShifterPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    {
        NormalisableRange<float> shift_range(0.0f, 5000.0f);
        shift_range.setSkewForCentre(50.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamShift, 1}, "Shift",
            shift_range, 10.0f,
            AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction([](float v, int) {
                    return String(v, 2);
                })));
    }

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamDirection, 1}, "Direction",
        StringArray{"Up", "Down", "Both"}, 0));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kParamFeedbackMode, 1}, "Feedback Loop",
        StringArray{"Single", "Ping-Pong", "Tape"}, 0));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamFeedback, 1}, "Feedback",
        NormalisableRange<float>(0.0f, 0.99f, 0.001f), 0.0f));

    {
        NormalisableRange<float> delay_range(1.0f, 2000.0f);
        delay_range.setSkewForCentre(200.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamDelay, 1}, "Delay",
            delay_range, 100.0f,
            AudioParameterFloatAttributes()
                .withLabel("ms")
                .withStringFromValueFunction([](float v, int) {
                    return juce::String(v, 2);
                })));
    }

    {
        NormalisableRange<float> lfo_range(0.0f, 10.0f);
        lfo_range.setSkewForCentre(1.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamLfoRate, 1}, "Mod 1",
            lfo_range, 0.0f,
            AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction([](float v, int) {
                    return juce::String(v, 2);
                })));
    }

    {
        NormalisableRange<float> depth_range(0.0f, 500.0f);
        depth_range.setSkewForCentre(20.0f);
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kParamLfoDepth, 1}, "Mod 2",
            depth_range, 0.0f,
            AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction([](float v, int) {
                    return juce::String(v, 2);
                })));
    }

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamTone, 1}, "Tone",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDrive, 1}, "Drive",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kParamDiffusion, 1}, "Diffusion",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

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
FrequencyShifterPlugin::FrequencyShifterPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_FREQSHIFT", make_params())
{
    p_shift_     = apvts_.getRawParameterValue(kParamShift);
    p_direction_     = apvts_.getRawParameterValue(kParamDirection);
    p_feedback_mode_ = apvts_.getRawParameterValue(kParamFeedbackMode);
    p_feedback_  = apvts_.getRawParameterValue(kParamFeedback);
    p_delay_     = apvts_.getRawParameterValue(kParamDelay);
    p_lfo_rate_  = apvts_.getRawParameterValue(kParamLfoRate);
    p_lfo_depth_ = apvts_.getRawParameterValue(kParamLfoDepth);
    p_tone_      = apvts_.getRawParameterValue(kParamTone);
    p_drive_     = apvts_.getRawParameterValue(kParamDrive);
    p_diffusion_ = apvts_.getRawParameterValue(kParamDiffusion);
    p_output_    = apvts_.getRawParameterValue(kParamOutput);
    p_mix_       = apvts_.getRawParameterValue(kParamMix);
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void FrequencyShifterPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void FrequencyShifterPlugin::releaseResources()
{
    dsp_.reset();
}

// ── Audio processing ───────────────────────────────────────────────────────────
void FrequencyShifterPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    dsp_.set_shift    (p_shift_->load());
    dsp_.set_direction    (static_cast<FreqShiftDirection>   (static_cast<int>(p_direction_->load())));
    dsp_.set_feedback_mode(static_cast<FreqShiftFeedbackMode>(static_cast<int>(p_feedback_mode_->load())));
    dsp_.set_feedback (p_feedback_->load());
    dsp_.set_delay_time(p_delay_->load());
    dsp_.set_lfo_rate (p_lfo_rate_->load());
    dsp_.set_lfo_depth(p_lfo_depth_->load());
    dsp_.set_tone     (p_tone_->load());
    dsp_.set_drive    (p_drive_->load());
    dsp_.set_diffusion(p_diffusion_->load());
    dsp_.set_output   (p_output_->load());
    dsp_.set_mix      (p_mix_->load());

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
void FrequencyShifterPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void FrequencyShifterPlugin::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

// ── Editor ─────────────────────────────────────────────────────────────────────
juce::AudioProcessorEditor* FrequencyShifterPlugin::createEditor()
{
    return new FrequencyShifterEditor(*this);
}

} // namespace kaos_engine

// ── JUCE plugin factory ────────────────────────────────────────────────────────
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::FrequencyShifterPlugin();
}
