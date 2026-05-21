#include "pitch_shifter_editor.h"
#include "pitch_shifter_plugin.h"

namespace kaos_engine {

using namespace juce;

static String fmt_pitch (double v) { return String(static_cast<int>(std::round(v))) + " st"; }
static String fmt_detune(double v) { return String(v, 1) + " ct"; }

// ── Construction ───────────────────────────────────────────────────────────────
PitchShifterEditor::PitchShifterEditor(PitchShifterPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);

    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& apvts = plugin_.get_apvts();

    // ── Algorithm selector ─────────────────────────────────────────────────────
    algo_box_.addItem("Granular",     1);
    algo_box_.addItem("Smooth",       2);
    algo_box_.addItem("Tape",         3);
    algo_box_.addItem("Phase Vocoder",4);
    algo_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(algo_box_);
    algo_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, "algorithm", algo_box_);
    algo_box_.onChange = [this] { update_algo_ui(); };

    // algo_label_ not shown -- description routed to algo_box_ tooltip in update_algo_ui().

    // ── Per-voice knobs ────────────────────────────────────────────────────────
    const char* gainIds[]  = { "gain1",  "gain2",  "gain3"  };
    const char* mod1Ids[]  = { "mod1_1", "mod1_2", "mod1_3" };
    const char* mod2Ids[]  = { "mod2_1", "mod2_2", "mod2_3" };
    const char* pitchIds[] = { "pitch1", "pitch2", "pitch3" };
    const char* detIds[]   = { "detune1","detune2","detune3" };

    for (int vi = 0; vi < 3; ++vi) {
        // Row 1: GAIN (with built-in text box)
        setup_knob(gain_knob_[vi],  gain_label_[vi],  "GAIN");
        gain_attach_[vi] = std::make_unique<Attachment>(apvts, gainIds[vi], gain_knob_[vi]);

        // Row 2: MOD 1, MOD 2 (with built-in text box)
        setup_knob(mod1_knob_[vi], mod1_label_[vi], "MOD 1");
        setup_knob(mod2_knob_[vi], mod2_label_[vi], "MOD 2");
        mod1_attach_[vi] = std::make_unique<Attachment>(apvts, mod1Ids[vi], mod1_knob_[vi]);
        mod2_attach_[vi] = std::make_unique<Attachment>(apvts, mod2Ids[vi], mod2_knob_[vi]);

        // Row 3: PITCH, DETUNE (no built-in text box — value shown in row 4)
        setup_knob(pitch_knob_[vi],  pitch_label_[vi],  "PITCH");
        setup_knob(detune_knob_[vi], detune_label_[vi], "DETUNE");
        pitch_knob_[vi].setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        detune_knob_[vi].setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        pitch_attach_[vi]  = std::make_unique<Attachment>(apvts, pitchIds[vi], pitch_knob_[vi]);
        detune_attach_[vi] = std::make_unique<Attachment>(apvts, detIds[vi],   detune_knob_[vi]);

        // Row 4: editable value boxes
        setup_value_box(pitch_box_[vi]);
        setup_value_box(detune_box_[vi]);

        // Slider → box sync
        pitch_knob_[vi].onValueChange  = [this, vi] {
            pitch_box_[vi].setText(fmt_pitch(pitch_knob_[vi].getValue()), dontSendNotification);
        };
        detune_knob_[vi].onValueChange = [this, vi] {
            detune_box_[vi].setText(fmt_detune(detune_knob_[vi].getValue()), dontSendNotification);
        };

        // Box → slider (validates and reformats on commit)
        pitch_box_[vi].onTextChange = [this, vi] {
            const float val = static_cast<float>(
                std::round(std::clamp(pitch_box_[vi].getText().getFloatValue(), -24.0f, 24.0f)));
            pitch_knob_[vi].setValue(val, sendNotification);
            pitch_box_[vi].setText(fmt_pitch(val), dontSendNotification);
        };
        detune_box_[vi].onTextChange = [this, vi] {
            const float val = std::clamp(
                detune_box_[vi].getText().getFloatValue(), -50.0f, 50.0f);
            detune_knob_[vi].setValue(val, sendNotification);
            detune_box_[vi].setText(fmt_detune(val), dontSendNotification);
        };
    }

    // Initialise text boxes after attachments have set the slider values
    for (int vi = 0; vi < 3; ++vi) {
        pitch_box_[vi].setText (fmt_pitch (pitch_knob_[vi].getValue()),  dontSendNotification);
        detune_box_[vi].setText(fmt_detune(detune_knob_[vi].getValue()), dontSendNotification);
    }

    // ── Tooltips ───────────────────────────────────────────────────────────────
    for (int vi = 0; vi < 3; ++vi) {
        gain_knob_[vi].setTooltip(
            "Gain (g): voice output level (0 = silent, 1 = unity). "
            "Voices with gain = 0 are skipped entirely -- zero CPU cost. "
            "The wet signal is normalised by total active gain, so enabling additional "
            "voices does not raise the overall output level. "
            + String(vi == 0 ? "Default 1.0." : "Default 0.0 -- raise to enable this voice."));
        pitch_knob_[vi].setTooltip(
            "Pitch (p): semitone shift for this voice (-24 to +24, integer steps). "
            "Drag or type a value in the box below. "
            "pitch_factor = 2^((p + d/100) / 12).");
        detune_knob_[vi].setTooltip(
            "Detune (d): fine pitch in cents (-50 to +50). "
            "Adds a fractional semitone on top of PITCH. "
            "Type a value in the box below.");
    }
    // MOD tooltips are set in update_algo_ui() since meaning depends on algorithm

    // ── Global knobs ───────────────────────────────────────────────────────────
    setup_knob(mix_knob_,    mix_label_,    "MIX");
    setup_knob(output_knob_, output_label_, "OUTPUT");
    mix_attach_    = std::make_unique<Attachment>(apvts, "mix",    mix_knob_);
    output_attach_ = std::make_unique<Attachment>(apvts, "output", output_knob_);

    mix_knob_.setTooltip(
        "Mix (w): wet/dry blend. out = in + w*(wet - in). "
        "0 = fully dry, 1 = fully wet.");
    output_knob_.setTooltip(
        "Output: post-mix gain (-20 to +6 dB). "
        "Trim the final level after mix.");

    update_algo_ui();
}

