#include "distortion_editor.h"
#include "distortion_plugin.h"

namespace kaos_engine {

using namespace juce;

// ── Formulas (indexed by DistortionMode) ───────────────────────────────────────
// g = DRIVE pre-gain.
// b = BIAS -active meaning depends on mode (see tooltips below).
struct DistModeInfo { const char* desc; const char* tip; };
static const DistModeInfo kModes[] = {
    {   // Soft
        "Soft clip (tanh) -- warm, smooth odd harmonics. Gently driven tube character.",
        "SOFT CLIP (tanh)\n"
        "\n"
        "Sound: Warm, smooth saturation with odd harmonics only. Closest digital equivalent\n"
        "to a gently driven tube amplifier or magnetic tape -- the nonlinearity is gradual\n"
        "and the clipping boundary is never hard.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = tanh(v)                      smooth saturation, bounded to +-1\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Hard
        "Hard clip (clamp) -- buzzy, aggressive, approaches square wave. Fuzz pedal.",
        "HARD CLIP (clamp)\n"
        "\n"
        "Sound: Aggressive flat-top clipping rich in odd harmonics, approaching a square\n"
        "wave at high drive. Emulates transistor fuzz pedals (ProCo RAT, Boss DS-1) or\n"
        "digital limiting. Buzzy and cutting -- useful for industrial or metal tones.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = clamp(v, -1, 1)             hard clip at threshold +-1\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Foldback
        "Foldback -- reflects signal at +-1. Metallic, FM-like timbres. Buchla wavefolder.",
        "FOLDBACK\n"
        "\n"
        "Sound: Reflects the signal back at the clip boundary instead of flattening it.\n"
        "Creates complex metallic harmonics similar to West Coast synthesis wavefolders\n"
        "(Buchla). Produces FM-like inharmonic timbres at high drive, bell-like at moderate.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = fold(v):                     reflect at +-1\n"
        "         while v > 1:  v = 2 - v\n"
        "         while v < -1: v = -2 - v\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Tube
        "Tube (asymmetric tanh) -- even harmonics add warmth. Use BIAS for more asymmetry.",
        "TUBE (asymmetric tanh)\n"
        "\n"
        "Sound: Asymmetric saturation that generates even harmonics (2nd, 4th...) alongside\n"
        "odd ones. Emulates a single-ended triode valve stage. The 2nd harmonic sits one\n"
        "octave above the fundamental -- musically consonant, the source of classic 'warmth'.\n"
        "BIAS shifts the operating point to increase asymmetry and even-harmonic content.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n] + b                pre-gain (DRIVE g), asymmetry offset (BIAS b)\n"
        "  f(v) = tanh(v) - tanh(b)           saturate + DC removal\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  b=BIAS  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Arctan
        "Arctan -- similar to tanh, slightly harder feel. Normalised odd-harmonic saturation.",
        "ARCTAN\n"
        "\n"
        "Sound: Similar character to Soft (tanh) but with a slightly harder feel at equivalent\n"
        "drive. Smooth odd-harmonic saturation. The output is normalised so it stays bounded\n"
        "to +-1 regardless of drive -- a common alternative to tanh in digital waveshaping.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = atan(v) / atan(g)           arctan, normalised to +-1 by atan(g)\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Log
        "Log -- compander-like gentle saturation. Vintage console push. Even harmonics with BIAS.",
        "LOGARITHMIC\n"
        "\n"
        "Sound: Logarithmic companding curve -- very gentle at low levels, firmer at high.\n"
        "Reminiscent of a vintage console preamp pushed softly into saturation. Even harmonics\n"
        "appear when the signal is biased asymmetrically via the BIAS knob.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n] + b                pre-gain (DRIVE g), optional BIAS b\n"
        "  f(v) = sign(v)*ln(1+|v|) / ln(1+g) log companding, normalised by ln(1+g)\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  b=BIAS  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Sine Fold
        "Sine fold -- sin(g*x). Dense harmonic stacks at high drive. Analog wavefolder.",
        "SINE FOLD\n"
        "\n"
        "Sound: Sine-based wavefolder. At low drive the output is gentle saturation; at high\n"
        "drive the waveform folds multiple times, creating dense harmonic stacks similar to\n"
        "analog wavefolders (Buchla 259, Serge). Generates FM-synthesis-like timbres.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = sin(v)                       sine wavefolder (folds at +-pi/2)\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Diode
        "Diode -- exponential I-V curve. TS-808 soft-clip topology. Warm mid-forward OD.",
        "DIODE CLIP\n"
        "\n"
        "Sound: Exponential approximation of the current-voltage curve of a silicon diode.\n"
        "Emulates soft-clipping diodes in an op-amp feedback loop (Tube Screamer / TS-808\n"
        "topology) -- warm, slightly asymmetric, with the mid-forward character of classic\n"
        "overdrive pedals.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = sign(v)*(1 - exp(-|v|))     exponential diode I-V approximation\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Half-wave
        "Half-wave rectify -- zeroes negative cycles. DC + even harmonics + octave shimmer.",
        "HALF-WAVE RECTIFIER\n"
        "\n"
        "Sound: Zeroes all negative half-cycles. Introduces a DC offset and adds even\n"
        "harmonics. At moderate mix, produces a subtle octave-up shimmer. Found in vintage\n"
        "'octave' effects and used as a precursor stage in many fuzz designs.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = max(0, v)                    zero negative half-cycles\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Full-wave
        "Full-wave rectify -- |g*x|. Output pitch doubles. Octave-up fuzz (Octavia style).",
        "FULL-WAVE RECTIFIER\n"
        "\n"
        "Sound: Flips negative half-cycles to positive -- the output frequency doubles.\n"
        "The signal sounds one octave above the input. The classic 'Green Ringer' / 'Octavia'\n"
        "octave-fuzz effect. Works best on monophonic sources.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  f(v) = |v|                          flip negative cycles positive\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Chebyshev
        "Chebyshev T3 -- 4v^3 - 3v. Generates only the 3rd harmonic from a sine input.",
        "CHEBYSHEV T3\n"
        "\n"
        "Sound: Applies the third Chebyshev polynomial, which -- for a unit-amplitude sine\n"
        "input -- generates only the 3rd harmonic and nothing else. Produces precise, tuned\n"
        "tonal coloring. At high drive the input clips and a richer spectrum develops.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g), keep |v| <= 1 for purity\n"
        "  f(v) = 4*v^3 - 3*v                 Chebyshev T3 polynomial\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Bitcrusher
        "Bit crush -- quantisation noise. Lo-fi digital grit. BIAS sets bit depth (1-16 bits).",
        "BITCRUSHER\n"
        "\n"
        "Sound: Reduces bit depth, adding quantization noise. Classic lo-fi digital grit:\n"
        "the character of early samplers, video game audio, and vintage 8-bit hardware.\n"
        "BIAS controls bit depth -- left = 1 bit (maximum crush), right = 16 bits (clean).\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  b    = bit depth 1-16 (BIAS knob)\n"
        "  f(v) = round(v * 2^b) / 2^b        quantise to b-bit resolution\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  b=BIAS (bit depth)  a=FEEDBACK  t=TONE  w=MIX"
    },
    {   // Sample Rate
        "Sample rate reduction -- ZOH aliasing. Inharmonic metallic crunch. BIAS sets rate.",
        "SAMPLE RATE REDUCTION\n"
        "\n"
        "Sound: Zero-order hold at a reduced effective sample rate -- intentional aliasing.\n"
        "Creates inharmonic 'zipper' distortion distinct from bitcrushing: frequency\n"
        "components fold unpredictably at the reduced Nyquist limit. The metallic crunch\n"
        "of early digital audio and lo-fi electronics.\n"
        "\n"
        "Signal flow:\n"
        "  v[n] = g * x[n]                    pre-gain (DRIVE g)\n"
        "  b    = downsampling factor 1-32x (BIAS knob)\n"
        "  f(v) = ZOH(v, fs/b):               hold output for b samples\n"
        "         output[n] = v[floor(n/b) * b]\n"
        "  y[n] = f(v[n]) + a * f(v[n-1])     waveshaper + FEEDBACK a\n"
        "  y[n] = LP(y[n], t)                  TONE low-pass cutoff t\n"
        "  out[n] = x[n] + w*(y[n] - x[n])    wet/dry MIX w\n"
        "\n"
        "Knob symbols: g=DRIVE  b=BIAS (rate factor)  a=FEEDBACK  t=TONE  w=MIX"
    },
};
static_assert(std::size(kModes) == 13, "mode count must match DistortionMode count");

// ── Construction ───────────────────────────────────────────────────────────────
DistortionEditor::DistortionEditor(DistortionPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);

    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // Mode combo -all 13 modes
    mode_box_.addItem("Soft",        1);
    mode_box_.addItem("Hard",        2);
    mode_box_.addItem("Foldback",    3);
    mode_box_.addItem("Tube",        4);
    mode_box_.addItem("Arctan",      5);
    mode_box_.addItem("Log",         6);
    mode_box_.addItem("Sine Fold",   7);
    mode_box_.addItem("Diode",       8);
    mode_box_.addItem("Half-wave",   9);
    mode_box_.addItem("Full-wave",  10);
    mode_box_.addItem("Chebyshev",  11);
    mode_box_.addItem("Bitcrusher", 12);
    mode_box_.addItem("Sample Rate",13);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "mode", mode_box_);

    // Formula label
    formula_label_.setFont(Font(11.0f));
    formula_label_.setJustificationType(Justification::centredLeft);
    formula_label_.setColour(Label::textColourId,
                             Colour(laf_.text_muted()));
    formula_label_.setColour(Label::backgroundColourId, Colour(0x00000000));
    addAndMakeVisible(formula_label_);

    mode_box_.onChange = [this] { update_mode_ui(); };

    // Knobs
    setup_knob(drive_knob_,    drive_label_,    "DRIVE");
    setup_knob(feedback_knob_, feedback_label_, "FEEDBACK");
    setup_knob(tone_knob_,     tone_label_,     "TONE");
    setup_knob(bias_knob_,     bias_label_,     "BIAS");
    setup_knob(output_knob_,   output_label_,   "OUTPUT");
    setup_knob(mix_knob_,      mix_label_,      "MIX");

    // Knob tooltips
    drive_knob_.setTooltip(
        "Drive (g): pre-gain applied to the input before the waveshaper. "
        "Controls how deep into the nonlinear region the signal is pushed.");

    feedback_knob_.setTooltip(
        "Feedback (a): adds a scaled copy of the previous waveshaper output "
        "to the current output: y[n] = f(x[n]) + a * f(x[n-1]). "
        "At low values this adds mild resonance and sustain; "
        "at higher values it creates comb-filter coloration and self-reinforcing harmonics. "
        "Default is 0 (no feedback).");

    tone_knob_.setTooltip(
        "Tone (t): post-distortion one-pole low-pass filter. "
        "Used in equations as t. Rolls off high-frequency harmonics after clipping. "
        "Left = 500 Hz, Right = 20 kHz.");

    bias_knob_.setTooltip(
        "Bias (b): only active when 'b' appears in the formula.\n"
        "Tube: asymmetry offset; shifts the operating point to add even harmonics.\n"
        "Bitcrusher: bit depth 16 (left, clean) to 1 (right, maximum crush).\n"
        "Sample Rate: downsampling factor 1x (left, no change) to 32x (right).\n"
        "Greyed out for all other modes.");

    output_knob_.setTooltip(
        "Output: post-processing gain, -20 dB to +6 dB. "
        "Use to compensate for level changes introduced by distortion.");

    mix_knob_.setTooltip(
        "Mix (w): wet/dry blend. out[n] = x[n] + w*(y[n] - x[n]). "
        "Left = fully dry (bypass), Right = fully wet (full effect).");

    drive_attach_    = std::make_unique<Attachment>(apvts, "drive",    drive_knob_);
    feedback_attach_ = std::make_unique<Attachment>(apvts, "feedback", feedback_knob_);
    tone_attach_     = std::make_unique<Attachment>(apvts, "tone",     tone_knob_);
    bias_attach_     = std::make_unique<Attachment>(apvts, "bias",     bias_knob_);
    output_attach_   = std::make_unique<Attachment>(apvts, "output",   output_knob_);
    mix_attach_      = std::make_unique<Attachment>(apvts, "mix",      mix_knob_);

    // ── Filter section ─────────────────────────────────────────────────────────
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

    cutoff_knob_.setTooltip(
        "Filter Cutoff: frequency at which the filter begins to attenuate. "
        "20 Hz (left) to 20 kHz (right) on a log scale. "
        "1 kHz is at the center of the knob.");

    resonance_knob_.setTooltip(
        "Resonance (Q): sharpness of the filter. "
        "Low Q (left, 0.1) = broad, gentle slope. "
        "Q = 0.707 = Butterworth (maximally flat). "
        "High Q (right, 10) = sharp resonant peak at the cutoff frequency.");

    blend_knob_.setTooltip(
        "Blend: crossfades between dry (left, 0) and fully filtered (right, 1). "
        "At 0 the filter is bypassed; at 1 the filter runs at full strength.");

    cutoff_attach_    = std::make_unique<Attachment>(apvts, "filter_cutoff", cutoff_knob_);
    resonance_attach_ = std::make_unique<Attachment>(apvts, "filter_res",    resonance_knob_);
    blend_attach_     = std::make_unique<Attachment>(apvts, "filter_blend",  blend_knob_);

    // Repaint the filter display whenever relevant knobs change
    cutoff_knob_.onValueChange    = [this] { repaint(); };
    resonance_knob_.onValueChange = [this] { repaint(); };
    blend_knob_.onValueChange     = [this] { repaint(); };
    filter_type_box_.onChange     = [this] { repaint(); };
    filter_on_btn_.onClick        = [this] { update_filter_ui(); repaint(); };
    filter_pos_box_.onChange      = [this] { repaint(); };

    update_mode_ui();
    update_filter_ui();
}

