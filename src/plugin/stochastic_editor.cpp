#include "stochastic_editor.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {
using namespace juce;

// Mode descriptions -- plain ASCII only (no literal Unicode)
static const char* kModeInfo[6] = {
    "S+H -- jumps to a random value at each clock tick. "
    "SHAPE: output range (0=narrow near 0, 1=full [-1..+1]).",

    "S+Glide -- like S+H but slides between values. "
    "SHAPE: glide fraction (0=instant jump, 1=always gliding).",

    "Smooth -- cubic interpolation between random targets; continuous, no jumps. "
    "SHAPE: curve (0=linear, 1=smoothstep).",

    "Brownian -- random walk (Ornstein-Uhlenbeck). Output drifts then returns. "
    "SHAPE: mean reversion (0=free drift, 1=strong pull to zero).",

    "Lorenz -- deterministic chaos; Lorenz strange attractor (x component). "
    "SHAPE: rho parameter (0=rho24 simple, 0.5=rho28 classic, 1=rho36 wilder). "
    "Display shows the x-z phase portrait.",

    "Logistic -- logistic map x -> r*x*(1-x). Tunes from periodic to fully chaotic. "
    "SHAPE: chaos amount (0=period-2 at r=3.5, 1=fully chaotic at r=4.0).",
};

// ── Construction ───────────────────────────────────────────────────────────────

StochasticEditor::StochasticEditor(StochasticPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& ap = plugin_.get_apvts();

    // Mode combo
    mode_box_.addItem("S+H",      1); mode_box_.addItem("S+Glide",  2);
    mode_box_.addItem("Smooth",   3); mode_box_.addItem("Brownian", 4);
    mode_box_.addItem("Lorenz",   5); mode_box_.addItem("Logistic", 6);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "mode", mode_box_);
    mode_box_.onChange = [this] { update_mode_label(); update_mode_ui(); };

    // mode_label_ not shown -- description text is routed to mode_box_ tooltip in update_mode_label().

    // Sync combo
    sync_box_.addItem("Free", 1);
    sync_box_.addItem("Sync", 2);
    sync_box_.setScrollWheelEnabled(false);
    sync_box_.setTooltip("Sync: Free = run at rate set by RATE knob. "
                         "Sync = lock to host BPM using the division combo.");
    addAndMakeVisible(sync_box_);
    sync_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "sync", sync_box_);
    sync_box_.onChange = [this] { update_mode_ui(); };

    sync_label_.setText("SYNC", dontSendNotification);
    sync_label_.setFont(Font(8.5f, Font::bold));
    sync_label_.setJustificationType(Justification::centredRight);
    sync_label_.setColour(Label::textColourId, Colour(laf_.text_muted()));
    addAndMakeVisible(sync_label_);

    // Division combo
    for (const char* s : {"Whole", "Half", "Dotted 1/4", "1/4",
                           "Dotted 1/8", "1/8", "1/8 Triplet", "1/16"})
        div_box_.addItem(s, div_box_.getNumItems() + 1);
    div_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(div_box_);
    div_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "sync_div", div_box_);

    // Output mode combo
    out_box_.addItem("MIDI CC",      1);
    out_box_.addItem("Audio CV",     2);
    out_box_.addItem("MIDI CC + CV", 3);
    out_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(out_box_);
    out_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "output_mode", out_box_);
    out_box_.onChange = [this] { update_mode_ui(); };

    // Trigger mode combo
    trig_box_.addItem("Free",           1);
    trig_box_.addItem("Note Retrigger", 2);
    trig_box_.addItem("Transport",      3);
    trig_box_.setScrollWheelEnabled(false);
    trig_box_.setTooltip(
        "Free: runs continuously.\n"
        "Note Retrigger: MIDI note-on resets state and starts; note-off holds.\n"
        "Transport: DAW play resets and starts; stop holds.");
    addAndMakeVisible(trig_box_);
    trig_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "trigger_mode", trig_box_);

    // Knobs
    setup_knob(rate_knob_,   rate_lbl_,   "RATE");
    setup_knob(depth_knob_,  depth_lbl_,  "DEPTH");
    setup_knob(shape_knob_,  shape_lbl_,  "SHAPE");
    setup_knob(offset_knob_, offset_lbl_, "OFFSET");
    rate_knob_  .setTooltip("Rate: clock frequency in Hz. Controls step/event speed for all modes.");
    depth_knob_ .setTooltip("Depth: amplitude scale. 0=off, 1=full range. "
                             "out = signal * depth + offset");
    shape_knob_ .setTooltip("Shape: mode-specific. See mode description for details.");
    offset_knob_.setTooltip("Offset: DC shift. 0=bipolar. +0.5 with depth=0.5 -> unipolar 0..+1.");

    rate_att_   = std::make_unique<Attachment>(ap, "rate",   rate_knob_);
    depth_att_  = std::make_unique<Attachment>(ap, "depth",  depth_knob_);
    shape_att_  = std::make_unique<Attachment>(ap, "shape",  shape_knob_);
    offset_att_ = std::make_unique<Attachment>(ap, "offset", offset_knob_);

    // CC text fields
    auto setup_cc = [&](Label& field, Label& lbl, const char* name,
                        const char* param_id, int lo, int hi) {
        field.setEditable(true, true, false);
        field.setJustificationType(Justification::centred);
        field.setFont(Font(14.0f));
        field.setColour(Label::textColourId,       Colour(laf_.text_primary()));
        field.setColour(Label::backgroundColourId, Colour(laf_.surface()));
        field.setColour(Label::outlineColourId,    Colour(laf_.border()));
        addAndMakeVisible(field);
        lbl.setText(name, dontSendNotification);
        lbl.setFont(Font(8.5f));
        lbl.setJustificationType(Justification::centred);
        lbl.setColour(Label::textColourId, Colour(laf_.text_primary()));
        addAndMakeVisible(lbl);
        auto* p = ap.getParameter(param_id);
        if (p) field.setText(String(juce::roundToInt(p->convertFrom0to1(p->getValue()))),
                              dontSendNotification);
        field.onTextChange = [&field, &ap, param_id, lo, hi] {
            const int v = juce::jlimit(lo, hi,
                              juce::roundToInt(field.getText().getFloatValue()));
            field.setText(String(v), dontSendNotification);
            if (auto* p2 = ap.getParameter(param_id))
                p2->setValueNotifyingHost(p2->convertTo0to1(float(v)));
        };
    };
    setup_cc(cc_num_field_, cc_num_lbl_, "CC NUM", "cc_number",  0, 127);
    setup_cc(cc_ch_field_,  cc_ch_lbl_,  "CC CH",  "cc_channel", 1, 16);
    cc_num_field_.setTooltip("CC Number: MIDI CC number (0-127).");
    cc_ch_field_ .setTooltip("CC Channel: MIDI channel (1-16).");

    update_mode_label();
    update_mode_ui();
    startTimerHz(30);
}