PitchShifterEditor::~PitchShifterEditor() { setLookAndFeel(nullptr); }

// ── Knob / value-box setup ─────────────────────────────────────────────────────
void PitchShifterEditor::setup_knob(Slider& knob, Label& label, const String& name)
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

void PitchShifterEditor::setup_value_box(Label& box)
{
    box.setEditable(true, true, false);
    box.setFont(Font(12.0f));
    box.setJustificationType(Justification::centred);
    box.setColour(Label::backgroundColourId,           Colour(laf_.surface()));
    box.setColour(Label::outlineColourId,              Colour(laf_.border()));
    box.setColour(Label::textColourId,                 Colour(laf_.text_primary()));
    box.setColour(Label::backgroundWhenEditingColourId, Colour(laf_.surface()));
    box.setColour(Label::outlineWhenEditingColourId,   Colour(laf_.accent_colour()));
    box.setColour(TextEditor::textColourId,            Colour(laf_.text_primary()));
    box.setColour(TextEditor::backgroundColourId,      Colour(laf_.surface()));
    box.setColour(TextEditor::highlightColourId,       Colour(laf_.accent_colour()).withAlpha(0.4f));
    addAndMakeVisible(box);
}

// ── Layout ─────────────────────────────────────────────────────────────────────
void PitchShifterEditor::resized()
{
    const int w      = getWidth();
    const int slot_w = (w - kPadX * 2) / kNumSlots;

    // Centre x of a (fractional) slot index
    auto cx = [&](float slot) {
        return kPadX + static_cast<int>(slot * slot_w + slot_w * 0.5f);
    };

    // Algo selector
    algo_box_.setBounds(kPadX, kAlgoY, kAlgoW, kAlgoH);

    // Row 1: GAIN (centred over each voice's 2 slots) + OUTPUT + MIX
    auto place1 = [&](Slider& k, Label& l, float slot) {
        const int x = cx(slot);
        k.setBounds(x - kKnobSize / 2, kKnobY1, kKnobSize, kKnobSize);
        l.setBounds(x - 40, kKnobY1 + kKnobSize + 2, 80, kLabelH);
    };
    for (int vi = 0; vi < 3; ++vi)
        place1(gain_knob_[vi], gain_label_[vi], static_cast<float>(vi * 2) + 0.5f);
    place1(output_knob_, output_label_, 6.0f);
    place1(mix_knob_,    mix_label_,    7.0f);

    // Row 2: MOD 1 (even slots), MOD 2 (odd slots) per voice
    auto place2 = [&](Slider& k, Label& l, int slot) {
        const int x = cx(static_cast<float>(slot));
        k.setBounds(x - kKnobSize / 2, kKnobY2, kKnobSize, kKnobSize);
        l.setBounds(x - 40, kKnobY2 + kKnobSize + 2, 80, kLabelH);
    };
    for (int vi = 0; vi < 3; ++vi) {
        place2(mod1_knob_[vi], mod1_label_[vi], vi * 2);
        place2(mod2_knob_[vi], mod2_label_[vi], vi * 2 + 1);
    }

    // Row 3: PITCH (even slots), DETUNE (odd slots) — NoTextBox, cap height to match arc size
    const int rotary_h = kKnobSize - 14;  // arc same size as TextBoxBelow knobs
    auto place3 = [&](Slider& k, Label& l, int slot) {
        const int x = cx(static_cast<float>(slot));
        k.setBounds(x - kKnobSize / 2, kKnobY3, kKnobSize, rotary_h);
        l.setBounds(x - 40, kKnobY3 + rotary_h + 2, 80, kLabelH);
    };
    for (int vi = 0; vi < 3; ++vi) {
        place3(pitch_knob_[vi],  pitch_label_[vi],  vi * 2);
        place3(detune_knob_[vi], detune_label_[vi], vi * 2 + 1);
    }

    // Row 4: text entry boxes aligned with their knobs
    const int bw = kKnobSize;
    for (int vi = 0; vi < 3; ++vi) {
        pitch_box_[vi].setBounds (cx(static_cast<float>(vi * 2))     - bw / 2, kTextBoxY, bw, kTextBoxH);
        detune_box_[vi].setBounds(cx(static_cast<float>(vi * 2 + 1)) - bw / 2, kTextBoxY, bw, kTextBoxH);
    }
}

