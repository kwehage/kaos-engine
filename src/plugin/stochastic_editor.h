#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "stochastic_plugin.h"

namespace kaos_engine {

class StochasticEditor final : public juce::AudioProcessorEditor,
                               public juce::Timer
{
public:
    explicit StochasticEditor(StochasticPlugin& plugin);
    ~StochasticEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    StochasticPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Mode selector ────────────────────────────────────────────────────────
    juce::ComboBox mode_box_;
    juce::Label    mode_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mode_attach_;

    // ── Sync + division ──────────────────────────────────────────────────────
    juce::ComboBox sync_box_, div_box_;
    juce::Label    sync_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sync_attach_, div_attach_;

    // ── Output mode + trigger mode ────────────────────────────────────────────
    juce::ComboBox out_box_, trig_box_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> out_attach_, trig_attach_;

    // ── Knobs ────────────────────────────────────────────────────────────────
    juce::Slider rate_knob_, depth_knob_, shape_knob_, offset_knob_;
    juce::Label  rate_lbl_,  depth_lbl_,  shape_lbl_,  offset_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> rate_att_, depth_att_, shape_att_, offset_att_;

    // ── CC text fields ────────────────────────────────────────────────────────
    juce::Label cc_num_field_, cc_ch_field_;
    juce::Label cc_num_lbl_,   cc_ch_lbl_;

    // ── Drawing ───────────────────────────────────────────────────────────────
    void draw_strip_chart(juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_mode_label();
    void update_mode_ui();

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 700;
    static constexpr int kHeight     = 248;
    static constexpr int kComboY     = 8;
    static constexpr int kComboH     = 22;
    static constexpr int kDispY      = kComboY + kComboH + 8;   // 38
    static constexpr int kDispH      = 100;
    static constexpr int kKnobY      = kDispY + kDispH + 14;    // 152
    static constexpr int kKnobSize   = 54;
    static constexpr int kKnobLabelH = 13;
    static constexpr int kNumCols    = 6;
    static constexpr int kPadX       = 14;
    static constexpr int kFooterH    = 16;

    // Scrolling strip-chart buffer: one sample per timer tick (30 Hz).
    // kTrailSize = kWidth gives a 1:1 pixel-to-sample mapping once full.
    static constexpr int kTrailSize = kWidth;
    std::vector<float> signal_trail_ = std::vector<float>(kTrailSize, 0.0f);
    int trail_write_ = 0;
    int trail_count_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StochasticEditor)
};

} // namespace kaos_engine
