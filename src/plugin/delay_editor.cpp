#include "delay_editor.h"
#include "delay_plugin.h"

namespace kaos_engine {

using namespace juce;

// ── Formulas and descriptions (indexed by DelayMode) ──────────────────────────
// d = TIME,  g = FEEDBACK,  m1 = MOD 1,  m2 = MOD 2

struct DelayModeInfo { const char* desc; const char* tip; };
static const DelayModeInfo kDelayModes[] = {
    {   // Standard
        "Classic feedback delay. m1 = LP cutoff darkening each repeat (500 Hz-20 kHz).",
        "STANDARD DELAY\n"
        "\n"
        "Sound: Classic feedback delay with a low-pass filter in the feedback path. m1\n"
        "controls the LP cutoff so high frequencies decay faster at lower settings,\n"
        "producing a warm darkening tail similar to magnetic tape echo.\n"
        "\n"
        "Signal flow:\n"
        "  y[n] = x[n-d] + g * LP(y[n-d], m1)   delay d, feedback g, LP cutoff m1\n"
        "  out[n] = x[n] + w*(y[n] - x[n])        wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (LP cutoff)  w=MIX"
    },
    {   // Slapback
        "Single echo, no repeats. Classic rockabilly/drum presence. No feedback.",
        "SLAPBACK\n"
        "\n"
        "Sound: A single short echo with no repeats. Classic in 1950s rockabilly vocals\n"
        "and drums -- adds presence and space without cluttering the mix.\n"
        "Works best under 200 ms. MOD 1 and MOD 2 are inactive.\n"
        "\n"
        "Signal flow:\n"
        "  y[n] = x[n-d]                           single echo at delay d\n"
        "  out[n] = x[n] + w*(y[n] - x[n])         wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  w=MIX  (FEEDBACK, MOD 1, MOD 2 inactive)"
    },
    {   // Ping-Pong
        "L/R alternating bounces. m1=cross-feed between channels, m2=LP cutoff per bounce.",
        "PING-PONG\n"
        "\n"
        "Sound: The delayed signal bounces alternately between left and right channels.\n"
        "m1 controls the cross-feedback amount between L and R. m2 controls the LP filter\n"
        "cutoff in the feedback path, darkening each successive bounce at lower settings.\n"
        "\n"
        "Signal flow:\n"
        "  L[n] = x[n-d] + m1*R[n-d] + g*LP(L[n-d], m2)   left with cross-feed m1\n"
        "  R[n] = x[n-d] + m1*L[n-d] + g*LP(R[n-d], m2)   right with cross-feed m1\n"
        "  out_L = x_L + w*(L[n]-x_L);  out_R = x_R + w*(R[n]-x_R)\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (cross-feed)  m2=MOD 2 (LP)  w=MIX"
    },
    {   // Tape
        "Tape echo -- LFO wow/flutter (m1=rate). tanh saturation warms the feedback path.",
        "TAPE ECHO\n"
        "\n"
        "Sound: Emulates a tape echo machine. m1 controls the LFO rate for wow/flutter\n"
        "pitch drift (0.05-5 Hz). m2 controls the LP filter cutoff in the feedback path.\n"
        "tanh saturation in the feedback path adds warmth and prevents harsh self-oscillation\n"
        "at high feedback settings.\n"
        "\n"
        "Signal flow:\n"
        "  phi[n] = d + depth*sin(m1_rate * n)     wow/flutter LFO, rate m1\n"
        "  y[n] = x[n-phi[n]] + g*tanh(LP(y[n-phi[n]], m2))  delay + saturated feedback\n"
        "  out[n] = x[n] + w*(y[n] - x[n])         wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (LFO rate)  m2=MOD 2 (LP)  w=MIX"
    },
    {   // Diffusion
        "Allpass pre-diffusor before delay. m1=allpass coeff, m2=LP. Dense reverb tail.",
        "DIFFUSION DELAY\n"
        "\n"
        "Sound: Input passes through four allpass filters (Schroeder diffusor) before\n"
        "entering the delay line. m1 controls the allpass coefficient, varying echo density\n"
        "from sparse reflections (low) to a dense reverb-like tail (high). m2 controls the\n"
        "LP filter cutoff in the feedback path.\n"
        "\n"
        "Signal flow:\n"
        "  d1[n] = AP(x[n], L1, m1)                allpass 1, coeff m1\n"
        "  d2[n] = AP(d1[n], L2, m1)               allpass 2\n"
        "  d3[n] = AP(d2[n], L3, m1)               allpass 3\n"
        "  d4[n] = AP(d3[n], L4, m1)               allpass 4\n"
        "  y[n] = d4[n-d] + g*LP(y[n-d], m2)       delay d, feedback g, LP m2\n"
        "  out[n] = x[n] + w*(y[n] - x[n])         wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (AP coeff)  m2=MOD 2 (LP)  w=MIX"
    },
    {   // Reverse
        "Plays last d ms backward before dry. Ghostly rising swell. No modulation.",
        "REVERSE DELAY\n"
        "\n"
        "Sound: Reads the delay buffer backward -- the last d milliseconds of audio\n"
        "play in reverse before the direct signal arrives. Creates ghostly reverse-echo\n"
        "effects. Feedback gently recirculates the reversed signal. MOD 1 and MOD 2 inactive.\n"
        "\n"
        "Signal flow:\n"
        "  y[n] = buf[write_pos - (d - (n mod d))]  read buffer in reverse\n"
        "  y[n] += g * y_prev                        optional feedback g\n"
        "  out[n] = x[n] + w*(y[n] - x[n])          wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  w=MIX  (MOD 1, MOD 2 inactive)"
    },
    {   // Comb
        "Resonant comb filter. Metallic peaks at 1/d Hz. m1=LP cutoff. Flanger at short d.",
        "COMB FILTER\n"
        "\n"
        "Sound: A resonant comb filter. m1 controls the LP filter cutoff (500 Hz-20 kHz):\n"
        "low settings give a dark muffled resonance; high settings produce bright metallic\n"
        "coloration. At short delay times (1-50 ms) the resonance creates pitched artifacts\n"
        "similar to a flanger at extreme settings.\n"
        "\n"
        "Signal flow:\n"
        "  y[n] = x[n] + g*LP(y[n-d], m1)          comb: delay d, feedback g, LP cutoff m1\n"
        "  Resonant peaks at f_k = k/d Hz, k=1,2,...\n"
        "  out[n] = x[n] + w*(y[n] - x[n])          wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (LP cutoff)  w=MIX"
    },
    {   // Multi-Tap
        "3 read heads at d, 1.5d, 2d. Rhythmic echoes. m1=tap balance, m2=LP cutoff.",
        "MULTI-TAP DELAY\n"
        "\n"
        "Sound: Three read heads at 1x, 1.5x, and 2x the delay time create rhythmically\n"
        "related echoes. m1 blends the tap levels: at zero the three taps are equal weight;\n"
        "at maximum the later taps are emphasized. m2 controls the LP filter cutoff in\n"
        "the feedback path.\n"
        "\n"
        "Signal flow:\n"
        "  tap1[n] = x[n-d]            first tap at d\n"
        "  tap2[n] = x[n-3d/2]         second tap at 1.5d\n"
        "  tap3[n] = x[n-2d]           third tap at 2d\n"
        "  y[n] = tap1 + m1*tap2 + m1^2*tap3 + g*LP(y[n-d], m2)  blended + feedback\n"
        "  out[n] = x[n] + w*(y[n] - x[n])  wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (tap balance)  m2=MOD 2 (LP)  w=MIX"
    },
    {   // Shimmer
        "Octave-up pitch shift in feedback. m1=shimmer amount, m2=LP cutoff.",
        "SHIMMER\n"
        "\n"
        "Sound: Pitch-shifted feedback: a second delay head reads at half the buffer speed,\n"
        "shifting the feedback signal up one octave using a sawtooth-crossfade grain.\n"
        "m1 controls how much of the octave-up signal mixes into the feedback loop.\n"
        "m2 controls the LP filter cutoff in the feedback path.\n"
        "\n"
        "Signal flow:\n"
        "  pitch[n] = octave_up(y[n-d])              +12 st grain shift of feedback\n"
        "  fb[n] = m1*pitch[n] + (1-m1)*LP(y[n-d], m2)  blend shifted + direct\n"
        "  y[n] = x[n-d] + g*fb[n]                   delay + blended feedback\n"
        "  out[n] = x[n] + w*(y[n] - x[n])           wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (shimmer)  m2=MOD 2 (LP)  w=MIX"
    },
    {   // Haas
        "Haas effect -- R delayed d ms (<40ms). Stereo width without an audible echo.",
        "HAAS (PRECEDENCE) EFFECT\n"
        "\n"
        "Sound: Left channel outputs the dry signal; right channel is delayed by d ms.\n"
        "Delays under 40 ms are perceived as stereo width rather than a distinct echo\n"
        "(the precedence effect). No feedback, MOD 1, or MOD 2. Best on mono sources.\n"
        "\n"
        "Signal flow:\n"
        "  L[n] = x[n]                               left = direct dry signal\n"
        "  R[n] = x[n-d]                             right = delayed d ms\n"
        "  No feedback. Keep d < 40 ms for width (not echo).\n"
        "  out_L = x + w*(L-x);  out_R = x + w*(R-x)\n"
        "\n"
        "Knob symbols: d=TIME  w=MIX  (FEEDBACK, MOD 1, MOD 2 inactive)"
    },
    {   // BBD
        "Bucket brigade warble. m1=LFO rate. Fixed 3kHz LP mimics clock artefact roll-off.",
        "BBD (BUCKET-BRIGADE DEVICE) EMULATION\n"
        "\n"
        "Sound: Emulates the warm, wobbly character of analog BBD chips (MN3007 etc.).\n"
        "m1 controls the LFO rate (clock modulation) for authentic chorus-like wow and\n"
        "flutter. A fixed 3 kHz LP on the output mimics the clock-artefact roll-off of\n"
        "real BBD chips. MOD 2 is inactive (the LP cannot be user-controlled in BBD mode).\n"
        "\n"
        "Signal flow:\n"
        "  phi[n] = d + depth*sin(m1_rate * n)       clock-rate LFO, rate m1\n"
        "  y[n] = LP_3kHz(x[n-phi[n]]) + g*y[n-phi[n]]  BBD read + feedback\n"
        "  out[n] = x[n] + w*(y[n] - x[n])           wet/dry MIX w\n"
        "\n"
        "Knob symbols: d=TIME  g=FEEDBACK  m1=MOD 1 (LFO rate)  w=MIX  (MOD 2 inactive)"
    },
};
static_assert(std::size(kDelayModes) == 11, "must match DelayMode count");

// ── Construction ───────────────────────────────────────────────────────────────
DelayEditor::DelayEditor(DelayPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);

    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    mode_box_.addItem("Standard",        1);
    mode_box_.addItem("Slapback",        2);
    mode_box_.addItem("Ping-Pong",       3);
    mode_box_.addItem("Tape",            4);
    mode_box_.addItem("Diffusion",       5);
    mode_box_.addItem("Reverse",         6);
    mode_box_.addItem("Comb",            7);
    mode_box_.addItem("Multi-Tap",       8);
    mode_box_.addItem("Shimmer",         9);
    mode_box_.addItem("Haas",           10);
    mode_box_.addItem("BBD",            11);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "mode", mode_box_);

    // formula_label_ not shown -- description routed to mode_box_ tooltip in update_mode_ui().

    mode_box_.onChange = [this] { update_mode_ui(); };

    setup_knob(time_knob_,     time_label_,     "TIME");
    setup_knob(feedback_knob_, feedback_label_,  "FEEDBACK");
    setup_knob(tone_knob_,     tone_label_,      "TONE");
    setup_knob(mod_knob_,      mod_label_,       "MOD 1");
    setup_knob(mod2_knob_,     mod2_label_,      "MOD 2");
    setup_knob(output_knob_,   output_label_,    "OUTPUT");
    setup_knob(mix_knob_,      mix_label_,       "MIX");

    time_knob_.setTooltip(
        "Time (d): delay length in milliseconds (1 - 2000 ms). "
        "Controls the gap between the dry signal and the first echo. "
        "In Karplus-Strong, sets the pitch (shorter = higher pitch).");

    feedback_knob_.setTooltip(
        "Feedback (g): how much of the output is fed back into the delay input. "
        "Higher values produce more repeats. "
        "Disabled in Slapback and Haas modes.");

    tone_knob_.setTooltip(
        "Tone: unused - LP filter cutoff is now controlled by MOD 1 (Standard, Comb) "
        "or MOD 2 (Ping-Pong, Tape, Diffusion, Multi-Tap, Shimmer, Karplus-Strong).");

    mod_knob_.setTooltip(
        "Mod 1 (m1): mode-specific modulation parameter.\n"
        "Standard / Comb: LP filter cutoff in feedback path (500 Hz - 20 kHz).\n"
        "Ping-Pong: cross-feedback amount between L and R channels.\n"
        "Tape / BBD: LFO rate for wow/flutter pitch modulation (0.05 - 5 Hz).\n"
        "Diffusion: allpass coefficient controlling echo density (0.1 - 0.9).\n"
        "Multi-Tap: balance between the three tap levels.\n"
        "Shimmer: octave-up blend in the feedback path.\n"
        "Slapback / Reverse / Haas: inactive (greyed out).");

    mod2_knob_.setTooltip(
        "Mod 2 (m2): LP filter cutoff in the feedback path (500 Hz - 20 kHz).\n"
        "Active for: Ping-Pong, Tape, Diffusion, Multi-Tap, Shimmer.\n"
        "Lower values darken each successive repeat; higher values preserve highs.\n"
        "Inactive (greyed out) for Standard, Comb, Slapback, Reverse, Haas, BBD.");

    output_knob_.setTooltip(
        "Output: post-processing gain, -20 dB to +6 dB. "
        "Use to compensate for level changes introduced by the delay effect.");

    mix_knob_.setTooltip(
        "Mix (w): wet/dry blend. out[n] = x[n] + w*(y[n] - x[n]). "
        "Left = fully dry (bypass), Right = fully wet (delay only).");

    time_attach_     = std::make_unique<Attachment>(apvts, "time",     time_knob_);
    feedback_attach_ = std::make_unique<Attachment>(apvts, "feedback", feedback_knob_);
    tone_attach_     = std::make_unique<Attachment>(apvts, "tone",     tone_knob_);
    mod_attach_      = std::make_unique<Attachment>(apvts, "mod",      mod_knob_);
    mod2_attach_     = std::make_unique<Attachment>(apvts, "mod2",     mod2_knob_);
    output_attach_   = std::make_unique<Attachment>(apvts, "output",   output_knob_);
    mix_attach_      = std::make_unique<Attachment>(apvts, "mix",      mix_knob_);

    update_mode_ui();
}

