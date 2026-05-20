#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "look_and_feel.h"
#include "noise_plugin.h"

namespace kaos_engine {

class NoiseEditor final : public juce::AudioProcessorEditor,
                          public juce::Timer
{
public:
    explicit NoiseEditor(NoisePlugin& plugin);
    ~NoiseEditor() override;

    void paint  (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    NoisePlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    // ── Type + Mode + Blend combos ────────────────────────────────────────────
    juce::ComboBox type_box_, mode_box_, blend_box_;
    juce::Label    desc_label_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        type_attach_, mode_attach_, blend_attach_;

    // ── Knobs ─────────────────────────────────────────────────────────────────
    juce::Slider gain_knob_, mod_knob_,  size_knob_,    density_knob_;
    juce::Slider threshold_knob_, attack_knob_, release_knob_;
    juce::Slider mix_knob_,  output_knob_;
    juce::Label  gain_lbl_,  mod_lbl_,   size_lbl_,     density_lbl_;
    juce::Label  threshold_lbl_, attack_lbl_, release_lbl_;
    juce::Label  mix_lbl_,   output_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> gain_att_,      mod_att_,    size_att_,   density_att_;
    std::unique_ptr<Attachment> threshold_att_, attack_att_, release_att_;
    std::unique_ptr<Attachment> mix_att_,       output_att_;

    // ── Waveform preview ─────────────────────────────────────────────────────
    void draw_preview(juce::Graphics& g, juce::Rectangle<int> area);
    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_mode_ui();
    void update_type_ui();
    void rebuild_preview();

    static constexpr int kPreviewSamples = 600;
    std::vector<float> preview_buf_ = std::vector<float>(kPreviewSamples, 0.0f);

    // Waveform trail for real-time scrolling display
    static constexpr int kTrailSize = 700;  // matches kWidth
    // wet = processed output (bright accent); dry = raw input (faint accent)
    std::vector<float> trail_     = std::vector<float>(kTrailSize, 0.0f);
    std::vector<float> dry_trail_ = std::vector<float>(kTrailSize, 0.0f);
    int trail_write_ = 0;
    int trail_count_ = 0;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 700;
    static constexpr int kHeight     = 290;
    static constexpr int kComboY     = 6;
    static constexpr int kComboH     = 20;
    static constexpr int kComboW     = 100;
    static constexpr int kSep1Y      = kComboY + kComboH * 2 + 10;
    static constexpr int kDispY      = kSep1Y + 4;
    static constexpr int kDispH      = 90;
    static constexpr int kSep2Y      = kDispY + kDispH + 4;
    static constexpr int kLabelY     = kSep2Y + 6;
    static constexpr int kLabelH     = 13;
    static constexpr int kKnobY      = kLabelY + kLabelH + 4;
    static constexpr int kKnobSize   = 54;
    static constexpr int kKnobLabelH = 13;
    static constexpr int kNumCols    = 9;
    static constexpr int kPadX       = 14;
    static constexpr int kSep3Y      = kKnobY + kKnobSize + kKnobLabelH + 4;
    static constexpr int kFooterH    = 16;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseEditor)
};

} // namespace kaos_engine
