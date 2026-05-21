#include "looper_editor.h"
#include <cmath>
#include <algorithm>

namespace kaos_engine {

// ── Helpers ───────────────────────────────────────────────────────────────────

void LooperEditor::setup_knob(juce::Slider& k, juce::Label& l, const juce::String& name)
{
    k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, kKnobSize, 13);
    k.setLookAndFeel(&laf_);
    addAndMakeVisible(k);

    l.setText(name, juce::dontSendNotification);
    l.setFont(juce::Font(8.5f));
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colour(laf_.text_primary()));
    l.setLookAndFeel(&laf_);
    addAndMakeVisible(l);
}

static const char* state_name(LooperState s)
{
    switch (s) {
        case LooperState::Idle:       return "IDLE";
        case LooperState::WaitRecord: return "WAIT REC";
        case LooperState::Recording:  return "RECORDING";
        case LooperState::WaitStop:   return "WAIT STOP";
        case LooperState::Playing:    return "PLAYING";
        case LooperState::Stopped:    return "STOPPED";
    }
    return "IDLE";
}

// ── Constructor ───────────────────────────────────────────────────────────────

LooperEditor::LooperEditor(LooperPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setSize(kWidth, kHeight);
    setLookAndFeel(&laf_);

    tooltip_window_ = std::make_unique<juce::TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // ── Combos ────────────────────────────────────────────────────────────────
    auto finish_combo = [&](juce::ComboBox& cb, juce::Label& lbl, const juce::String& name,
                            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                            const juce::String& param_id)
    {
        cb.setScrollWheelEnabled(false);
        cb.setLookAndFeel(&laf_);
        addAndMakeVisible(cb);
        lbl.setText(name, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setLookAndFeel(&laf_);
        addAndMakeVisible(lbl);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, param_id, cb);
    };

    sync_box_.addItem("Freeform",  1);
    sync_box_.addItem("Time Sync", 2);
    finish_combo(sync_box_, sync_lbl_, "SYNC", sync_attach_, "loop_sync_mode");

    bars_box_.addItem("1", 1);
    bars_box_.addItem("2", 2);
    bars_box_.addItem("4", 3);
    bars_box_.addItem("8", 4);
    finish_combo(bars_box_, bars_lbl_, "BARS", bars_attach_, "loop_bars");

    playback_box_.addItem("Forward",          1);
    playback_box_.addItem("Backward",         2);
    playback_box_.addItem("Bounce",           3);
    playback_box_.addItem("Accumulate",       4);
    playback_box_.addItem("Accumulate Reverse", 5);
    finish_combo(playback_box_, playback_lbl_, "PLAYBACK", playback_attach_, "loop_playback");

    // ── Knobs (right side) ────────────────────────────────────────────────────
    setup_knob(feedback_knob_, feedback_lbl_, "FEEDBACK");
    setup_knob(input_knob_,    input_lbl_,    "INPUT");
    setup_knob(output_knob_,   output_lbl_,   "OUTPUT");
    setup_knob(mix_knob_,      mix_lbl_,      "MIX");

    feedback_knob_.setTooltip("Feedback (f): overdub decay per loop pass in Accumulate and Accumulate Reverse modes. "
                              "0 = silence the previous buffer on each pass, 1 = keep it fully. "
                              "Used in equations as f. Range 0 - 1.");
    input_knob_.setTooltip("Input Gain: pre-gain applied to the live input before recording or overdubbing. "
                           "Boost quiet sources or attenuate loud ones. Range -20 to +12 dB.");
    output_knob_.setTooltip("Output (o): final output level applied after the dry/wet mix. "
                            "Used in equations as o. Range -20 to +6 dB.");
    mix_knob_.setTooltip("Mix (w): blend between live input passthrough and loop playback. "
                         "out = (1-w)*input + w*loop. 0 = input only, 1 = loop only. Range 0 - 1.");

    feedback_att_ = std::make_unique<Attachment>(apvts, "loop_feedback",   feedback_knob_);
    input_att_    = std::make_unique<Attachment>(apvts, "loop_input_gain", input_knob_);
    output_att_   = std::make_unique<Attachment>(apvts, "loop_out_gain",   output_knob_);
    mix_att_      = std::make_unique<Attachment>(apvts, "loop_mix",        mix_knob_);

    // ── CC text fields (left side) ────────────────────────────────────────────
    setup_cc_field(cc_rec_field_,  cc_rec_lbl_,  "CC REC",  "loop_cc_record",  127);
    setup_cc_field(cc_stop_field_, cc_stop_lbl_, "CC STOP", "loop_cc_stop",    127);
    setup_cc_field(cc_clr_field_,  cc_clr_lbl_,  "CC CLEAR",   "loop_cc_clear",   127);
    setup_cc_field(cc_chan_field_,  cc_chan_lbl_,  "CC CHANNEL", "loop_cc_channel", 16);
    cc_rec_field_ .setTooltip("CC Record: MIDI CC number that triggers the REC command (leading edge, value >= 64). Range 0 - 127.");
    cc_stop_field_.setTooltip("CC Stop: MIDI CC number that triggers the STOP command (leading edge, value >= 64). Range 0 - 127.");
    cc_clr_field_ .setTooltip("CC Clear: MIDI CC number that triggers the CLEAR command (leading edge, value >= 64). Range 0 - 127.");
    cc_chan_field_ .setTooltip("CC Channel: MIDI channel to listen on for REC/STOP/CLEAR commands. 0 = respond to any channel. Range 0 - 16.");

    // ── Transport buttons ──────────────────────────────────────────────────────
    rec_btn_.setLookAndFeel(&laf_);
    stop_btn_.setLookAndFeel(&laf_);
    clear_btn_.setLookAndFeel(&laf_);
    addAndMakeVisible(rec_btn_);
    addAndMakeVisible(stop_btn_);
    addAndMakeVisible(clear_btn_);

    rec_btn_.onClick   = [this] { plugin_.get_dsp().cmd_record(); };
    stop_btn_.onClick  = [this] { plugin_.get_dsp().cmd_stop();   };
    clear_btn_.onClick = [this] { plugin_.get_dsp().cmd_clear();  };

    update_button_colors();
    startTimerHz(30);
}

