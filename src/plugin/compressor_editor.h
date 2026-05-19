#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "compressor_plugin.h"

namespace kaos_engine {

class CompressorEditor final : public juce::AudioProcessorEditor,
                               public juce::Timer
{
public:
    explicit CompressorEditor(CompressorPlugin& plugin);
    ~CompressorEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    CompressorPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Algorithm selector ───────────────────────────────────────────────────
    juce::ComboBox algo_box_;
    juce::Label    algo_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algo_attach_;

    // ── Knobs ────────────────────────────────────────────────────────────────
    juce::Slider threshold_knob_, ratio_knob_, knee_knob_;
    juce::Slider attack_knob_,    release_knob_, makeup_knob_;
    juce::Slider output_knob_,    mix_knob_;

    juce::Label threshold_lbl_, ratio_lbl_, knee_lbl_;
    juce::Label attack_lbl_,    release_lbl_, makeup_lbl_;
    juce::Label output_lbl_,    mix_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment>
        threshold_att_, ratio_att_, knee_att_,
        attack_att_,    release_att_, makeup_att_,
        output_att_,    mix_att_;

    // ── Drawing helpers ──────────────────────────────────────────────────────
    void draw_transfer_curve(juce::Graphics& g, juce::Rectangle<int> area);
    void draw_gr_meter      (juce::Graphics& g, juce::Rectangle<int> area);

    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_algo_label();

    float gr_display_ = 0.0f;   // smoothed GR value for meter (UI thread)

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ───────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 700;
    static constexpr int kHeight     = 340;
    static constexpr int kComboY     = 8;
    static constexpr int kComboH     = 22;
    static constexpr int kComboW     = 110;
    static constexpr int kSep1Y      = kComboY + kComboH + 6;
    static constexpr int kDisplayY   = kSep1Y + 4;
    static constexpr int kDisplayH   = 140;      // transfer function
    static constexpr int kMeterW     = 44;        // GR meter strip
    static constexpr int kSep2Y      = kDisplayY + kDisplayH + 4;
    static constexpr int kLabelY     = kSep2Y + 6;
    static constexpr int kLabelH     = 13;
    static constexpr int kKnobY      = kLabelY + kLabelH + 4;
    static constexpr int kKnobSize   = 64;
    static constexpr int kKnobLabelH = 13;
    static constexpr int kPadX       = 14;
    static constexpr int kNumCols    = 8;         // THRESH RATIO KNEE ATK REL MAKEUP OUT MIX
    static constexpr int kFooterH    = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressorEditor)
};

} // namespace kaos_engine