DistortionEditor::~DistortionEditor()
{
    setLookAndFeel(nullptr);
}

void DistortionEditor::setup_knob(Slider& knob, Label& label, const String& name)
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
void DistortionEditor::resized()
{
    const int w = getWidth();

    // Waveshaper section
    mode_box_.setBounds(kPadX, kModeY, kModeW, kModeH);
    const int formula_x = kPadX + kModeW + 12;
    formula_label_.setBounds(formula_x, kModeY, w - formula_x - kPadX, kModeH);

    const int slot_w = (w - kPadX * 2) / 6;
    auto place_knob = [&](Slider& knob, Label& label, int idx) {
        const int cx = kPadX + idx * slot_w + slot_w / 2;
        knob.setBounds(cx - kKnobSize / 2, kKnobY, kKnobSize, kKnobSize);
        label.setBounds(cx - 40, kKnobY + kKnobSize + 2, 80, kLabelH);
    };
    place_knob(drive_knob_,    drive_label_,    0);
    place_knob(feedback_knob_, feedback_label_,  1);
    place_knob(tone_knob_,     tone_label_,     2);
    place_knob(bias_knob_,     bias_label_,     3);
    place_knob(output_knob_,   output_label_,   4);
    place_knob(mix_knob_,      mix_label_,      5);

    // Filter header row
    filter_on_btn_.setBounds  (kPadX,       kFilterHeaderY, 65, 22);
    filter_pos_box_.setBounds (kPadX + 73,  kFilterHeaderY, 72, 22);
    filter_type_box_.setBounds(kPadX + 153, kFilterHeaderY, 60, 22);

    // Filter knobs — 3 evenly spaced across full width
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
void DistortionEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    // Separator above waveshaper knobs
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
    g.drawText("kaos-engine::distortion", kPadX, getHeight() - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Filter frequency response display ──────────────────────────────────────────
void DistortionEditor::draw_filter_response(Graphics& g, Rectangle<int> area)
{
    const int   w  = area.getWidth();
    const int   h  = area.getHeight();
    const float ax = (float)area.getX();
    const float ay = (float)area.getY();

    // Background
    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float db_min = -40.0f, db_max = 20.0f, db_range = db_max - db_min;
    const bool  on     = filter_on_btn_.getToggleState();

    // Grid lines (dB horizontals + frequency verticals)
    g.setColour(Colour(laf_.border()).withAlpha(0.6f));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f }) {
        const float y = ay + h * (db_max - db) / db_range;
        g.drawLine(ax, y, ax + w, y, 0.5f);
    }
    // Frequency verticals: 100 Hz, 1 kHz, 10 kHz
    g.setFont(Font(9.0f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.7f));
    for (auto [freq, label] : { std::pair{100.0f,"100"}, {1000.0f,"1k"}, {10000.0f,"10k"} }) {
        const float x = ax + w * std::log10(freq / 20.0f) / 3.0f;
        g.setColour(Colour(laf_.border()).withAlpha(0.6f));
        g.drawLine(x, ay, x, ay + h, 0.5f);
        g.setColour(Colour(laf_.text_muted()).withAlpha(0.7f));
        g.drawText(label, (int)x - 12, area.getBottom() - 14, 24, 12,
                   Justification::centred);
    }

    // Compute and draw frequency response curve
    const float fc    = (float)cutoff_knob_.getValue();
    const float q     = (float)resonance_knob_.getValue();
    const float blend = (float)blend_knob_.getValue();
    const int   type  = filter_type_box_.getSelectedItemIndex();

    Path curve;
    for (int px = 0; px < w; ++px) {
        const float freq  = 20.0f * std::pow(1000.0f, (float)px / w);
        const float ratio = freq / fc;
        const float r2    = ratio * ratio;
        const float denom = std::sqrt((1.0f - r2) * (1.0f - r2)
                                    + r2 / (q * q)) + 1e-12f;

        float mag = 1.0f;
        if (on) {
            float raw_mag;
            switch (type) {
                case 0: raw_mag = 1.0f / denom;           break; // LP
                case 1: raw_mag = r2   / denom;           break; // HP
                case 2: raw_mag = (ratio / q) / denom;    break; // BP
                default: raw_mag = 1.0f;
            }
            const float db_raw = 20.0f * std::log10(std::max(raw_mag, 1e-9f));
            const float db_blended = blend * db_raw;
            mag = std::pow(10.0f, db_blended / 20.0f);
        }

        const float db  = 20.0f * std::log10(std::max(mag, 1e-9f));
        const float fy  = ay + h * std::clamp((db_max - db) / db_range, 0.0f, 1.0f);
        if (px == 0) curve.startNewSubPath(ax, fy);
        else         curve.lineTo(ax + px, fy);
    }

    // Filled area beneath curve
    Path filled = curve;
    filled.lineTo(ax + w, ay + h);
    filled.lineTo(ax,     ay + h);
    filled.closeSubPath();
    g.setColour(Colour(laf_.accent_colour()).withAlpha(on ? 0.13f : 0.04f));
    g.fillPath(filled);

    // Curve stroke
    g.setColour(Colour(laf_.accent_colour()).withAlpha(on ? 0.85f : 0.25f));
    g.strokePath(curve, PathStrokeType(1.5f, PathStrokeType::curved,
                                       PathStrokeType::rounded));

    // Cutoff marker
    if (on && fc > 0.0f) {
        const float cx = ax + w * std::log10(fc / 20.0f) / 3.0f;
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.35f));
        g.drawLine(cx, ay, cx, ay + h, 1.0f);
    }

    // Border
    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
}

// ── Mode-dependent UI update ───────────────────────────────────────────────────
void DistortionEditor::update_mode_ui()
{
    const int idx = mode_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 13) return;

    formula_label_.setText(kModes[idx].desc, dontSendNotification);
    formula_label_.setTooltip(kModes[idx].tip);

    const auto mode      = static_cast<DistortionMode>(idx);
    const bool bias_on   = mode_uses_bias(mode);

    bias_knob_.setEnabled(bias_on);
    bias_label_.setEnabled(bias_on);

    bias_label_.setColour(Label::textColourId,
        Colour(bias_on ? laf_.text_primary() : laf_.text_muted()));
}

// ── Filter section enable/disable ──────────────────────────────────────────────
void DistortionEditor::update_filter_ui()
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
