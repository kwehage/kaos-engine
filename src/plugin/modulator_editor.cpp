#include "modulator_editor.h"
#include "modulator_plugin.h"

namespace kaos_engine {

using namespace juce;

// ── Mode info (indexed by ModulatorMode) ───────────────────────────────────────
struct ModModeInfo { const char* desc; const char* tip; };
static const ModModeInfo kModes[] = {
    {   // Tremolo
        "Tremolo -- amplitude flutter via slow LFO. Classic volume wobble and vibrato.",
        "TREMOLO\n"
        "\n"
        "Sound: The oscillator runs at sub-audio rate (< 20 Hz) and its output is converted\n"
        "to a unipolar envelope that multiplies the input amplitude. DEPTH sets how deep\n"
        "the flutter cuts -- 0 = no effect, 1 = full silence at the trough. PHASE offsets\n"
        "the right channel for classic stereo tremolo (try 90 degrees / PHASE = 0.5).\n"
        "\n"
        "Signal flow:\n"
        "  osc[n] = waveform(phase[n])          oscillator at RATE f\n"
        "  pos[n] = 0.5 + 0.5 * osc[n]         rectify to [0, 1]\n"
        "  m[n]   = (1 - d) + d * pos[n]        envelope: range [(1-d), 1]\n"
        "  y[n]   = x[n] * m[n]                 amplitude multiply\n"
        "  out[n] = x[n] + w*(y[n] - x[n])      wet/dry MIX w\n"
        "\n"
        "Knob symbols: f=RATE  d=DEPTH  p=PHASE  w=MIX\n"
        "(BIAS inactive for this mode)"
    },
    {   // AM
        "AM -- amplitude modulation at audio rate. Adds sum/difference frequency sidebands.",
        "AMPLITUDE MODULATION (AM)\n"
        "\n"
        "Sound: The oscillator runs at audio rate (typically 100 Hz - 10 kHz) and multiplies\n"
        "the input with a biased carrier. For each input frequency f_in, the output gains\n"
        "sidebands at f_in + RATE and |f_in - RATE|. BIAS controls how much of the original\n"
        "signal is preserved: at BIAS = 1 the original plus sidebands are heard (classic AM);\n"
        "at BIAS = 0 only the sidebands remain (ring mod character).\n"
        "\n"
        "Signal flow:\n"
        "  osc[n] = waveform(phase[n])           carrier at RATE f\n"
        "  m[n]   = b + d * osc[n]               modulator: range [b-d, b+d]\n"
        "  y[n]   = x[n] * m[n]                  amplitude multiply\n"
        "  out[n] = x[n] + w*(y[n] - x[n])       wet/dry MIX w\n"
        "\n"
        "Sidebands: for input f_in -> output at f_in +/- RATE\n"
        "\n"
        "Knob symbols: f=RATE  d=DEPTH  b=BIAS  p=PHASE  w=MIX"
    },
    {   // Ring Mod
        "Ring mod -- carrier suppressed AM. Metallic sidebands only; original signal removed.",
        "RING MODULATION\n"
        "\n"
        "Sound: Pure multiplication of input by a bipolar carrier. The original signal is\n"
        "completely suppressed at full MIX -- only the sum and difference sidebands remain.\n"
        "When RATE is a harmonic multiple of the input fundamental, sidebands land on\n"
        "existing harmonics (bright but still musical). When RATE is inharmonic relative\n"
        "to the input, the result is metallic and bell-like.\n"
        "\n"
        "Signal flow:\n"
        "  osc[n] = waveform(phase[n])           carrier at RATE f\n"
        "  y[n]   = x[n] * osc[n]               ring multiply: original suppressed\n"
        "  out[n] = x[n] + w*(y[n] - x[n])       wet/dry MIX w\n"
        "\n"
        "Sidebands: for input f_in -> output at f_in +/- RATE (original absent at MIX = 1)\n"
        "\n"
        "Knob symbols: f=RATE  p=PHASE  w=MIX\n"
        "(DEPTH and BIAS inactive for this mode)"
    },
};
static_assert(std::size(kModes) == 3, "mode count must match ModulatorMode count");

// ── Construction ───────────────────────────────────────────────────────────────
ModulatorEditor::ModulatorEditor(ModulatorPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);

    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // Mode combo
    mode_box_.addItem("Tremolo",  1);
    mode_box_.addItem("AM",       2);
    mode_box_.addItem("Ring Mod", 3);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "mode", mode_box_);

    // Waveform combo
    waveform_box_.addItem("Sine",     1);
    waveform_box_.addItem("Triangle", 2);
    waveform_box_.addItem("Square",   3);
    waveform_box_.addItem("Saw",      4);
    waveform_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(waveform_box_);
    waveform_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "waveform", waveform_box_);

    // Formula label
    // formula_label_ not shown -- description routed to mode_box_ tooltip in update_mode_ui().

    mode_box_.onChange = [this] { update_mode_ui(); };

    // Knobs
    setup_knob(rate_knob_,   rate_label_,   "RATE");
    setup_knob(depth_knob_,  depth_label_,  "DEPTH");
    setup_knob(bias_knob_,   bias_label_,   "BIAS");
    setup_knob(phase_knob_,  phase_label_,  "PHASE");
    setup_knob(output_knob_, output_label_, "OUTPUT");
    setup_knob(mix_knob_,    mix_label_,    "MIX");

    // Knob tooltips
    rate_knob_.setTooltip(
        "Rate (f): oscillator frequency. "
        "Tremolo: use 0.05-20 Hz for classic flutter. "
        "AM / Ring Mod: use audio-rate values (100-10000 Hz) for sidebands. "
        "Log scale; centre of knob = 100 Hz.");

    depth_knob_.setTooltip(
        "Depth (d): modulation amount. "
        "Tremolo: 0 = no flutter, 1 = full silence at trough. "
        "AM: modulation index 0-1 (0 = no sidebands, 1 = 100% AM). "
        "Inactive for Ring Mod (use MIX for blending).");

    bias_knob_.setTooltip(
        "Bias (b): DC offset added to the carrier before multiplication. Active in AM mode only.\n"
        "b = 1: full AM -- original signal is preserved alongside the sidebands.\n"
        "b = 0: ring mod character -- carrier suppressed, only sidebands remain.\n"
        "Intermediate values blend between these two characters.\n"
        "Note: at b = 1 and d = 1 the output amplitude can reach 2x input -- "
        "use OUTPUT to compensate.");

    phase_knob_.setTooltip(
        "Phase (p): stereo phase offset of the right-channel oscillator (0-180 degrees). "
        "0 = identical L/R (mono modulation). "
        "0.5 = 90 degrees -- classic quadrature stereo tremolo wobble. "
        "1.0 = 180 degrees anti-phase -- creates a ping-pong-like stereo sweep.");

    output_knob_.setTooltip(
        "Output (o): post-processing gain, -20 dB to +6 dB. "
        "Use to compensate for level changes. "
        "AM at full depth (b=1, d=1) can produce up to +6 dB of gain.");

    mix_knob_.setTooltip(
        "Mix (w): wet/dry blend. out[n] = x[n] + w*(y[n] - x[n]). "
        "Left = fully dry (bypass), Right = fully wet. "
        "For Ring Mod, MIX controls how much of the ring-modulated signal replaces the original.");

    rate_attach_   = std::make_unique<Attachment>(apvts, "rate",   rate_knob_);
    depth_attach_  = std::make_unique<Attachment>(apvts, "depth",  depth_knob_);
    bias_attach_   = std::make_unique<Attachment>(apvts, "bias",   bias_knob_);
    phase_attach_  = std::make_unique<Attachment>(apvts, "phase",  phase_knob_);
    output_attach_ = std::make_unique<Attachment>(apvts, "output", output_knob_);
    mix_attach_    = std::make_unique<Attachment>(apvts, "mix",    mix_knob_);

    update_mode_ui();
}

