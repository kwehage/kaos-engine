#include "frequency_shifter_editor.h"
#include "frequency_shifter_plugin.h"

namespace kaos_engine {

using namespace juce;

// ── Direction info (indexed by FreqShiftDirection) ─────────────────────────────
struct FreqShiftDirInfo { const char* desc; const char* tip; };
static const FreqShiftDirInfo kDirs[] = {
    {   // Up
        "Up-shift: adds +D Hz to every frequency. Inharmonic partials; metallic at large D.",
        "UP SHIFT\n"
        "\n"
        "Sound: Adds a constant +D Hz offset to every frequency component simultaneously.\n"
        "Unlike pitch shifting (which scales by a ratio), frequency shifting destroys\n"
        "harmonic relationships. Below ~5 Hz D produces beating and chorusing. At larger\n"
        "D the signal becomes metallic and bell-like. With FEEDBACK > 0, each delay echo\n"
        "has passed through the shifter again, adding another +D Hz -- creating a Risset\n"
        "endless glissando: a perpetual, infinite pitch ascent.\n"
        "\n"
        "Signal flow:\n"
        "  z[n] = x[n] + g * delay_out[n]          mix input + feedback echo (FEEDBACK g)\n"
        "  I[n] = allpass_path2(z[n])               in-phase component (Hilbert path 2)\n"
        "  Q[n] = allpass_path1(z[n-1])             quadrature component (path 1 + 1-sample delay)\n"
        "  ph[n] += 2*pi * (D + m2*sin(m1*t)) / fs    phasor (SHIFT D, MOD 1 m1, MOD 2 m2)\n"
        "  y[n]   = I[n]*cos(ph) - Q[n]*sin(ph)      SSB up-shift\n"
        "  col[n] = LP(y[n], t)                      TONE LP filter (500Hz-20kHz)\n"
        "  col[n] = tanh(4*k * col[n])               DRIVE saturation (k=DRIVE; 0=bypass)\n"
        "  col[n] = AP2(AP1(col[n], a), a)           DIFFUSION 2-stage allpass (a=DIFFUSION*0.7)\n"
        "  delay_write(col[n])                        write coloured signal to feedback buffer\n"
        "  out[n] = x[n] + w*(y[n] - x[n])          direct y[n] to output; colour is feedback only\n"
        "\n"
        "Knob symbols: D=SHIFT  g=FEEDBACK  d=DELAY  m1=MOD 1  m2=MOD 2  t=TONE  k=DRIVE  a=DIFFUSION  w=MIX"
    },
    {   // Down
        "Down-shift: subtracts D Hz from every frequency. Mirror of up-shift.",
        "DOWN SHIFT\n"
        "\n"
        "Sound: Adds a constant -D Hz offset to every frequency component. Mirror image\n"
        "of up-shift. Partials that cross 0 Hz fold back as aliased negative-frequency\n"
        "content, which can sound interesting on rich harmonic material. With FEEDBACK,\n"
        "creates a Risset descending glissando -- a perpetual infinite pitch descent.\n"
        "\n"
        "Signal flow:\n"
        "  z[n] = x[n] + g * delay_out[n]          mix input + feedback echo (FEEDBACK g)\n"
        "  I[n] = allpass_path2(z[n])               in-phase component\n"
        "  Q[n] = allpass_path1(z[n-1])             quadrature component\n"
        "  ph[n] += 2*pi * (D + m2*sin(m1*t)) / fs    phasor\n"
        "  y[n]   = I[n]*cos(ph) + Q[n]*sin(ph)      SSB down-shift (note: +Q*sin)\n"
        "  col[n] = LP(y[n], t)                      TONE LP filter\n"
        "  col[n] = tanh(4*k * col[n])               DRIVE saturation\n"
        "  col[n] = AP2(AP1(col[n], a), a)           DIFFUSION 2-stage allpass\n"
        "  delay_write(col[n])\n"
        "  out[n] = x[n] + w*(y[n] - x[n])          direct y[n] to output\n"
        "\n"
        "Knob symbols: D=SHIFT  g=FEEDBACK  d=DELAY  m1=MOD 1  m2=MOD 2  t=TONE  k=DRIVE  a=DIFFUSION  w=MIX"
    },
    {   // Both
        "Both: up + down sum. Sidebands at f_in +/- D. Ring-mod character without the phase shift.",
        "BOTH (UP + DOWN)\n"
        "\n"
        "Sound: Sums the up-shifted and down-shifted outputs. The Q*sin terms cancel,\n"
        "leaving I[n]*cos(ph). This produces sidebands at f_in + D and f_in - D,\n"
        "similar to ring modulation, but using the Hilbert in-phase path (I) rather\n"
        "than the raw input. With FEEDBACK and a matched DELAY, produces a symmetrical\n"
        "barberpole phasing effect -- a Shepard-Risset auditory illusion of a filter\n"
        "perpetually sweeping up while also sweeping down.\n"
        "\n"
        "Signal flow:\n"
        "  z[n] = x[n] + g * delay_out[n]          mix input + feedback echo (FEEDBACK g)\n"
        "  I[n] = allpass_path2(z[n])               in-phase component\n"
        "  y_up   = I*cos - Q*sin\n"
        "  y_down = I*cos + Q*sin\n"
        "  y[n]   = (y_up + y_down) / 2 = I[n]*cos(ph)   Q terms cancel\n"
        "  col[n] = LP(y[n], t)                      TONE LP filter\n"
        "  col[n] = tanh(4*k * col[n])               DRIVE saturation\n"
        "  col[n] = AP2(AP1(col[n], a), a)           DIFFUSION 2-stage allpass\n"
        "  delay_write(col[n])\n"
        "  out[n] = x[n] + w*(y[n] - x[n])          direct y[n] to output\n"
        "\n"
        "Knob symbols: D=SHIFT  g=FEEDBACK  d=DELAY  m1=MOD 1  m2=MOD 2  t=TONE  k=DRIVE  a=DIFFUSION  w=MIX"
    },
};
static_assert(std::size(kDirs) == 3, "direction count must match FreqShiftDirection count");

// ── Construction ───────────────────────────────────────────────────────────────
FrequencyShifterEditor::FrequencyShifterEditor(FrequencyShifterPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);

    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // Direction combo
    direction_box_.addItem("Up",   1);
    direction_box_.addItem("Down", 2);
    direction_box_.addItem("Both", 3);
    direction_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(direction_box_);
    direction_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "direction", direction_box_);

    // Formula label
    formula_label_.setFont(Font(11.0f));
    formula_label_.setJustificationType(Justification::centredLeft);
    formula_label_.setColour(Label::textColourId,       Colour(laf_.text_muted()));
    formula_label_.setColour(Label::backgroundColourId, Colour(0x00000000));
    addAndMakeVisible(formula_label_);

    // Feedback loop combo
    feedback_mode_box_.addItem("Single",    1);
    feedback_mode_box_.addItem("Ping-Pong", 2);
    feedback_mode_box_.addItem("Tape",      3);
    feedback_mode_box_.setScrollWheelEnabled(false);
    feedback_mode_box_.setTooltip(
        "Feedback Loop: determines how the feedback echo is read from the delay buffer.\n"
        "\n"
        "Single: each channel reads its own delay buffer. Standard discrete echoes.\n"
        "  Risset glissando: both channels ascend/descend in parallel.\n"
        "\n"
        "Ping-Pong: L reads from the R delay buffer and R reads from the L delay buffer.\n"
        "  Each echo crosses to the opposite speaker. With frequency shift enabled, each\n"
        "  crossing also adds another D Hz -- a bouncing, alternating Risset glissando.\n"
        "\n"
        "Tape: a 1.5 Hz LFO gently modulates the delay read position (+/-0.3% of DELAY).\n"
        "  Simulates wow/flutter: each echo has a subtle pitch wobble on top of the\n"
        "  frequency shift, giving a warm, unstable analogue character.\n"
        "  The flutter is always on when this mode is active; its depth scales with DELAY.");
    addAndMakeVisible(feedback_mode_box_);
    feedback_mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "feedback_mode", feedback_mode_box_);

    direction_box_.onChange = [this] { update_direction_ui(); };

    // Row 1 knobs
    setup_knob(shift_knob_,     shift_label_,     "SHIFT");
    setup_knob(feedback_knob_,  feedback_label_,  "FEEDBACK");
    setup_knob(delay_knob_,     delay_label_,     "DELAY");
    setup_knob(lfo_rate_knob_,  lfo_rate_label_,  "MOD 1");
    setup_knob(lfo_depth_knob_, lfo_depth_label_, "MOD 2");

    // Row 2 knobs
    setup_knob(tone_knob_,      tone_label_,      "TONE");
    setup_knob(drive_knob_,     drive_label_,     "DRIVE");
    setup_knob(diffusion_knob_, diffusion_label_, "DIFFUSION");
    setup_knob(output_knob_,    output_label_,    "OUTPUT");
    setup_knob(mix_knob_,       mix_label_,       "MIX");

    // Tooltips
    shift_knob_.setTooltip(
        "Shift (D): frequency offset applied to every component. "
        "0 Hz = no shift (bypass). "
        "Below ~5 Hz produces chorusing/beating; above that, inharmonic metallic tones. "
        "Log scale; skew centre at 50 Hz.");

    feedback_knob_.setTooltip(
        "Feedback (g): level of the delayed shifted signal fed back to the input. "
        "At g > 0 each echo has been shifted an additional D Hz, creating a Risset "
        "endless glissando -- a perpetual ascending or descending pitch sweep. "
        "0 = no feedback (single-pass shift only).");

    delay_knob_.setTooltip(
        "Delay (d): time between echoes in the feedback loop (1-2000 ms). "
        "Sets the rhythm of the glissando repeats. "
        "At FEEDBACK = 0 this parameter has no audible effect. "
        "Log scale; skew centre at 200 ms.");

    lfo_rate_knob_.setTooltip(
        "Mod 1 (m1): LFO rate sweeping the shift amount (0-10 Hz). "
        "Modulates SHIFT up and down at this rate, creating a barberpole phaser sweep. "
        "0 = no modulation. Log scale; skew centre at 1 Hz.");

    lfo_depth_knob_.setTooltip(
        "Mod 2 (m2): LFO depth -- peak deviation of the shift sweep (0-500 Hz). "
        "effective_shift = D + m2*sin(m1*t). "
        "Has no effect if MOD 1 = 0. "
        "Log scale; skew centre at 20 Hz.");

    tone_knob_.setTooltip(
        "Tone (t): one-pole LP filter applied to the feedback signal before delay write. "
        "Left (0) = 500 Hz cutoff -- each echo darkens progressively. "
        "Right (1) = 20 kHz -- no frequency change (bypass). "
        "Only affects the feedback tail, not the direct shifted output.");

    drive_knob_.setTooltip(
        "Drive (k): tanh saturation applied to the feedback signal before delay write. "
        "0 = bypass (no saturation). "
        "Higher values compress and warm the echoes; prevents runaway at high feedback. "
        "Only affects the feedback tail, not the direct shifted output.");

    diffusion_knob_.setTooltip(
        "Diffusion (a): 2-stage Schroeder allpass applied to the feedback signal. "
        "0 = bypass (discrete echoes). "
        "Higher values smear each echo into a dense, reverb-like shimmer tail. "
        "Internal allpass coefficient = a * 0.7. "
        "Only affects the feedback tail, not the direct shifted output.");

    output_knob_.setTooltip(
        "Output: post-processing gain, -20 dB to +6 dB. "
        "Trim the output level after mixing.");

    mix_knob_.setTooltip(
        "Mix (w): wet/dry blend. out[n] = x[n] + w*(y[n] - x[n]). "
        "Left = fully dry (bypass), Right = fully wet.");

    shift_attach_     = std::make_unique<Attachment>(apvts, "shift",     shift_knob_);
    feedback_attach_  = std::make_unique<Attachment>(apvts, "feedback",  feedback_knob_);
    delay_attach_     = std::make_unique<Attachment>(apvts, "delay",     delay_knob_);
    lfo_rate_attach_  = std::make_unique<Attachment>(apvts, "lfo_rate",  lfo_rate_knob_);
    lfo_depth_attach_ = std::make_unique<Attachment>(apvts, "lfo_depth", lfo_depth_knob_);
    tone_attach_      = std::make_unique<Attachment>(apvts, "tone",      tone_knob_);
    drive_attach_     = std::make_unique<Attachment>(apvts, "drive",     drive_knob_);
    diffusion_attach_ = std::make_unique<Attachment>(apvts, "diffusion", diffusion_knob_);
    output_attach_    = std::make_unique<Attachment>(apvts, "output",    output_knob_);
    mix_attach_       = std::make_unique<Attachment>(apvts, "mix",       mix_knob_);

    update_direction_ui();
}

