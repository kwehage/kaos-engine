#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"

namespace kaos_engine {

class FrequencyShifterPlugin;

class FrequencyShifterEditor final : public juce::AudioProcessorEditor {
public:
    explicit FrequencyShifterEditor(FrequencyShifterPlugin& plugin);
    ~FrequencyShifterEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    FrequencyShifterPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Top row ───────────────────────────────────────────────────────────────
    juce::ComboBox direction_box_;
    juce::ComboBox feedback_mode_box_;
    juce::Label    formula_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> direction_attach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> feedback_mode_attach_;

    // ── Row 1: shift + feedback controls ─────────────────────────────────────
    juce::Slider shift_knob_, feedback_knob_, delay_knob_,
                 lfo_rate_knob_, lfo_depth_knob_;
    juce::Label  shift_label_, feedback_label_, delay_label_,
                 lfo_rate_label_, lfo_depth_label_;

    // ── Row 2: feedback colour + output ──────────────────────────────────────
    juce::Slider tone_knob_, drive_knob_, diffusion_knob_, output_knob_, mix_knob_;
    juce::Label  tone_label_, drive_label_, diffusion_label_, output_label_, mix_label_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> shift_attach_, feedback_attach_, delay_attach_,
                                lfo_rate_attach_, lfo_depth_attach_,
                                tone_attach_, drive_attach_, diffusion_attach_,
                                output_attach_, mix_attach_;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void setup_knob(juce::Slider& knob, juce::Label& label, const juce::String& name);
    void update_direction_ui();

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kWidth    = 700;
    static constexpr int kHeight   = 275;
    static constexpr int kModeY    = 12;
    static constexpr int kModeH    = 24;
    static constexpr int kModeW    = 100;  // direction combo
    static constexpr int kLoopW    = 110;  // feedback loop combo
    static constexpr int kKnobY1   = 52;   // row 1: shift + feedback
    static constexpr int kSepY     = 152;  // separator between rows
    static constexpr int kKnobY2   = 160;  // row 2: feedback colour + output
    static constexpr int kKnobSize = 76;
    static constexpr int kLabelH   = 16;
    static constexpr int kPadX     = 20;
    static constexpr int kFooterH  = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyShifterEditor)
};

} // namespace kaos_engine
