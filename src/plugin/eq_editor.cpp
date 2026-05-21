#include "eq_editor.h"
#include "eq_plugin.h"
#include <cmath>

namespace kaos_engine {
using namespace juce;

// ── Per-band accent colours ────────────────────────────────────────────────────
static constexpr uint32 kBandColour[7] = {
    0xff6ca8d4,   // 1  sky blue
    0xff68c87a,   // 2  green
    0xffd06868,   // 3  red
    0xffc8a44a,   // 4  amber
    0xffa068cc,   // 5  purple
    0xffd48860,   // 6  orange
    0xff60c8c8,   // 7  cyan
};

// Band type display names (indexed by BandType enum value)
static const char* kTypeNames[] = {
    "-", "BELL", "NOTCH", "LO SHF", "HI SHF", "HP 12", "HP 24", "LP 12", "LP 24"
};

// ── Coordinate helpers ─────────────────────────────────────────────────────────
float EqEditor::freq_to_x(float freq, float width)
    { return width * std::log(freq / 20.0f) / std::log(1000.0f); }
float EqEditor::x_to_freq(float x, float width)
    { return 20.0f * std::pow(1000.0f, x / width); }
float EqEditor::db_to_y(float db, float height, float db_min, float db_max)
    { return height * (db_max - db) / (db_max - db_min); }
float EqEditor::y_to_db(float y, float height, float db_min, float db_max)
    { return db_max - y / height * (db_max - db_min); }

// ── Parameter helpers ──────────────────────────────────────────────────────────
void EqEditor::set_param(const String& id, float raw)
{
    auto& ap    = plugin_.get_apvts();
    auto  range = ap.getParameterRange(id);
    ap.getParameter(id)->setValueNotifyingHost(
        range.convertTo0to1(jlimit(range.start, range.end, raw)));
}
void EqEditor::begin_gesture(const String& id)
{
    if (auto* p = plugin_.get_apvts().getParameter(id)) p->beginChangeGesture();
}
void EqEditor::end_gesture(const String& id)
{
    if (auto* p = plugin_.get_apvts().getParameter(id)) p->endChangeGesture();
}

// ── Node helpers ───────────────────────────────────────────────────────────────
Point<float> EqEditor::node_screen_pos(int b) const
{
    auto& ap  = plugin_.get_apvts();
    const auto type = static_cast<BandType>(
        roundToInt(ap.getRawParameterValue(type_param_id(b))->load()));
    if (type == BandType::Off) return { -100.0f, -100.0f };

    const float freq = ap.getRawParameterValue(freq_param_id(b))->load();
    const float gain = ap.getRawParameterValue(gain_param_id(b))->load();
    const float q    = ap.getRawParameterValue(q_param_id(b))->load();
    const float w    = float(kWidth);
    const float h    = float(kDisplayH);
    const double fs  = plugin_.getSampleRate() > 0 ? plugin_.getSampleRate() : 44100.0;

    const float x = freq_to_x(freq, w);
    float y;

    if (band_has_gain(type)) {
        y = db_to_y(gain, h, kDbMin, kDbMax);
    } else if (type == BandType::Notch) {
        y = db_to_y(0.0f, h, kDbMin, kDbMax);  // notch node sits on 0 dB line
    } else {
        // HP/LP: position on the curve at the cutoff frequency
        using BC = BiquadCoeffs;
        double mag = 0.0;
        switch (type) {
            case BandType::HP12: mag = BC::high_pass(fs,freq,q).magnitude_db(freq,fs); break;
            case BandType::HP24:
                mag = BC::high_pass(fs,freq,kBW4_Q1).magnitude_db(freq,fs)
                    + BC::high_pass(fs,freq,kBW4_Q2).magnitude_db(freq,fs); break;
            case BandType::LP12: mag = BC::low_pass(fs,freq,q).magnitude_db(freq,fs); break;
            case BandType::LP24:
                mag = BC::low_pass(fs,freq,kBW4_Q1).magnitude_db(freq,fs)
                    + BC::low_pass(fs,freq,kBW4_Q2).magnitude_db(freq,fs); break;
            default: mag = 0.0; break;
        }
        y = db_to_y(float(mag), h, kDbMin, kDbMax);
    }
    return { x, jlimit(0.0f, float(kDisplayH), y) };
}

int EqEditor::hit_test_node(Point<float> pos) const
{
    if (pos.y < 0.0f || pos.y > float(kDisplayH)) return -1;
    int   best      = -1;
    float best_dist = kNodeRadius + 6.0f;
    for (int b = 0; b < kNumBands; ++b) {
        auto& ap = plugin_.get_apvts();
        const auto type = static_cast<BandType>(
            roundToInt(ap.getRawParameterValue(type_param_id(b))->load()));
        if (type == BandType::Off) continue;
        const float d = pos.getDistanceFrom(node_screen_pos(b));
        if (d < best_dist) { best_dist = d; best = b; }
    }
    return best;
}

// ── Type context menu ──────────────────────────────────────────────────────────
void EqEditor::show_type_menu(int band)
{
    auto& ap = plugin_.get_apvts();
    const int cur = roundToInt(ap.getRawParameterValue(type_param_id(band))->load());

    PopupMenu menu;
    auto add = [&](int id, const char* name) {
        menu.addItem(id, name, true, cur == id - 1);
    };
    add(1, "Off");
    menu.addSeparator();
    add(2, "Bell");
    add(3, "Notch");
    add(4, "Low Shelf");
    add(5, "High Shelf");
    menu.addSeparator();
    add(6, "HP 12 dB/oct");
    add(7, "HP 24 dB/oct");
    menu.addSeparator();
    add(8, "LP 12 dB/oct");
    add(9, "LP 24 dB/oct");

    menu.showMenuAsync(PopupMenu::Options{}, [this, band](int result) {
        if (result <= 0) return;
        set_param(type_param_id(band), float(result - 1));
    });
}

// ── Construction ───────────────────────────────────────────────────────────────
EqEditor::EqEditor(EqPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    for (int b = 0; b < kNumBands; ++b) {
        setup_knob(freq_slider_[b], freq_lbl_[b], "FREQ");
        setup_knob(gain_slider_[b], gain_lbl_[b], "GAIN");
        setup_knob(q_slider_[b],    q_lbl_[b],    "Q");

        // Type label — non-interactive child; mouse events pass through to editor
        type_lbl_[b].setFont(Font(9.0f, Font::bold));
        type_lbl_[b].setJustificationType(Justification::centred);
        type_lbl_[b].setInterceptsMouseClicks(false, false);
        addAndMakeVisible(type_lbl_[b]);

        freq_att_[b] = std::make_unique<Attachment>(apvts, freq_param_id(b), freq_slider_[b]);
        gain_att_[b] = std::make_unique<Attachment>(apvts, gain_param_id(b), gain_slider_[b]);
        q_att_[b]    = std::make_unique<Attachment>(apvts, q_param_id(b),    q_slider_[b]);

        const Colour bc = Colour(kBandColour[b]);
        freq_slider_[b].setColour(Slider::rotarySliderFillColourId, bc);
        gain_slider_[b].setColour(Slider::rotarySliderFillColourId, bc);
        q_slider_[b]   .setColour(Slider::rotarySliderFillColourId, bc);
    }

    setup_knob(output_knob_, output_lbl_, "OUTPUT");
    setup_knob(mix_knob_,    mix_lbl_,   "MIX");
    output_att_ = std::make_unique<Attachment>(apvts, "output", output_knob_);
    mix_att_    = std::make_unique<Attachment>(apvts, "mix",    mix_knob_);

    output_knob_.setTooltip("Output: post-EQ gain (-20 to +6 dB). Compensates for level changes.");
    mix_knob_.setTooltip("Mix: wet/dry blend. Left = bypass, Right = full EQ.");

    update_band_ui();
    startTimerHz(30);
}

EqEditor::~EqEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void EqEditor::setup_knob(Slider& k, Label& l, const String& name)
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
void EqEditor::timerCallback()
{
    update_spectrum();
    update_band_ui();
    repaint(0, 0, kWidth, kDisplayH + kTypeLblH + 8);
}

void EqEditor::update_spectrum()
{
    if (!plugin_.pull_fft_block(fft_data_)) return;
    window_.multiplyWithWindowingTable(fft_data_, kFftSize);
    fft_.performFrequencyOnlyForwardTransform(fft_data_);

    const float scale = Decibels::gainToDecibels(float(kFftSize));
    const float sr    = float(plugin_.getSampleRate() > 0 ? plugin_.getSampleRate() : 44100.0);

    for (int i = 0; i < kScopeSize; ++i) {
        const float freq = 20.0f * std::pow(1000.0f, float(i) / float(kScopeSize - 1));
        const int   bin  = jlimit(0, kFftSize / 2 - 1,
                               roundToInt(freq * float(kFftSize) / sr));
        const float lvl  = Decibels::gainToDecibels(std::max(fft_data_[bin], 1e-10f)) - scale;
        const float nv   = jmap(lvl, -80.0f, 0.0f, 0.0f, 1.0f);
        const float c    = nv > scope_data_[i] ? 0.4f : 0.92f;
        scope_data_[i]   = c * scope_data_[i] + (1.0f - c) * nv;
    }
}

void EqEditor::update_band_ui()
{
    auto& ap = plugin_.get_apvts();
    for (int b = 0; b < kNumBands; ++b) {
        const auto type = static_cast<BandType>(
            roundToInt(ap.getRawParameterValue(type_param_id(b))->load()));

        const bool active   = (type != BandType::Off);
        const bool has_gain = band_has_gain(type);
        // HP24/LP24 use fixed Butterworth Q — Q knob has no effect
        const bool has_q    = active && type != BandType::HP24 && type != BandType::LP24;

        freq_slider_[b].setEnabled(active);   freq_lbl_[b].setAlpha(active   ? 1.0f : 0.35f);
        gain_slider_[b].setEnabled(has_gain); gain_lbl_[b].setAlpha(has_gain ? 1.0f : 0.35f);
        q_slider_[b]   .setEnabled(has_q);    q_lbl_[b]   .setAlpha(has_q   ? 1.0f : 0.35f);

        const int t = int(type);
        type_lbl_[b].setText(kTypeNames[t], dontSendNotification);
        type_lbl_[b].setColour(Label::textColourId,
            active ? Colour(kBandColour[b]) : Colour(laf_.text_muted()));
    }
}

// ── Mouse ──────────────────────────────────────────────────────────────────────
void EqEditor::mouseDown(const MouseEvent& e)
{
    // Click on band type label area → open type menu for that band
    if (e.position.y >= float(kTypeLblY) && e.position.y <= float(kTypeLblY + kTypeLblH + 4)) {
        const int w    = getWidth();
        const int colw = (w - kPadX * 2) / kNumCols;
        const int col  = int((e.position.x - kPadX) / colw);
        if (col >= 0 && col < kNumBands) { show_type_menu(col); return; }
    }

    // Display area
    if (e.position.y > float(kDisplayH)) return;

    const int b = hit_test_node(e.position);

    if (e.mods.isRightButtonDown()) {
        if (b >= 0) show_type_menu(b);
        else {
            // Right-click on empty display area: find next Off band and offer to enable it
            for (int i = 0; i < kNumBands; ++i) {
                auto& ap = plugin_.get_apvts();
                const auto type = static_cast<BandType>(
                    roundToInt(ap.getRawParameterValue(type_param_id(i))->load()));
                if (type == BandType::Off) { show_type_menu(i); break; }
            }
        }
        return;
    }

    if (b < 0) return;

    auto& ap = plugin_.get_apvts();
    drag_type_ = static_cast<BandType>(
        roundToInt(ap.getRawParameterValue(type_param_id(b))->load()));

    dragging_band_    = b;
    drag_start_mouse_ = e.position;
    drag_start_freq_  = ap.getRawParameterValue(freq_param_id(b))->load();
    drag_start_gain_  = band_has_gain(drag_type_)
                        ? ap.getRawParameterValue(gain_param_id(b))->load() : 0.0f;

    begin_gesture(freq_param_id(b));
    if (band_has_gain(drag_type_)) begin_gesture(gain_param_id(b));
}

void EqEditor::mouseDrag(const MouseEvent& e)
{
    if (dragging_band_ < 0) return;
    const int b = dragging_band_;

    const float new_x    = jlimit(0.0f, float(kWidth),
                               drag_start_mouse_.x + float(e.getDistanceFromDragStartX()));
    const float new_freq = x_to_freq(new_x, float(kWidth));
    set_param(freq_param_id(b), new_freq);

    if (band_has_gain(drag_type_)) {
        const float new_y  = jlimit(0.0f, float(kDisplayH),
                                 drag_start_mouse_.y + float(e.getDistanceFromDragStartY()));
        set_param(gain_param_id(b), y_to_db(new_y, float(kDisplayH), kDbMin, kDbMax));
    }
}

void EqEditor::mouseUp(const MouseEvent&)
{
    if (dragging_band_ < 0) return;
    end_gesture(freq_param_id(dragging_band_));
    if (band_has_gain(drag_type_)) end_gesture(gain_param_id(dragging_band_));
    dragging_band_ = -1;
    drag_type_     = BandType::Off;
}

void EqEditor::mouseMove(const MouseEvent& e)
{
    const int prev = hovered_band_;
    hovered_band_ = (e.position.y < float(kDisplayH)) ? hit_test_node(e.position) : -1;
    setMouseCursor(hovered_band_ >= 0
        ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    if (hovered_band_ != prev)
        repaint(0, 0, kWidth, kDisplayH);
}

void EqEditor::mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wd)
{
    const int b = (dragging_band_ >= 0) ? dragging_band_ : hit_test_node(e.position);
    if (b < 0) return;

    auto& ap     = plugin_.get_apvts();
    const auto t = static_cast<BandType>(
        roundToInt(ap.getRawParameterValue(type_param_id(b))->load()));
    if (t == BandType::HP24 || t == BandType::LP24 || t == BandType::Off) return;

    const String qid = q_param_id(b);
    auto  range      = ap.getParameterRange(qid);
    const float cur  = ap.getRawParameterValue(qid)->load();
    const float step = (range.end - range.start) * 0.04f * wd.deltaY;
    begin_gesture(qid);
    set_param(qid, cur + step);
    end_gesture(qid);
}

// ── Layout ─────────────────────────────────────────────────────────────────────
void EqEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;
    auto cx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 28; };
    auto ty = [&](int col) { return kPadX + col * colw + colw / 2 - 28; };

    for (int b = 0; b < kNumBands; ++b) {
        freq_slider_[b].setBounds(cx(b), kKnobY1, kKnobSize, kKnobSize);
        freq_lbl_[b]   .setBounds(lx(b), kKnobY1 + kKnobSize + 1, 56, kLabelH);
        gain_slider_[b].setBounds(cx(b), kKnobY2, kKnobSize, kKnobSize);
        gain_lbl_[b]   .setBounds(lx(b), kKnobY2 + kKnobSize + 1, 56, kLabelH);
        q_slider_[b]   .setBounds(cx(b), kKnobY3, kKnobSize, kKnobSize);
        q_lbl_[b]      .setBounds(lx(b), kKnobY3 + kKnobSize + 1, 56, kLabelH);
        type_lbl_[b]   .setBounds(ty(b), kTypeLblY, 56, kTypeLblH);
    }

    // OUTPUT and MIX in the last two columns, centred on GAIN row
    const int outCol = kNumBands;
    const int mixCol = kNumBands + 1;
    output_knob_.setBounds(cx(outCol), kKnobY2, kKnobSize, kKnobSize);
    output_lbl_ .setBounds(lx(outCol), kKnobY2 + kKnobSize + 1, 56, kLabelH);
    mix_knob_   .setBounds(cx(mixCol), kKnobY2, kKnobSize, kKnobSize);
    mix_lbl_    .setBounds(lx(mixCol), kKnobY2 + kKnobSize + 1, 56, kLabelH);

    (void)ty; // suppress unused warning
}

