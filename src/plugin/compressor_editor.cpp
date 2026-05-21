#include "compressor_editor.h"
#include <cmath>

namespace kaos_engine {
using namespace juce;

// ── Algorithm descriptions ─────────────────────────────────────────────────────
static const struct { const char* label; const char* tip; } kAlgoInfo[3] = {
    {
        "VCA -- feed-forward, fixed attack/release. Clean and precise. "
        "Hear the compression working exactly as set.",
        "VCA COMPRESSOR\n\n"
        "Sound: Transparent, precise, modern. The standard compressor topology. "
        "Attack and release behave exactly as set regardless of signal content. "
        "Best for: mixing bus glue, vocals, any situation where predictable control matters.\n\n"
        "Signal flow:\n"
        "  level = rms(dry_L, dry_R)              feed-forward detection\n"
        "  gc = gain_computer(level, T, ratio, W) hard or soft knee\n"
        "  gr = attack/release_smooth(gc)          program-independent\n"
        "  out = dry * 10^((gr + makeup) / 20)"
    },
    {
        "Optical -- feed-forward, level-dependent release (LA-2A style). "
        "Fast peaks release quickly; sustained compression releases slowly.",
        "OPTICAL COMPRESSOR\n\n"
        "Sound: Warm, musical, forgiving. Modelled on the LA-2A optical attenuator. "
        "The release time lengthens when the compressor has been working hard -- deep "
        "or sustained gain reduction produces a slow, tape-like release tail; brief "
        "transient peaks pop back quickly. This program-dependent behaviour makes it "
        "nearly impossible to set badly.\n\n"
        "Signal flow:\n"
        "  level = rms(dry_L, dry_R)\n"
        "  gc = gain_computer(level, T, ratio, W)\n"
        "  if compressing: gr += (1-alpha_a) * (gc - gr)   attack as set\n"
        "  else:           blend alpha_r between fast and slow based on |gr|/12 dB\n"
        "                  (light GR -> fast release; deep GR -> slow tail)"
    },
    {
        "FET -- feed-back topology (1176 style). "
        "Compressed output drives the detector. Faster at higher ratios.",
        "FET COMPRESSOR\n\n"
        "Sound: Aggressive, colored, glued. Modelled on the 1176 FET compressor. "
        "The detector sees the already-compressed output rather than the input -- "
        "this self-modulating feedback produces nonlinear character: the more the "
        "signal is compressed, the less the detector responds, creating a natural "
        "saturation ceiling. Attack gets faster at higher ratios (matching 1176 behaviour). "
        "Best for: drums, bass, anything that needs presence and weight.\n\n"
        "Signal flow:\n"
        "  level = rms(out_prev_L, out_prev_R)    feed-BACK from previous output\n"
        "  gc = gain_computer(level, T, ratio, W)\n"
        "  attack_eff = attack / (ratio * 0.5)    faster attack at higher ratios\n"
        "  gr = attack_eff/release_smooth(gc)\n"
        "  out = dry * 10^((gr + makeup) / 20)"
    }
};

// ── Construction ───────────────────────────────────────────────────────────────
CompressorEditor::CompressorEditor(CompressorPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // Algorithm selector
    algo_box_.addItem("VCA",     1);
    algo_box_.addItem("Optical", 2);
    algo_box_.addItem("FET",     3);
    algo_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(algo_box_);
    algo_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "algorithm", algo_box_);
    algo_box_.onChange = [this] { update_algo_label(); };

    // algo_label_ not shown -- description routed to algo_box_ tooltip in update_algo_label().

    // Knobs
    setup_knob(threshold_knob_, threshold_lbl_, "THRESHOLD");
    setup_knob(ratio_knob_,     ratio_lbl_,     "RATIO");
    setup_knob(knee_knob_,      knee_lbl_,      "KNEE");
    setup_knob(attack_knob_,    attack_lbl_,    "ATTACK");
    setup_knob(release_knob_,   release_lbl_,   "RELEASE");
    setup_knob(makeup_knob_,    makeup_lbl_,    "MAKEUP");
    setup_knob(output_knob_,    output_lbl_,    "OUTPUT");
    setup_knob(mix_knob_,       mix_lbl_,       "MIX");

