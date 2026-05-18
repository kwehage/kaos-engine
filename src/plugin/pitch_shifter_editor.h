#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"

namespace kaos_engine {

class PitchShifterPlugin;

class PitchShifterEditor final : public juce::AudioProcessorEditor {
public:
    explicit PitchShifterEditor(PitchShifterPlugin& plugin);
    ~PitchShifterEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PitchShifterPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Algorithm selector ─────────────────────────────────────────────────────
    juce::ComboBox algo_box_;
    juce::Label    algo_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algo_attach_;

    // ── Per-voice knobs (3 voices, 5 knobs each) ───────────────────────────────
    // Row 1: GAIN
    juce::Slider gain_knob_[3];
    juce::Label  gain_label_[3];
    // Row 2: MOD 1, MOD 2
    juce::Slider mod1_knob_[3], mod2_knob_[3];
    juce::Label  mod1_label_[3], mod2_label_[3];
    // Row 3: PITCH, DETUNE  (NoTextBox — value shown in text box row)
    juce::Slider pitch_knob_[3], detune_knob_[3];
    juce::Label  pitch_label_[3], detune_label_[3];
    // Text entry boxes for PITCH and DETUNE
    juce::Label  pitch_box_[3], detune_box_[3];

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> gain_attach_[3];
    std::unique_ptr<Attachment> mod1_attach_[3], mod2_attach_[3];
    std::unique_ptr<Attachment> pitch_attach_[3], detune_attach_[3];

    // ── Global knobs ───────────────────────────────────────────────────────────
    juce::Slider mix_knob_, output_knob_;
    juce::Label  mix_label_, output_label_;
    std::unique_ptr<Attachment> mix_attach_, output_attach_;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    void setup_knob     (juce::Slider& knob, juce::Label& label, const juce::String& name);
    void setup_value_box(juce::Label& box);
    void update_algo_ui ();

    // ── Layout ────────────────────────────────────────────────────────────────
    // 8 equal slots: 2 per voice (×3 voices) + 2 global (MIX, OUT)
    // Row 1 (y=kKnobY1): GAIN centred over each voice's 2 slots + MIX + OUT
    // Row 2 (y=kKnobY2): MOD 1, MOD 2 per voice (slots 0-5, global slots empty)
    // Row 3 (y=kKnobY3): PITCH, DETUNE per voice (NoTextBox, rotary_h = kKnobSize-14)
    // Row 4 (y=kTextBoxY): editable value boxes for PITCH and DETUNE
    static constexpr int kWidth    = 760;
    static constexpr int kHeight   = 400;
    static constexpr int kAlgoY    = 12;
    static constexpr int kAlgoH    = 24;
    static constexpr int kAlgoW    = 140;
    static constexpr int kSepY     = 46;
    static constexpr int kHdrY     = 50;
    static constexpr int kKnobY1   = 66;   // GAIN row
    static constexpr int kKnobY2   = 166;  // MOD 1 / MOD 2 row  (66+76+2+16+6)
    static constexpr int kKnobY3   = 266;  // PITCH / DETUNE row  (166+76+2+16+6)
    static constexpr int kTextBoxY = 352;  // text boxes          (266+62+2+16+6)
    static constexpr int kTextBoxH = 22;
    static constexpr int kKnobSize = 76;
    static constexpr int kLabelH   = 16;
    static constexpr int kPadX     = 20;
    static constexpr int kFooterH  = 16;
    static constexpr int kNumSlots = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchShifterEditor)
};

} // namespace kaos_engine