StochasticEditor::~StochasticEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void StochasticEditor::setup_knob(Slider& k, Label& l, const String& name)
{
    k.setSliderStyle(Slider::RotaryVerticalDrag);
    k.setTextBoxStyle(Slider::TextBoxBelow, false, kKnobSize, 13);
    k.setPopupDisplayEnabled(true, false, this);
    addAndMakeVisible(k);
    l.setText(name, dontSendNotification);
    l.setFont(Font(8.5f));
    l.setJustificationType(Justification::centred);
    l.setColour(Label::textColourId, Colour(laf_.text_primary()));
    addAndMakeVisible(l);
}

// ── Timer ──────────────────────────────────────────────────────────────────────

void StochasticEditor::timerCallback()
{
    auto& ap = plugin_.get_apvts();
    auto refresh_cc = [&](Label& field, const char* param_id) {
        if (!field.isBeingEdited())
            if (auto* p = ap.getParameter(param_id))
                field.setText(String(juce::roundToInt(p->convertFrom0to1(p->getValue()))),
                              dontSendNotification);
    };
    refresh_cc(cc_num_field_, "cc_number");
    refresh_cc(cc_ch_field_,  "cc_channel");

    // Sample the live output once per tick and append to the strip-chart buffer.
    signal_trail_[trail_write_] = plugin_.get_output();
    trail_write_ = (trail_write_ + 1) % kTrailSize;
    if (trail_count_ < kTrailSize) ++trail_count_;
    repaint(0, kDispY, kWidth, kDispH);
}

void StochasticEditor::update_mode_label()
{
    const int idx = mode_box_.getSelectedItemIndex();
    if (idx >= 0 && idx < 6)
        mode_box_.setTooltip(kModeInfo[idx]);
}

