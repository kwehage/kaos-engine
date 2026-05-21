#include "noise_editor.h"

namespace kaos_engine {
using namespace juce;

// ── Construction ───────────────────────────────────────────────────────────────

NoiseEditor::NoiseEditor(NoisePlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    auto& ap = plugin_.get_apvts();

    // Type combo -- manual APVTS sync (not ComboBoxAttachment) so addSeparator() works
    // Item IDs are enum_value + 1 so getSelectedId()-1 == enum int directly.
    type_box_.addItem("White",          1);
    type_box_.addItem("Pink",           2);
    type_box_.addItem("Blue",           3);
    type_box_.addItem("Brown",          4);
    type_box_.addItem("Granular",       5);
    type_box_.addItem("Feedback Comb",  6);
    type_box_.addItem("Simplex",        7);
    type_box_.addItem("Lorenz",         8);
    type_box_.addItem("Duffing",        9);
    type_box_.addItem("Gendyn",        10);
    type_box_.addItem("Harsh Wall",    11);
    type_box_.addItem("Chua's Circuit", 12);
    type_box_.addSeparator();
    type_box_.addItem("Residual",      13);
    type_box_.addItem("Coupled",       14);
    type_box_.addItem("Diffuse",       15);
    type_box_.addItem("Modal",         16);
    type_box_.addItem("Simplex 2D",    17);
    type_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(type_box_);
    // Initial value from APVTS
    type_box_.setSelectedId(juce::roundToInt(ap.getRawParameterValue("noise_type")->load()) + 1,
                            dontSendNotification);
    type_box_.onChange = [this, &ap] {
        const int id = type_box_.getSelectedId();
        if (id > 0)
            if (auto* p = ap.getParameter("noise_type"))
                p->setValueNotifyingHost(p->convertTo0to1(float(id - 1)));
        update_type_ui();
    };

    // Mode combo
    mode_box_.addItem("Follow",    1);
    mode_box_.addItem("Gated",     2);
    mode_box_.addItem("Always On", 3);
    mode_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(mode_box_);
    mode_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "noise_mode", mode_box_);
    mode_box_.onChange = [this] { update_mode_ui(); update_type_ui(); };

