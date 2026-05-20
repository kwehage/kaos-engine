#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "look_and_feel.h"
#include "spectrogram_plugin.h"

namespace kaos_engine {

class SpectrogramEditor final : public juce::AudioProcessorEditor,
                                public juce::Timer
{
public:
    explicit SpectrogramEditor(SpectrogramPlugin& plugin);
    ~SpectrogramEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SpectrogramPlugin&    plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── FFT state ─────────────────────────────────────────────────────────────
    static constexpr int kFftOrder  = SpectrogramPlugin::kFftOrder;
    static constexpr int kFftSize   = SpectrogramPlugin::kFftSize;

    juce::dsp::FFT                      fft_    { kFftOrder };
    juce::dsp::WindowingFunction<float> window_ {
        static_cast<size_t>(kFftSize),
        juce::dsp::WindowingFunction<float>::hann };

    float fft_data_[kFftSize * 2] {};

    // ── Spectrogram image ─────────────────────────────────────────────────────
    juce::Image spectrogram_img_;   // kDispW x kDispH, drawn top=oldest/bottom=newest

    void update_spectrogram();
    void draw_spectrogram(juce::Graphics&, juce::Rectangle<int> area);

    static juce::Colour map_colour(float level_0_1);

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth     = 700;
    static constexpr int kHeight    = 480;
    static constexpr int kDispX     = 0;
    static constexpr int kDispY     = 0;
    static constexpr int kDispW     = kWidth;
    static constexpr int kDispH     = kHeight - 22 - 20;  // leave room for freq axis + footer
    static constexpr int kAxisY     = kDispH;
    static constexpr int kAxisH     = 22;
    static constexpr int kFooterY   = kAxisY + kAxisH;
    static constexpr int kFooterH   = 20;

    // Precomputed per-column fractional FFT bin — rebuilt once when sample rate
    // is known. Storing floats lets the row-writer interpolate between adjacent
    // bins rather than snapping, which smooths the visible block boundaries.
    float freq_bin_lut_[kDispW] {};
    bool  lut_built_ = false;
    void build_freq_lut();

    // Precomputed colour table (256 entries). Stored as packed 0xAARRGGBB for
    // direct uint32 pixel writes, bypassing setPixelColour overhead.
    uint32_t colour_lut_[256] {};
    void     build_colour_lut();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrogramEditor)
};

} // namespace kaos_engine
