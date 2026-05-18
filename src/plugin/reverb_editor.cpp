#include "reverb_editor.h"
#include "reverb_plugin.h"

namespace kaos_engine {

using namespace juce;

// ── Construction ───────────────────────────────────────────────────────────────
ReverbEditor::ReverbEditor(ReverbPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);

    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // Algorithm combo
    algo_box_.addItem("Dattorro",     1);
    algo_box_.addItem("Schroeder",   2);
    algo_box_.addItem("FDN",         3);
    algo_box_.addItem("Gardner",     4);
    algo_box_.addItem("Moorer",      5);
    algo_box_.addItem("Velvet Noise",6);
    algo_box_.addItem("Shimmer",     7);
    algo_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(algo_box_);
    algo_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "algorithm", algo_box_);
    algo_box_.onChange = [this] { update_algo_ui(); };

    // Equation label (to the right of combo)
    algo_eq_label_.setFont(Font(10.5f));
    algo_eq_label_.setJustificationType(Justification::centredLeft);
    algo_eq_label_.setColour(Label::textColourId,       Colour(laf_.text_muted()));
    algo_eq_label_.setColour(Label::backgroundColourId, Colour(0x00000000));
    addAndMakeVisible(algo_eq_label_);

    // Main knobs
    setup_knob(pre_delay_knob_, pre_delay_label_, "PRE-DELAY");
    setup_knob(size_knob_,      size_label_,      "SIZE");
    setup_knob(decay_knob_,     decay_label_,     "DECAY");
    setup_knob(damping_knob_,   damping_label_,   "DAMPING");
    setup_knob(diffusion_knob_, diffusion_label_,  "DIFFUSION");
    setup_knob(mod_knob_,       mod_label_,       "MOD 1");
    setup_knob(mod2_knob_,      mod2_label_,      "MOD 2");
    setup_knob(output_knob_,    output_label_,    "OUTPUT");
    setup_knob(mix_knob_,       mix_label_,       "MIX");

    pre_delay_knob_.setTooltip(
        "Pre-Delay (p): time before the reverb tail begins (0-200 ms). "
        "Used in equations as p. Only active in Dattorro.");
    size_knob_.setTooltip(
        "Size (s): scales all internal delay lengths. "
        "Used in equations as s. 0 = smallest room, 1 = largest.");
    decay_knob_.setTooltip(
        "Decay (g): feedback gain controlling the RT60 tail length. "
        "Used in equations as g. Maps to g = decay * 0.97.");
    damping_knob_.setTooltip(
        "Damping (d): LP cutoff in the feedback path. "
        "Used in equations as d. 0 = dark (500 Hz), 1 = bright (20 kHz). "
        "Higher d passes more high frequencies through the feedback loop.");
    diffusion_knob_.setTooltip(
        "Diffusion (a): allpass coefficient controlling echo density. "
        "Used in equations as a. Range 0.4-0.9. "
        "Low = sparse onset, high = dense smeared onset.");
    mod_knob_.setTooltip(
        "Mod 1 (m1): LFO rate for pitch detuning modulation. "
        "Used in equations as m1. Range 0.05-2 Hz. "
        "Controls how fast the delay modulation cycles. Active for all algorithms.");
    mod2_knob_.setTooltip(
        "Mod 2 (m2): LFO depth controlling detuning amount (0-16 samples). "
        "Used in equations as m2. 0 = no pitch variation. "
        "At maximum, produces ~4 cents peak pitch deviation. Active for all algorithms.");
    output_knob_.setTooltip(
        "Output: post-mix output gain. -20 dB to +6 dB.");
    mix_knob_.setTooltip(
        "Mix (w): wet/dry blend. out = in + w*(wet - in). "
        "0 = fully dry, 1 = fully wet.");

    pre_delay_attach_ = std::make_unique<Attachment>(apvts, "pre_delay",  pre_delay_knob_);
    size_attach_      = std::make_unique<Attachment>(apvts, "size",       size_knob_);
    decay_attach_     = std::make_unique<Attachment>(apvts, "decay",      decay_knob_);
    damping_attach_   = std::make_unique<Attachment>(apvts, "damping",    damping_knob_);
    diffusion_attach_ = std::make_unique<Attachment>(apvts, "diffusion",  diffusion_knob_);
    mod_attach_       = std::make_unique<Attachment>(apvts, "mod",        mod_knob_);
    mod2_attach_      = std::make_unique<Attachment>(apvts, "mod2",       mod2_knob_);
    output_attach_    = std::make_unique<Attachment>(apvts, "output",     output_knob_);
    mix_attach_       = std::make_unique<Attachment>(apvts, "mix",        mix_knob_);

    // Filter section
    filter_on_btn_.setButtonText("FILTER");
    filter_on_btn_.setClickingTogglesState(true);
    filter_on_btn_.setColour(TextButton::buttonColourId,   Colour(laf_.surface()));
    filter_on_btn_.setColour(TextButton::buttonOnColourId, Colour(laf_.accent_colour()).withAlpha(0.35f));
    filter_on_btn_.setColour(TextButton::textColourOffId,  Colour(laf_.text_muted()));
    filter_on_btn_.setColour(TextButton::textColourOnId,   Colour(laf_.text_primary()));
    addAndMakeVisible(filter_on_btn_);
    filter_on_attach_ = std::make_unique<ButtonAttachment>(apvts, "filter_on", filter_on_btn_);

    filter_pos_box_.addItem("Pre",  1);
    filter_pos_box_.addItem("Post", 2);
    filter_pos_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(filter_pos_box_);
    filter_pos_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "filter_pos", filter_pos_box_);

    filter_type_box_.addItem("LP", 1);
    filter_type_box_.addItem("HP", 2);
    filter_type_box_.addItem("BP", 3);
    filter_type_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(filter_type_box_);
    filter_type_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "filter_type", filter_type_box_);

    setup_knob(cutoff_knob_,    cutoff_label_,    "CUTOFF");
    setup_knob(resonance_knob_, resonance_label_,  "RESONANCE");
    setup_knob(blend_knob_,     blend_label_,     "BLEND");

    cutoff_attach_    = std::make_unique<Attachment>(apvts, "filter_cutoff", cutoff_knob_);
    resonance_attach_ = std::make_unique<Attachment>(apvts, "filter_res",    resonance_knob_);
    blend_attach_     = std::make_unique<Attachment>(apvts, "filter_blend",  blend_knob_);

    cutoff_knob_.onValueChange    = [this] { repaint(); };
    resonance_knob_.onValueChange = [this] { repaint(); };
    blend_knob_.onValueChange     = [this] { repaint(); };
    filter_type_box_.onChange     = [this] { repaint(); };
    filter_on_btn_.onClick        = [this] { update_filter_ui(); repaint(); };
    filter_pos_box_.onChange      = [this] { repaint(); };

    update_filter_ui();
    update_algo_ui();
}

