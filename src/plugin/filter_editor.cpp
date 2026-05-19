#include "filter_editor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {
using namespace juce;

// ── Mode descriptions ──────────────────────────────────────────────────────────
static const char* kModeLabels[12] = {
    "Low-pass 12 dB/oct -- rolls off highs; warm resonant character.",
    "Low-pass 24 dB/oct -- steep rolloff; classic synthesiser tone.",
    "High-pass 12 dB/oct -- rolls off lows; bright, open sound.",
    "High-pass 24 dB/oct -- steep low cut; tight, focused character.",
    "Band-pass -- passes a band, attenuates above and below.",
    "Notch -- removes a narrow band; tames resonances.",
    "All-pass -- flat magnitude, rotating phase near the cutoff.",
    "Peak (bell) -- boosts or cuts a band. GAIN sets amount.",
    "Low shelf -- boosts or cuts all frequencies below cutoff. GAIN sets amount.",
    "Hi shelf  -- boosts or cuts all frequencies above cutoff. GAIN sets amount.",
    "Comb -- resonant peaks at fc, 2fc, 3fc... RESONANCE = feedback gain, DRIVE = harmonic damping.",
    "4-pole Moog-style ladder LP. High RESONANCE -> self-oscillation.",
};

// ── Construction ───────────────────────────────────────────────────────────────

FilterEditor::FilterEditor(FilterPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& ap = plugin_.get_apvts();

    mode_box_.addItem("LP 12",     1);  mode_box_.addItem("LP 24",     2);
    mode_box_.addItem("HP 12",     3);  mode_box_.addItem("HP 24",     4);
    mode_box_.addItem("Band Pass", 5);  mode_box_.addItem("Notch",     6);
    mode_box_.addItem("All Pass",  7);  mode_box_.addItem("Peak",      8);
    mode_box_.addItem("Low Shelf", 9);  mode_box_.addItem("Hi Shelf", 10);
    mode_box_.addItem("Comb",     11);  mode_box_.addItem("Ladder",   12);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "mode", mode_box_);
    mode_box_.onChange = [this] { update_mode_ui(); };

    mode_label_.setFont(Font(10.5f));
    mode_label_.setJustificationType(Justification::centredLeft);
    mode_label_.setColour(Label::textColourId, Colour(laf_.text_muted()));
    mode_label_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(mode_label_);

    setup_knob(cutoff_knob_,    cutoff_lbl_,    "CUTOFF");
    setup_knob(resonance_knob_, resonance_lbl_, "RESONANCE");
    setup_knob(drive_knob_,     drive_lbl_,     "DRIVE");
    setup_knob(gain_knob_,      gain_lbl_,      "GAIN");
    setup_knob(mix_knob_,       mix_lbl_,       "MIX");
    setup_knob(output_knob_,    output_lbl_,    "OUTPUT");

    cutoff_knob_   .setTooltip("Cutoff (fc): filter cutoff frequency. 20 Hz – 20 kHz.");
    resonance_knob_.setTooltip("Resonance (Q): filter resonance. 0.707 = Butterworth (flat passband). "
                               "Higher Q = sharper peak at cutoff. "
                               "For Comb: feedback gain (0=off, high=strong resonance). "
                               "For Ladder: Q > 4 = self-oscillation.");
    drive_knob_    .setTooltip("Drive: pre-filter soft saturation (all modes except Comb).\n"
                               "Comb mode: feedback damping -- progressively attenuates higher "
                               "harmonics, leaving only the lower comb teeth prominent. "
                               "0 = bright (all harmonics equal), 1 = dark (only fundamental survives).");
    gain_knob_     .setTooltip("Gain: boost or cut in dB. Active for Peak, Low Shelf, Hi Shelf only.");
    mix_knob_      .setTooltip("Mix: dry/wet blend. 0 = bypass, 1 = fully filtered.");
    output_knob_   .setTooltip("Output: post-mix gain trim. -20 to +6 dB.");

    cutoff_att_    = std::make_unique<Attachment>(ap, "cutoff",    cutoff_knob_);
    resonance_att_ = std::make_unique<Attachment>(ap, "resonance", resonance_knob_);
    drive_att_     = std::make_unique<Attachment>(ap, "drive",     drive_knob_);
    gain_att_      = std::make_unique<Attachment>(ap, "gain",      gain_knob_);
    mix_att_       = std::make_unique<Attachment>(ap, "mix",       mix_knob_);
    output_att_    = std::make_unique<Attachment>(ap, "output",    output_knob_);

    update_mode_ui();
    startTimerHz(30);
}

