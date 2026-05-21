#include "lfo_editor.h"
#include "../../src/framework/lfo/lfo_processor.h"
#include <cmath>

namespace kaos_engine {
using namespace juce;

// ── Waveform descriptions ──────────────────────────────────────────────────────
static const struct { const char* label; const char* tip; } kWaveInfo[11] = {
    { "Sine -- smooth oscillation, pure fundamental.",
      "SINE\n\nSmooth sinusoidal oscillation. No harmonics — a single pure tone "
      "of the LFO frequency. The most musical waveform for slow modulation of "
      "filter cutoff, vibrato, or tremolo.\n\nSHAPE: no effect." },
    { "Triangle -- linear ramp up and down, softer than square.",
      "TRIANGLE\n\nLinear rise and fall. Odd harmonics only, much gentler than "
      "square. Good for vibrato and rhythmic sweeps.\n\nSHAPE: no effect." },
    { "Square -- hard on/off pulses. Gating and rhythmic effects.",
      "SQUARE\n\nAbrupt toggle between +1 and -1 at 50% duty. Rich odd harmonics. "
      "Good for hard tremolo and key-sync gating. For variable duty use Pulse.\n\n"
      "SHAPE: no effect (use Pulse for variable duty cycle)." },
    { "Sawtooth -- rising ramp. Classic filter sweep.",
      "SAWTOOTH\n\nRises linearly from -1 to +1 then resets. All harmonics. "
      "The classic opening filter sweep shape.\n\nSHAPE: no effect." },
    { "Rev. Saw -- falling ramp. Reverse filter sweep.",
      "REVERSE SAW\n\nFalls from +1 to -1 then resets. Mirror of sawtooth — "
      "produces a closing sweep.\n\nSHAPE: no effect." },
    { "Half Sine -- two bumps per cycle. Bouncing-ball feel.",
      "HALF SINE\n\nabs(sin(2\xcf\x80t)) — always \xe2\x89\xa5 0, two identical bumps per cycle. "
      "Output is naturally unipolar [0,+1]; use Offset to shift it.\n\n"
      "Common on analogue hardware for soft pulsing and bouncing-ball modulation.\n\n"
      "SHAPE: no effect." },
    { "Exp Ramp -- slow start, fast finish. Capacitor charge curve.",
      "EXPONENTIAL RAMP\n\nRises from -1 to +1 following (e^kp - 1)/(e^k - 1). "
      "Mimics an analogue capacitor charging curve — slow at first, then accelerates "
      "toward the end. Mirror image of Log Ramp.\n\n"
      "SHAPE: curve steepness. 0 = near-linear, 1 = very steep." },
    { "Log Ramp -- fast start, slow finish. Capacitor discharge.",
      "LOGARITHMIC RAMP\n\nRises from -1 to +1 following ln(1+p(e^k-1))/k. "
      "Mimics capacitor discharge — rises quickly then levels off. "
      "Mirror image of Exp Ramp.\n\n"
      "SHAPE: curve steepness. 0 = near-linear, 1 = very steep." },
    { "Pulse -- square with variable duty cycle.",
      "PULSE\n\nSame as Square but the high/low split point is set by SHAPE. "
      "Narrow pulses (low SHAPE) give brief spikes; wide pulses (high SHAPE) "
      "spend most of the cycle high. At SHAPE=0.5 identical to Square.\n\n"
      "SHAPE: duty cycle. 0 = very narrow, 0.5 = square, 1 = very wide." },
    { "Staircase Up -- quantised rising ramp.",
      "STAIRCASE UP\n\nA sawtooth quantised to discrete steps, rising in equal "
      "increments from -1 to +1. At 2 steps: square wave. At 16 steps: nearly "
      "indistinguishable from a sawtooth.\n\n"
      "SHAPE: step count. 0 = 2 steps, 0.5 \xe2\x89\x88 9 steps, 1 = 16 steps." },
    { "Staircase Down -- quantised falling ramp.",
      "STAIRCASE DOWN\n\nMirror of Staircase Up. Falls in equal steps from +1 to -1. "
      "At 2 steps: square wave. At 16 steps: nearly indistinguishable from Rev. Saw.\n\n"
      "SHAPE: step count. 0 = 2 steps, 0.5 \xe2\x89\x88 9 steps, 1 = 16 steps." },
};

// ── Construction ───────────────────────────────────────────────────────────────

LfoEditor::LfoEditor(LfoPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // Waveform selector
    wave_box_.addItem("Sine",      1);
    wave_box_.addItem("Triangle",  2);
    wave_box_.addItem("Square",    3);
    wave_box_.addItem("Sawtooth",  4);
    wave_box_.addItem("Rev. Saw",  5);
    wave_box_.addItem("Half Sine", 6);
    wave_box_.addItem("Exp Ramp",  7);
    wave_box_.addItem("Log Ramp",  8);
    wave_box_.addItem("Pulse",     9);
    wave_box_.addItem("Staircase Up",   10);
    wave_box_.addItem("Staircase Down", 11);
    wave_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(wave_box_);
    wave_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "waveform", wave_box_);
    wave_box_.onChange = [this] { update_wave_label(); };

