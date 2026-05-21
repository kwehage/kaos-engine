#include "gate_editor.h"
#include <cmath>

namespace kaos_engine {
using namespace juce;

// ── Algorithm descriptions ─────────────────────────────────────────────────────
static const struct { const char* label; const char* tip; } kAlgoInfo[3] = {
    {
        "Gate -- binary open/close. Hold prevents re-triggering; "
        "hysteresis prevents chattering near threshold.",
        "NOISE GATE\n\n"
        "Sound: Hard binary cut. Signal above the threshold passes at full level; "
        "below it the gain drops to Range (typically -60 dB or mute). The hold timer "
        "keeps the gate open for a fixed duration after the signal drops, preventing "
        "clicks and chattering on decaying notes. Hysteresis sets a separate close "
        "threshold (threshold - hysteresis dB), so the gate won't close until the "
        "signal falls clearly below the open point.\n\n"
        "Signal flow:\n"
        "  det = highpass(dry, 100 Hz)             prevent bass from holding gate open\n"
        "  level = rms(det_L, det_R)\n"
        "  if level > thr:    target = 0 dB        open\n"
        "  elif level > thr - hys: target = hold_state  hysteresis band\n"
        "  else:              target = range dB    close\n"
        "  if closing: hold_counter counts down before release starts\n"
        "  gain = attack/release smooth(target)"
    },
    {
        "Expander -- ratio-based attenuation below threshold. "
        "Softer than a gate; gradual gain reduction.",
        "EXPANDER\n\n"
        "Sound: Below threshold the signal is attenuated proportionally -- "
        "signals near the threshold lose only a little; signals well below lose more. "
        "This is more transparent and musical than a hard gate: quiet audio is turned "
        "down rather than muted. Useful for reducing background noise without the "
        "unnatural snap of a gate, or for tightening the dynamic range of a bus.\n\n"
        "Signal flow:\n"
        "  level = rms(dry_L, dry_R)\n"
        "  if level >= threshold: target = 0 dB\n"
        "  else: excess = threshold - level\n"
        "        target = max(-excess * (1 - 1/ratio), range)\n"
        "  Ratio 2:1 = gentle reduction; 10:1 = near-gate; 100:1 = hard gate.\n"
        "  Hysteresis still applies to prevent chattering."
    },
    {
        "Ducker -- attenuates when ABOVE threshold. "
        "Reduces main signal when a trigger exceeds the threshold.",
        "DUCKER\n\n"
        "Sound: The opposite of a gate -- the signal is attenuated when it is LOUDER "
        "than the threshold, not quieter. Useful for sidechaining: the music ducks "
        "down when the vocal (or kick drum) exceeds a threshold, then returns to full "
        "level during quiet passages. Can be used on any signal with a clear "
        "loud/quiet contrast.\n\n"
        "Signal flow:\n"
        "  level = rms(dry_L, dry_R)\n"
        "  if level <= threshold: target = 0 dB   (pass through)\n"
        "  else: excess = level - threshold\n"
        "        target = max(-excess * (1 - 1/ratio), range)\n"
        "  Hold prevents the ducker from opening too quickly after a loud event."
    }
};

// ── Construction ───────────────────────────────────────────────────────────────
GateEditor::GateEditor(GatePlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    algo_box_.addItem("Gate",     1);
    algo_box_.addItem("Expander", 2);
    algo_box_.addItem("Ducker",   3);
    algo_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(algo_box_);
    algo_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "algorithm", algo_box_);
    algo_box_.onChange = [this] { update_algo_ui(); };

    // algo_label_ not shown -- description routed to algo_box_ tooltip in update_algo_ui().

    setup_knob(threshold_knob_,  threshold_lbl_,  "THRESHOLD");
    setup_knob(range_knob_,      range_lbl_,      "RANGE");
    setup_knob(ratio_knob_,      ratio_lbl_,      "RATIO");
    setup_knob(attack_knob_,     attack_lbl_,     "ATTACK");
    setup_knob(hold_knob_,       hold_lbl_,       "HOLD");
    setup_knob(release_knob_,    release_lbl_,    "RELEASE");
    setup_knob(hysteresis_knob_, hysteresis_lbl_, "HYSTERESIS");
    setup_knob(output_knob_,     output_lbl_,     "OUTPUT");
    setup_knob(mix_knob_,        mix_lbl_,        "MIX");