FilterEditor::~FilterEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void FilterEditor::setup_knob(Slider& k, Label& l, const String& name)
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

void FilterEditor::timerCallback()
{
    repaint(0, kDispY, kWidth, kDispH);
    update_mode_ui();
}

void FilterEditor::update_mode_ui()
{
    const int idx = mode_box_.getSelectedItemIndex();
    if (idx >= 0 && idx < 12)
        mode_label_.setText(kModeLabels[idx], dontSendNotification);

    // GAIN only active for Peak / LowShelf / HiShelf
    const bool gain_active = (idx == 7 || idx == 8 || idx == 9);
    gain_knob_.setEnabled(gain_active);
    gain_knob_.setAlpha(gain_active ? 1.0f : 0.4f);
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void FilterEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    mode_box_  .setBounds(kPadX, kComboY, kComboW, kComboH);
    mode_label_.setBounds(kPadX + kComboW + 8, kComboY,
                          w - kPadX - kComboW - 14, kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 36; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY, kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 72, kKnobLblH);
    };
    place(cutoff_knob_,    cutoff_lbl_,    0);
    place(resonance_knob_, resonance_lbl_, 1);
    place(drive_knob_,     drive_lbl_,     2);
    place(gain_knob_,      gain_lbl_,      3);
    place(mix_knob_,       mix_lbl_,       4);
    place(output_knob_,    output_lbl_,    5);
}

// ── Frequency response computation ────────────────────────────────────────────
// All RBJ biquad formulas evaluate H(e^jω) analytically.

static float biquad_db(float b0, float b1, float b2,
                       float a1, float a2, float w)
{
    const float cw  = std::cos(w), sw  = std::sin(w);
    const float c2w = std::cos(2.0f * w), s2w = std::sin(2.0f * w);
    const float nr = b0 + b1*cw + b2*c2w,  ni = -(b1*sw + b2*s2w);
    const float dr = 1.0f + a1*cw + a2*c2w, di = -(a1*sw + a2*s2w);
    return 10.0f * std::log10(std::max((nr*nr + ni*ni) / std::max(dr*dr + di*di, 1e-30f), 1e-30f));
}

