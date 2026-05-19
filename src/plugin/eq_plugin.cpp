#include "eq_plugin.h"
#include "eq_editor.h"

namespace kaos_engine {

// ── Parameter ID helpers ───────────────────────────────────────────────────────
static juce::String freq_pid(int b) { return "band_" + juce::String(b) + "_freq"; }
static juce::String gain_pid(int b) { return "band_" + juce::String(b) + "_gain"; }
static juce::String q_pid   (int b) { return "band_" + juce::String(b) + "_q";    }
static juce::String type_pid(int b) { return "band_" + juce::String(b) + "_type"; }

// ── Default band setup ────────────────────────────────────────────────────────
// Band  0 : HP 12  80 Hz
// Bands 1-5: Bell  200/500/1500/5000/12000 Hz, 0 dB gain
// Band  6 : LP 12  18000 Hz
struct BandDefaults { int type; float freq; };
static constexpr BandDefaults kDefaults[7] = {
    { int(BandType::HP12),  80.0f    },
    { int(BandType::Bell),  200.0f   },
    { int(BandType::Bell),  500.0f   },
    { int(BandType::Bell),  1500.0f  },
    { int(BandType::Bell),  5000.0f  },
    { int(BandType::Bell),  12000.0f },
    { int(BandType::LP12),  18000.0f },
};

// ── Parameter layout ───────────────────────────────────────────────────────────
juce::AudioProcessorValueTreeState::ParameterLayout EqPlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto freq_range = [](float lo, float hi, float centre) {
        NormalisableRange<float> r(lo, hi, 1.0f);
        r.setSkewForCentre(centre);
        return r;
    };
    auto q_range = []() {
        NormalisableRange<float> r(0.1f, 30.0f, 0.01f);
        r.setSkewForCentre(1.0f);
        return r;
    };

    const StringArray type_choices {
        "Off", "Bell", "Notch", "Low Shelf", "High Shelf",
        "HP 12dB", "HP 24dB", "LP 12dB", "LP 24dB"
    };

    for (int b = 0; b < kNumBands; ++b) {
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{ freq_pid(b), 1 },
            "Band " + String(b+1) + " Freq",
            freq_range(20.0f, 20000.0f, 1000.0f),
            kDefaults[b].freq,
            AudioParameterFloatAttributes().withLabel("Hz")));

        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{ gain_pid(b), 1 },
            "Band " + String(b+1) + " Gain",
            NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel("dB")));

        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{ q_pid(b), 1 },
            "Band " + String(b+1) + " Q",
            q_range(), 1.0f));

        p.push_back(std::make_unique<AudioParameterChoice>(
            ParameterID{ type_pid(b), 1 },
            "Band " + String(b+1) + " Type",
            type_choices,
            kDefaults[b].type));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"output", 1}, "Output",
        NormalisableRange<float>(-20.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel("dB")));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{"mix", 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return { p.begin(), p.end() };
}

// ── Constructor ────────────────────────────────────────────────────────────────
EqPlugin::EqPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_EQ2", make_params())
{
    for (int b = 0; b < kNumBands; ++b) {
        p_freq_[b] = apvts_.getRawParameterValue(freq_pid(b));
        p_gain_[b] = apvts_.getRawParameterValue(gain_pid(b));
        p_q_   [b] = apvts_.getRawParameterValue(q_pid(b));
        p_type_[b] = apvts_.getRawParameterValue(type_pid(b));
    }
    p_output_ = apvts_.getRawParameterValue("output");
    p_mix_    = apvts_.getRawParameterValue("mix");
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────
void EqPlugin::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp_.prepare(sampleRate, samplesPerBlock);
}

void EqPlugin::releaseResources() { dsp_.reset(); }

// ── FIFO ───────────────────────────────────────────────────────────────────────
void EqPlugin::push_to_fifo(float sample)
{
    if (fifo_.getFreeSpace() > 0) {
        int s1, n1, s2, n2;
        fifo_.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 > 0) fifo_buf_[s1] = sample;
        fifo_.finishedWrite(n1);
    }
}

bool EqPlugin::pull_fft_block(float* fft_out)
{
    if (fifo_.getNumReady() < kFftSize) return false;
    int s1, n1, s2, n2;
    fifo_.prepareToRead(kFftSize, s1, n1, s2, n2);
    if (n1 > 0) std::copy(fifo_buf_+s1, fifo_buf_+s1+n1, fft_out);
    if (n2 > 0) std::copy(fifo_buf_+s2, fifo_buf_+s2+n2, fft_out+n1);
    fifo_.finishedRead(n1 + n2);
    return true;
}

// ── Audio processing ───────────────────────────────────────────────────────────
void EqPlugin::processBlock(juce::AudioBuffer<float>& buffer,
                             juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals no_denormals;

    for (int b = 0; b < kNumBands; ++b) {
        BandConfig cfg;
        cfg.type = static_cast<BandType>(juce::roundToInt(p_type_[b]->load()));
        cfg.freq = p_freq_[b]->load();
        cfg.gain = p_gain_[b]->load();
        cfg.q    = p_q_[b]->load();
        dsp_.set_band(b, cfg);
    }
    dsp_.set_output(p_output_->load());
    dsp_.set_mix   (p_mix_->load());

    const int ns  = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, ns);

    if (nch >= 2) {
        const float* L = buffer.getReadPointer(0);
        const float* R = buffer.getReadPointer(1);
        for (int i = 0; i < ns; ++i) push_to_fifo(0.5f * (L[i] + R[i]));
        dsp_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), ns);
    } else if (nch == 1) {
        const float* ch0 = buffer.getReadPointer(0);
        for (int i = 0; i < ns; ++i) push_to_fifo(ch0[i]);
        juce::HeapBlock<float> tmp(static_cast<size_t>(ns));
        std::copy(ch0, ch0 + ns, tmp.getData());
        dsp_.process(buffer.getWritePointer(0), tmp.getData(), ns);
    }
}

// ── State ──────────────────────────────────────────────────────────────────────
void EqPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void EqPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* EqPlugin::createEditor()
{
    return new EqEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::EqPlugin();
}
