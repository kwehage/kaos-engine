# kaos-engine

A collection of VST3 audio effects plugins for algorithmic sound design. Each plugin is
built around a focused set of controls, a minimal UI, and a selection of algorithms
covering a wide range of sonic character.

All plugins share the same dark visual palette (background `#141414`, cadmium red `#D22B2B`
accent) and follow consistent parameter naming conventions across the suite.

---

## Plugins

### kaos-engine::distortion

A waveshaper / distortion unit with 13 algorithms, a feedback path, and an optional
State Variable Filter that can be placed before or after the waveshaper.

![kaos-engine::distortion](doc/images/distortion.png)

**Algorithms**

| Mode | Character |
|---|---|
| **Soft** | tanh soft clip — warm, smooth, odd harmonics only |
| **Hard** | digital hard clip — aggressive, buzzy, square-wave character |
| **Foldback** | wavefolding — metallic, bell-like, FM-style inharmonics at high drive |
| **Tube** | asymmetric polynomial — even harmonics, warm tube emulation |
| **Arctan** | arctan soft clip — slightly harder feel than Soft; approaches hard clip at high drive |
| **Log** | logarithmic companding — Harmor Log-style, warm even harmonics when biased |
| **Sine Fold** | sin(g·x) — gentle saturation at low gain, complex FM-like content at high gain |
| **Diode** | exponential diode model — smooth saturation, Tube Screamer topology character |
| **Half-wave** | half-wave rectification — zeros one polarity, adds DC and even harmonics |
| **Full-wave** | full-wave rectification — flips negative cycles, octave-up character |
| **Chebyshev** | Chebyshev T₃ polynomial — precise odd harmonic generation |
| **Bitcrusher** | bit-depth reduction — quantization grit, coarse at low bit depths |
| **Sample Rate** | zero-order hold downsampling — intentional aliasing, metallic artifacts |

**Parameters**

| Knob | Symbol | Range | Description |
|---|---|---|---|
| Drive | g | 0–1 | Pre-gain into the waveshaper; higher = more harmonic content |
| Feedback | a | 0–1 | Feeds the distorted signal back into the input; adds resonance and sustain |
| Tone | t | 0–1 | One-pole LP filter on the output; rolls off high-frequency harshness |
| Bias | b | -1–+1 | DC offset before the waveshaper; introduces even harmonics (asymmetry) |
| Output | — | -20 to +6 dB | Post-processing output trim |
| Mix | w | 0–1 | Dry/wet blend: `out = dry + w*(wet - dry)` |

**Filter section** (independent SVF, optional)

| Control | Options | Description |
|---|---|---|
| Filter On | toggle | Enables the filter |
| Position | Pre / Post | Insert before or after the waveshaper |
| Type | LP / HP / BP | Simper state-variable filter topology |
| Cutoff | 20 Hz – 20 kHz | Filter cutoff frequency |
| Resonance | 0.1 – 10 | Q factor |
| Blend | 0–1 | Mixes filtered and unfiltered signal |

---

### kaos-engine::delay

A stereo digital delay with 11 modes covering everything from clean utility echoes to
creative tape and granular effects. MOD 1 and MOD 2 control LFO rate and depth
respectively, with meaning specific to each mode.

![kaos-engine::delay](doc/images/delay.png)

**Modes**

| Mode | Character |
|---|---|
| **Standard** | Clean stereo delay — utility echo, faithful repeats |
| **Slapback** | Short single repeat (70–200 ms) — vintage rockabilly/drum room |
| **Ping-Pong** | Alternating L/R feedback — bouncing stereo spread |
| **Tape** | Multi-head with LFO pitch modulation — warm, degrading, wow/flutter |
| **Diffusion** | Allpass chain before delay — dense echo cloud, reverb-like onset |
| **Reverse** | Backward playback — ghostly rising swells |
| **Comb** | Short resonant delay (no LP) — pitched metallic drone, reverb building block |
| **Multi-Tap** | Multiple independent read heads — rhythmic BPM-synced patterns |
| **Shimmer** | Allpass diffusor + pitch-shifted (+1 octave) feedback — ethereal swell |
| **Haas** | Fixed L/R offset (<40 ms) — mono-safe stereo widening |
| **BBD** | Bucket-brigade emulation — warm analog chorus/delay with clock modulation |