DelayEditor::~DelayEditor()
{
    setLookAndFeel(nullptr);
}

void DelayEditor::setup_knob(Slider& knob, Label& label, const String& name)
{
    knob.setSliderStyle(Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(Slider::TextBoxBelow, false, 54, 13);
    knob.setPopupDisplayEnabled(true, false, this);
    addAndMakeVisible(knob);

    label.setText(name, dontSendNotification);
    label.setFont(Font(8.5f));
    label.setJustificationType(Justification::centred);
    label.setColour(Label::textColourId, Colour(laf_.text_primary()));
    addAndMakeVisible(label);
}

// ── Layout ─────────────────────────────────────────────────────────────────────
void DelayEditor::resized()
{
    const int w = getWidth();

    mode_box_.setBounds(kPadX, kModeY, kModeW, kModeH);

    // 7 knobs: TIME, FEEDBACK, TONE, MOD 1, MOD 2, OUTPUT, MIX
    const int slot_w = (w - kPadX * 2) / 7;
    auto place = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * slot_w + slot_w / 2;
        knob.setBounds(cx - kKnobSize / 2, kKnobY, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kKnobY + kKnobSize + 2, 80, kLabelH);
    };

    place(time_knob_,     time_label_,     0);
    place(feedback_knob_, feedback_label_,  1);
    place(tone_knob_,     tone_label_,      2);
    place(mod_knob_,      mod_label_,       3);
    place(mod2_knob_,     mod2_label_,      4);
    place(output_knob_,   output_label_,    5);
    place(mix_knob_,      mix_label_,       6);
}