    // Blend combo
    blend_box_.addItem("Add",       1);
    blend_box_.addItem("AM",        2);
    blend_box_.addItem("Saturate",  3);
    blend_box_.addItem("Spectral",  4);
    blend_box_.addItem("Phase Random", 5);
    blend_box_.addItem("Ring Mod",  6);
    blend_box_.addItem("Infrasonic AM", 7);
    blend_box_.addItem("Roughness",    8);
    blend_box_.setScrollWheelEnabled(false);
    addAndMakeVisible(blend_box_);
    blend_attach_ = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
        ap, "noise_blend", blend_box_);
    blend_box_.onChange = [this] { update_type_ui(); };

    // desc_label_ not shown -- description text is routed to combo tooltips in update_type_ui / update_mode_ui.

    // Knobs
    setup_knob(gain_knob_,      gain_lbl_,      "GAIN");
    setup_knob(mod_knob_,       mod_lbl_,       "MOD");
    setup_knob(size_knob_,      size_lbl_,      "SIZE");
    setup_knob(density_knob_,   density_lbl_,   "DENSITY");
    setup_knob(threshold_knob_, threshold_lbl_, "THRESHOLD");
    setup_knob(attack_knob_,    attack_lbl_,    "ATTACK");
    setup_knob(release_knob_,   release_lbl_,   "RELEASE");
    setup_knob(mix_knob_,       mix_lbl_,       "MIX");
    setup_knob(output_knob_,    output_lbl_,    "OUTPUT");

    gain_knob_     .setTooltip("Gain (g): noise amplitude before mixing. 0 = silent, 1 = full amplitude.");
    mod_knob_      .setTooltip("Mod (m): Saturate/Spectral/Phase Random: injection depth. "
                               "Saturate: y = tanh(x + m*n). "
                               "Spectral: |X_k|' = |X_k|*(1 + m*n_k). "
                               "Phase Rnd: phase rotation depth (0 = none, 1 = full +-pi). "
                               "Modal: T60 decay time (0 = 0.1 s percussive, 1 = 6 s bell ring). "
                               "Simplex: persistence / spectral slope (0.5 = brown-like, 0.95 = near-white). "
                               "Simplex 2D: how strongly input level drives the y-axis (0 = ignore input, 1 = full range). "
                               "Gendyn: mutation rate per breakpoint crossing (0 = frozen quasi-periodic tone, 1 = rapid noise evolution).");
    size_knob_     .setTooltip("Size (s): Granular: grain length (5-500 ms). "
                               "Residual: LP smoothing time -- smaller = higher HP cutoff. "
                               "Lorenz: integration step dt (small = low/slow, large = high/fast). "
                               "Modal: fundamental frequency of mode 1 (100-2000 Hz). "
                               "Fdbk Comb: comb delay time -- sets the comb frequency (1/delay). "
                               "Simplex / Simplex 2D: noise evolution speed (0.5 Hz at min, 50 Hz at max). "
                               "Gendyn: average breakpoint duration (small = high pitch / fast cycle, large = low / sub-audio).");
    density_knob_  .setTooltip("Density (d): Granular: grain spawn rate (0=sparse, 1=dense). "
                               "Coupled: chaos coupling level (0=periodic, 1=fully chaotic). "
                               "Diffuse: allpass coefficient (0=transparent, 1=heavily diffused). "
                               "Lorenz: rho parameter (0=near bifurcation/ordered, 1=fully chaotic). "
                               "Modal: inharmonicity B (0=harmonic bar modes, 1=strongly inharmonic). "
                               "Fdbk Comb: feedback gain (0=no resonance, 0.98=near self-oscillation). "
                               "Simplex / Simplex 2D: fBm octave count (0=1 octave, 1=8 octaves). "
                               "Gendyn: breakpoint count (0=2 points near-triangular wave, 1=16 points complex waveform).");
    threshold_knob_.setTooltip("Threshold: Follow/Gated modes -- input level that activates noise (dBFS).");
    attack_knob_   .setTooltip("Attack: Follow/Gated modes -- time for noise to fade IN after signal exceeds threshold. Range 0.1-500 ms.");
    release_knob_  .setTooltip("Release: Follow/Gated modes -- time for noise to fade OUT after signal drops. Range 1-5000 ms.");
    mix_knob_      .setTooltip("Mix (w): blend amount. Add: 0=dry / 1=noise only. "
                               "AM: 0=dry / 1=full amplitude modulation. "
                               "Saturate: 0=clean / 1=fully saturated.");
    output_knob_   .setTooltip("Output (o): final output level in dB. Applied after dry/wet mix.");

    gain_att_      = std::make_unique<Attachment>(ap, "gain",       gain_knob_);
    mod_att_       = std::make_unique<Attachment>(ap, "noise_mod",  mod_knob_);
    size_att_      = std::make_unique<Attachment>(ap, "grain_size", size_knob_);
    density_att_   = std::make_unique<Attachment>(ap, "density",    density_knob_);
    threshold_att_ = std::make_unique<Attachment>(ap, "threshold",  threshold_knob_);
    attack_att_    = std::make_unique<Attachment>(ap, "attack",     attack_knob_);
    release_att_   = std::make_unique<Attachment>(ap, "release",    release_knob_);
    mix_att_       = std::make_unique<Attachment>(ap, "mix",        mix_knob_);
    output_att_    = std::make_unique<Attachment>(ap, "output",     output_knob_);

    update_type_ui();
    update_mode_ui();
    startTimerHz(30);
}

NoiseEditor::~NoiseEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void NoiseEditor::setup_knob(Slider& k, Label& l, const String& name)
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