    // wave_label_ not shown -- description routed to wave_box_ tooltip in update_wave_label().

    // Sync combo
    sync_box_.addItem("Free",  1);
    sync_box_.addItem("Sync",  2);
    sync_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(sync_box_);
    sync_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "sync", sync_box_);
    sync_box_.onChange = [this] { update_mode_ui(); };

    // sync_label_ not shown -- sync combo items "Free"/"Sync" are self-explanatory.

    // Division combo
    for (const char* s : {"Whole", "Half", "Dotted 1/4", "1/4",
                           "Dotted 1/8", "1/8", "1/8 Triplet", "1/16"})
        div_box_.addItem(s, div_box_.getNumItems() + 1);
    div_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(div_box_);
    div_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "sync_div", div_box_);

    // Output mode combo
    mode_box_.addItem("MIDI CC",       1);
    mode_box_.addItem("Audio CV",      2);
    mode_box_.addItem("MIDI CC + CV",  3);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "output_mode", mode_box_);
    mode_box_.onChange = [this] { update_mode_ui(); };

    // Trigger mode combo
    trigger_box_.addItem("Free",           1);
    trigger_box_.addItem("Note Retrigger", 2);
    trigger_box_.addItem("Transport",      3);
    trigger_box_.addItem("Sidechain",      4);
    trigger_box_.setScrollWheelEnabled(false);
    trigger_box_.setTooltip(
        "Free: LFO runs continuously.\n"
        "Note Retrigger: MIDI note-on resets phase and starts the LFO; note-off holds.\n"
        "Transport: play resets and starts; stop holds.\n"
        "Sidechain: rising edge on Trigger In bus (>0.6) resets and starts; "
        "falling edge (<0.4) holds. Connect a Gate CV bus here.");
    addAndMakeVisible(trigger_box_);
    trigger_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "trigger_mode", trigger_box_);

    // Knobs
    setup_knob(rate_knob_,   rate_lbl_,   "RATE");
    setup_knob(depth_knob_,  depth_lbl_,  "DEPTH");
    setup_knob(shape_knob_,  shape_lbl_,  "SHAPE");
    setup_knob(phase_knob_,  phase_lbl_,  "PHASE");
    setup_knob(offset_knob_, offset_lbl_, "OFFSET");
    rate_knob_  .setTooltip("Rate: LFO frequency in Hz (Sync off). Range 0.01-100 Hz.");
    depth_knob_ .setTooltip("Depth: amplitude scale. 0 = off, 1 = full range. "
                            "out = waveform * depth + offset");
    shape_knob_ .setTooltip("Shape: waveform-specific modifier.\n"
                            "Pulse: duty cycle (0=narrow, 0.5=square, 1=wide)\n"
                            "Staircase: step count (0=2 steps, 1=16 steps)\n"
                            "Exp/Log Ramp: curve steepness (0=linear, 1=steep)\n"
                            "All others: no effect");
    phase_knob_ .setTooltip("Phase: starting phase offset. "
                            "0 = cycle start; 0.5 = 180\xc2\xb0 into cycle.");
    offset_knob_.setTooltip("Offset: DC shift. 0 = bipolar (-1..+1). "
                            "+0.5 with depth=0.5 gives unipolar 0..+1. "
                            "Output clamped to [-1, 1].");

    rate_att_   = std::make_unique<Attachment>(apvts, "rate",      rate_knob_);
    depth_att_  = std::make_unique<Attachment>(apvts, "depth",     depth_knob_);
    shape_att_  = std::make_unique<Attachment>(apvts, "shape",     shape_knob_);
    phase_att_  = std::make_unique<Attachment>(apvts, "phase_off", phase_knob_);
    offset_att_ = std::make_unique<Attachment>(apvts, "offset",    offset_knob_);

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
        auto* p = apvts.getParameter(param_id);
        if (p) field.setText(String(juce::roundToInt(p->convertFrom0to1(p->getValue()))),
                              dontSendNotification);
        field.onTextChange = [&field, &apvts, param_id, lo, hi] {
            const int v = juce::jlimit(lo, hi,
                              juce::roundToInt(field.getText().getFloatValue()));
            field.setText(String(v), dontSendNotification);
            if (auto* p2 = apvts.getParameter(param_id))
                p2->setValueNotifyingHost(p2->convertTo0to1(float(v)));
        };
    };
    setup_cc(cc_num_field_, cc_num_lbl_, "CC NUM", "cc_number",  0, 127);
    setup_cc(cc_ch_field_,  cc_ch_lbl_,  "CC CH",  "cc_channel", 1, 16);
    cc_num_field_.setTooltip("CC Number: MIDI CC number (0-127). "
                             "Common: 74=filter, 71=resonance, 7=volume.");
    cc_ch_field_ .setTooltip("CC Channel: MIDI channel for CC output (1-16).");

    update_wave_label();
    update_mode_ui();
    startTimerHz(30);
}