ReverbEditor::~ReverbEditor()
{
    setLookAndFeel(nullptr);
}

void ReverbEditor::setup_knob(Slider& knob, Label& label, const String& name)
{
    knob.setSliderStyle(Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 14);
    knob.setPopupDisplayEnabled(true, false, this);
    addAndMakeVisible(knob);

    label.setText(name, dontSendNotification);
    label.setFont(Font(10.0f));
    label.setJustificationType(Justification::centred);
    label.setColour(Label::textColourId, Colour(laf_.text_primary()));
    addAndMakeVisible(label);
}

// ── Layout ─────────────────────────────────────────────────────────────────────
void ReverbEditor::resized()
{
    const int w = getWidth();

    // Algorithm selector + equation
    algo_box_.setBounds(kPadX, kAlgoY, kAlgoW, kAlgoH);
    algo_eq_label_.setBounds(kPadX + kAlgoW + 10, kAlgoY, w - kPadX - kAlgoW - 20, kAlgoH);

    // 9 knobs across the top section
    const int num_knobs = 9;
    const int slot_w    = (w - kPadX * 2) / num_knobs;
    auto place_knob = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * slot_w + slot_w / 2;
        knob.setBounds(cx - kKnobSize / 2, kKnobY, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kKnobY + kKnobSize + 2, 80, kLabelH);
    };
    place_knob(pre_delay_knob_,  pre_delay_label_,  0);
    place_knob(size_knob_,       size_label_,       1);
    place_knob(decay_knob_,      decay_label_,      2);
    place_knob(damping_knob_,    damping_label_,    3);
    place_knob(diffusion_knob_,  diffusion_label_,  4);
    place_knob(mod_knob_,        mod_label_,        5);
    place_knob(mod2_knob_,       mod2_label_,       6);
    place_knob(output_knob_,     output_label_,     7);
    place_knob(mix_knob_,        mix_label_,        8);

    // Filter header
    filter_on_btn_.setBounds  (kPadX,       kFilterHeaderY, 65, 22);
    filter_pos_box_.setBounds (kPadX + 73,  kFilterHeaderY, 72, 22);
    filter_type_box_.setBounds(kPadX + 153, kFilterHeaderY, 60, 22);

    // Filter knobs
    const int fslot_w = (w - kPadX * 2) / 3;
    auto place_fknob = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * fslot_w + fslot_w / 2;
        knob.setBounds(cx - kKnobSize / 2, kFilterKnobY, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kFilterKnobY + kKnobSize + 2, 80, kLabelH);
    };
    place_fknob(cutoff_knob_,    cutoff_label_,    0);
    place_fknob(resonance_knob_, resonance_label_,  1);
    place_fknob(blend_knob_,     blend_label_,     2);
}