// ── Painting ───────────────────────────────────────────────────────────────────
void EqEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    const Rectangle<int> display(0, 0, getWidth(), kDisplayH);
    draw_display(g, display);
    draw_band_labels(g);

    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::eq", kPadX, getHeight() - kFooterH - 4,
               200, kFooterH, Justification::centredLeft);
}

void EqEditor::draw_display(Graphics& g, Rectangle<int> area)
{
    Graphics::ScopedSaveState ss(g);
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);
    draw_grid    (g, area);
    draw_spectrum(g, area);
    draw_eq_curve(g, area);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
}

void EqEditor::draw_grid(Graphics& g, Rectangle<int> area)
{
    const float w = float(area.getWidth()), h = float(area.getHeight());
    const float ax = float(area.getX()),    ay = float(area.getY());

    for (float db : { -18.0f,-12.0f,-6.0f,0.0f,6.0f,12.0f,18.0f }) {
        const float y      = ay + db_to_y(db, h, kDbMin, kDbMax);
        const bool  is_zero = std::abs(db) < 0.5f;
        g.setColour(Colour(laf_.border()).withAlpha(is_zero ? 0.9f : 0.4f));
        g.drawLine(ax, y, ax + w, y, is_zero ? 1.0f : 0.5f);
        g.setFont(Font(8.5f));
        g.setColour(Colour(laf_.text_muted()).withAlpha(0.6f));
        g.drawText(String(int(db)) + " dB", int(ax)+3, int(y)-8, 36, 10,
                   Justification::centredLeft);
    }
    const float freq_marks[] = { 50,100,200,500,1000,2000,5000,10000 };
    const char* freq_labels[]= { "50","100","200","500","1k","2k","5k","10k" };
    for (int fi = 0; fi < 8; ++fi) {
        const float x = ax + freq_to_x(freq_marks[fi], w);
        g.setColour(Colour(laf_.border()).withAlpha(0.4f));
        g.drawLine(x, ay, x, ay + h, 0.5f);
        g.setFont(Font(8.5f));
        g.setColour(Colour(laf_.text_muted()).withAlpha(0.6f));
        g.drawText(freq_labels[fi], int(x)-12, area.getBottom()-12, 24, 10,
                   Justification::centred);
    }
}