**Parameters**

| Knob | Symbol | Range | Description |
|---|---|---|---|
| Time | d | 1–3000 ms | Delay time; meaning varies by mode |
| Feedback | g | 0–1 | Feedback gain; controls repeat count and tail length |
| Tone | t | 0–1 | LP filter in feedback path; 0 = dark, 1 = bright |
| Mod 1 | m1 | 0–1 | LFO rate (e.g. wow/flutter speed for Tape; clock rate for BBD) |
| Mod 2 | m2 | 0–1 | LFO depth (e.g. pitch deviation depth; detuning amount) |
| Output | — | -20 to +6 dB | Post-mix output trim |
| Mix | w | 0–1 | Dry/wet blend |

---

### kaos-engine::reverb

An algorithmic reverb with 7 fundamentally different reverb engines, pre/post filter,
and separate MOD 1 / MOD 2 controls for LFO rate and detuning depth. All algorithms
apply LFO modulation to break up standing-wave resonances.

![kaos-engine::reverb](doc/images/reverb.png)

**Algorithms**

| Algorithm | Character |
|---|---|
| **Dattorro** | Classic plate reverb (JAES 1997) — smooth, bright, dense. Cross-coupled modulated tank. Best for vocals, snare, melodic instruments |
| **Schroeder** | First digital reverb (1962) — parallel combs + series allpasses. Coloured, ringy; use high DIFFUSION to tame metallic resonance |
| **FDN** | Feedback Delay Network — 4 delay lines, Hadamard mixing. Clean, transparent, uniform. Closest to convolution quality |
| **Gardner** | Room reverb (1992) — nested allpass feedback loop. Warm, intimate, distinct early reflections. Good for drums, acoustic instruments |
| **Moorer** | 8-tap early reflection tapped delay + Schroeder tail (1979). Most natural-sounding of the classic algorithms |
| **Velvet Noise** | Sparse FIR with ±1 pulses and exponential decay. No modal coloration, clean noise-like tail. SIZE controls tail/RT60, DIFFUSION controls pulse density |
| **Shimmer** | Dattorro plate with granular pitch shifter (+0 to +12 semitones) in the cross-feedback loop. MOD 1 = shimmer mix; MOD 2 = pitch interval |

**Parameters**

| Knob | Symbol | Range | Description |
|---|---|---|---|
| Pre-Delay | p | 0–200 ms | Time before reverb onset; conveys source distance |
| Size | s | 0–1 | Scales all internal delay lengths; affects room size and RT60 |
| Decay | g | 0–1 | Feedback gain; controls tail length (RT60) |
| Damping | D | 0–1 | LP cutoff in feedback path; 0 = dark (500 Hz), 1 = bright (20 kHz) |
| Diffusion | a | 0–1 | Allpass coefficient; 0 = sparse/echoy onset, 1 = dense/smooth onset |
| Mod 1 | m1 | 0–1 | LFO rate (0.05–2 Hz); detuning modulation speed. Active for all algorithms |
| Mod 2 | m2 | 0–1 | LFO depth (0–16 samples); detuning amount. 0 = no pitch variation |
| Output | — | -20 to +6 dB | Post-mix output trim |
| Mix | w | 0–1 | Dry/wet blend |

For **Shimmer** only: MOD 1 = shimmer mix (0 = pure plate, 1 = full pitch-shifted feedback); MOD 2 = pitch interval (0 = unison, 1 = +12 semitones / +1 octave).

For **Velvet Noise**: DECAY and MOD are unused; SIZE controls both room size and RT60.

**Filter section** (same SVF as distortion, optional)

