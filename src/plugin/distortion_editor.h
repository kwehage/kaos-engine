#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"

namespace kaos_engine {

class DistortionPlugin;

class DistortionEditor final : public juce::AudioProcessorEditor {
public:
    explicit DistortionEditor(DistortionPlugin& plugin);
    ~DistortionEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DistortionPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Mode selector + formula display ───────────────────────────────────
    juce::ComboBox mode_box_;
    juce::Label    formula_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mode_attach_;

    // ── Waveshaper knobs ───────────────────────────────────────────────────
    juce::Slider drive_knob_, feedback_knob_, tone_knob_, bias_knob_, output_knob_, mix_knob_;
    juce::Label  drive_label_, feedback_label_, tone_label_, bias_label_, output_label_, mix_label_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> drive_attach_, feedback_attach_, tone_attach_,
                                bias_attach_, output_attach_, mix_attach_;

    // ── Filter section ─────────────────────────────────────────────────────
    juce::TextButton filter_on_btn_;
    juce::ComboBox   filter_pos_box_;
    juce::ComboBox   filter_type_box_;
    juce::Slider     cutoff_knob_, resonance_knob_, blend_knob_;
    juce::Label      cutoff_label_, resonance_label_, blend_label_;

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> filter_on_attach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        filter_pos_attach_, filter_type_attach_;
    std::unique_ptr<Attachment> cutoff_attach_, resonance_attach_, blend_attach_;

    // ── Helpers ────────────────────────────────────────────────────────────
    void setup_knob(juce::Slider& knob, juce::Label& label, const juce::String& name);
    void update_mode_ui();
    void update_filter_ui();
    void draw_filter_response(juce::Graphics& g, juce::Rectangle<int> area);

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout constants ───────────────────────────────────────────────────
    static constexpr int kWidth         = 630;
    static constexpr int kHeight        = 370;

    // Waveshaper section
    static constexpr int kModeY         = 12;
    static constexpr int kModeH         = 24;
    static constexpr int kModeW         = 160;
    static constexpr int kKnobY         = 52;
    static constexpr int kKnobSize      = 54;
    static constexpr int kLabelH        = 16;
    static constexpr int kPadX          = 20;
    static constexpr int kFooterH       = 16;

    // Filter section
    static constexpr int kFilterSepY    = 149;
    static constexpr int kFilterHeaderY = 157;
    static constexpr int kFilterDisplayY= 183;
    static constexpr int kFilterDisplayH= 80;
    static constexpr int kFilterKnobY   = 271;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortionEditor)
};

} // namespace kaos_engine