float FilterEditor::response_db(float freq_hz) const
{
    const float fs  = float(std::max(plugin_.get_sample_rate(), 1.0));
    const float fc  = std::min(plugin_.get_cutoff(), fs * 0.499f);
    const float Q   = plugin_.get_resonance();
    const float gdb = plugin_.get_gain_db();
    const FilterMode mode = plugin_.get_mode();

    const float w0  = 2.0f * float(M_PI) * freq_hz / fs;
    const float w0c = 2.0f * float(M_PI) * fc / fs;
    const float sc  = std::sin(w0c), cc = std::cos(w0c);
    const float al  = sc / (2.0f * Q);

    switch (mode) {

    case FilterMode::LP12: {
        const float a0 = 1.0f + al;
        return biquad_db((1.0f-cc)*0.5f/a0, (1.0f-cc)/a0, (1.0f-cc)*0.5f/a0,
                         -2.0f*cc/a0, (1.0f-al)/a0, w0);
    }
    case FilterMode::LP24: {
        const float a0 = 1.0f + al;
        float db = biquad_db((1.0f-cc)*0.5f/a0, (1.0f-cc)/a0, (1.0f-cc)*0.5f/a0,
                             -2.0f*cc/a0, (1.0f-al)/a0, w0);
        return db * 2.0f;  // two cascaded stages
    }
    case FilterMode::HP12: {
        const float a0 = 1.0f + al;
        return biquad_db((1.0f+cc)*0.5f/a0, -(1.0f+cc)/a0, (1.0f+cc)*0.5f/a0,
                         -2.0f*cc/a0, (1.0f-al)/a0, w0);
    }
    case FilterMode::HP24: {
        const float a0 = 1.0f + al;
        float db = biquad_db((1.0f+cc)*0.5f/a0, -(1.0f+cc)/a0, (1.0f+cc)*0.5f/a0,
                             -2.0f*cc/a0, (1.0f-al)/a0, w0);
        return db * 2.0f;
    }
    case FilterMode::BandPass: {
        const float a0 = 1.0f + al;
        return biquad_db(al/a0, 0.0f, -al/a0, -2.0f*cc/a0, (1.0f-al)/a0, w0);
    }
    case FilterMode::Notch: {
        const float a0 = 1.0f + al;
        return biquad_db(1.0f/a0, -2.0f*cc/a0, 1.0f/a0, -2.0f*cc/a0, (1.0f-al)/a0, w0);
    }
    case FilterMode::AllPass:
        // magnitude is 0 dB everywhere — show just the 0 dB line
        return 0.0f;

    case FilterMode::Peak: {
        const float A  = std::pow(10.0f, gdb / 40.0f);
        const float a0 = 1.0f + al / A;
        return biquad_db((1.0f + al*A)/a0, -2.0f*cc/a0, (1.0f - al*A)/a0,
                         -2.0f*cc/a0, (1.0f - al/A)/a0, w0);
    }
    case FilterMode::LowShelf: {
        const float A   = std::pow(10.0f, gdb / 40.0f);
        const float sqA = std::sqrt(A);
        const float al2 = sc / 2.0f * std::sqrt((A + 1.0f/A) * (1.0f/1.0f - 1.0f) + 2.0f);
        const float a0  = (A+1.0f) + (A-1.0f)*cc + 2.0f*sqA*al2;
        return biquad_db(
            A*((A+1.0f)-(A-1.0f)*cc + 2.0f*sqA*al2) / a0,
            2.0f*A*((A-1.0f)-(A+1.0f)*cc)            / a0,
            A*((A+1.0f)-(A-1.0f)*cc - 2.0f*sqA*al2)  / a0,
            -2.0f*((A-1.0f)+(A+1.0f)*cc)              / a0,
            ((A+1.0f)+(A-1.0f)*cc - 2.0f*sqA*al2)     / a0, w0);
    }
    case FilterMode::HiShelf: {
        const float A   = std::pow(10.0f, gdb / 40.0f);
        const float sqA = std::sqrt(A);
        const float al2 = sc / 2.0f;
        const float a0  = (A+1.0f) - (A-1.0f)*cc + 2.0f*sqA*al2;
        return biquad_db(
            A*((A+1.0f)+(A-1.0f)*cc + 2.0f*sqA*al2) / a0,
            -2.0f*A*((A-1.0f)+(A+1.0f)*cc)           / a0,
            A*((A+1.0f)+(A-1.0f)*cc - 2.0f*sqA*al2)  / a0,
            2.0f*((A-1.0f)-(A+1.0f)*cc)               / a0,
            ((A+1.0f)-(A-1.0f)*cc - 2.0f*sqA*al2)     / a0, w0);
    }
    case FilterMode::Comb: {
        // Feedback comb with one-pole LP damping in feedback path.
        // H(z) ≈ 1 / (1 - g * D(z) * z^-N)  where D(z) is the LP.
        // We approximate by attenuating g per harmonic: each pass through the
        // LP at frequency f loses a factor of sqrt(1/(1+(f/fc_damp)²)).
        const float fb    = std::clamp(Q / 21.0f * 20.0f, 0.0f, 0.97f);
        const float N     = float(std::max(1, int(std::round(fs / fc))));
        const float drive = plugin_.get_apvts().getRawParameterValue("drive")->load();
        const float damp_fc = 20000.0f * std::pow(0.01f, drive);
        // LP magnitude at freq_hz
        const float lp_mag = 1.0f / std::sqrt(1.0f + (freq_hz / damp_fc) * (freq_hz / damp_fc));
        const float g_eff  = fb * lp_mag;  // effective feedback after damping at this frequency
        const float wN     = w0 * N;
        const float denom  = 1.0f + g_eff*g_eff - 2.0f*g_eff*std::cos(wN);
        return -10.0f * std::log10(std::max(denom, 1e-20f));
    }
    case FilterMode::Ladder: {
        // Approximate as LP24 with Q boosted by the resonance amount.
        // The exact non-linear ladder response isn't analytically tractable.
        const float Q_eff = 0.5f + (Q / 5.0f) * 3.0f;  // Q 0.5..∞ approximation
        const float al_eff = sc / (2.0f * std::max(Q_eff, 0.1f));
        const float a0_eff = 1.0f + al_eff;
        float db = biquad_db((1.0f-cc)*0.5f/a0_eff, (1.0f-cc)/a0_eff, (1.0f-cc)*0.5f/a0_eff,
                             -2.0f*cc/a0_eff, (1.0f-al_eff)/a0_eff, w0);
        return db * 2.0f;
    }
    default: return 0.0f;
    }
}

// ── Painting ───────────────────────────────────────────────────────────────────

void FilterEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    draw_response(g, Rectangle<int>(0, kDispY, kWidth, kDispH));

    // Knob column header labels
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;
    const char* col_labels[] = { "CUTOFF", "RESONANCE", "DRIVE", "GAIN", "MIX", "OUTPUT" };
    g.setFont(Font(8.5f, Font::bold));
    g.setColour(Colour(laf_.text_muted()));
    for (int c = 0; c < kNumCols; ++c) {
        const int cx = kPadX + c * colw + colw / 2;
        g.drawText(col_labels[c], cx - 36, kLabelY, 72, kLabelH, Justification::centred);
    }

    // Separator lines
    g.setColour(Colour(laf_.border()));
    g.fillRect(kPadX, kSep1Y, w - kPadX * 2, 1);
    g.fillRect(kPadX, kSep2Y, w - kPadX * 2, 1);
    g.fillRect(kPadX, kSep3Y, w - kPadX * 2, 1);

    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::filter", kPadX, kHeight - kFooterH - 4,
               200, kFooterH, Justification::centredLeft);
}

// ── Frequency response display ─────────────────────────────────────────────────

void FilterEditor::draw_response(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax = float(area.getX());
    const float ay = float(area.getY());
    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());

    // Frequency→pixel (log scale, 20 Hz – 20 kHz)
    auto freq_to_x = [&](float f) {
        return ax + w * std::log(f / 20.0f) / std::log(1000.0f);
    };
    auto x_to_freq = [&](float px) {
        return 20.0f * std::pow(1000.0f, (px - ax) / w);
    };
    // dB→pixel
    auto db_to_y = [&](float db) {
        return ay + h * (kDbMax - db) / (kDbMax - kDbMin);
    };

    // ── Frequency grid lines ──────────────────────────────────────────────────
    g.setColour(Colour(laf_.border()).withAlpha(0.4f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f }) {
        const float px = freq_to_x(f);
        if (px >= ax && px <= ax + w)
            g.drawLine(px, ay, px, ay + h, 0.5f);
    }
    // dB grid lines
    for (float db : { -24.0f, -12.0f, 0.0f, 12.0f }) {
        const float py = db_to_y(db);
        g.drawLine(ax, py, ax + w, py, db == 0.0f ? 0.9f : 0.4f);
    }

    // ── Grid labels ───────────────────────────────────────────────────────────
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    for (auto [f, label] : std::initializer_list<std::pair<float, const char*>>{
             {100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}}) {
        const float px = freq_to_x(f);
        g.drawText(label, int(px) - 12, int(ay) + 2, 24, 9, Justification::centred);
    }
    g.drawText("+12", int(ax) + 2, int(db_to_y(12.0f)) - 4,  24, 9, Justification::centredLeft);
    g.drawText("  0", int(ax) + 2, int(db_to_y( 0.0f)) - 4,  24, 9, Justification::centredLeft);
    g.drawText("-12", int(ax) + 2, int(db_to_y(-12.0f)) - 4, 24, 9, Justification::centredLeft);

    // ── Response curve ────────────────────────────────────────────────────────
    const int n_pts = int(w);
    Path curve;
    for (int px = 0; px < n_pts; ++px) {
        const float freq  = x_to_freq(ax + float(px));
        const float db    = std::clamp(response_db(freq), kDbMin - 6.0f, kDbMax + 6.0f);
        const float cy    = db_to_y(db);
        const float cx    = ax + float(px);
        if (px == 0) curve.startNewSubPath(cx, cy);
        else         curve.lineTo(cx, cy);
    }

    // Filled area between curve and 0 dB line
    Path filled = curve;
    filled.lineTo(ax + w, db_to_y(0.0f));
    filled.lineTo(ax,     db_to_y(0.0f));
    filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.15f));
    g.fillPath(filled);

    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
    g.strokePath(curve, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // ── Cutoff frequency marker ───────────────────────────────────────────────
    const float fc  = plugin_.get_cutoff();
    const float cpx = freq_to_x(fc);
    if (cpx >= ax && cpx <= ax + w) {
        g.setColour(Colour(laf_.text_primary()).withAlpha(0.55f));
        g.drawLine(cpx, ay + 2.0f, cpx, ay + h - 2.0f, 1.2f);

        // Frequency label on marker
        String freq_str;
        if (fc >= 1000.0f) freq_str = String(fc / 1000.0f, 1) + " kHz";
        else               freq_str = String(int(fc)) + " Hz";
        g.setFont(Font(7.5f));
        const int lx = int(cpx) + 3;
        g.drawText(freq_str, lx, int(ay) + 2, 48, 9, Justification::centredLeft);
    }

    // AllPass annotation
    if (plugin_.get_mode() == FilterMode::AllPass) {
        g.setFont(Font(8.5f));
        g.setColour(Colour(laf_.text_muted()));
        g.drawText("flat magnitude -- phase shift only", int(ax) + 4, int(ay + h/2) - 6,
                   int(w) - 8, 13, Justification::centred);
    }

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);

    g.restoreState();
}

} // namespace kaos_engine