ModulatorEditor::~ModulatorEditor()
{
    setLookAndFeel(nullptr);
}

void ModulatorEditor::setup_knob(Slider& knob, Label& label, const String& name)
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
void ModulatorEditor::resized()
{
    const int w = getWidth();

    // Top row
    mode_box_.setBounds(kPadX, kModeY, kModeW, kModeH);
    waveform_box_.setBounds(kPadX + kModeW + 8, kModeY, kWaveW, kModeH);


    // Knob row
    const int slot_w = (w - kPadX * 2) / 6;
    auto place_knob = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * slot_w + slot_w / 2;
        knob.setBounds (cx - kKnobSize / 2, kKnobY, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kKnobY + kKnobSize + 2, 80, kLabelH);
    };
    place_knob(rate_knob_,   rate_label_,   0);
    place_knob(depth_knob_,  depth_label_,  1);
    place_knob(bias_knob_,   bias_label_,   2);
    place_knob(phase_knob_,  phase_label_,  3);
    place_knob(output_knob_, output_label_, 4);
    place_knob(mix_knob_,    mix_label_,    5);
}

// ── Painting ───────────────────────────────────────────────────────────────────
void ModulatorEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));


    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::modulator", kPadX, getHeight() - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Mode-dependent UI update ───────────────────────────────────────────────────
void ModulatorEditor::update_mode_ui()
{
    const int idx = mode_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 3) return;

    mode_box_.setTooltip(kModes[idx].tip);

    const auto mode       = static_cast<ModulatorMode>(idx);
    const bool bias_on    = modulator_uses_bias(mode);
    const bool depth_on   = modulator_uses_depth(mode);

    bias_knob_.setEnabled(bias_on);
    bias_label_.setEnabled(bias_on);
    bias_label_.setColour(Label::textColourId,
        Colour(bias_on ? laf_.text_primary() : laf_.text_muted()));

    depth_knob_.setEnabled(depth_on);
    depth_label_.setEnabled(depth_on);
    depth_label_.setColour(Label::textColourId,
        Colour(depth_on ? laf_.text_primary() : laf_.text_muted()));
}

} // namespace kaos_engine