    threshold_knob_.setTooltip("Threshold: level at which the gate opens (Gate/Expander) "
        "or the ducker activates. Range -80 to 0 dBFS. "
        "Set just above your noise floor for gating.");
    range_knob_.setTooltip("Range: minimum gain when gate is closed. "
        "0 dB = gate has no effect; -60 dB = near-silence. "
        "A finite range sounds more natural than full muting.");
    ratio_knob_.setTooltip("Ratio: how aggressively the expander or ducker attenuates "
        "below/above threshold. 2:1 = gentle; 10:1 = near-gate character; 100:1 = hard cut. "
        "Inactive in Gate mode (Gate is always binary).");
    attack_knob_.setTooltip("Attack: how quickly the gate opens when signal exceeds threshold. "
        "Very fast (0.1 ms) = maximum transient preservation. "
        "Slower = smoother onset but may clip fast attacks.");
    hold_knob_.setTooltip("Hold: time the gate stays open after signal drops below "
        "the close threshold. Prevents chattering on decaying notes. "
        "Typical: 50-200 ms for drums; longer for sustained sources.");
    release_knob_.setTooltip("Release: how quickly the gate closes after the hold expires. "
        "Fast = tight, abrupt cut. Slow = natural fade; may let noise through longer.");
    hysteresis_knob_.setTooltip("Hysteresis: the close threshold is (threshold - hysteresis) dB. "
        "A signal must fall this far below the open threshold before the gate begins closing. "
        "Prevents chattering when signal hovers near threshold. "
        "Set 3-10 dB for most sources; 0 = no hysteresis (binary threshold).");
    output_knob_.setTooltip("Output: post-gate level trim. -20 to +6 dB.");
    mix_knob_.setTooltip("Mix: 0 = fully dry (bypass), 1 = fully gated. "
        "Parallel gating blends gated and ungated signal for a gentler effect.");

    threshold_att_  = std::make_unique<Attachment>(apvts, "threshold",  threshold_knob_);
    range_att_      = std::make_unique<Attachment>(apvts, "range",      range_knob_);
    ratio_att_      = std::make_unique<Attachment>(apvts, "ratio",      ratio_knob_);
    attack_att_     = std::make_unique<Attachment>(apvts, "attack",     attack_knob_);
    hold_att_       = std::make_unique<Attachment>(apvts, "hold",       hold_knob_);
    release_att_    = std::make_unique<Attachment>(apvts, "release",    release_knob_);
    hysteresis_att_ = std::make_unique<Attachment>(apvts, "hysteresis", hysteresis_knob_);
    output_att_     = std::make_unique<Attachment>(apvts, "output",     output_knob_);
    mix_att_        = std::make_unique<Attachment>(apvts, "mix",        mix_knob_);

    update_algo_ui();
    startTimerHz(30);
}

GateEditor::~GateEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void GateEditor::setup_knob(Slider& k, Label& l, const String& name)
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
void GateEditor::timerCallback()
{
    // Smooth display values: fast attack, slow release for level meter
    const float lvl    = plugin_.get_level_db();
    const float lvl_c  = lvl > level_display_ ? 0.1f : 0.92f;
    level_display_ = lvl_c * level_display_ + (1.0f - lvl_c) * lvl;

    const float gr     = plugin_.get_gr_db();
    const float gr_c   = gr < gr_display_ ? 0.1f : 0.88f;
    gr_display_    = gr_c * gr_display_ + (1.0f - gr_c) * gr;

    repaint(0, kDispY, kWidth, kDispH);
}

void GateEditor::update_algo_ui()
{
    const int idx = algo_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 3) return;
    algo_box_.setTooltip(kAlgoInfo[idx].tip);

    // Ratio only meaningful for Expander / Ducker
    const bool ratio_active = (idx != 0);
    ratio_knob_.setEnabled(ratio_active);
    ratio_knob_.setAlpha (ratio_active ? 1.0f : 0.35f);
    ratio_lbl_ .setAlpha (ratio_active ? 1.0f : 0.35f);

    // Hysteresis only meaningful for Gate / Expander (not Ducker)
    const bool hys_active = (idx != 2);
    hysteresis_knob_.setEnabled(hys_active);
    hysteresis_knob_.setAlpha(hys_active ? 1.0f : 0.35f);
    hysteresis_lbl_ .setAlpha(hys_active ? 1.0f : 0.35f);
}