void NoiseEditor::update_type_ui()
{
    // Use item ID (not index) so separator lines don't shift the value
    const int idx  = type_box_ .getSelectedId() - 1;  // 0-based enum value
    const int bidx = blend_box_.getSelectedItemIndex();

    // MOD: Saturate/Spectral/PhaseRandom/InfrasonicAM/Roughness blends;
    //      Modal(15) T60, Simplex(6) persistence, SimplexDriven(16) y-depth,
    //      Gendyn(9) mutation, Duffing(8) omega, HarshWall(10) saturation
    const bool mod_active     = (bidx == 2 || bidx == 3 || bidx == 4 || bidx == 6 || bidx == 7 ||
                                  idx == 6 || idx == 8 || idx == 9 || idx == 10 || idx == 15 || idx == 16);
    // SIZE: Granular(4), FeedbackComb(5), Simplex(6), Lorenz(7), Duffing(8), Gendyn(9),
    //       HarshWall(10), Chua(11), Residual(12), Modal(15), SimplexDriven(16)
    const bool size_active    = (idx == 4 || idx == 5 || idx == 6 || idx == 7 || idx == 8 ||
                                  idx == 9 || idx == 10 || idx == 11 || idx == 12 || idx == 15 || idx == 16);
    // DENSITY: Granular(4), FeedbackComb(5), Simplex(6), Lorenz(7), Duffing(8), Gendyn(9),
    //          HarshWall(10), Chua(11), Coupled(13), Diffuse(14), Modal(15), SimplexDriven(16)
    const bool density_active = (idx == 4 || idx == 5 || idx == 6 || idx == 7 || idx == 8 ||
                                  idx == 9 || idx == 10 || idx == 11 || idx == 13 || idx == 14 || idx == 15 || idx == 16);

    size_knob_   .setEnabled(size_active);    size_knob_   .setAlpha(size_active    ? 1.0f : 0.4f);
    density_knob_.setEnabled(density_active); density_knob_.setAlpha(density_active ? 1.0f : 0.4f);
    mod_knob_    .setEnabled(mod_active);     mod_knob_    .setAlpha(mod_active      ? 1.0f : 0.4f);

    // Descriptions indexed by enum value (0-16). Must stay in sync with NoiseType enum order.
    static const char* type_descs[17] = {
        // 0-3: coloured noise
        "White -- flat spectrum, equal energy at all frequencies. Broadband hiss.",
        "Pink -- -3 dB/oct (1/f). More low-frequency energy. Warmer, natural sounding noise.",
        "Blue -- +3 dB/oct (differentiated white noise: n = w - w_prev). High-frequency emphasis; air, presence, sibilance. No active parameters.",
        "Brown -- -6 dB/oct (1/f^2). Deep rumble and low roar. Brownian motion.",
        // 4-11: structured independent noise
        "Granular -- Hann-windowed noise bursts. SIZE and DENSITY control texture.",
        "Feedback Comb -- resonant comb filter on white noise. SIZE = delay/pitch, DENSITY = feedback (resonance). Deep drone at long SIZE; metallic ring at short.",
        "Simplex -- smooth fBm coherent noise. Evolves without random jumps. SIZE = speed (0.5-50 Hz), DENSITY = octaves (1-8), MOD = persistence (spectral slope).",
        "Lorenz -- Lorenz attractor (RK4). SIZE = integration step (small = low/slow, large = high/fast). DENSITY = rho (low = ordered, high = chaotic).",
        "Duffing -- forced double-well oscillator (RK4). Transitions from periodic to chaotic with DENSITY. SIZE = integration step (speed/pitch), DENSITY = forcing amplitude (0.3=near-periodic, 1.2=strongly chaotic), MOD = forcing frequency (0.9-1.5 rad/s).",
        "Gendyn -- Xenakis stochastic waveform. N breakpoints mutate each cycle via random walk. SIZE = period/pitch, DENSITY = breakpoints (2-16), MOD = mutation rate (0=frozen tone, 1=evolving noise).",
        "Harsh Wall -- 4 parallel linear comb filters (prime delays 7:11:17:23 * scale). SIZE = spectral center (~6kHz at min, ~200Hz at max). DENSITY = comb feedback 0.50-0.95. MOD = output saturation drive 1-15.",
        "Chua's Circuit -- piecewise-linear 3D chaos with double-scroll attractor. Intermittent pitch-switching between two lobes; different character from Lorenz/Duffing. SIZE = integration step (speed/pitch), DENSITY = alpha 5-12 (shapes attractor, controls spectral density).",
        // 12-16: input-reactive
        "Residual -- high-pass residual of input: n = x - LP(x). SIZE sets LP cutoff (small = high-pass).",
        "Coupled -- logistic chaos driven by input energy. DENSITY sets base chaos level.",
        "Diffuse -- Schroeder allpass cascade of input. Same spectrum, smeared in time. DENSITY sets diffusion.",
        "Modal -- inharmonic resonator bank excited by input. SIZE = fundamental freq (100-2000 Hz). DENSITY = inharmonicity B. MOD = T60 decay time.",
        "Simplex 2D -- 2D simplex field navigated by input level (y-axis). Each dynamic maps to a distinct texture region. SIZE = speed, DENSITY = octaves, MOD = y-axis depth.",
    };
    if (idx >= 0 && idx < 17)
        type_box_.setTooltip(type_descs[idx]);

    static const char* blend_tooltips[8] = {
        "Add: y = (1-mix)*x + mix*n. Dry/wet blend of input with noise.",
        "AM: y = x*(1 + mix*n). Noise amplitude-modulates the signal. Output peaks at +6 dB at full mix.",
        "Saturate: y = lerp(x, tanh(x + mod*n), mix). MOD controls noise injection depth into the saturation.",
        "Spectral: per-bin magnitude scaling -- |X_k|' = |X_k|*(1 + mod*n_k). MOD controls depth. ~11 ms latency.",
        "Phase Random: OLA bin phase randomization by MOD depth. Preserves magnitudes, smears transients. ~11 ms latency.",
        "Ring Mod: y = (1-mix)*x + mix*(x*n). Suppressed-carrier AM -- only sidebands remain at full mix.",
        "Infrasonic AM: sub-20 Hz LFO amplitude-modulates the signal. y = x*(1 + mix*sin(2*pi*f*t)). MOD = frequency (0.1-19 Hz), MIX = depth. Creates psychoacoustic unease and low-frequency pulsing.",
        "Roughness: ring modulation at sub-200 Hz carrier. y = (1-mix)*x + mix*(x*sin(2*pi*fc*t)). MOD = carrier frequency (20-200 Hz). Adds sidebands within one critical bandwidth of each harmonic -- directly targets Plomp-Levelt roughness. MIX = depth.",
    };
    if (bidx >= 0 && bidx < 8)
        blend_box_.setTooltip(blend_tooltips[bidx]);

    rebuild_preview();
}