LooperEditor::~LooperEditor()
{
    setLookAndFeel(nullptr);
    rec_btn_.setLookAndFeel(nullptr);
    stop_btn_.setLookAndFeel(nullptr);
    clear_btn_.setLookAndFeel(nullptr);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void LooperEditor::resized()
{
    const int col_w = (kWidth - 2 * kPadX) / kNumCols;

    // Waveform: full width
    // (drawn in paint, no child components)

    // Transport buttons: left-aligned with state label on the right
    rec_btn_  .setBounds(kPadX,              kTransY, kBtnW, kTransH);
    stop_btn_ .setBounds(kPadX + kBtnW + 6,  kTransY, kBtnW, kTransH);
    clear_btn_.setBounds(kPadX + kBtnW * 2 + 12, kTransY, kBtnW, kTransH);

    // Combos: three across the full width
    const int combo_total_w = kWidth - 2 * kPadX;
    const int combo_each    = combo_total_w / 3 - 8;
    for (int i = 0; i < 3; ++i) {
        const int cx = kPadX + i * (combo_each + 8);
        juce::ComboBox* boxes[]  = { &sync_box_,  &bars_box_,  &playback_box_  };
        juce::Label*    labels[] = { &sync_lbl_,  &bars_lbl_,  &playback_lbl_  };
        labels[i]->setBounds(cx, kComboLblY, combo_each, kComboLblH);
        boxes[i] ->setBounds(cx, kComboY,    combo_each, kComboH);
    }

    // CC text fields: left side (cols 0-3); label above field, both in knob area
    const int field_h = 22;
    const int field_y = kKnobY + kKnobLblH + 2;
    juce::Label* cc_fields[] = { &cc_rec_field_, &cc_stop_field_, &cc_clr_field_, &cc_chan_field_ };
    juce::Label* cc_hdrs[]   = { &cc_rec_lbl_,   &cc_stop_lbl_,   &cc_clr_lbl_,   &cc_chan_lbl_  };
    for (int c = 0; c < 4; ++c) {
        const int lx = kPadX + c * col_w;
        cc_hdrs[c]  ->setBounds(lx, kKnobY, col_w, kKnobLblH);
        cc_fields[c]->setBounds(lx + 4, field_y, col_w - 8, field_h);
    }

    // Knobs: right side (cols 4-7); label BELOW knob
    juce::Slider* knobs[]   = { &feedback_knob_, &input_knob_, &output_knob_, &mix_knob_ };
    juce::Label*  kn_lbls[] = { &feedback_lbl_,  &input_lbl_,  &output_lbl_,  &mix_lbl_  };
    for (int c = 0; c < 4; ++c) {
        const int col = c + 4;
        const int cx  = kPadX + col * col_w + (col_w - kKnobSize) / 2;
        knobs[c]  ->setBounds(cx, kKnobY, kKnobSize, kKnobSize);
        kn_lbls[c]->setBounds(kPadX + col * col_w, kKnobY + kKnobSize + 1, col_w, kKnobLblH);
    }
}

// ── Painting ──────────────────────────────────────────────────────────────────

void LooperEditor::paint(juce::Graphics& g)
{
    const auto bg   = juce::Colour(laf_.background());
    const auto surf = juce::Colour(laf_.surface());
    const auto bdr  = juce::Colour(laf_.border());
    const auto tp   = juce::Colour(laf_.text_primary());
    const auto tm   = juce::Colour(laf_.text_muted());
    const auto acc  = juce::Colour(laf_.accent_colour());

    g.fillAll(bg);


    // ── Waveform display ──────────────────────────────────────────────────────
    const auto wave_rect = juce::Rectangle<int>(0, kWaveY, kWidth, kWaveH);
    g.setColour(surf);
    g.fillRect(wave_rect);
    draw_waveform(g, wave_rect);


    // ── State indicator (right of transport buttons) ──────────────────────────
    const LooperState st = plugin_.get_dsp().get_state();
    juce::Colour state_col;
    switch (st) {
        case LooperState::Recording:
        case LooperState::WaitStop:   state_col = acc; break;
        case LooperState::WaitRecord: state_col = acc.withAlpha(0.55f); break;
        case LooperState::Playing:    state_col = juce::Colour(0xff66cc66); break;
        case LooperState::Stopped:    state_col = juce::Colour(0xff6699cc); break;
        default:                      state_col = tm; break;
    }
    g.setColour(state_col);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    const int state_x = kPadX + kBtnW * 3 + 24;
    g.drawText(state_name(st), state_x, kTransY, kWidth - state_x - kPadX, kTransH,
               juce::Justification::centredLeft, false);

    // ── Footer ────────────────────────────────────────────────────────────────
    g.setFont(juce::Font(12.0f));
    g.setColour(juce::Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::looper", kPadX, kHeight - kFooterH - 4,
               300, kFooterH, juce::Justification::centredLeft, false);
}

void LooperEditor::draw_waveform(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto acc  = juce::Colour(laf_.accent_colour());
    const auto bdr  = juce::Colour(laf_.border());
    const auto tm   = juce::Colour(laf_.text_muted());

    const int w = area.getWidth();
    const int h = area.getHeight();
    const int cy = area.getY() + h / 2;

    // Center line
    g.setColour(bdr);
    g.drawHorizontalLine(cy, float(area.getX()), float(area.getRight()));

    const int  ll       = plugin_.get_dsp().get_loop_length();
    const auto* pl      = plugin_.get_dsp().peaks_l();
    const auto* pr      = plugin_.get_dsp().peaks_r();
    const int   play_p  = plugin_.get_dsp().get_play_pos();
    const int   rec_p   = plugin_.get_dsp().get_rec_pos();
    const LooperState st = plugin_.get_dsp().get_state();

    // Draw waveform if there's a loop
    if (ll > 0) {
        const float half_h = float(h) * 0.45f;
        g.setColour(acc.withAlpha(0.55f));
        for (int px = 0; px < w; ++px) {
            const int bin = px * LooperProcessor::kPeakBins / w;
            if (bin >= LooperProcessor::kPeakBins) break;
            const float peak = std::min(std::max(pl[bin], pr[bin]), 1.0f);
            const int amp    = int(peak * half_h);
            g.drawVerticalLine(area.getX() + px, float(cy - amp), float(cy + amp + 1));
        }

        // Playback cursor
        if (st == LooperState::Playing || st == LooperState::Stopped) {
            const int cursor_x = area.getX() + int(int64_t(play_p) * w / ll);
            g.setColour(acc);
            g.drawVerticalLine(cursor_x, float(area.getY() + 2), float(area.getBottom() - 2));
        }
    }

    // Recording cursor / progress bar
    if (st == LooperState::Recording || st == LooperState::WaitStop) {
        const int max_s  = int(plugin_.getSampleRate() * LooperProcessor::kMaxSeconds);
        const int rec_x  = max_s > 0 ? int(int64_t(rec_p) * w / max_s) : 0;
        g.setColour(acc.withAlpha(0.25f));
        g.fillRect(area.getX(), area.getY(), rec_x, h);
        g.setColour(acc);
        g.drawVerticalLine(area.getX() + rec_x, float(area.getY() + 2), float(area.getBottom() - 2));
    }

    // WaitRecord: pulsing text
    if (st == LooperState::WaitRecord || st == LooperState::WaitStop) {
        g.setColour(acc.withAlpha(0.6f));
        g.setFont(juce::Font(10.0f));
        const char* msg = (st == LooperState::WaitRecord) ? "waiting for bar..." : "will stop at bar...";
        g.drawText(msg, area.reduced(8, 4), juce::Justification::centred, false);
    }

    // Empty state hint
    if (st == LooperState::Idle && ll == 0) {
        g.setColour(tm);
        g.setFont(juce::Font(10.0f));
        g.drawText("press REC or send MIDI CC to begin recording",
                   area.reduced(8, 4), juce::Justification::centred, false);
    }

    // Loop boundary markers at left/right edges when playing
    if (ll > 0 && (st == LooperState::Playing || st == LooperState::Stopped)) {
        g.setColour(bdr.brighter(0.3f));
        g.drawVerticalLine(area.getX(),        float(area.getY()), float(area.getBottom()));
        g.drawVerticalLine(area.getRight() - 1, float(area.getY()), float(area.getBottom()));
    }
}

// ── Timer ─────────────────────────────────────────────────────────────────────

void LooperEditor::update_button_colors()
{
    const LooperState st = plugin_.get_dsp().get_state();
    const auto acc  = juce::Colour(laf_.accent_colour());
    const auto surf = juce::Colour(laf_.surface());
    const auto tp   = juce::Colour(laf_.text_primary());

    const bool recording = (st == LooperState::Recording || st == LooperState::WaitRecord || st == LooperState::WaitStop);
    const bool playing   = (st == LooperState::Playing);

    rec_btn_.setColour(juce::TextButton::buttonColourId,
        recording ? acc : surf);
    rec_btn_.setColour(juce::TextButton::textColourOffId,
        recording ? juce::Colours::white : tp);

    stop_btn_.setColour(juce::TextButton::buttonColourId,
        playing ? juce::Colour(0xff336633) : surf);
    stop_btn_.setColour(juce::TextButton::textColourOffId, tp);

    clear_btn_.setColour(juce::TextButton::buttonColourId, surf);
    clear_btn_.setColour(juce::TextButton::textColourOffId, tp);
}

void LooperEditor::setup_cc_field(juce::Label& field, juce::Label& lbl,
                                   const juce::String& name,
                                   const juce::String& param_id, int max_val)
{
    const auto surf = juce::Colour(laf_.surface());
    const auto tp   = juce::Colour(laf_.text_primary());
    const auto bdr  = juce::Colour(laf_.border());
    const auto acc  = juce::Colour(laf_.accent_colour());

    field.setEditable(true, true, false);
    field.setJustificationType(juce::Justification::centred);
    field.setFont(juce::Font(11.0f));
    field.setColour(juce::Label::backgroundColourId,          surf);
    field.setColour(juce::Label::textColourId,                tp);
    field.setColour(juce::Label::outlineColourId,             bdr);
    field.setColour(juce::Label::backgroundWhenEditingColourId, surf.brighter(0.08f));
    field.setColour(juce::Label::textWhenEditingColourId,     tp);
    field.setColour(juce::Label::outlineWhenEditingColourId,  acc);

    if (auto* p = plugin_.get_apvts().getRawParameterValue(param_id))
        field.setText(juce::String(juce::roundToInt(p->load())), juce::dontSendNotification);

    field.onTextChange = [this, &field, param_id, max_val] {
        const int val = juce::jlimit(0, max_val, field.getText().getIntValue());
        field.setText(juce::String(val), juce::dontSendNotification);
        if (auto* param = plugin_.get_apvts().getParameter(param_id))
            param->setValueNotifyingHost(param->convertTo0to1(float(val)));
    };

    addAndMakeVisible(field);

    lbl.setText(name, juce::dontSendNotification);
    lbl.setJustificationType(juce::Justification::centred);
    lbl.setFont(juce::Font(8.5f));
    lbl.setColour(juce::Label::textColourId, tp);
    addAndMakeVisible(lbl);
}

void LooperEditor::timerCallback()
{
    const LooperState st = plugin_.get_dsp().get_state();
    const bool dirty = plugin_.get_dsp().take_waveform_dirty();

    // Refresh CC field values from parameters (handles automation / preset load)
    auto refresh_cc = [&](juce::Label& field, const char* param_id) {
        if (!field.isBeingEdited()) {
            if (auto* p = plugin_.get_apvts().getRawParameterValue(param_id)) {
                const int val = juce::roundToInt(p->load());
                if (field.getText().getIntValue() != val)
                    field.setText(juce::String(val), juce::dontSendNotification);
            }
        }
    };
    refresh_cc(cc_rec_field_,  "loop_cc_record");
    refresh_cc(cc_stop_field_, "loop_cc_stop");
    refresh_cc(cc_clr_field_,  "loop_cc_clear");
    refresh_cc(cc_chan_field_,  "loop_cc_channel");

    if (st != last_state_) {
        last_state_ = st;
        update_button_colors();
        repaint();
    } else if (dirty || st == LooperState::Playing || st == LooperState::Recording
               || st == LooperState::WaitStop) {
        repaint(0, kWaveY, kWidth, kWaveH + 2);
        repaint(0, kTransY, kWidth, kTransH);
    }
}

} // namespace kaos_engine