// ── Layout ─────────────────────────────────────────────────────────────────────
void GateEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    algo_box_.setBounds(kPadX, kComboY, kComboW, kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 34; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY,                        kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 68,    kKnobLabelH);
    };
    place(threshold_knob_,  threshold_lbl_,  0);
    place(range_knob_,      range_lbl_,      1);
    place(ratio_knob_,      ratio_lbl_,      2);
    place(attack_knob_,     attack_lbl_,     3);
    place(hold_knob_,       hold_lbl_,       4);
    place(release_knob_,    release_lbl_,    5);
    place(hysteresis_knob_, hysteresis_lbl_, 6);
    place(output_knob_,     output_lbl_,     7);
    place(mix_knob_,        mix_lbl_,        8);
}

// ── Painting ───────────────────────────────────────────────────────────────────
void GateEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    const int tw = kWidth - kMeterW;   // transfer / activity display width
    draw_transfer(g, { 0,          kDispY, tw,       kDispH });
    draw_gr_meter(g, { tw,         kDispY, kMeterW,  kDispH });

    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::gate", kPadX, kHeight - kFooterH - 4,
               200, kFooterH, Justification::centredLeft);
}

// ── Transfer function display ──────────────────────────────────────────────────
void GateEditor::draw_transfer(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    // Tint background based on current gate state
    const GateDisplayState st = plugin_.get_disp_state();
    Colour bg_tint;
    switch (st) {
        case GateDisplayState::Open:    bg_tint = Colour(0xff1a2e1a); break;  // dark green
        case GateDisplayState::Hold:    bg_tint = Colour(0xff2e2a12); break;  // dark amber
        case GateDisplayState::Release: bg_tint = Colour(0xff2a1a10); break;  // dark orange
        default:                        bg_tint = Colour(laf_.surface()); break;
    }
    g.setColour(bg_tint);
    g.fillRect(area);

    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());
    const float ax = float(area.getX());
    const float ay = float(area.getY());

    static constexpr float kDbLo = -80.0f, kDbHi = 0.0f;
    auto dbx = [&](float db) { return ax + (db - kDbLo) / (kDbHi - kDbLo) * w; };
    auto dby = [&](float db) { return ay + (kDbHi - db) / (kDbHi - kDbLo) * h; };

    // Grid
    for (float db : { -60.0f, -40.0f, -20.0f }) {
        g.setColour(Colour(laf_.border()).withAlpha(0.35f));
        g.drawLine(dbx(db), ay, dbx(db), ay + h, 0.5f);
        g.drawLine(ax, dby(db), ax + w, dby(db), 0.5f);
    }
    g.setColour(Colour(laf_.border()).withAlpha(0.7f));
    g.drawLine(dbx(0), ay, dbx(0), ay + h, 1.0f);
    g.drawLine(ax, dby(0), ax + w, dby(0), 1.0f);

    // Axis labels
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.55f));
    for (float db : { -60.0f, -40.0f, -20.0f })
        g.drawText(String(int(db)), int(dbx(db)) - 14, int(ay) + 2, 28, 9, Justification::centred);
    g.drawText("IN",  int(ax + w) - 16, int(ay + h) - 11, 16, 9, Justification::centredLeft);
    g.drawText("OUT", int(ax) + 2,      int(ay) + 2,       20, 9, Justification::centredLeft);

    // Unity diagonal
    g.setColour(Colour(laf_.border()).withAlpha(0.4f));
    g.drawLine(dbx(kDbLo), dby(kDbLo), dbx(kDbHi), dby(kDbHi), 1.0f);

    // Threshold and close-threshold markers
    auto& ap               = plugin_.get_apvts();
    const float thr        = ap.getRawParameterValue("threshold")->load();
    const float hys        = ap.getRawParameterValue("hysteresis")->load();
    const float thr_close  = thr - hys;

    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    g.drawLine(dbx(thr),       ay, dbx(thr),       ay + h, 1.0f);
    if (hys > 0.5f) {
        g.setColour(Colour(laf_.text_muted()).withAlpha(0.3f));
        g.drawLine(dbx(thr_close), ay, dbx(thr_close), ay + h, 1.0f);
    }

    // Transfer curve
    const float range = ap.getRawParameterValue("range")->load();
    const float ratio = ap.getRawParameterValue("ratio")->load();
    const int   algo  = juce::roundToInt(ap.getRawParameterValue("algorithm")->load());

    auto gc = [&](float ldb) -> float {
        switch (algo) {
            case 0: return (ldb >= thr) ? 0.0f : range;          // Gate
            case 1: {                                              // Expander
                if (ldb >= thr) return 0.0f;
                return std::max(-(thr - ldb) * (1.0f - 1.0f / ratio), range);
            }
            case 2: {                                              // Ducker
                if (ldb <= thr) return 0.0f;
                return std::max(-(ldb - thr) * (1.0f - 1.0f / ratio), range);
            }
            default: return 0.0f;
        }
    };

    Path curve;
    for (int px = 0; px < int(w); ++px) {
        const float in_db  = kDbLo + float(px) / w * (kDbHi - kDbLo);
        const float out_db = in_db + gc(in_db);
        const float cx_    = ax + float(px);
        const float cy_    = jlimit(ay, ay + h, dby(out_db));
        if (px == 0) curve.startNewSubPath(cx_, cy_);
        else         curve.lineTo(cx_, cy_);
    }

    Path filled = curve;
    filled.lineTo(ax + w, dby(kDbLo)); filled.lineTo(ax, dby(kDbLo)); filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.12f));
    g.fillPath(filled);
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
    g.strokePath(curve, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // Input level indicator (vertical line)
    const float lvl_x = jlimit(ax, ax + w, dbx(level_display_));
    g.setColour(Colour(0xffffee88).withAlpha(0.7f));
    g.drawLine(lvl_x, ay + 2.0f, lvl_x, ay + h - 2.0f, 1.5f);

    // Gate state label in centre
    const char* state_str[] = { "CLOSED", "RELEASE", "HOLD", "OPEN" };
    const Colour state_col[] = {
        Colour(0xffcc4444), Colour(0xffcc8844),
        Colour(0xffccaa22), Colour(0xff44cc44)
    };
    const int si = int(st);
    g.setFont(Font(11.0f, Font::bold));
    g.setColour(state_col[si].withAlpha(0.85f));
    g.drawText(state_str[si], area.reduced(4), Justification::centred);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
    g.restoreState();
}