| Control | Options | Description |
|---|---|---|
| Filter On | toggle | Enables the filter |
| Position | Pre / Post | Insert before or after the reverb |
| Type | LP / HP / BP | Simper state-variable filter |
| Cutoff | 20 Hz – 20 kHz | Filter cutoff |
| Resonance | 0.1 – 10 | Q factor |
| Blend | 0–1 | Parallel blend between filtered and direct signal |

---

### kaos-engine::pitch-shifter

A granular pitch shifter with 3 independent voices. Each voice has its own pitch, fine
detune, gain, and per-algorithm modulation controls. Voices with GAIN set to zero are
skipped entirely with no CPU cost. PITCH and DETUNE can be entered numerically via
editable text boxes at the bottom of the UI.

![kaos-engine::pitch-shifter](doc/images/pitch-shifter.png)

**Algorithms**

| Algorithm | Character | MOD 1 | MOD 2 |
|---|---|---|---|
| **Granular** | Dual-grain OLA with triangular crossfade windows. Slight constant graininess, works on any material | Grain size (20–200 ms) — smaller = more percussive; larger = more smeared | Chaos — random grain read-position scatter (0 = deterministic, 1 = ±30% of grain) |
| **Smooth** | Dual-grain OLA with Hann windows (ea + eb = 1, no amplitude ripple). Less grainy, better for sustained notes and pads | Grain size (80–300 ms) | Chaos — same as Granular |
| **Tape** | Single moving read pointer with smooth crossfade on wrap. Transparent between crossfades; periodic stutter at crossfade points (~every 400 ms at +12 st) | Flutter rate (0–8 Hz) — sinusoidal tape wow/flutter | Flutter depth (0–±16 samples ≈ ±4 cents at 1 kHz); has no effect if MOD 1 = 0 |

**Per-voice parameters** (×3 voices)

| Knob | Symbol | Range | Description |
|---|---|---|---|
| Gain | g | 0–1 | Voice output level. Set to 0 to silence and skip the voice entirely |
| Mod 1 | m1 | 0–1 | Grain size (Granular/Smooth) or flutter rate (Tape); see table above |
| Mod 2 | m2 | 0–1 | Chaos scatter (Granular/Smooth) or flutter depth (Tape) |
| Pitch | p | -24 to +24 st | Pitch shift in semitones (integer steps). Also editable via text box |
| Detune | d | -50 to +50 ct | Fine pitch offset in cents. `pitch_factor = 2^((p + d/100) / 12)` |

**Global parameters**

| Knob | Range | Description |
|---|---|---|
| Mix | 0–1 | Dry/wet blend. Wet signal is the gain-weighted sum of active voices |
| Output | -20 to +6 dB | Post-mix output trim |

**Default voice configuration:** Voice 1 is active at unity gain (GAIN = 1.0), Voices 2
and 3 are silent (GAIN = 0.0). Raise their GAIN to add additional harmony or detuning
layers. The three voices accumulate additively — use the Output knob to compensate if
level increases when enabling more voices.

---

## Building

Requires [Meson](https://mesonbuild.com/) >= 1.1 and a C++17 compiler.

```sh
meson setup build --buildtype=release
meson compile -C build
```

Targets: **Windows** (x86_64, MinGW-w64) and **Linux** (x86_64).
Output format: **VST3** (plus standalone executables for testing).

---

## Project layout

```
kaos-engine/
├── meson.build              # top-level build definition
├── src/
│   ├── effects/
│   │   ├── distortion/      # waveshaper DSP
│   │   ├── delay/           # delay line DSP
│   │   ├── reverb/          # reverb algorithms
│   │   └── shifter/         # pitch shifting DSP (3 voices, 3 algorithms)
│   ├── plugin/              # JUCE AudioProcessor wrappers + editors + shared LookAndFeel
│   └── standalone/          # standalone app build targets
├── third_party/             # JUCE 7 (git submodule)
└── build/                   # generated by meson (not committed)
```

All C++ symbols live in the `kaos_engine` namespace.

---

## License

[GNU Affero General Public License v3.0](https://www.gnu.org/licenses/agpl-3.0.html) (AGPL-3.0)