void NoiseEditor::update_mode_ui()
{
    static const char* mode_tooltips[3] = {
        "Follow: noise amplitude tracks input envelope above THRESHOLD. Noise fades out as signal drops below threshold.",
        "Gated: noise gates on/off when input crosses THRESHOLD. Binary open/close, smoothed by ATTACK/RELEASE.",
        "Always On: noise runs continuously at fixed gain. THRESHOLD, ATTACK and RELEASE are inactive.",
    };
    const int idx = mode_box_.getSelectedItemIndex();
    if (idx >= 0 && idx < 3)
        mode_box_.setTooltip(mode_tooltips[idx]);
    const bool envelope = (idx == 0 || idx == 1);  // Follow or Gated
    threshold_knob_.setEnabled(envelope); threshold_knob_.setAlpha(envelope ? 1.0f : 0.4f);
    attack_knob_   .setEnabled(envelope); attack_knob_   .setAlpha(envelope ? 1.0f : 0.4f);
    release_knob_  .setEnabled(envelope); release_knob_  .setAlpha(envelope ? 1.0f : 0.4f);
}

void NoiseEditor::rebuild_preview()
{
    // Render static preview at sample_rate = kPreviewSamples (1 sec = kPreviewSamples samples)
    NoiseProcessor prev;
    prev.set_type       (static_cast<NoiseType>(type_box_.getSelectedItemIndex()));
    prev.set_mode       (NoiseMode::AlwaysOn);
    prev.set_gain       (gain_knob_.getValue() > 0.0 ? float(gain_knob_.getValue()) : 0.5f);
    prev.set_grain_size_ms (float(size_knob_.getValue()) > 0.0f
                              ? float(size_knob_.getValue()) : 50.0f);
    prev.set_grain_density (float(density_knob_.getValue()) > 0.0f
                              ? float(density_knob_.getValue()) : 0.5f);
    prev.prepare(kPreviewSamples, kPreviewSamples);

    for (int i = 0; i < kPreviewSamples; ++i)
        preview_buf_[i] = prev.next_preview_sample();
}

