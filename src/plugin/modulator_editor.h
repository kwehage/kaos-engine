#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"

namespace kaos_engine {

class ModulatorPlugin;

class ModulatorEditor final : public juce::AudioProcessorEditor {
public:
    explicit ModulatorEditor(ModulatorPlugin& plugin);
    ~ModulatorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ModulatorPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Top row ───────────────────────────────────────────────────────────────
    juce::ComboBox mode_box_;
    juce::ComboBox waveform_box_;
    juce::Label    formula_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mode_attach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveform_attach_;

    // ── Knobs ─────────────────────────────────────────────────────────────────
    juce::Slider rate_knob_, depth_knob_, bias_knob_, phase_knob_, output_knob_, mix_knob_;
    juce::Label  rate_label_, depth_label_, bias_label_, phase_label_, output_label_, mix_label_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> rate_attach_, depth_attach_, bias_attach_,
                                phase_attach_, output_attach_, mix_attach_;

    // ── Helpers ───────────────────────────────────────────────────────────────
    void setup_knob(juce::Slider& knob, juce::Label& label, const juce::String& name);
    void update_mode_ui();

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int kWidth    = 630;
    static constexpr int kHeight   = 185;
    static constexpr int kModeY    = 12;
    static constexpr int kModeH    = 24;
    static constexpr int kModeW    = 120;
    static constexpr int kWaveW    = 100;
    static constexpr int kKnobY    = 52;
    static constexpr int kKnobSize = 76;
    static constexpr int kLabelH   = 16;
    static constexpr int kPadX     = 20;
    static constexpr int kFooterH  = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulatorEditor)
};

} // namespace kaos_engine
