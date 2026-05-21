#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "look_and_feel.h"
#include "filter_plugin.h"

namespace kaos_engine {

class FilterEditor final : public juce::AudioProcessorEditor,
                           public juce::Timer
{
public:
    explicit FilterEditor(FilterPlugin& plugin);
    ~FilterEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    FilterPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Mode selector ────────────────────────────────────────────────────────
    juce::ComboBox mode_box_;
    juce::Label    mode_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mode_attach_;

    // ── Knobs ────────────────────────────────────────────────────────────────
    juce::Slider cutoff_knob_, resonance_knob_, drive_knob_;
    juce::Slider gain_knob_, mix_knob_, output_knob_;

    juce::Label cutoff_lbl_, resonance_lbl_, drive_lbl_;
    juce::Label gain_lbl_, mix_lbl_, output_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> cutoff_att_, resonance_att_, drive_att_;
    std::unique_ptr<Attachment> gain_att_, mix_att_, output_att_;

    // ── Spectrum analyser ─────────────────────────────────────────────────────
    static constexpr int kFftOrder  = FilterPlugin::kFftOrder;
    static constexpr int kFftSize   = FilterPlugin::kFftSize;
    static constexpr int kScopeSize = 512;

    juce::dsp::FFT                      fft_    { kFftOrder };
    juce::dsp::WindowingFunction<float> window_ {
        static_cast<size_t>(kFftSize),
        juce::dsp::WindowingFunction<float>::hann };

    float fft_data_  [kFftSize * 2] {};
    float scope_data_[kScopeSize]   {};

    void update_spectrum();

    // ── Helpers ───────────────────────────────────────────────────────────────
    void draw_response(juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_mode_ui();

    // Computes frequency response magnitude in dB at freq_hz for the current
    // plugin parameters (called from the UI thread only).
    float response_db(float freq_hz) const;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth     = 600;
    static constexpr int kHeight    = 268;
    static constexpr int kComboY    = 8;
    static constexpr int kComboH    = 22;
    static constexpr int kComboW    = 110;
    static constexpr int kDispY     = kComboY + kComboH + 8;
    static constexpr int kDispH     = 120;
    static constexpr int kKnobY     = kDispY + kDispH + 14;
    static constexpr int kKnobSize  = 54;
    static constexpr int kKnobLblH  = 13;
    static constexpr int kNumCols   = 6;   // CUTOFF RESONANCE DRIVE GAIN MIX OUTPUT
    static constexpr int kPadX      = 14;
    static constexpr int kFooterH   = 16;

    // Response display dB range
    static constexpr float kDbMin = -36.0f;
    static constexpr float kDbMax =  24.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterEditor)
};

} // namespace kaos_engine