    threshold_knob_.setTooltip("Threshold (T): level above which compression starts. "
        "Signals below T pass unchanged; above T the ratio is applied. Range -60 to 0 dBFS.");
    ratio_knob_.setTooltip("Ratio: compression steepness above threshold. "
        "2:1 = gentle; 4:1 = moderate; 10:1 = heavy; 20:1 = near-limiting. "
        "Affects FET attack speed: higher ratio → faster attack.");
    knee_knob_.setTooltip("Knee width: 0 = hard knee (sharp corner at threshold). "
        "Higher values create a soft-knee parabolic transition that starts reducing gain "
        "before the threshold and reaches full ratio above it. Range 0-20 dB.");
    attack_knob_.setTooltip("Attack: time constant for gain reduction to build up after "
        "signal exceeds threshold (1-tau, 63% of target). Shorter = faster, more transparent; "
        "longer = lets transients through.");
    release_knob_.setTooltip("Release: time constant for gain reduction to decay after "
        "signal drops below threshold. Optical: this sets the fast component; the slow "
        "tail extends to 10x this value when compression is deep.");
    makeup_knob_.setTooltip("Makeup gain: compensates for level lost to compression. "
        "Applied after the gain reduction but before Output and Mix.");
    output_knob_.setTooltip("Output: final level trim after makeup gain and wet/dry mix. "
        "-20 to +6 dB.");
    mix_knob_.setTooltip("Mix (parallel compression): 0 = fully dry (bypass), "
        "1 = fully wet (full compression). Blend preserves transients while adding density.");

    threshold_att_ = std::make_unique<Attachment>(apvts, "threshold", threshold_knob_);
    ratio_att_     = std::make_unique<Attachment>(apvts, "ratio",     ratio_knob_);
    knee_att_      = std::make_unique<Attachment>(apvts, "knee",      knee_knob_);
    attack_att_    = std::make_unique<Attachment>(apvts, "attack",    attack_knob_);
    release_att_   = std::make_unique<Attachment>(apvts, "release",   release_knob_);
    makeup_att_    = std::make_unique<Attachment>(apvts, "makeup",    makeup_knob_);
    output_att_    = std::make_unique<Attachment>(apvts, "output",    output_knob_);
    mix_att_       = std::make_unique<Attachment>(apvts, "mix",       mix_knob_);

    update_algo_label();
    startTimerHz(30);
}

CompressorEditor::~CompressorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void CompressorEditor::setup_knob(Slider& k, Label& l, const String& name)
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
void CompressorEditor::timerCallback()
{
    // Smooth the GR meter value toward the current reading (fast attack, slow release)
    const float target = plugin_.get_gain_reduction_db();
    const float coeff  = target < gr_display_ ? 0.1f : 0.92f;
    gr_display_ = coeff * gr_display_ + (1.0f - coeff) * target;
    repaint(kWidth - kMeterW, kDisplayY, kMeterW, kDisplayH);
    // Also repaint the transfer curve when parameters change (30Hz is fine)
    repaint(0, kDisplayY, kWidth - kMeterW, kDisplayH);
}

void CompressorEditor::update_algo_label()
{
    const int idx = algo_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 3) return;
    algo_box_.setTooltip(kAlgoInfo[idx].tip);
}

// ── Layout ─────────────────────────────────────────────────────────────────────
void CompressorEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    algo_box_.setBounds(kPadX, kComboY, kComboW, kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 36; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY,                         kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 72,     kKnobLabelH);
    };
    place(threshold_knob_, threshold_lbl_, 0);
    place(ratio_knob_,     ratio_lbl_,     1);
    place(knee_knob_,      knee_lbl_,      2);
    place(attack_knob_,    attack_lbl_,    3);
    place(release_knob_,   release_lbl_,   4);
    place(makeup_knob_,    makeup_lbl_,    5);
    place(output_knob_,    output_lbl_,    6);
    place(mix_knob_,       mix_lbl_,       7);
}

// ── Painting ───────────────────────────────────────────────────────────────────
void CompressorEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    // Display area: transfer function on left, GR meter on right
    const Rectangle<int> curve_area(0, kDisplayY, kWidth - kMeterW, kDisplayH);
    const Rectangle<int> meter_area(kWidth - kMeterW, kDisplayY, kMeterW, kDisplayH);

    draw_transfer_curve(g, curve_area);
    draw_gr_meter      (g, meter_area);

    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::compressor", kPadX, kHeight - kFooterH - 4,
               220, kFooterH, Justification::centredLeft);
}