// ── Painting ───────────────────────────────────────────────────────────────────
void PitchShifterEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    const int w      = getWidth();
    const int slot_w = (w - kPadX * 2) / kNumSlots;


    // Voice group headers
    g.setFont(Font(9.5f));
    g.setColour(Colour(laf_.text_muted()));
    for (int vi = 0; vi < 3; ++vi)
        g.drawText(String("VOICE ") + String(vi + 1),
                   kPadX + vi * 2 * slot_w, kHdrY, 2 * slot_w, 14,
                   Justification::centred);
    g.drawText("GLOBAL", kPadX + 6 * slot_w, kHdrY, 2 * slot_w, 14,
               Justification::centred);

    // Vertical separators spanning all rows down to the text box bottom
    const int sep_top = kHdrY;
    const int sep_bot = kTextBoxY + kTextBoxH + 4;
    g.setColour(Colour(laf_.border()));
    for (int sep : { 2, 4, 6 })
        g.fillRect(kPadX + sep * slot_w, sep_top, 1, sep_bot - sep_top);

    // Footer
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::pitch-shifter", kPadX, getHeight() - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Algorithm description + MOD knob tooltips ─────────────────────────────────
void PitchShifterEditor::update_algo_ui()
{
    struct AlgoInfo {
        const char* desc;
        const char* tip;
        const char* mod1_tip;
        const char* mod2_tip;
    };

    static const AlgoInfo kInfo[] = {
        {   // Granular
            "Granular -- dual-grain OLA, triangular window. MOD 1 = grain size, MOD 2 = chaos scatter.",
            "GRANULAR PITCH SHIFTER\n"
            "\n"
            "Sound: Classic grain-based OLA. Two overlapping grains with triangular windows.\n"
            "MOD 1 controls grain size (20..200 ms): smaller = more percussive and grainy;\n"
            "larger = smoother and more smeared. MOD 2 adds chaos: each grain's read position\n"
            "is offset by a random amount (up to 30%% of grain size), breaking the periodic\n"
            "flanging pattern that causes the characteristic metallic-robotic quality. At low\n"
            "chaos the randomisation is subtle and organic; at high chaos the output becomes\n"
            "diffuse and cloud-like. Set GAIN to 0 to skip a voice entirely (zero CPU cost).\n"
            "\n"
            "Signal flow (per active voice):\n"
            "  pf = 2^((p + d/100) / 12)                  pitch factor\n"
            "  gs = 20ms + m1*180ms (in samples)           per-voice grain size from MOD 1\n"
            "  chaos_range = m2 * gs * 0.3                 scatter window from MOD 2\n"
            "  [each grain period] c = random * chaos_range  new chaos offset\n"
            "  da = pf*(gs-pa) + c + 1 ;  db = pf*(gs-pb) + c + 1   read delays\n"
            "  v = ea*buf[n-da] + eb*buf[n-db]             triangular-windowed output\n"
            "  wet = (sum of g*v) / (sum of g)             normalised across active voices\n"
            "\n"
            "Knob symbols:  p=PITCH  d=DETUNE  g=GAIN  m1=MOD 1  m2=MOD 2  w=MIX",
            "Mod 1 (m1): grain size for this voice.\n"
            "Granular: 0 = 20 ms (small, percussive, grainy); 1 = 200 ms (large, smeared).\n"
            "Default 0.3 gives ~75 ms -- a balanced starting point.",
            "Mod 2 (m2): chaos -- random scatter of grain read position.\n"
            "Granular: 0 = deterministic (periodic, slight flanging); 1 = grains scattered\n"
            "up to 30%% of grain size -- diffuse, organic, cloud-like texture.\n"
            "Default 0 = no chaos."
        },
        {   // Smooth
            "Smooth -- dual-grain OLA, Hann window. MOD 1 = grain size, MOD 2 = chaos scatter.",
            "SMOOTH PITCH SHIFTER\n"
            "\n"
            "Sound: Larger-grain OLA with Hann (raised-cosine) windows. Hann windows sum to\n"
            "exactly 1 at 50%% overlap, eliminating amplitude modulation artifacts. Larger\n"
            "grains (~80..300 ms via MOD 1) mean the periodic re-alignment occurs less often,\n"
            "giving a smoother, more smeared result on sustained notes and pads. MOD 2 applies\n"
            "the same chaos scatter as Granular -- breaks up any remaining periodic flanging.\n"
            "Better for vocals and sustained instruments; worse than Granular for transients.\n"
            "\n"
            "Signal flow (per active voice):\n"
            "  gs = 80ms + m1*220ms                        per-voice grain size from MOD 1\n"
            "  ea = 0.5 - 0.5*cos(2*pi*pa/gs)             Hann window A  (ea+eb = 1)\n"
            "  chaos applied same as Granular (m2 * gs * 0.3)\n"
            "  da = pf*(gs-pa) + c + 1 ;  db = pf*(gs-pb) + c + 1\n"
            "  v = ea*buf[n-da] + eb*buf[n-db]\n"
            "\n"
            "Knob symbols:  p=PITCH  d=DETUNE  g=GAIN  m1=MOD 1  m2=MOD 2  w=MIX",
            "Mod 1 (m1): grain size for this voice.\n"
            "Smooth: 0 = 80 ms; 1 = 300 ms. Larger grains are smoother but more smeared.\n"
            "Default 0.3 gives ~146 ms.",
            "Mod 2 (m2): chaos -- random scatter of grain read position.\n"
            "Smooth: same as Granular -- 0 = deterministic, 1 = up to 30%% scatter.\n"
            "Default 0 = no chaos."
        },
        {   // Tape
            "Tape -- moving read pointer. MOD 1 = flutter rate (0..8 Hz), MOD 2 = flutter depth.",
            "TAPE PITCH SHIFTER\n"
            "\n"
            "Sound: Single read pointer that drifts at pitch_factor speed. Between the rare\n"
            "crossfades (every ~400 ms at +12 st) the sound is completely smooth -- no\n"
            "granulation. MOD 1 adds wow/flutter: a sinusoidal modulation of the read pointer\n"
            "speed at 0..8 Hz. MOD 2 sets the depth (0..±16 samples, ≈ ±4 cents at 1 kHz).\n"
            "Low flutter (m1~0.1, m2~0.2) gives vintage tape warmth; high flutter (m1~0.5,\n"
            "m2~1.0) produces strong seasick detuning. Best for small-interval harmony and\n"
            "detuning effects. At m2=0 (default) flutter is fully off regardless of m1.\n"
            "\n"
            "Signal flow (per active voice):\n"
            "  flutter = m2*16 * sin(2*pi*m1*8*t)          wow/flutter from MOD 1 + MOD 2\n"
            "  delay += (1 - pf)                            delay drifts toward 0 or max\n"
            "  v = fade*buf[n - delay - flutter]\n"
            "    + (1-fade)*buf[n - delay_prev - flutter]   blend on crossfade\n"
            "  if delay out of range: jump + 10 ms crossfade\n"
            "\n"
            "Knob symbols:  p=PITCH  d=DETUNE  g=GAIN  m1=MOD 1  m2=MOD 2  w=MIX",
            "Mod 1 (m1): flutter rate for this voice (0 to 8 Hz).\n"
            "Tape: sets how fast the tape speed wavers. 0 = no flutter; 0.1 = 0.8 Hz (slow\n"
            "warm wow); 0.5 = 4 Hz (vibrato-speed flutter). Has no effect if MOD 2 = 0.",
            "Mod 2 (m2): flutter depth for this voice (0 to +-16 samples, ~4 cents).\n"
            "Tape: amplitude of the speed variation. 0 = no flutter (default); 0.5 = +-8\n"
            "samples (~2 cents at 1 kHz); 1.0 = +-16 samples (~4 cents at 1 kHz)."
        },
        {   // Phase Vocoder
            "Phase Vocoder -- STFT bin remapping. Smooth on sustained tones; phasey on transients.",
            "PHASE VOCODER PITCH SHIFTER\n"
            "\n"
            "Sound: Analyzes the input with a 1024-point STFT (75% overlap, Hann window), shifts\n"
            "frequency bins by the pitch ratio, then resynthesizes. Produces the smoothest possible\n"
            "pitch shift on sustained tones and chords -- no grain boundaries, no flanging. The\n"
            "characteristic artifact is 'phasiness': transients (attacks, consonants) smear and\n"
            "lose clarity because the STFT window averages over 23 ms of audio. MOD 1 and MOD 2\n"
            "are inactive for this algorithm. Best for: harmony on sustained pads, strings,\n"
            "vocals held on vowels, and large creative shifts (>+/-4 semitones).\n"
            "\n"
            "Signal flow (per active voice):\n"
            "  Every kHop=256 samples:\n"
            "    X[k] = STFT(windowed input, N=1024)      analysis frame\n"
            "    f_true[k] = k/N + wrap(angle(X[k]) - last_phase[k] - 2*pi*k*H/N) * N/(2*pi*H)\n"
            "    X_out[round(k*pf)] = max(mag, X[k])     remap bins by pitch factor\n"
            "    synth_phase[k] += f_true_out[k] * 2*pi*H/N\n"
            "    y_frame = ISTFT(X_out, synth_phase)      synthesis frame\n"
            "  Output = overlap-add of y_frame every H samples, normalised by 1.5 (COLA)\n"
            "\n"
            "Latency: N - H = 768 samples (~17 ms at 44.1 kHz).\n"
            "Knob symbols: p=PITCH  d=DETUNE  g=GAIN  w=MIX  (MOD 1 and MOD 2 inactive)",
            "Mod 1: inactive for Phase Vocoder mode.",
            "Mod 2: inactive for Phase Vocoder mode."
        },
    };

    const int idx = algo_box_.getSelectedItemIndex();
    if (idx < 0 || idx >= 4) return;

    algo_box_.setTooltip(kInfo[idx].tip);

    // Update MOD knob tooltips to reflect the current algorithm's meaning
    for (int vi = 0; vi < 3; ++vi) {
        mod1_knob_[vi].setTooltip(kInfo[idx].mod1_tip);
        mod2_knob_[vi].setTooltip(kInfo[idx].mod2_tip);
    }
}

} // namespace kaos_engine