void StochasticEditor::update_mode_ui()
{
    const int  out_mode = out_box_.getSelectedItemIndex();
    const bool show_cc  = (out_mode == 0 || out_mode == 2);
    cc_num_field_.setEnabled(show_cc); cc_num_field_.setAlpha(show_cc ? 1.0f : 0.4f);
    cc_num_lbl_  .setAlpha(show_cc ? 1.0f : 0.4f);
    cc_ch_field_ .setEnabled(show_cc); cc_ch_field_ .setAlpha(show_cc ? 1.0f : 0.4f);
    cc_ch_lbl_   .setAlpha(show_cc ? 1.0f : 0.4f);

    const bool synced = sync_box_.getSelectedItemIndex() == 1;
    rate_knob_.setEnabled(!synced);
    rate_knob_.setAlpha(synced ? 0.4f : 1.0f);
    div_box_.setEnabled(synced);
    div_box_.setAlpha(synced ? 1.0f : 0.4f);
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void StochasticEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    // All combos across the top row; OUTPUT and TRIGGER right-justified
    mode_box_ .setBounds(kPadX,       kComboY, 110, kComboH);
    sync_box_ .setBounds(kPadX + 118, kComboY,  70, kComboH);
    div_box_  .setBounds(kPadX + 196, kComboY, 120, kComboH);
    trig_box_ .setBounds(w - kPadX - 150,       kComboY, 150, kComboH);
    out_box_  .setBounds(w - kPadX - 150 - 8 - 150, kComboY, 150, kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 36; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY,              kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 72, kKnobLabelH);
    };
    place(rate_knob_,   rate_lbl_,   0);
    place(depth_knob_,  depth_lbl_,  1);
    place(shape_knob_,  shape_lbl_,  2);
    place(offset_knob_, offset_lbl_, 3);

    auto place_cc = [&](Label& field, Label& lbl, int col) {
        const int fx = kPadX + col * colw + colw / 2 - 27;
        const int fy = kKnobY + (kKnobSize - 28) / 2;
        field.setBounds(fx, fy, 54, 28);
        lbl.setBounds(kPadX + col * colw + colw / 2 - 36,
                      kKnobY + kKnobSize + 1, 72, kKnobLabelH);
    };
    place_cc(cc_num_field_, cc_num_lbl_, 4);
    place_cc(cc_ch_field_,  cc_ch_lbl_,  5);

    // sync_label_ not shown -- sync is self-evident from combo items "Free"/"Sync"
}

// ── Painting ───────────────────────────────────────────────────────────────────

void StochasticEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    draw_strip_chart(g, Rectangle<int>(0, kDispY, kWidth, kDispH));

    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::stochastic", kPadX, kHeight - kFooterH - 4,
               250, kFooterH, Justification::centredLeft);
}

// ── Scrolling strip chart (all modes) ─────────────────────────────────────────
// One value per timer tick is appended to signal_trail_ in timerCallback.
// The buffer is drawn oldest-left to newest-right across the full display width.

void StochasticEditor::draw_strip_chart(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax = float(area.getX());
    const float ay = float(area.getY());
    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());

    // Zero line
    g.setColour(Colour(laf_.border()).withAlpha(0.5f));
    g.drawLine(ax, ay + h * 0.5f, ax + w, ay + h * 0.5f, 0.5f);

    static constexpr float kYRange = 1.1f;
    auto sig_y = [&](float v) { return ay + (1.0f - v / kYRange) * 0.5f * h; };

    if (trail_count_ < 2) {
        // Not enough data yet
        g.setFont(Font(9.0f));
        g.setColour(Colour(laf_.text_muted()));
        g.drawText("Set Trigger Mode to Free to start",
                   int(ax) + 4, int(ay + h / 2) - 6, int(w) - 8, 13,
                   Justification::centred);
    } else {
        // Always 1 px = 1 sample.  The newest n samples occupy the rightmost n
        // pixels; pixels to the left of that are drawn at 0 (flat baseline).
        // This keeps x-axis spacing constant while the buffer fills.
        const int n          = trail_count_;
        const int data_start = int(w) - n;   // first pixel that has real data

        Path curve;
        for (int px = 0; px < int(w); ++px) {
            float v = 0.0f;
            if (px >= data_start) {
                const int age = px - data_start;   // 0 = oldest, n-1 = newest
                const int si  = (trail_write_ - n + age + kTrailSize) % kTrailSize;
                v = signal_trail_[si];
            }
            const float cy = sig_y(v);
            const float cx = ax + float(px);
            if (px == 0) curve.startNewSubPath(cx, cy);
            else         curve.lineTo(cx, cy);
        }

        // Fill under the curve
        Path filled = curve;
        filled.lineTo(ax + w, sig_y(0.0f));
        filled.lineTo(ax,     sig_y(0.0f));
        filled.closeSubPath();
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.15f));
        g.fillPath(filled);

        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
        g.strokePath(curve, PathStrokeType(1.5f, PathStrokeType::curved,
                                           PathStrokeType::rounded));

        // Bright dot at the newest value (right edge)
        const int newest = (trail_write_ - 1 + kTrailSize) % kTrailSize;
        const float head_y = sig_y(signal_trail_[newest]);
        g.setColour(Colour(laf_.text_primary()).withAlpha(0.85f));
        g.fillEllipse(ax + w - 3.0f, head_y - 3.0f, 6.0f, 6.0f);
    }

    // Axis labels
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    g.drawText("+1", int(ax) + 2, int(ay) + 2,                20, 9, Justification::centredLeft);
    g.drawText(" 0", int(ax) + 2, int(sig_y(0.0f)) - 4,       20, 9, Justification::centredLeft);
    g.drawText("-1", int(ax) + 2, int(ay + h) - 11,            20, 9, Justification::centredLeft);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
    g.restoreState();
}

} // namespace kaos_engine
