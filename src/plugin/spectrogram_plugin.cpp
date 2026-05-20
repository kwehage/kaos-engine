#include "spectrogram_plugin.h"
#include "spectrogram_editor.h"

namespace kaos_engine {

juce::AudioProcessorValueTreeState::ParameterLayout
SpectrogramPlugin::make_params()
{
    return {};
}

SpectrogramPlugin::SpectrogramPlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_SPECT", make_params())
{}

bool SpectrogramPlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SpectrogramPlugin::prepareToPlay(double, int) {}

void SpectrogramPlugin::processBlock(juce::AudioBuffer<float>& buf,
                                      juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals ndn;

    // Pure passthrough: output = input.
    const int ns  = buf.getNumSamples();
    const int nch = buf.getNumChannels();

    // Feed mono mix into the FIFO for the spectrum display.
    if (nch >= 2) {
        const float* L = buf.getReadPointer(0);
        const float* R = buf.getReadPointer(1);
        for (int i = 0; i < ns; ++i) push_to_fifo(0.5f * (L[i] + R[i]));
    } else if (nch == 1) {
        const float* ch = buf.getReadPointer(0);
        for (int i = 0; i < ns; ++i) push_to_fifo(ch[i]);
    }
}

void SpectrogramPlugin::push_to_fifo(float sample)
{
    if (fifo_.getFreeSpace() > 0) {
        int s1, n1, s2, n2;
        fifo_.prepareToWrite(1, s1, n1, s2, n2);
        if (n1 > 0) fifo_buf_[s1] = sample;
        fifo_.finishedWrite(n1);
    }
}

bool SpectrogramPlugin::pull_fft_block(float* fft_out)
{
    if (fifo_.getNumReady() < kFftSize) return false;
    int s1, n1, s2, n2;
    fifo_.prepareToRead(kFftSize, s1, n1, s2, n2);
    if (n1 > 0) std::copy(fifo_buf_+s1, fifo_buf_+s1+n1, fft_out);
    if (n2 > 0) std::copy(fifo_buf_+s2, fifo_buf_+s2+n2, fft_out+n1);
    fifo_.finishedRead(n1 + n2);
    return true;
}

void SpectrogramPlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void SpectrogramPlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SpectrogramPlugin::createEditor()
{
    return new SpectrogramEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::SpectrogramPlugin();
}
