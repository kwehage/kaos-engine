#include "envelope_follower_editor.h"

namespace kaos_engine {
using namespace juce;

// ── Construction ───────────────────────────────────────────────────────────────

EnvelopeFollowerEditor::EnvelopeFollowerEditor(EnvelopeFollowerPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& ap = plugin_.get_apvts();

    // Detector combo
    detector_box_.addItem("Peak", 1);
    detector_box_.addItem("RMS",  2);
    detector_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(detector_box_);
    detector_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "detector", detector_box_);

    detector_box_.onChange = [this] {
        const int idx = detector_box_.getSelectedItemIndex();
        detector_box_.setTooltip(
            idx == 0 ? "Peak: rectifies and smooths. Fast transient response. "
                       "Best when transient tracking speed matters."
                     : "RMS: tracks signal power (perceived loudness). "
                       "Smoother, less sensitive to spikes. Preferred for musical use.");
    };
    detector_box_.onChange();

    // Output shape combo
    shape_box_.addItem("Follow",  1);
    shape_box_.addItem("Duck",    2);
    shape_box_.addItem("Rise",    3);
    shape_box_.addItem("Fall",    4);
    shape_box_.addItem("Release", 5);
    shape_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(shape_box_);
    shape_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "output_shape", shape_box_);
    shape_box_.onChange = [this] {
        static const char* descs[5] = {
            "Follow: CV tracks envelope directly. Louder input = higher CV.",
            "Duck: inverted -- silent input = high CV, loud = low CV. Classic sidechain ducking.",
            "Rise: outputs only while signal is rising (attack phase). 0 during decay.",
            "Fall: outputs only while signal is falling (release phase). 0 during attack.",
            "Release: rises 0->1 as signal falls from its peak back to silence. "
                      "0 during attack and sustain; starts after signal begins decaying.",
        };
        const int idx = shape_box_.getSelectedItemIndex();
        if (idx >= 0 && idx < 5)
            shape_box_.setTooltip(descs[idx]);
    };
    // shape_label_ not shown -- description routed to shape_box_ tooltip.
    shape_box_.onChange();

    // Output mode combo
    out_box_.addItem("MIDI CC",      1);
    out_box_.addItem("Audio CV",     2);
    out_box_.addItem("MIDI CC + CV", 3);
    out_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(out_box_);
    out_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "output_mode", out_box_);
    out_box_.onChange = [this] { update_mode_ui(); };

    // Knobs
    setup_knob(attack_knob_,  attack_lbl_,  "ATTACK");
    setup_knob(release_knob_, release_lbl_, "RELEASE");
    setup_knob(gain_knob_,    gain_lbl_,    "GAIN");
    setup_knob(depth_knob_,   depth_lbl_,   "DEPTH");
    attack_knob_ .setTooltip("Attack: time for envelope to respond to a rising signal. "
                              "Short = catches transients fast. Range 0.1-500 ms.");
    release_knob_.setTooltip("Release: time for envelope to decay after signal drops. "
                              "Short = snappy/pumpy. Long = smooth. Range 1-5000 ms.");
    gain_knob_   .setTooltip("Gain: pre-detection input scaling (0.1x to 20x). "
                              "Increase to make quiet signals fill the output range.");
    depth_knob_  .setTooltip("Depth: output scale. 0 = no output, 1 = full 0..+1 range.");

    attack_att_  = std::make_unique<Attachment>(ap, "attack",  attack_knob_);
    release_att_ = std::make_unique<Attachment>(ap, "release", release_knob_);
    gain_att_    = std::make_unique<Attachment>(ap, "gain",    gain_knob_);
    depth_att_   = std::make_unique<Attachment>(ap, "depth",   depth_knob_);

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

    update_mode_ui();
    startTimerHz(30);
}

EnvelopeFollowerEditor::~EnvelopeFollowerEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void EnvelopeFollowerEditor::setup_knob(Slider& k, Label& l, const String& name)
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

void EnvelopeFollowerEditor::timerCallback()
{
    env_trail_[trail_write_] = plugin_.get_envelope();
    cv_trail_ [trail_write_] = plugin_.get_output();
    trail_write_ = (trail_write_ + 1) % kTrailSize;
    if (trail_count_ < kTrailSize) ++trail_count_;
    repaint(0, kDispY, kWidth, kDispH);
    auto& ap = plugin_.get_apvts();
    auto refresh_cc = [&](Label& field, const char* param_id) {
        if (!field.isBeingEdited())
            if (auto* p = ap.getParameter(param_id))
                field.setText(String(juce::roundToInt(p->convertFrom0to1(p->getValue()))),
                              dontSendNotification);
    };
    refresh_cc(cc_num_field_, "cc_number");
    refresh_cc(cc_ch_field_,  "cc_channel");
}