// ── Painting ───────────────────────────────────────────────────────────────────
void DelayEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::delay", kPadX, getHeight() - kFooterH - 4,
               280, kFooterH, Justification::centredLeft);
}

// ── Mode-dependent UI ──────────────────────────────────────────────────────────
void DelayEditor::update_mode_ui()
{
    const int idx = mode_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 11) return;

    mode_box_.setTooltip(kDelayModes[idx].tip);

    const auto mode    = static_cast<DelayMode>(idx);
    const bool mod_on  = delay_mode_uses_mod(mode);
    const bool mod2_on = delay_mode_uses_mod2(mode);
    const bool fb_on   = delay_mode_uses_feedback(mode);

    // TONE is always disabled - LP is now controlled by MOD 1 or MOD 2
    tone_knob_.setEnabled(false);
    tone_label_.setEnabled(false);
    tone_label_.setColour(Label::textColourId, Colour(laf_.text_muted()));

    mod_knob_.setEnabled(mod_on);
    mod_label_.setEnabled(mod_on);
    mod_label_.setColour(Label::textColourId,
        Colour(mod_on ? laf_.text_primary() : laf_.text_muted()));

    mod2_knob_.setEnabled(mod2_on);
    mod2_label_.setEnabled(mod2_on);
    mod2_label_.setColour(Label::textColourId,
        Colour(mod2_on ? laf_.text_primary() : laf_.text_muted()));

    feedback_knob_.setEnabled(fb_on);
    feedback_label_.setEnabled(fb_on);
    feedback_label_.setColour(Label::textColourId,
        Colour(fb_on ? laf_.text_primary() : laf_.text_muted()));
}

} // namespace kaos_engine