LfoEditor::~LfoEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void LfoEditor::setup_knob(Slider& k, Label& l, const String& name)
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

void LfoEditor::timerCallback()
{
    phase_display_ = plugin_.get_phase();
    repaint(0, kDispY, kWidth, kDispH);
    update_mode_ui();
    // Refresh CC text fields from parameter state (skip if user is editing)
    auto& apvts = plugin_.get_apvts();
    auto refresh_cc = [&](Label& field, const char* param_id) {
        if (!field.isBeingEdited())
            if (auto* p = apvts.getParameter(param_id))
                field.setText(String(juce::roundToInt(p->convertFrom0to1(p->getValue()))),
                              dontSendNotification);
    };
    refresh_cc(cc_num_field_, "cc_number");
    refresh_cc(cc_ch_field_,  "cc_channel");
}

void LfoEditor::update_wave_label()
{
    const int idx = wave_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 11) return;
    wave_box_.setTooltip(kWaveInfo[idx].tip);
}

void LfoEditor::update_mode_ui()
{
    const int mode   = mode_box_.getSelectedItemIndex();   // 0=MIDI, 1=CV, 2=both
    const bool synced = sync_box_.getSelectedItemIndex() == 1;
    const bool show_cc = (mode == 0 || mode == 2);

    rate_knob_.setEnabled(!synced);
    rate_knob_.setAlpha(synced ? 0.4f : 1.0f);
    div_box_.setEnabled(synced);
    div_box_.setAlpha(synced ? 1.0f : 0.4f);

    cc_num_field_.setEnabled(show_cc); cc_num_field_.setAlpha(show_cc ? 1.0f : 0.4f);
    cc_num_lbl_  .setAlpha(show_cc ? 1.0f : 0.4f);
    cc_ch_field_ .setEnabled(show_cc); cc_ch_field_ .setAlpha(show_cc ? 1.0f : 0.4f);
    cc_ch_lbl_   .setAlpha(show_cc ? 1.0f : 0.4f);
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void LfoEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    // All combos in one top row; OUTPUT and TRIGGER right-justified
    wave_box_   .setBounds(kPadX,       kComboY, kComboW, kComboH);
    sync_box_   .setBounds(kPadX + kComboW + 8,  kComboY,  70, kComboH);
    div_box_    .setBounds(kPadX + kComboW + 86,  kComboY, 110, kComboH);
    trigger_box_.setBounds(w - kPadX - 150,       kComboY, 150, kComboH);
    mode_box_   .setBounds(w - kPadX - 308,       kComboY, 150, kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 36; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY,                    kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 72, kKnobLabelH);
    };
    place(rate_knob_,   rate_lbl_,   0);
    place(depth_knob_,  depth_lbl_,  1);
    place(shape_knob_,  shape_lbl_,  2);
    place(phase_knob_,  phase_lbl_,  3);
    place(offset_knob_, offset_lbl_, 4);

    // CC text fields -- centered in the same column space as a knob
    auto place_cc = [&](Label& field, Label& lbl, int col) {
        const int fx = kPadX + col * colw + colw / 2 - 27;
        const int fy = kKnobY + (kKnobSize - 28) / 2;
        field.setBounds(fx, fy, 54, 28);
        lbl.setBounds(kPadX + col * colw + colw / 2 - 36,
                      kKnobY + kKnobSize + 1, 72, kKnobLabelH);
    };
    place_cc(cc_num_field_, cc_num_lbl_, 5);
    place_cc(cc_ch_field_,  cc_ch_lbl_,  6);

    // (ctrl row removed -- all combos are now in the top row)
}