// ── Painting ───────────────────────────────────────────────────────────────────
void ReverbEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    // Separator above knobs
    g.setColour(Colour(laf_.border()));
    g.fillRect(kPadX, kKnobY - 8, getWidth() - kPadX * 2, 1);

    // Separator above filter section
    g.fillRect(kPadX, kFilterSepY, getWidth() - kPadX * 2, 1);

    // Filter frequency response display
    const Rectangle<int> display(kPadX, kFilterDisplayY,
                                 getWidth() - kPadX * 2, kFilterDisplayH);
    draw_filter_response(g, display);

    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::reverb", kPadX, getHeight() - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Filter frequency response display ──────────────────────────────────────────
void ReverbEditor::draw_filter_response(Graphics& g, Rectangle<int> area)
{
    const int   w  = area.getWidth();
    const int   h  = area.getHeight();
    const float ax = (float)area.getX();
    const float ay = (float)area.getY();

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float db_min = -40.0f, db_max = 20.0f, db_range = db_max - db_min;
    const bool  on     = filter_on_btn_.getToggleState();

    g.setColour(Colour(laf_.border()).withAlpha(0.6f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f }) {
        const float y = ay + h * (db_max - db) / db_range;
        g.drawLine(ax, y, ax + w, y, 0.5f);
    }
    g.setFont(Font(9.0f));
    for (auto [freq, label] : { std::pair{100.0f,"100"}, {1000.0f,"1k"}, {10000.0f,"10k"} }) {
        const float x = ax + w * std::log10(freq / 20.0f) / 3.0f;
        g.setColour(Colour(laf_.border()).withAlpha(0.6f));
        g.drawLine(x, ay, x, ay + h, 0.5f);
        g.setColour(Colour(laf_.text_muted()).withAlpha(0.7f));
        g.drawText(label, (int)x - 12, area.getBottom() - 14, 24, 12,
                   Justification::centred);
    }

    const float fc    = (float)cutoff_knob_.getValue();
    const float q     = (float)resonance_knob_.getValue();
    const float blend = (float)blend_knob_.getValue();
    const int   type  = filter_type_box_.getSelectedItemIndex();

    Path curve;
    for (int px = 0; px < w; ++px) {
        const float freq  = 20.0f * std::pow(1000.0f, (float)px / w);
        const float ratio = freq / std::max(fc, 1.0f);
        const float r2    = ratio * ratio;
        const float denom = std::sqrt((1.0f - r2) * (1.0f - r2)
                                    + r2 / (q * q)) + 1e-12f;
        float mag = 1.0f;
        if (on) {
            float raw_mag;
            switch (type) {
                case 0: raw_mag = 1.0f / denom;        break; // LP
                case 1: raw_mag = r2   / denom;        break; // HP
                case 2: raw_mag = (ratio/q) / denom;   break; // BP
                default: raw_mag = 1.0f;
            }
            const float db_blended = blend * 20.0f * std::log10(std::max(raw_mag, 1e-9f));
            mag = std::pow(10.0f, db_blended / 20.0f);
        }
        const float db = 20.0f * std::log10(std::max(mag, 1e-9f));
        const float fy = ay + h * std::clamp((db_max - db) / db_range, 0.0f, 1.0f);
        if (px == 0) curve.startNewSubPath(ax, fy);
        else         curve.lineTo(ax + px, fy);
    }

    Path filled = curve;
    filled.lineTo(ax + w, ay + h);
    filled.lineTo(ax,     ay + h);
    filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(on ? 0.13f : 0.04f));
    g.fillPath(filled);

    g.setColour(Colour(laf_.accent_colour()).withAlpha(on ? 0.85f : 0.25f));
    g.strokePath(curve, PathStrokeType(1.5f, PathStrokeType::curved,
                                       PathStrokeType::rounded));

    if (on && fc > 0.0f) {
        const float cx = ax + w * std::log10(fc / 20.0f) / 3.0f;
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.35f));
        g.drawLine(cx, ay, cx, ay + h, 1.0f);
    }

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
}

