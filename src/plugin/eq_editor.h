#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "look_and_feel.h"
#include "eq_plugin.h"

namespace kaos_engine {

class EqEditor final : public juce::AudioProcessorEditor,
                       public juce::Timer
{
public:
    explicit EqEditor(EqPlugin& plugin);
    ~EqEditor() override;

    void paint    (juce::Graphics&) override;
    void resized  () override;
    void timerCallback() override;

    // ── Mouse ─────────────────────────────────────────────────────────────────
    void mouseDown     (const juce::MouseEvent&) override;
    void mouseDrag     (const juce::MouseEvent&) override;
    void mouseUp       (const juce::MouseEvent&) override;
    void mouseMove     (const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,
                        const juce::MouseWheelDetails&) override;

private:
    EqPlugin& plugin_;
    NullEngineLookAndFeel laf_ { ColourPalette::dark(0xffD22B2B) };

    static constexpr int kNumBands = EqPlugin::kNumBands;   // 7
    static constexpr int kNumCols  = kNumBands + 2;         // 9 (7 bands + OUTPUT + MIX)

    // ── Per-band knobs (arrays) ────────────────────────────────────────────────
    juce::Slider freq_slider_[kNumBands];
    juce::Slider gain_slider_[kNumBands];
    juce::Slider q_slider_   [kNumBands];
    juce::Label  freq_lbl_   [kNumBands];
    juce::Label  gain_lbl_   [kNumBands];
    juce::Label  q_lbl_      [kNumBands];
    juce::Label  type_lbl_   [kNumBands];   // shows current filter type

    juce::Slider output_knob_, mix_knob_;
    juce::Label  output_lbl_,  mix_lbl_;

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> freq_att_[kNumBands];
    std::unique_ptr<Attachment> gain_att_[kNumBands];
    std::unique_ptr<Attachment> q_att_   [kNumBands];
    std::unique_ptr<Attachment> output_att_, mix_att_;

    // ── Spectrum analyzer ──────────────────────────────────────────────────────
    static constexpr int kFftOrder  = EqPlugin::kFftOrder;
    static constexpr int kFftSize   = EqPlugin::kFftSize;
    static constexpr int kScopeSize = 512;

    juce::dsp::FFT                      fft_    { kFftOrder };
    juce::dsp::WindowingFunction<float> window_ {
        static_cast<size_t>(kFftSize),
        juce::dsp::WindowingFunction<float>::hann };

    float fft_data_  [kFftSize * 2] {};
    float scope_data_[kScopeSize]   {};

    // ── Drawing helpers ────────────────────────────────────────────────────────
    void draw_display    (juce::Graphics&, juce::Rectangle<int> area);
    void draw_grid       (juce::Graphics&, juce::Rectangle<int> area);
    void draw_spectrum   (juce::Graphics&, juce::Rectangle<int> area);
    void draw_eq_curve   (juce::Graphics&, juce::Rectangle<int> area);
    void draw_band_labels(juce::Graphics&);

    void setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name);
    void update_spectrum();
    void update_band_ui();        // enable/disable gain+Q knobs per type
    void show_type_menu(int band);

    // ── Node interaction ───────────────────────────────────────────────────────
    int                hit_test_node  (juce::Point<float> pos) const;
    juce::Point<float> node_screen_pos(int band) const;
    void               set_param      (const juce::String& id, float raw_value);
    void               begin_gesture  (const juce::String& id);
    void               end_gesture    (const juce::String& id);

    static float freq_to_x(float freq, float width);
    static float x_to_freq(float x,    float width);
    static float db_to_y  (float db,   float height, float db_min, float db_max);
    static float y_to_db  (float y,    float height, float db_min, float db_max);

    static juce::String freq_param_id(int b) { return "band_"+juce::String(b)+"_freq"; }
    static juce::String gain_param_id(int b) { return "band_"+juce::String(b)+"_gain"; }
    static juce::String q_param_id   (int b) { return "band_"+juce::String(b)+"_q";    }
    static juce::String type_param_id(int b) { return "band_"+juce::String(b)+"_type"; }

    static constexpr float kNodeRadius = 8.0f;

    int   hovered_band_  = -1;
    int   dragging_band_ = -1;
    BandType drag_type_  = BandType::Off;
    float drag_start_freq_  = 0.0f;
    float drag_start_gain_  = 0.0f;
    juce::Point<float> drag_start_mouse_;

    std::unique_ptr<juce::TooltipWindow> tooltip_window_;

    // ── Layout ────────────────────────────────────────────────────────────────
    static constexpr int kWidth      = 700;
    static constexpr int kHeight     = 440;
    static constexpr int kDisplayH   = 160;
    static constexpr int kTypeLblY   = kDisplayH + 4;
    static constexpr int kTypeLblH   = 13;
    static constexpr int kSepY       = kTypeLblY + kTypeLblH + 4;
    static constexpr int kKnobRowH   = 69;
    static constexpr int kKnobY1     = kSepY + 6;
    static constexpr int kKnobY2     = kKnobY1 + kKnobRowH;
    static constexpr int kKnobY3     = kKnobY2 + kKnobRowH;
    static constexpr int kKnobSize   = 52;
    static constexpr int kLabelH     = 13;
    static constexpr int kPadX       = 14;
    static constexpr int kFooterH    = 16;

    static constexpr float kDbMin = -24.0f;
    static constexpr float kDbMax =  24.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqEditor)
};

} // namespace kaos_engine