// ── GR Meter ──────────────────────────────────────────────────────────────────
void GateEditor::draw_gr_meter(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);
    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax  = float(area.getX());
    const float ay  = float(area.getY());
    const float w   = float(area.getWidth());
    const float h   = float(area.getHeight()) - 14.0f;

    static constexpr float kGrMax = 60.0f;
    const float gr_abs  = std::min(kGrMax, std::abs(gr_display_));
    const float fill_h  = gr_abs / kGrMax * h;

    const float norm = gr_abs / kGrMax;
    const Colour col = norm < 0.5f
        ? Colour::fromHSV(0.33f - norm * 0.33f, 0.85f, 0.8f, 1.0f)
        : Colour::fromHSV(0.0f, 0.85f, 0.8f, 1.0f);

    g.setColour(col.withAlpha(0.85f));
    g.fillRect(ax + 2.0f, ay + 1.0f, w - 4.0f, fill_h);

    g.setFont(Font(7.0f));
    for (float mark : { 0.0f, -20.0f, -40.0f, -60.0f }) {
        const float ym = ay + std::abs(mark) / kGrMax * h;
        g.setColour(Colour(laf_.border()).withAlpha(0.8f));
        g.drawLine(ax + 2.0f, ym, ax + w - 2.0f, ym, 0.5f);
        if (mark != 0.0f) {
            g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
            g.drawText(String(int(mark)), int(ax), int(ym) - 4, int(w), 8, Justification::centred);
        }
    }

    g.setFont(Font(8.0f, Font::bold));
    g.setColour(Colour(laf_.text_muted()));
    g.drawText("GR", int(ax), int(ay + h + 2), int(w), 11, Justification::centred);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
    g.restoreState();
}

} // namespace kaos_engine
