#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"

namespace kaos_engine {

class ReverbPlugin;

class ReverbEditor final : public juce::AudioProcessorEditor {
public:
    explicit ReverbEditor(ReverbPlugin& plugin);
    ~ReverbEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ReverbPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Algorithm selector ─────────────────────────────────────────────────────
    juce::ComboBox algo_box_;
    juce::Label    algo_eq_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algo_attach_;

    // ── Main knobs ─────────────────────────────────────────────────────────────
    juce::Slider pre_delay_knob_, size_knob_, decay_knob_, damping_knob_,
                 diffusion_knob_, mod_knob_, mod2_knob_, output_knob_, mix_knob_;
    juce::Label  pre_delay_label_, size_label_, decay_label_, damping_label_,
                 diffusion_label_, mod_label_, mod2_label_, output_label_, mix_label_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> pre_delay_attach_, size_attach_, decay_attach_,
                                damping_attach_, diffusion_attach_, mod_attach_,
                                mod2_attach_, output_attach_, mix_attach_;

    // ── Filter section ─────────────────────────────────────────────────────────
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

    // ── Helpers ────────────────────────────────────────────────────────────────
    void setup_knob(juce::Slider& knob, juce::Label& label, const juce::String& name);
    void update_filter_ui();
    void update_algo_ui();
    void draw_filter_response(juce::Graphics& g, juce::Rectangle<int> area);

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout constants ───────────────────────────────────────────────────────
    static constexpr int kWidth         = 780;
    static constexpr int kHeight        = 348;
    static constexpr int kAlgoY         = 12;
    static constexpr int kAlgoH         = 24;
    static constexpr int kAlgoW         = 120;
    static constexpr int kKnobY         = 52;
    static constexpr int kKnobSize      = 54;
    static constexpr int kLabelH        = 16;
    static constexpr int kPadX          = 20;
    static constexpr int kFooterH       = 16;
    static constexpr int kFilterHeaderY = 133;   // 9px after knob label row
    static constexpr int kFilterDisplayY= 159;   // kFilterHeaderY + 22 + 4
    static constexpr int kFilterDisplayH= 80;
    static constexpr int kFilterKnobY   = 247;   // kFilterDisplayY + kFilterDisplayH + 8

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbEditor)
};

} // namespace kaos_engine