// ── Timer ──────────────────────────────────────────────────────────────────────

void NoiseEditor::timerCallback()
{
    // Sync type combo with APVTS (handles host automation and preset loads)
    const int typeId = juce::roundToInt(
        plugin_.get_apvts().getRawParameterValue("noise_type")->load()) + 1;
    if (type_box_.getSelectedId() != typeId)
        type_box_.setSelectedId(typeId, dontSendNotification);

    trail_    [trail_write_] = plugin_.get_output_sample();
    dry_trail_[trail_write_] = plugin_.get_dry_sample();
    trail_write_ = (trail_write_ + 1) % kTrailSize;
    if (trail_count_ < kTrailSize) ++trail_count_;
    repaint(0, kDispY, kWidth, kDispH);
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void NoiseEditor::resized()
{
    const int w    = getWidth();
    const int colw = (w - kPadX * 2) / kNumCols;

    type_box_ .setBounds(kPadX,                     kComboY, kComboW, kComboH);
    mode_box_ .setBounds(kPadX + kComboW + 8,       kComboY, kComboW, kComboH);
    blend_box_.setBounds(kPadX + (kComboW + 8) * 2, kComboY, kComboW, kComboH);

    auto kx = [&](int col) { return kPadX + col * colw + colw / 2 - kKnobSize / 2; };
    auto lx = [&](int col) { return kPadX + col * colw + colw / 2 - 36; };
    auto place = [&](Slider& k, Label& l, int col) {
        k.setBounds(kx(col), kKnobY,               kKnobSize, kKnobSize);
        l.setBounds(lx(col), kKnobY + kKnobSize + 1, 72,      kKnobLabelH);
    };
    place(gain_knob_,      gain_lbl_,      0);
    place(mod_knob_,       mod_lbl_,       1);
    place(size_knob_,      size_lbl_,      2);
    place(density_knob_,   density_lbl_,   3);
    place(threshold_knob_, threshold_lbl_, 4);
    place(attack_knob_,    attack_lbl_,    5);
    place(release_knob_,   release_lbl_,   6);
    place(mix_knob_,       mix_lbl_,       7);
    place(output_knob_,    output_lbl_,    8);
}

// ── Painting ───────────────────────────────────────────────────────────────────

void NoiseEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));

    draw_preview(g, Rectangle<int>(0, kDispY, kWidth, kDispH));

    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::noise", kPadX, kHeight - kFooterH - 4,
               300, kFooterH, Justification::centredLeft);
}

// ── Waveform display ───────────────────────────────────────────────────────────