FrequencyShifterEditor::~FrequencyShifterEditor()
{
    setLookAndFeel(nullptr);
}

void FrequencyShifterEditor::setup_knob(Slider& knob, Label& label, const String& name)
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
void FrequencyShifterEditor::resized()
{
    const int w = getWidth();

    // Top row: direction + feedback loop + formula label
    direction_box_.setBounds    (kPadX,                        kModeY, kModeW, kModeH);
    feedback_mode_box_.setBounds(kPadX + kModeW + 8,           kModeY, kLoopW, kModeH);
    const int formula_x = kPadX + kModeW + 8 + kLoopW + 8;
    formula_label_.setBounds(formula_x, kModeY, w - formula_x - kPadX, kModeH);

    const int slot_w = (w - kPadX * 2) / 5;

    // Row 1: shift + feedback controls
    auto place1 = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * slot_w + slot_w / 2;
        knob.setBounds (cx - kKnobSize / 2, kKnobY1, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kKnobY1 + kKnobSize + 2, 80, kLabelH);
    };
    place1(shift_knob_,     shift_label_,     0);
    place1(feedback_knob_,  feedback_label_,  1);
    place1(delay_knob_,     delay_label_,     2);
    place1(lfo_rate_knob_,  lfo_rate_label_,  3);
    place1(lfo_depth_knob_, lfo_depth_label_, 4);

    // Row 2: feedback colour + output
    auto place2 = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * slot_w + slot_w / 2;
        knob.setBounds (cx - kKnobSize / 2, kKnobY2, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kKnobY2 + kKnobSize + 2, 80, kLabelH);
    };
    place2(tone_knob_,      tone_label_,      0);
    place2(drive_knob_,     drive_label_,     1);
    place2(diffusion_knob_, diffusion_label_, 2);
    place2(output_knob_,    output_label_,    3);
    place2(mix_knob_,       mix_label_,       4);
}

// ── Painting ───────────────────────────────────────────────────────────────────
void FrequencyShifterEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    // Separators
    g.setColour(Colour(laf_.border()));
    g.fillRect(kPadX, kKnobY1 - 8, getWidth() - kPadX * 2, 1);
    g.fillRect(kPadX, kSepY,       getWidth() - kPadX * 2, 1);

    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::frequency-shifter", kPadX, getHeight() - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Direction-dependent UI update ──────────────────────────────────────────────
void FrequencyShifterEditor::update_direction_ui()
{
    const int idx = direction_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 3) return;
    formula_label_.setText(kDirs[idx].desc, dontSendNotification);
    formula_label_.setTooltip(kDirs[idx].tip);
}

} // namespace kaos_engine
