#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "lfo_plugin.h"

namespace kaos_engine {

class LfoEditor final : public juce::AudioProcessorEditor,
                        public juce::Timer
{
public:
    explicit LfoEditor(LfoPlugin& plugin);
    ~LfoEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LfoPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Waveform selector ────────────────────────────────────────────────────
    juce::ComboBox wave_box_;
    juce::Label    wave_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> wave_attach_;

    // ── Sync + division ──────────────────────────────────────────────────────
    juce::ComboBox sync_box_;
    juce::ComboBox div_box_;
    juce::Label    sync_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sync_attach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> div_attach_;

    // ── Output mode + trigger mode ────────────────────────────────────────────
    juce::ComboBox mode_box_;
    juce::ComboBox trigger_box_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mode_attach_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> trigger_attach_;

    // ── Knobs ────────────────────────────────────────────────────────────────
    juce::Slider rate_knob_, depth_knob_, shape_knob_, phase_knob_, offset_knob_;
    juce::Label  rate_lbl_,  depth_lbl_,  shape_lbl_,  phase_lbl_,  offset_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> rate_att_, depth_att_, shape_att_, phase_att_, offset_att_;

    // ── CC text fields ────────────────────────────────────────────────────────
    juce::Label cc_num_field_, cc_ch_field_;
    juce::Label cc_num_lbl_,   cc_ch_lbl_;

    // ── Drawing ───────────────────────────────────────────────────────────────
    void draw_waveform_display(juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_wave_label();
    void update_mode_ui();    // show/hide CC knobs, sync div

    float phase_display_ = 0.0f;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 700;
    static constexpr int kHeight     = 248;
    static constexpr int kComboY     = 8;
    static constexpr int kComboH     = 22;
    static constexpr int kComboW     = 120;
    // Waveform display
    static constexpr int kDispY      = kComboY + kComboH + 8;
    static constexpr int kDispH      = 100;
    // Main knob row
    static constexpr int kKnobY      = kDispY + kDispH + 14;
    static constexpr int kKnobSize   = 54;
    static constexpr int kKnobLabelH = 13;
    static constexpr int kNumCols    = 7;   // RATE DEPTH SHAPE PHASE OFFSET CC_NUM CC_CH
    static constexpr int kPadX       = 14;
    static constexpr int kFooterH    = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoEditor)
};

} // namespace kaos_engine