void NoiseEditor::draw_preview(Graphics& g, Rectangle<int> area)
{
    g.saveState();
    g.reduceClipRegion(area);

    g.setColour(Colour(laf_.surface()));
    g.fillRect(area);

    const float ax = float(area.getX());
    const float ay = float(area.getY());
    const float w  = float(area.getWidth());
    const float h  = float(area.getHeight());
    const float mid = ay + h * 0.5f;

    // Gridlines
    g.setColour(Colour(laf_.border()).withAlpha(0.4f));
    g.drawLine(ax, mid, ax + w, mid, 0.5f);
    g.drawLine(ax, ay + h * 0.25f, ax + w, ay + h * 0.25f, 0.5f);
    g.drawLine(ax, ay + h * 0.75f, ax + w, ay + h * 0.75f, 0.5f);

    if (trail_count_ >= 2) {
        const int n  = trail_count_;
        const int ds = int(w) - n;

        auto build_path = [&](const std::vector<float>& buf) {
            Path p;
            for (int px = 0; px < int(w); ++px) {
                float v = 0.0f;
                if (px >= ds) {
                    const int age = px - ds;
                    const int si  = (trail_write_ - n + age + kTrailSize) % kTrailSize;
                    v = std::clamp(buf[si], -1.0f, 1.0f);
                }
                const float cy = mid - v * h * 0.45f;
                const float cx = ax + float(px);
                if (px == 0) p.startNewSubPath(cx, cy);
                else         p.lineTo(cx, cy);
            }
            return p;
        };

        // ── Dry (input) line -- faint accent underneath ──────────────────────
        Path dry_curve = build_path(dry_trail_);
        Path dry_fill  = dry_curve;
        dry_fill.lineTo(ax + w, mid); dry_fill.lineTo(ax, mid); dry_fill.closeSubPath();
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.07f));
        g.fillPath(dry_fill);
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.30f));
        g.strokePath(dry_curve, PathStrokeType(1.0f, PathStrokeType::curved,
                                               PathStrokeType::rounded));

        // ── Wet (output) line -- bright accent with fill on top ──────────────
        Path wet_curve = build_path(trail_);
        Path wet_fill  = wet_curve;
        wet_fill.lineTo(ax + w, mid); wet_fill.lineTo(ax, mid); wet_fill.closeSubPath();
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.13f));
        g.fillPath(wet_fill);
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.90f));
        g.strokePath(wet_curve, PathStrokeType(1.8f, PathStrokeType::curved,
                                               PathStrokeType::rounded));

        // Head dot on wet line
        const int   newest = (trail_write_ - 1 + kTrailSize) % kTrailSize;
        const float hv     = std::clamp(trail_[newest], -1.0f, 1.0f);
        g.setColour(Colour(laf_.accent_colour()));
        g.fillEllipse(ax + w - 3.0f, mid - hv * h * 0.45f - 3.0f, 6.0f, 6.0f);

        // Legend
        g.setFont(Font(7.5f));
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.35f));
        g.drawText("input",  int(ax + w) - 90, int(ay) + 2,  86, 9, Justification::centredRight);
        g.setColour(Colour(laf_.accent_colour()).withAlpha(0.80f));
        g.drawText("output", int(ax + w) - 90, int(ay) + 12, 86, 9, Justification::centredRight);
    } else {
        g.setFont(Font(9.0f));
        g.setColour(Colour(laf_.text_muted()));
        g.drawText("Play audio to see input / output", int(ax) + 4, int(ay + h/2) - 6,
                   int(w) - 8, 13, Justification::centred);
    }

    // Axis labels
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.5f));
    g.drawText("+1", int(ax) + 2, int(ay) + 2,          20, 9, Justification::centredLeft);
    g.drawText(" 0", int(ax) + 2, int(mid) - 4,          20, 9, Justification::centredLeft);
    g.drawText("-1", int(ax) + 2, int(ay + h) - 11,      20, 9, Justification::centredLeft);

    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);
    g.restoreState();
}

} // namespace kaos_engine
