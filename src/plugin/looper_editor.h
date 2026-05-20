#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "looper_plugin.h"

namespace kaos_engine {

class LooperEditor final : public juce::AudioProcessorEditor,
                           public juce::Timer
{
public:
    explicit LooperEditor(LooperPlugin& plugin);
    ~LooperEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    LooperPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Combos ───────────────────────────────────────────────────────────────
    juce::ComboBox sync_box_, bars_box_, playback_box_;
    juce::Label    sync_lbl_, bars_lbl_, playback_lbl_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        sync_attach_, bars_attach_, playback_attach_;

    // ── Knobs ─────────────────────────────────────────────────────────────────
    juce::Slider feedback_knob_, input_knob_, output_knob_, mix_knob_;
    juce::Label  feedback_lbl_, input_lbl_, output_lbl_, mix_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> feedback_att_, input_att_, output_att_, mix_att_;

    // ── CC text fields (left side of knob row) ────────────────────────────────
    juce::Label cc_rec_field_, cc_stop_field_, cc_clr_field_, cc_chan_field_;
    juce::Label cc_rec_lbl_,   cc_stop_lbl_,   cc_clr_lbl_,   cc_chan_lbl_;
    void setup_cc_field(juce::Label& field, juce::Label& lbl,
                        const juce::String& name, const juce::String& param_id, int max_val);

    // ── Transport buttons ─────────────────────────────────────────────────────
    juce::TextButton rec_btn_   { "REC" };
    juce::TextButton stop_btn_  { "STOP" };
    juce::TextButton clear_btn_ { "CLEAR" };

    // ── Waveform display ──────────────────────────────────────────────────────
    void draw_waveform(juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_button_colors();

    LooperState last_state_ = LooperState::Idle;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth       = 700;
    static constexpr int kHeight      = 340;
    static constexpr int kHeaderH     = 24;
    static constexpr int kWaveY       = kHeaderH;
    static constexpr int kWaveH       = 130;
    static constexpr int kSep1Y       = kWaveY + kWaveH + 2;
    static constexpr int kTransY      = kSep1Y + 4;
    static constexpr int kTransH      = 28;
    static constexpr int kBtnW        = 70;
    static constexpr int kSep2Y       = kTransY + kTransH + 4;
    static constexpr int kComboLblY   = kSep2Y + 4;
    static constexpr int kComboLblH   = 12;
    static constexpr int kComboY      = kComboLblY + kComboLblH + 2;
    static constexpr int kComboH      = 20;
    static constexpr int kSep3Y       = kComboY + kComboH + 6;
    static constexpr int kKnobLblY    = kSep3Y + 4;
    static constexpr int kKnobLblH    = 13;
    static constexpr int kKnobY       = kKnobLblY + kKnobLblH + 4;
    static constexpr int kKnobSize    = 54;
    static constexpr int kValLblY     = kKnobY + kKnobSize;
    static constexpr int kValLblH     = 13;
    static constexpr int kFooterY     = kValLblY + kValLblH + 2;
    static constexpr int kFooterH     = 16;
    static constexpr int kNumCols     = 8;
    static constexpr int kPadX        = 14;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperEditor)
};

} // namespace kaos_engine