// ── Painting ───────────────────────────────────────────────────────────────────

void LfoEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    draw_waveform_display(g, Rectangle<int>(0, kDispY, kWidth, kDispH));

    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::lfo", kPadX, kHeight - kFooterH - 4,
               200, kFooterH, Justification::centredLeft);
}

// ── Waveform display ───────────────────────────────────────────────────────────

void LfoEditor::draw_waveform_display(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax = float(area.getX());
    const float ay = float(area.getY());
    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());

    auto db_y = [&](float v) { return ay + (1.0f - v) * 0.5f * h; };

    // Centre line
    g.setColour(Colour(laf_.border()).withAlpha(0.5f));
    g.drawLine(ax, ay + h * 0.5f, ax + w, ay + h * 0.5f, 0.5f);

    // Get current parameters from APVTS
    auto& ap = plugin_.get_apvts();
    const int   wf_idx    = wave_box_.getSelectedItemIndex();
    const float depth     = ap.getRawParameterValue("depth")    ->load();
    const float shape     = ap.getRawParameterValue("shape")    ->load();
    const float offset    = ap.getRawParameterValue("offset")   ->load();
    const float phase_off = ap.getRawParameterValue("phase_off")->load();

    LfoProcessor prev;
    prev.prepare(double(int(w)), 1);
    prev.set_waveform    (static_cast<LfoWaveform>(wf_idx));
    prev.set_rate_hz     (1.0f);
    prev.set_running     (true);
    prev.set_depth       (depth);
    prev.set_shape       (shape);
    prev.set_offset      (offset);
    prev.set_phase_offset(phase_off);   // must precede reset() so phase_ = phase_off_
    prev.reset();

    // Map signal value to pixel Y using a [-1.1, +1.1] display range so that
    // full-scale signals (±1) sit ~5% inside the border — enough for the stroke.
    static constexpr float kYRange = 1.1f;
    auto sig_y = [&](float v) { return ay + (1.0f - v / kYRange) * 0.5f * h; };

    const int n_pts = int(w);
    Path curve;
    for (int px = 0; px < n_pts; ++px) {
        const float v  = prev.next_sample();
        const float cy = sig_y(v);
        const float cx = ax + float(px);
        if (px == 0) curve.startNewSubPath(cx, cy);
        else         curve.lineTo(cx, cy);
    }

    // Fill between curve and the zero line (v=0 → centre of display)
    Path filled = curve;
    const float base_y = sig_y(0.0f);   // pixel Y of v=0
    filled.lineTo(ax + w, base_y);
    filled.lineTo(ax,     base_y);
    filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.15f));
    g.fillPath(filled);

    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
    g.strokePath(curve, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    const float phase_x = ax + phase_display_ * w;
    g.setColour(Colour(laf_.text_primary()).withAlpha(0.7f));
    g.drawLine(phase_x, ay + 4.0f, phase_x, ay + h - 4.0f, 1.5f);

    // Axis labels
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    g.drawText("+1", int(ax) + 2, int(ay) + 2,             20, 9, Justification::centredLeft);
    g.drawText(" 0", int(ax) + 2, int(sig_y(0.0f)) - 4,    20, 9, Justification::centredLeft);
    g.drawText("-1", int(ax) + 2, int(ay + h) - 11,         20, 9, Justification::centredLeft);
    g.drawText("1 cycle", int(ax + w) - 52, int(ay + h) - 11, 48, 9, Justification::centredRight);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);

    g.restoreState();
}

} // namespace kaos_engine