// ── Algorithm description + tooltip ───────────────────────────────────────────
void ReverbEditor::update_algo_ui()
{
    struct AlgoInfo {
        const char* desc;  // qualitative one-liner shown beside the combo box
        const char* tip;   // sound character + full step-by-step on hover
    };
    static const AlgoInfo kInfo[] = {
        {   // Dattorro
            "Plate reverb -- smooth, bright, dense tail. Emulates a vibrating metal sheet.",
            "DATTORRO PLATE (Dattorro 1997)\n"
            "\n"
            "Sound: Smooth, bright, and uniformly dense -- the classic studio plate sound.\n"
            "Emulates a large sheet of metal suspended in a frame and driven by a transducer,\n"
            "originally used in studios from the 1950s. Excellent stereo spread, no flutter\n"
            "echoes, and a musical non-intrusive tail. Best for vocals, snare, and melodic\n"
            "instruments. m1 sets the LFO rate; m2 sets the detuning depth applied to the\n"
            "tank allpass delays, breaking up metallic resonances for a smoother tail.\n"
            "\n"
            "Signal flow:\n"
            "  y1 = D(x, p)                           pre-delay p ms\n"
            "  y2 = LP(y1, d)                          input bandwidth LP, cutoff d\n"
            "  y3 = AP(AP(AP(AP(y2,a,s),a,s),a,s),a,s)  4x allpass diffusion\n"
            "  -- Tank L (mirror for R; cross-fed: in_L = y3 + g*fbR) --\n"
            "  y4  = AP_m(y3 + g*fbR, a, s, m1, m2)  modulated allpass, rate m1, depth m2\n"
            "  y5  = LP(y4, d) * g                     damp by d, attenuate by g\n"
            "  y6  = AP(y5, a, s)                      second allpass\n"
            "  fbL = D(y6, s)                           post-delay -> L feedback\n"
            "  outL = 0.6*fbR + 0.4*tap(fbL,s/3)      cross-stereo tap\n"
            "\n"
            "Knob symbols:  p=PRE-DELAY  s=SIZE  g=DECAY  d=DAMPING  a=DIFFUSION\n"
            "               m1=MOD 1 (LFO rate 0.05-2 Hz)  m2=MOD 2 (detune depth 0-16 smp)\n"
            "\n"
            "Character guide (m2=0 unless noted):\n"
            "  Plate : p=20ms s=0.50 g=0.55 d=0.65 a=0.80 m1=0.25  bright dense\n"
            "  Room  : p=0ms  s=0.25 g=0.35 d=0.55 a=0.65 m1=0.15  small natural\n"
            "  Hall  : p=25ms s=0.80 g=0.70 d=0.40 a=0.75 m1=0.30  large warm\n"
            "  Cave  : p=35ms s=1.00 g=0.85 d=0.20 a=0.70 m1=0.35  dark cavernous"
        },
        {   // Schroeder
            "Schroeder (1962) -- colored, ringy. First digital reverb. Raise DIFFUSION to tame ringing.",
            "SCHROEDER REVERBERATOR (Schroeder 1962)\n"
            "\n"
            "Sound: Functional but distinctly colored -- the first algorithmic reverb ever built.\n"
            "Parallel comb filters create audible resonant peaks at multiples of 1/delay Hz,\n"
            "giving the tail a characteristic metallic ringing quality. This coloration was\n"
            "acceptable in the 1960s and is now used deliberately for retro or lo-fi textures,\n"
            "sci-fi sound design, and special effects. Less smooth than Dattorro or FDN, but\n"
            "simpler and historically significant. Raise DIFFUSION to increase allpass smoothing.\n"
            "m1 sets the LFO rate; m2 sets the detuning depth applied to the allpass stages.\n"
            "\n"
            "Signal flow:\n"
            "  c_k[n] = x[n] + g * LP(c_k[n - L_k*s], d)     comb k (k=1..4)\n"
            "           L_k = {1307,1637,1871,2273} samples at 44.1kHz, scaled by s\n"
            "  y1  = (c1 + c2 + c3 + c4) / 4                  sum combs\n"
            "  y2  = AP_m(y1, a, s, m1, m2)                   allpass 1, rate m1, depth m2\n"
            "  out = AP_m(y2, a, s, m1, m2)                   allpass 2, rate m1, depth m2\n"
            "\n"
            "Knob symbols:  s=SIZE  g=DECAY  d=DAMPING  a=DIFFUSION\n"
            "               m1=MOD 1 (LFO rate 0.05-2 Hz)  m2=MOD 2 (detune depth 0-16 smp)\n"
            "PRE-DELAY is unused. R channel uses L_k * 1.03 for stereo decorrelation."
        },
        {   // FDN
            "FDN hall reverb -- clean, smooth, diffuse. Modern character. Closest to convolution.",
            "FEEDBACK DELAY NETWORK -- 4 lines, Hadamard mixing\n"
            "\n"
            "Sound: Clean, transparent, and uniformly diffuse -- the closest an algorithmic\n"
            "reverb gets to convolution quality without an impulse response. The lossless\n"
            "Hadamard matrix ensures all delay lines exchange energy equally, preventing the\n"
            "modal colorations of Schroeder combs. Suited to large halls, orchestral spaces,\n"
            "and any context where the reverb should add space without drawing attention to\n"
            "itself. Good default for instruments that need space without character.\n"
            "m1 sets the LFO rate; m2 sets the detuning depth applied to each delay line read.\n"
            "\n"
            "Signal flow:\n"
            "  s_k[n] = LP(D_k[n - L_k*s + m2*sin(m1*t + ph_k)], d) * g   LFO-modulated read\n"
            "           L_k = {1051,1307,1559,2003} samples at 44.1kHz, scaled by s\n"
            "           ph_k = {0, pi/2, pi, 3pi/2} -- staggered phases per line\n"
            "  t = H4 * s                                Hadamard 4x4 lossless mix\n"
            "       t0=0.5*(s0+s1+s2+s3)  t1=0.5*(s0-s1+s2-s3)\n"
            "       t2=0.5*(s0+s1-s2-s3)  t3=0.5*(s0-s1-s2+s3)\n"
            "  D_k[n+1] = t_k + x                       inject mono input\n"
            "  outL = (s0+s2)/2;  outR = (s1+s3)/2      cross-pair stereo taps\n"
            "\n"
            "Knob symbols:  s=SIZE  g=DECAY  d=DAMPING\n"
            "               m1=MOD 1 (LFO rate 0.05-2 Hz)  m2=MOD 2 (detune depth 0-16 smp)\n"
            "DIFFUSION and PRE-DELAY are unused by this algorithm."
        },
        {   // Gardner
            "Gardner room (1992) -- warm, intimate, natural. Distinct early reflections.",
            "GARDNER ROOM REVERB (Gardner 1992)\n"
            "\n"
            "Sound: Warm, natural, and intimate -- designed specifically to emulate small-to-\n"
            "medium rooms rather than large halls or artificial plate sounds. The nested allpass\n"
            "feedback loop creates a dense cluster of early reflections that blends organically\n"
            "into the tail, giving a sense of physical space rather than a diffuse wash. More\n"
            "transparent than Schroeder (no comb-filter coloring) but more characterful than\n"
            "FDN. Good for drums, acoustic guitar, piano, and anything needing room ambience.\n"
            "m1 sets the LFO rate; m2 sets the detuning depth applied to the allpass stages.\n"
            "\n"
            "Signal flow:\n"
            "  y1 = x + g * fb                            add feedback (gain g)\n"
            "  y2 = AP_m(y1, a, s, m1, m2)               allpass 1, rate m1, depth m2\n"
            "  y3 = AP_m(y2, a, s, m1, m2)               allpass 2, phase +120 deg\n"
            "  y4 = AP_m(y3, a, s, m1, m2)               allpass 3, phase +240 deg\n"
            "  y5 = LP(y4, d)                              damping LP, cutoff d\n"
            "  fb = D(y5, s)                               post-delay length s\n"
            "  out = fb                                    output tap\n"
            "\n"
            "Knob symbols:  s=SIZE  g=DECAY  d=DAMPING  a=DIFFUSION\n"
            "               m1=MOD 1 (LFO rate 0.05-2 Hz)  m2=MOD 2 (detune depth 0-16 smp)\n"
            "PRE-DELAY is unused. R delays scaled by 1.03 for stereo decorrelation."
        },
        {   // Moorer
            "Moorer (1979) -- early reflections + Schroeder tail. Most natural-sounding classic.",
            "MOORER REVERBERATOR (Moorer 1979)\n"
            "\n"
            "Sound: The most perceptually accurate of the classic algorithmic reverbs. Adds an\n"
            "explicit early-reflection stage before the diffuse tail, which is the key cue for\n"
            "room size and distance. The 8-tap tapped delay line models discrete wall bounces;\n"
            "taps alternate in sign to simulate phase inversions at each reflection. The late\n"
            "tail is a Schroeder comb+allpass network with LP damping. More expensive than\n"
            "Schroeder but noticeably more realistic. Suited to acoustic instruments, choral\n"
            "music, and any context where source distance needs to feel believable.\n"
            "m1 sets LFO rate; m2 sets detuning depth applied to the allpass diffusion stages.\n"
            "\n"
            "Signal flow:\n"
            "  early = sum_k(tap_k * D(mono, L_k*s))     8 taps, L_k = {190..2400} @ 44.1kHz\n"
            "          tap_k = (-1)^k * 0.2 * 0.8^k       alternating signs, exponential decay\n"
            "  c_k[n] = early + g * LP(c_k[n-L_k*s], d)  4 parallel combs (k=1..4)\n"
            "  y1  = (c1+c2+c3+c4) / 4                   sum combs\n"
            "  y2  = AP_m(y1, a, s, m1, m2)              allpass 1, rate m1, depth m2\n"
            "  out = AP_m(y2, a, s, m1, m2)              allpass 2, phase +90 deg\n"
            "\n"
            "Knob symbols:  s=SIZE  g=DECAY  d=DAMPING  a=DIFFUSION\n"
            "               m1=MOD 1 (LFO rate 0.05-2 Hz)  m2=MOD 2 (detune depth 0-16 smp)\n"
            "PRE-DELAY is unused. R comb delays scaled by 1.03 for stereo decorrelation."
        },
        {   // Velvet Noise
            "Velvet Noise -- multiplication-free sparse FIR. Clean, colourless, noise-like tail.",
            "VELVET NOISE REVERB\n"
            "\n"
            "Sound: Uniquely clean and colourless -- no comb resonances, no modal ringing,\n"
            "no allpass smear. The impulse response is a sparse sequence of +1 and -1 pulses\n"
            "at pseudo-random positions, with an exponential -60 dB envelope over the tail\n"
            "length. Convolving with this sequence requires only additions and sign flips (no\n"
            "multiplications), making it extremely efficient. The result sounds like late-field\n"
            "statistical noise rather than physical reflections -- abstract and spacious. Works\n"
            "well for long ambient tails, experimental textures, or as a clean diffuse bed\n"
            "under a separate early-reflection stage. DIFFUSION controls pulse density.\n"
            "\n"
            "Signal flow:\n"
            "  vn[k] = (+/-1) * env(pos_k)               pre-generated pulse sequence\n"
            "          pos_k in [k*seg, (k+1)*seg)        one pulse per time segment\n"
            "          env(p) = 10^(-3*p/tail)            -60 dB at tail = (0.3+s*1.7) sec\n"
            "          seg = sr / (density * (0.5+a*0.5)) density = 500-1000 pulses/sec\n"
            "  y[n]  = sum_k(vn_k * buf[n - pos_k])      sparse FIR convolution\n"
            "  out   = LP(y, d)                           LP damping, cutoff d\n"
            "\n"
            "Knob symbols:  s=SIZE (tail/RT60)  d=DAMPING  a=DIFFUSION (pulse density)\n"
            "DECAY, MOD 1, MOD 2, PRE-DELAY are unused by this algorithm.\n"
            "L/R decorrelated by independent sign sequences from separate random seeds."
        },
        {   // Shimmer
            "Shimmer -- Dattorro plate with granular octave-up pitch shift in the feedback loop.",
            "SHIMMER REVERB\n"
            "\n"
            "Sound: Ethereal, angelic, and self-generating. A Dattorro plate reverb with a\n"
            "granular pitch shifter inside the feedback loop. Each pass through the loop raises\n"
            "the pitch by up to +12 semitones (one octave), creating an infinite harmonic ascent\n"
            "that accumulates over time. At low MOD 1 the shimmer is a subtle brightening; at\n"
            "high MOD 1 with high DECAY the reverb becomes self-sustaining and swells even after\n"
            "the source stops. Best for sustained pads, guitar held notes, and cinematic effects.\n"
            "MOD 1 controls the shimmer mix (0 = pure plate, 1 = full pitch-shifted feedback).\n"
            "MOD 2 controls the pitch interval (0 = unison, 1 = +12 semitones = octave up).\n"
            "\n"
            "Signal flow:\n"
            "  -- Granular pitch shifter on feedback (two overlapping grains) --\n"
            "  pa = grain_phase mod gs;  pb = (pa + gs/2) mod gs  two phases 180 deg apart\n"
            "  ea = 1 - |pa/gs*2 - 1|;  eb = 1 - |pb/gs*2 - 1|  triangular windows\n"
            "  pf = 2^m2  (m2=0 -> 1.0x, m2=1 -> 2.0x = +12 st)\n"
            "  pitched = ea*grain[pf*(gs-pa)] + eb*grain[pf*(gs-pb)]  interpolated read\n"
            "  fb = (1-m1)*tank_fb + m1*pitched  blend shimmer into Dattorro cross-feedback\n"
            "  -- Dattorro plate with modified feedback fb --\n"
            "  (same signal flow as Dattorro; see Dattorro tooltip for details)\n"
            "  grain[n+1] = tank_fb[n]  write new feedback into grain buffer each sample\n"
            "\n"
            "Knob symbols:  p=PRE-DELAY  s=SIZE  g=DECAY  d=DAMPING  a=DIFFUSION\n"
            "               m1=MOD 1 (shimmer mix 0-1)  m2=MOD 2 (pitch 0 to +12 semitones)"
        },
    };

    const int idx = algo_box_.getSelectedItemIndex();
    if (idx >= 0 && idx < 7) {
        algo_eq_label_.setText(kInfo[idx].desc, juce::dontSendNotification);
        algo_eq_label_.setTooltip(kInfo[idx].tip);
    }
}

// ── Filter section enable/disable ─────────────────────────────────────────────
void ReverbEditor::update_filter_ui()
{
    const bool on = filter_on_btn_.getToggleState();

    filter_pos_box_.setEnabled(on);
    filter_type_box_.setEnabled(on);
    cutoff_knob_.setEnabled(on);
    resonance_knob_.setEnabled(on);
    blend_knob_.setEnabled(on);

    const auto col = [&](bool active) {
        return Colour(active ? laf_.text_primary() : laf_.text_muted());
    };
    cutoff_label_.setColour   (Label::textColourId, col(on));
    resonance_label_.setColour(Label::textColourId, col(on));
    blend_label_.setColour    (Label::textColourId, col(on));
}

} // namespace kaos_engine