void EqEditor::draw_spectrum(Graphics& g, Rectangle<int> area)
{
    const float w = float(area.getWidth()), h = float(area.getHeight());
    const float ax = float(area.getX()),    ay = float(area.getY());

    Path spectrum;
    for (int i = 0; i < kScopeSize; ++i) {
        const float x = ax + float(i) / float(kScopeSize-1) * w;
        const float y = ay + h * (1.0f - scope_data_[i]);
        if (i == 0) spectrum.startNewSubPath(x, y); else spectrum.lineTo(x, y);
    }
    Path filled = spectrum;
    filled.lineTo(ax+w, ay+h); filled.lineTo(ax, ay+h); filled.closeSubPath();
    g.setColour(Colour(laf_.text_primary()).withAlpha(0.08f));
    g.fillPath(filled);
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.4f));
    g.strokePath(spectrum, PathStrokeType(1.0f));
}

void EqEditor::draw_eq_curve(Graphics& g, Rectangle<int> area)
{
    const float w = float(area.getWidth()), h = float(area.getHeight());
    const float ax = float(area.getX()),    ay = float(area.getY());

    auto& ap        = plugin_.get_apvts();
    const double fs = plugin_.getSampleRate() > 0 ? plugin_.getSampleRate() : 44100.0;
    using BC = BiquadCoeffs;

    // Precompute band coefficients once — only magnitude_db (cheap arithmetic) runs per-pixel.
    struct BandInfo { BandType type; BC c[2]; int ns; };
    BandInfo bands[kNumBands];
    for (int b = 0; b < kNumBands; ++b) {
        const auto t    = static_cast<BandType>(
            roundToInt(ap.getRawParameterValue(type_param_id(b))->load()));
        const double f  = ap.getRawParameterValue(freq_param_id(b))->load();
        const double gd = ap.getRawParameterValue(gain_param_id(b))->load();
        const double q  = ap.getRawParameterValue(q_param_id(b))->load();
        bands[b].type   = t;
        bands[b].ns     = 0;
        switch (t) {
            case BandType::Bell:      bands[b].c[0]=BC::peak      (fs,f,q,gd); bands[b].ns=1; break;
            case BandType::Notch:     bands[b].c[0]=BC::notch     (fs,f,q);    bands[b].ns=1; break;
            case BandType::LowShelf:  bands[b].c[0]=BC::low_shelf (fs,f,q,gd); bands[b].ns=1; break;
            case BandType::HighShelf: bands[b].c[0]=BC::high_shelf(fs,f,q,gd); bands[b].ns=1; break;
            case BandType::HP12:      bands[b].c[0]=BC::high_pass (fs,f,q);    bands[b].ns=1; break;
            case BandType::HP24:
                bands[b].c[0]=BC::high_pass(fs,f,kBW4_Q1);
                bands[b].c[1]=BC::high_pass(fs,f,kBW4_Q2); bands[b].ns=2; break;
            case BandType::LP12:      bands[b].c[0]=BC::low_pass  (fs,f,q);    bands[b].ns=1; break;
            case BandType::LP24:
                bands[b].c[0]=BC::low_pass(fs,f,kBW4_Q1);
                bands[b].c[1]=BC::low_pass(fs,f,kBW4_Q2);  bands[b].ns=2; break;
            default: break;
        }
    }

    // Build the combined EQ curve
    Path curve;
    for (int px = 0; px < int(w); ++px) {
        const double freq = 20.0 * std::pow(1000.0, double(px) / w);
        double db = 0.0;
        for (int b = 0; b < kNumBands; ++b)
            for (int s = 0; s < bands[b].ns; ++s)
                db += bands[b].c[s].magnitude_db(freq, fs);
        const float yc = jlimit(ay, ay+h, ay + db_to_y(float(db), h, kDbMin, kDbMax));
        if (px == 0) curve.startNewSubPath(ax, yc); else curve.lineTo(ax+float(px), yc);
    }

    // Fill between curve and 0 dB line
    const float zero_y = ay + db_to_y(0.0f, h, kDbMin, kDbMax);
    Path filled = curve;
    filled.lineTo(ax+w, zero_y); filled.lineTo(ax, zero_y); filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.10f));
    g.fillPath(filled);
    g.setColour(Colour(laf_.accent_colour()).withAlpha(0.9f));
    g.strokePath(curve, PathStrokeType(1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    // ── Band nodes ──────────────────────────────────────────────────────────────
    for (int b = 0; b < kNumBands; ++b) {
        if (bands[b].type == BandType::Off) continue;
        const auto   np     = node_screen_pos(b);
        const bool   active = (b == hovered_band_ || b == dragging_band_);
        const float  r      = active ? kNodeRadius + 2.0f : kNodeRadius;
        const Colour col    = Colour(kBandColour[b]);

        g.setColour(active ? col : col.withAlpha(0.7f));
        g.fillEllipse(np.x - r, np.y - r, r*2.0f, r*2.0f);
        g.setColour(active ? col.brighter(0.5f) : Colour(laf_.border()));
        g.drawEllipse(np.x - r, np.y - r, r*2.0f, r*2.0f, 1.5f);

        // Band number label inside the node
        g.setColour(active ? Colour(0xffffffff) : Colour(0xffcccccc));
        g.setFont(Font(8.0f, Font::bold));
        g.drawText(String(b + 1),
                   int(np.x - r), int(np.y - r),
                   int(r*2.0f), int(r*2.0f),
                   Justification::centred);
    }
}

void EqEditor::draw_band_labels(Graphics& g)
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;
    g.setFont(Font(9.0f));
    g.setColour(Colour(laf_.text_muted()));
    g.drawText("OUTPUT",
               kPadX + kNumBands * colw + colw/2 - 32, kTypeLblY, 64, kTypeLblH,
               Justification::centred);
    g.drawText("MIX",
               kPadX + (kNumBands+1) * colw + colw/2 - 32, kTypeLblY, 64, kTypeLblH,
               Justification::centred);
}

} // namespace kaos_engine
