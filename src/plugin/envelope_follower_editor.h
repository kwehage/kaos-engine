#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "envelope_follower_plugin.h"

namespace kaos_engine {

class EnvelopeFollowerEditor final : public juce::AudioProcessorEditor,
                                     public juce::Timer
{
public:
    explicit EnvelopeFollowerEditor(EnvelopeFollowerPlugin& plugin);
    ~EnvelopeFollowerEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    EnvelopeFollowerPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Detector + output shape + output mode ────────────────────────────────
    juce::ComboBox detector_box_, shape_box_, out_box_;
    juce::Label    detector_label_, shape_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        detector_attach_, shape_attach_, out_attach_;

    // ── Knobs ────────────────────────────────────────────────────────────────
    juce::Slider attack_knob_, release_knob_, gain_knob_;
    juce::Slider depth_knob_,  cc_num_knob_,  cc_ch_knob_;
    juce::Label  attack_lbl_,  release_lbl_,  gain_lbl_;
    juce::Label  depth_lbl_,   cc_num_lbl_,   cc_ch_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> attack_att_, release_att_, gain_att_;
    std::unique_ptr<Attachment> depth_att_,  cc_num_att_,  cc_ch_att_;

    // ── Strip chart ───────────────────────────────────────────────────────────
    void draw_strip_chart(juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_mode_ui();

    static constexpr int kTrailSize = 700;
    // env_trail_ = raw ballistics envelope [0,1] (drawn in accent colour)
    // cv_trail_  = shaped CV output [0,1]         (drawn in text_primary)
    std::vector<float> env_trail_ = std::vector<float>(kTrailSize, 0.0f);
    std::vector<float> cv_trail_  = std::vector<float>(kTrailSize, 0.0f);
    int trail_write_ = 0;
    int trail_count_ = 0;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 600;
    static constexpr int kHeight     = 325;
    static constexpr int kComboY     = 6;
    static constexpr int kComboH     = 20;
    static constexpr int kComboW     = 90;
    // Two label rows below the combos
    static constexpr int kSep1Y      = kComboY + kComboH * 3 + 10;
    static constexpr int kDispY      = kSep1Y + 4;
    static constexpr int kDispH      = 88;
    static constexpr int kSep2Y      = kDispY + kDispH + 4;
    static constexpr int kLabelY     = kSep2Y + 6;
    static constexpr int kLabelH     = 13;
    static constexpr int kKnobY      = kLabelY + kLabelH + 4;
    static constexpr int kKnobSize   = 54;
    static constexpr int kKnobLabelH = 13;
    static constexpr int kNumCols    = 6;   // ATTACK RELEASE GAIN DEPTH CC_NUM CC_CH
    static constexpr int kPadX       = 14;
    static constexpr int kSep3Y      = kKnobY + kKnobSize + kKnobLabelH + 4;
    static constexpr int kCtrlY      = kSep3Y + 6;
    static constexpr int kCtrlH      = 24;
    static constexpr int kFooterH    = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeFollowerEditor)
};

} // namespace kaos_engine