// ── Transfer function display ──────────────────────────────────────────────────
void CompressorEditor::draw_transfer_curve(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());
    const float ax = float(area.getX());
    const float ay = float(area.getY());

    // Input/output range: -60 to 0 dBFS
    static constexpr float kDbLo = -60.0f, kDbHi = 0.0f;
    auto db_to_px_x = [&](float db) { return ax + (db - kDbLo) / (kDbHi - kDbLo) * w; };
    auto db_to_px_y = [&](float db) { return ay + (kDbHi - db) / (kDbHi - kDbLo) * h; };

    // Grid lines
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f }) {
        const float xv = db_to_px_x(db), yv = db_to_px_y(db);
        g.setColour(Colour(laf_.border()).withAlpha(0.4f));
        g.drawLine(xv, ay, xv, ay + h, 0.5f);  // vertical (input)
        g.drawLine(ax, yv, ax + w, yv, 0.5f);  // horizontal (output)
    }
    // 0 dB axis lines
    g.setColour(Colour(laf_.border()).withAlpha(0.7f));
    g.drawLine(db_to_px_x(0), ay, db_to_px_x(0), ay + h, 1.0f);
    g.drawLine(ax, db_to_px_y(0), ax + w, db_to_px_y(0), 1.0f);

    // Axis labels
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.6f));
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f }) {
        g.drawText(String(int(db)), int(db_to_px_x(db)) - 14, int(ay) + 2, 28, 9,
                   Justification::centred);
    }
    g.drawText("IN",  int(ax + w) - 16, int(ay + h) - 11, 16, 9, Justification::centredLeft);
    g.drawText("OUT", int(ax) + 2,      int(ay) + 2,       20, 9, Justification::centredLeft);

    // Unity-gain diagonal
    g.setColour(Colour(laf_.border()).withAlpha(0.5f));
    g.drawLine(db_to_px_x(kDbLo), db_to_px_y(kDbLo),
               db_to_px_x(kDbHi), db_to_px_y(kDbHi), 1.0f);

    // Threshold vertical marker
    const float thr = plugin_.get_apvts().getRawParameterValue("threshold")->load();
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    g.drawLine(db_to_px_x(thr), ay, db_to_px_x(thr), ay + h, 1.0f);

    // Transfer function curve (reads threshold, ratio, knee from APVTS)
    auto& ap         = plugin_.get_apvts();
    const float ratio = ap.getRawParameterValue("ratio")->load();
    const float knee  = ap.getRawParameterValue("knee")->load();
    const float makeup= ap.getRawParameterValue("makeup")->load();

    // Temporarily set processor params to current values for gain_computer call
    // (We replicate the formula here rather than calling the processor to avoid
    //  thread-safety issues; the formula is simple enough to inline.)
    auto gc = [&](float ldb) -> float {
        const float x = ldb - thr;
        const float W = knee;
        if (2.0f * x < -W)             return 0.0f;
        if (2.0f * std::abs(x) <= W)   return -(x + W*0.5f)*(x + W*0.5f)/(2.0f*W)*(1.0f-1.0f/ratio);
        return x * (1.0f / ratio - 1.0f);
    };

    Path curve;
    for (int px = 0; px < int(w); ++px) {
        const float in_db  = kDbLo + float(px) / w * (kDbHi - kDbLo);
        const float out_db = in_db + gc(in_db) + makeup;
        const float cx_    = ax + float(px);
        const float cy_    = jlimit(ay, ay + h, db_to_px_y(out_db));
        if (px == 0) curve.startNewSubPath(cx_, cy_);
        else         curve.lineTo(cx_, cy_);
    }

    // Filled area between curve and unity line
    Path filled = curve;
    filled.lineTo(ax + w, db_to_px_y(kDbHi));
    filled.lineTo(ax,     db_to_px_y(kDbLo));
    filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.12f));
    g.fillPath(filled);

    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
    g.strokePath(curve, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // Border
    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);

    g.restoreState();
}

// ── GR Meter ───────────────────────────────────────────────────────────────────
void CompressorEditor::draw_gr_meter(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax = float(area.getX());
    const float ay = float(area.getY());
    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight()) - 14.0f;  // reserve bottom for label

    // Bar: 0 dB at top, -24 dB at bottom
    static constexpr float kGrMax = 24.0f;   // full scale
    const float gr_abs  = std::min(kGrMax, std::abs(gr_display_));
    const float fill_h  = gr_abs / kGrMax * h;

    // Color gradient: green → yellow → red
    const float norm = gr_abs / kGrMax;
    const Colour col = norm < 0.5f
        ? Colour::fromHSV(0.33f - norm * 0.33f, 0.85f, 0.8f, 1.0f)  // green → yellow
        : Colour::fromHSV(0.0f,  0.85f, 0.8f, 1.0f);                // orange/red

    // Fill bar (top = 0 dB, bar grows downward)
    g.setColour(col.withAlpha(0.85f));
    g.fillRect(ax + 2.0f, ay + 1.0f, w - 4.0f, fill_h);

    // Tick marks at 0, -6, -12, -18, -24 dB
    g.setFont(Font(7.0f));
    for (float mark_db : { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f }) {
        const float ymark = ay + std::abs(mark_db) / kGrMax * h;
        g.setColour(Colour(laf_.border()).withAlpha(0.8f));
        g.drawLine(ax + 2.0f, ymark, ax + w - 2.0f, ymark, 0.5f);
        if (mark_db != 0.0f) {
            g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
            g.drawText(String(int(mark_db)),
                       int(ax), int(ymark) - 4, int(w), 8,
                       Justification::centred);
        }
    }

    // "GR" label at bottom
    g.setFont(Font(8.0f, Font::bold));
    g.setColour(Colour(laf_.text_muted()));
    g.drawText("GR", int(ax), int(ay + h + 2), int(w), 11, Justification::centred);

    // Border
    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);

    g.restoreState();
}

} // namespace kaos_engine
