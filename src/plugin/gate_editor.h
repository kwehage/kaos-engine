#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "gate_plugin.h"

namespace kaos_engine {

class GateEditor final : public juce::AudioProcessorEditor,
                         public juce::Timer
{
public:
    explicit GateEditor(GatePlugin& plugin);
    ~GateEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    GatePlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Algorithm selector ───────────────────────────────────────────────────
    juce::ComboBox algo_box_;
    juce::Label    algo_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algo_attach_;

    // ── Knobs (9 parameters) ─────────────────────────────────────────────────
    juce::Slider threshold_knob_, range_knob_,   ratio_knob_;
    juce::Slider attack_knob_,    hold_knob_,    release_knob_;
    juce::Slider hysteresis_knob_, output_knob_, mix_knob_;

    juce::Label threshold_lbl_, range_lbl_,   ratio_lbl_;
    juce::Label attack_lbl_,    hold_lbl_,    release_lbl_;
    juce::Label hysteresis_lbl_, output_lbl_, mix_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment>
        threshold_att_, range_att_,   ratio_att_,
        attack_att_,    hold_att_,    release_att_,
        hysteresis_att_, output_att_, mix_att_;

    // ── Drawing ──────────────────────────────────────────────────────────────
    void draw_activity   (juce::Graphics& g, juce::Rectangle<int> area);
    void draw_transfer   (juce::Graphics& g, juce::Rectangle<int> area);
    void draw_gr_meter   (juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob      (juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_algo_ui  ();

    // Meter display state (UI thread, updated by timer)
    float            gr_display_    = -60.0f;
    float            level_display_ = -100.0f;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ───────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 700;
    static constexpr int kHeight     = 258;
    static constexpr int kComboY     = 8;
    static constexpr int kComboH     = 22;
    static constexpr int kComboW     = 110;
    // Display section
    static constexpr int kDispY      = kComboY + kComboH + 8;
    static constexpr int kDispH      = 110;
    static constexpr int kMeterW     = 44;    // GR meter strip width
    static constexpr int kActivityW  = 130;   // gate state indicator width
    // Knob section
    static constexpr int kKnobY      = kDispY + kDispH + 14;
    static constexpr int kKnobSize   = 54;
    static constexpr int kKnobLabelH = 13;
    static constexpr int kPadX       = 14;
    static constexpr int kNumCols    = 9;   // THRESH RANGE RATIO ATK HOLD REL HYS OUT MIX
    static constexpr int kFooterH    = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GateEditor)
};

} // namespace kaos_engine
