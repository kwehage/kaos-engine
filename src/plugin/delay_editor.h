#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"

namespace kaos_engine {

class DelayPlugin;

class DelayEditor final : public juce::AudioProcessorEditor {
public:
    explicit DelayEditor(DelayPlugin& plugin);
    ~DelayEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DelayPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    juce::ComboBox mode_box_;
    juce::Label    formula_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mode_attach_;

    juce::Slider time_knob_, feedback_knob_, tone_knob_, mod_knob_, mod2_knob_, output_knob_, mix_knob_;
    juce::Label  time_label_, feedback_label_, tone_label_, mod_label_, mod2_label_, output_label_, mix_label_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> time_attach_, feedback_attach_, tone_attach_,
                                mod_attach_, mod2_attach_, output_attach_, mix_attach_;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    void setup_knob(juce::Slider& knob, juce::Label& label, const juce::String& name);
    void update_mode_ui();

    static constexpr int kWidth    = 700;
    static constexpr int kHeight   = 185;
    static constexpr int kModeY    = 12;
    static constexpr int kModeH    = 24;
    static constexpr int kModeW    = 160;
    static constexpr int kKnobY    = 52;
    static constexpr int kKnobSize = 76;
    static constexpr int kLabelH   = 16;
    static constexpr int kPadX     = 20;
    static constexpr int kFooterH  = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayEditor)
};

} // namespace kaos_engine