void EnvelopeFollowerEditor::update_mode_ui()
{
    const int  out_mode = out_box_.getSelectedItemIndex();
    const bool show_cc  = (out_mode == 0 || out_mode == 2);
    cc_num_field_.setEnabled(show_cc); cc_num_field_.setAlpha(show_cc ? 1.0f : 0.4f);
    cc_num_lbl_  .setAlpha(show_cc ? 1.0f : 0.4f);
    cc_ch_field_ .setEnabled(show_cc); cc_ch_field_ .setAlpha(show_cc ? 1.0f : 0.4f);
    cc_ch_lbl_   .setAlpha(show_cc ? 1.0f : 0.4f);
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void EnvelopeFollowerEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    // All combos in one top row; OUTPUT right-justified
    detector_box_.setBounds(kPadX,               kComboY, kComboW,       kComboH);
    shape_box_   .setBounds(kPadX + kComboW + 8, kComboY, kComboW + 20,  kComboH);
    out_box_     .setBounds(w - kPadX - 160,     kComboY, 160,           kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 36; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY,               kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 72,      kKnobLabelH);
    };
    place(attack_knob_,  attack_lbl_,  0);
    place(release_knob_, release_lbl_, 1);
    place(gain_knob_,    gain_lbl_,    2);
    place(depth_knob_,  depth_lbl_,  3);

    auto place_cc = [&](Label& field, Label& lbl, int col) {
        const int fx = kPadX + col * colw + colw / 2 - 27;
        const int fy = kKnobY + (kKnobSize - 28) / 2;
        field.setBounds(fx, fy, 54, 28);
        lbl.setBounds(kPadX + col * colw + colw / 2 - 36,
                      kKnobY + kKnobSize + 1, 72, kKnobLabelH);
    };
    place_cc(cc_num_field_, cc_num_lbl_, 4);
    place_cc(cc_ch_field_,  cc_ch_lbl_,  5);

    // (out_box_ moved to top row above)
}

// ── Painting ───────────────────────────────────────────────────────────────────

void EnvelopeFollowerEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    draw_strip_chart(g, Rectangle<int>(0, kDispY, kWidth, kDispH));

    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::envelope-follower", kPadX, kHeight - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Strip chart ────────────────────────────────────────────────────────────────
// Two lines:
//   accent colour  (semi-transparent fill) = raw envelope level
//   text_primary   (solid line)            = shaped CV output
// Both are unipolar [0,1].

void EnvelopeFollowerEditor::draw_strip_chart(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax = float(area.getX());
    const float ay = float(area.getY());
    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());

    // Gridline at 0.5
    g.setColour(Colour(laf_.border()).withAlpha(0.4f));
    g.drawLine(ax, ay + h * 0.5f, ax + w, ay + h * 0.5f, 0.5f);

    auto sig_y = [&](float v) { return ay + (1.0f - v) * h; };

    auto build_path = [&](const std::vector<float>& trail) -> Path {
        const int n = trail_count_;
        const int ds = int(w) - n;
        Path p;
        for (int px = 0; px < int(w); ++px) {
            float v = 0.0f;
            if (px >= ds) {
                const int age = px - ds;
                const int si  = (trail_write_ - n + age + kTrailSize) % kTrailSize;
                v = trail[si];
            }
            const float cy = sig_y(v);
            const float cx = ax + float(px);
            if (px == 0) p.startNewSubPath(cx, cy);
            else         p.lineTo(cx, cy);
        }
        return p;
    };

    if (trail_count_ >= 2) {
        // ── Envelope line (input level) -- faint accent, dashed feel ────────
        Path env_curve = build_path(env_trail_);
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.30f));
        g.strokePath(env_curve, PathStrokeType(1.0f, PathStrokeType::curved,
                                               PathStrokeType::rounded));

        // ── CV output line -- full accent with fill, same as other plots ────
        Path cv_curve = build_path(cv_trail_);
        Path cv_fill  = cv_curve;
        cv_fill.lineTo(ax + w, sig_y(0.0f));
        cv_fill.lineTo(ax,     sig_y(0.0f));
        cv_fill.closeSubPath();
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.15f));
        g.fillPath(cv_fill);
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
        g.strokePath(cv_curve, PathStrokeType(1.8f, PathStrokeType::curved,
                                              PathStrokeType::rounded));

        // Head dot on the CV line
        const int   newest  = (trail_write_ - 1 + kTrailSize) % kTrailSize;
        const float cv_head = sig_y(cv_trail_[newest]);
        g.setColour(Colour(laf_.accent_colour()));
        g.fillEllipse(ax + w - 3.0f, cv_head - 3.0f, 6.0f, 6.0f);

        // Legend
        g.setFont(Font(7.5f));
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.35f));
        g.drawText("envelope", int(ax + w) - 90, int(ay) + 2, 86, 9,
                   Justification::centredRight);
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.75f));
        g.drawText("CV out",   int(ax + w) - 90, int(ay) + 12, 86, 9,
                   Justification::centredRight);
    } else {
        g.setFont(Font(9.0f));
        g.setColour(Colour(laf_.text_muted()));
        g.drawText("Connect a signal to the Input or Sidechain bus",
                   int(ax) + 4, int(ay + h / 2) - 6, int(w) - 8, 13,
                   Justification::centred);
    }

    // Axis labels
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    g.drawText("+1", int(ax) + 2, int(ay) + 2,           20, 9, Justification::centredLeft);
    g.drawText("0.5", int(ax) + 2, int(sig_y(0.5f)) - 4, 20, 9, Justification::centredLeft);
    g.drawText(" 0",  int(ax) + 2, int(ay + h) - 11,     20, 9, Justification::centredLeft);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
    g.restoreState();
}

} // namespace kaos_engine
