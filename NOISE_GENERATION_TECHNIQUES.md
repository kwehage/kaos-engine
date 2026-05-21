# Noise Generation Techniques
## Algorithmic and Mathematical Reference for Experimental Noise, Soundscapes, Harsh Noise Wall, and Horror

---

## Table of Contents

1. [Stochastic Noise Sources](#1-stochastic-noise-sources)
2. [Coherent and Simplex Noise](#2-coherent-and-simplex-noise)
3. [Harsh Noise Wall Architecture](#3-harsh-noise-wall-architecture)
4. [Granular Noise Synthesis](#4-granular-noise-synthesis)
5. [Deterministic Chaos Systems](#5-deterministic-chaos-systems)
6. [Spectral Domain Techniques](#6-spectral-domain-techniques)
7. [Feedback Synthesis and Exploited Instability](#7-feedback-synthesis-and-exploited-instability)
8. [Xenakis Stochastic Synthesis (GENDYN)](#8-xenakis-stochastic-synthesis-gendyn)
9. [Physical Modeling for Horror](#9-physical-modeling-for-horror)
10. [Convolution-Based Industrial Soundscapes](#10-convolution-based-industrial-soundscapes)
11. [Psychoacoustic Unease Techniques](#11-psychoacoustic-unease-techniques)
12. [Velvet Noise](#12-velvet-noise)
13. [Input-Source Interaction Guide](#13-input-source-interaction-guide)
14. [Implementation Priority Reference](#14-implementation-priority-reference)

---

## 1. Stochastic Noise Sources

### 1.1 White Noise

Flat spectrum; all frequencies carry equal energy. The reference against which all colored noise is measured.

**XORShift generator (fast, good statistical properties):**

```cpp
uint32_t state = 0xdeadbeef;
float white_noise() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return float(state) / float(0x80000000u) - 1.0f;
}
```

### 1.2 Pink Noise (1/f)

Spectrum falls at -3 dB/octave. Energy per octave is equal — matches human loudness perception much more closely than white noise. Foundation of most natural ambient textures.

**Paul Kellett 7-variable IIR approximation:**

```cpp
float b0, b1, b2, b3, b4, b5, b6;

float pink_noise(float white) {
    b0 = 0.99886f * b0 + white * 0.0555179f;
    b1 = 0.99332f * b1 + white * 0.0750759f;
    b2 = 0.96900f * b2 + white * 0.1538520f;
    b3 = 0.86650f * b3 + white * 0.3104856f;
    b4 = 0.55000f * b4 + white * 0.5329522f;
    b5 = -0.7616f * b5 - white * 0.0168980f;
    float pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f) * 0.11f;
    b6 = white * 0.115926f;
    return pink;
}
```

### 1.3 Brown / Red Noise (1/f^2)

Spectrum falls at -6 dB/octave. A random walk (Brownian motion) in amplitude. Deep rumble; the lowest spectral color.

**Leaky integrator:**

```cpp
float brn = 0.0f;

float brown_noise(float white) {
    brn = (brn + 0.02f * white) * 0.998f;
    return brn * 3.5f;  // scale to match pink/white loudness
}
```

### 1.4 Blue Noise (f) and Violet / Purple Noise (f^2)

Spectrum rises at +3 dB/octave (blue) or +6 dB/octave (violet). High-frequency emphasis; anti-correlated samples. Obtained by differentiating white noise:

```cpp
float prev = 0.0f;

float blue_noise(float white) {
    float blue = white - prev;
    prev = white;
    return blue * 0.7071f;  // normalize by sqrt(2) to match RMS
}
```

Violet: differentiate twice (`blue - prev_blue`). Violet noise sounds like a hiss with strongly accentuated sibilance; useful for tape hiss or HF excitation.

### 1.5 Band-Limited and Resonant Noise

Pass white noise through a resonant bandpass filter to concentrate energy at a target frequency:

```
y[n] = 2r*cos(omega)*y[n-1] - r^2*y[n-2] + (1 - r)*x[n]

r = exp(-pi * BW / fs)     // BW = bandwidth in Hz
omega = 2*pi*fc/fs         // fc = center frequency
```

A bank of resonant noise sources (each tuned to a different frequency) can synthesize the spectral envelope of any instrument or space.

---

## 2. Coherent and Simplex Noise

Unlike stochastic noise (uncorrelated samples), **coherent noise** produces smooth, continuous random values that interpolate naturally between adjacent points in time or space. At audio rates this means the output has a strong low-frequency bias but never sounds mechanical or periodic. The NoiseSpace iOS app (2025) uses simplex noise as a grain source for its granular engine — the result has more organic texture than white-noise grains while remaining aperiodic.

### 2.1 Perlin Noise (1D Audio Rate)

Ken Perlin's original coherent noise function (1983). Produces smooth random values by interpolating between randomly oriented gradient vectors at integer lattice points.

**1D Perlin noise at audio rate:**

```cpp
// Permutation table (256 entries, doubled to avoid modulo)
static const uint8_t perm[512] = { /* standard shuffle of 0..255 twice */ };

float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
float lerp(float a, float b, float t) { return a + t * (b - a); }
float grad1(int hash, float x) {
    return (hash & 1) ? x : -x;
}

float perlin1d(float x) {
    int X  = int(std::floor(x)) & 255;
    x     -= std::floor(x);
    float u = fade(x);
    return lerp(grad1(perm[X], x), grad1(perm[X+1], x-1.0f), u);
}
```

Advance the x coordinate each sample: `x += freq / fs`. At `freq = 1 Hz` the noise has a 1-second characteristic period. Typical useful range: 0.5-20 Hz for LFO-like textures; 20-200 Hz for slow-moving noise that blends with audio.

**Sonic character:** Smooth, rounded, heavily low-pass filtered random variation. Much lower mean frequency content than white noise. Useful as a very organic tremolo or pitch-wobble source.

### 2.2 Simplex Noise

Ken Perlin's 2001 improvement: simpler gradient scheme, no directional artifacts, faster in higher dimensions, and less expensive for N > 2D.

**1D Simplex at audio rate (simplified):**

```cpp
// Gradient table: alternating +1/-1 values
static const float grad_table[8] = { 1,-1, 1,-1, 1,-1, 1,-1 };

float simplex1d(float x) {
    int i0 = int(std::floor(x));
    int i1 = i0 + 1;
    float x0 = x - float(i0);
    float x1 = x0 - 1.0f;

    // Contribution from lattice point i0
    float t0 = 1.0f - x0*x0;
    t0 = t0 * t0;
    float n0 = t0 * t0 * grad_table[i0 & 7] * x0;

    // Contribution from lattice point i1
    float t1 = 1.0f - x1*x1;
    t1 = t1 * t1;
    float n1 = t1 * t1 * grad_table[i1 & 7] * x1;

    return 0.395f * (n0 + n1);   // scale to approx [-1, 1]
}
```

### 2.3 Fractional Brownian Motion (fBm) — Octave Stacking

Sum multiple octaves of simplex/Perlin noise at geometrically increasing frequencies and decreasing amplitudes. This is the standard technique for adding spectral richness to coherent noise:

```cpp
float fbm(float x, int octaves, float lacunarity = 2.0f, float persistence = 0.5f) {
    float value = 0.0f, amplitude = 0.5f, frequency = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        value     += amplitude * simplex1d(x * frequency);
        frequency *= lacunarity;    // double frequency each octave
        amplitude *= persistence;   // halve amplitude each octave
    }
    return value;
}
```

**Spectral slope of fBm:** `persistence = 0.5` gives a -6 dB/octave spectral slope (brown noise character). `persistence = 0.71` gives -3 dB/octave (pink noise character). Any value between 0 and 1 gives a coherent colored noise between red and white.

| Octaves | Character |
|---|---|
| 1 | Single smooth wave, very low frequency |
| 2-3 | Organic LFO-like texture |
| 4-6 | Rich, multi-rate variation — good for slow evolving noise |
| 7+ | Approaches brown/pink noise character |

### 2.4 2D Simplex as Navigable Noise Space

The key concept behind NoiseSpace: treat a 2D simplex noise field as a spatial map. One axis is time; the other is a "position" within the noise space that you navigate by slowly changing a second coordinate. Different paths through the space produce different evolving textures from the same source.

```cpp
// Navigate through 2D noise space:
float noise_x = 0.0f;   // time axis (advances at audio rate)
float noise_y = 0.0f;   // position in noise space (navigate slowly)

// Per-sample:
noise_x += time_rate / fs;        // e.g. time_rate = 1.0 Hz
noise_y += explore_rate / fs;     // e.g. explore_rate = 0.01 Hz (slow drift)

float sample = simplex2d(noise_x, noise_y);
```

Different `noise_y` paths through the same field sound completely different — even though they are deterministic. This enables reproducible textures (same seed = same result) with organic, non-repeating character.

### 2.5 Simplex Noise as Granular Grain Source (NoiseSpace Approach)

Use 2D simplex noise to generate grain waveforms. Each grain reads from a different (x, y) region of the noise field — grains have more coherence than white noise but more variation than a bandpass-filtered sine:

```cpp
struct NoiseGrain {
    float pos_x, pos_y;   // position in 2D noise field
    float rate_x;         // playback speed through x axis
    float duration;       // grain length in samples
    float phase;          // 0..1 playback position
    float amplitude;
};

// Per-grain sample:
float grain_sample(NoiseGrain& g, int octaves) {
    float t   = g.phase * g.duration;
    float raw = fbm(g.pos_x + t * g.rate_x, g.pos_y, octaves);
    // Hann window:
    float win = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * g.phase));
    g.phase  += 1.0f / g.duration;
    return win * raw * g.amplitude;
}
```

Each newly spawned grain gets a randomly chosen `(pos_x, pos_y)` offset from the current navigation position — nearby grains share local noise character; distant ones are different. This creates a coherence gradient controllable by how far apart grains' starting positions are.

### 2.6 Domain Warping

Feed the output of one fBm evaluation back in as the input coordinate of another. This creates dramatically more complex, swirling structures:

```cpp
float domain_warp(float x, float y) {
    // First layer: displacement field
    float dx = fbm(x + 0.0f, y + 0.0f, 4);
    float dy = fbm(x + 5.2f, y + 1.3f, 4);
    // Second layer: evaluate fbm at warped coordinates
    return fbm(x + 4.0f * dx, y + 4.0f * dy, 4);
}
```

Domain warping produces highly organic, folded textures that look (and sound) like fluid turbulence or geological formations. At audio rate, first-order domain warping creates spectral variations that are unlike any simple filter shape.

---

## 3. Harsh Noise Wall Architecture

HNW aims for a monolithic, static, spectrally full texture. No rhythm, no melody, no dynamics — a pure wall of saturated grain.

### 2.1 Layered Spectral Structure

Stack three complementary layers with intentional overlap:

```
Layer 1 (low):  HPF(20Hz) -> LPF(400Hz) -> heavy_waveshaper    -> out_L
Layer 2 (mid):  BPF(300Hz, 4kHz)        -> feedback_distortion  -> out_M
Layer 3 (high): HPF(3kHz)              -> bitcrush + wavefold   -> out_H

final = tanh(k * (out_L + out_M + out_H))    // master saturation glue, k = 3-8
```

The 300-400 Hz overlap between L and M is deliberate — intermodulation between layers prevents them reading as separate objects.

### 2.2 Feedback Oscillator Core

The characteristic HNW texture comes from self-oscillating feedback loops driven into saturation:

```
y[n] = tanh(g * (x[n] + f * y[n - D]))
```

| Parameter | Range | Effect |
|---|---|---|
| `g` (pre-gain) | 8-20 | Saturation depth |
| `f` (feedback) | 0.85-0.98 | Density; above 0.95 = very dense |
| `D` (delay samples) | 1-100 | Resonant frequency approx. `fs/D` Hz |

Running 4-8 units in parallel with incommensurate `D` values (7, 13, 17, 23, 31, 41...) creates a dense comb resonance that fills the spectrum when saturated. The tanh clamp bounds the feedback and shapes the characteristic texture.

### 2.3 Stacked Waveshaping

Series waveshaping cascades fold the spectrum progressively upward:

```cpp
float v1 = std::tanh(g1 * x);
float v2 = v1 - (v1 * v1 * v1) / 3.0f;   // cubic on already-saturated signal
float v3 = std::clamp(g3 * v2, -1.0f, 1.0f);  // hard clip final pass
```

Insert a resonant BPF (500-5000 Hz) between stages to control spectral color. Each stage folds harmonics further upward.

### 2.4 Anti-Aliasing for Stacked Distortion

Without oversampling, stacked waveshapers alias severely. Oversample 4-8x:

1. Upsample by L (polyphase FIR, ~32 taps per phase)
2. Apply waveshaper chain at L*fs
3. Downsample with steep LPF (cutoff at `fs_orig / 2`)

At 4x oversampling, alias products fold at 88.2 kHz instead of 22.05 kHz — inaudible for most waveshaper functions.

**ADAA (Antiderivative Anti-Aliasing)** as a 1x-rate alternative:

```
y[n] = (F1(x[n]) - F1(x[n-1])) / (x[n] - x[n-1])

// Fall back to f((x[n]+x[n-1])/2) when |x[n]-x[n-1]| < 1e-5
```

Where `F1` is the antiderivative of the waveshaper function:
- tanh: `F1(x) = ln(cosh(x))`
- hard clip T: `F1(x) = x^2/2` for `|x| <= T`; `T*x - T^2/2` for `|x| > T`
- foldback sin: `F1(x) = -cos(x)`

---

## 4. Granular Noise Synthesis

### 3.1 Grain Cloud Fundamentals

Asynchronous granular synthesis places windowed noise bursts stochastically in time, amplitude, and space. Each grain is an independent random event.

**Grain parameters:**

| Parameter | Symbol | Range | Distribution |
|---|---|---|---|
| Onset time | `t` | Controlled by density | Poisson: `t ~ Exp(1/density)` |
| Duration | `t_dur` | 5-400 ms | Uniform or Gaussian |
| Source position | `pos` | 0..buffer_len | Uniform + jitter |
| Amplitude | `A` | 0-1 | Gaussian or beta |
| Pan | `p` | -1..+1 | Uniform |
| Playback rate | `r` | 0.5-2.0 | Lognormal |

**Output equation:**

```
y[n] = sum_k { A_k * w_k[n - t_k] * source[pos_k + rate_k * (n - t_k)] }
```

where `w_k` is the grain window (Hann recommended) and the sum is over all active grains.

### 3.2 Scatter Parameters

```cpp
float pos    = base_pos + rand_uniform(-spray, spray);     // position spray
float t_next = t_last + (1.0f / density) + rand_gaussian(0, jitter_ms * fs / 1000.0f);
float amp    = amp_base * (1.0f + rand_gaussian(0.0f, amp_variance));
```

| Density | Character |
|---|---|
| > 200 grains/sec | Continuous texture, individual grains inaudible |
| 30-200 grains/sec | Granular shimmering, slight periodicity |
| < 30 grains/sec | Individual grains audible as clicks or bursts |

### 3.3 Noise-Specific Grain Configurations

**Harsh texture:** band-limited white noise source, 100-500 grains/sec, 5-20 ms duration, large position spray (±50%), sharp attack (1 ms) + instant decay.

**Industrial cloud:** pink noise source, 50-150 grains/sec, 50-100 ms duration, Hann window, slight rate scatter ±0.05 for pitch-smearing roughness.

**Horror shimmer:** resonant noise BPF at 800-2000 Hz, 20-80 grains/sec, 80-200 ms duration, random pan, amplitude modulated by slow LFO (0.1-0.5 Hz).

### 3.4 Real-Time Scheduling

Maintain a priority queue of grain onset times sorted by `t_next`. Each audio block, process all grains with `t_onset < block_end`. Use a pre-allocated pool of grain objects (64-256 maximum simultaneous grains) to avoid heap allocation on the audio thread.

---

## 5. Deterministic Chaos Systems

Chaotic systems produce aperiodic, deterministic outputs. Sonic character lies between white noise (flat spectrum) and a tone (discrete spectrum) — attractor-specific colored spectra that never repeat but are not random.

### 4.1 Logistic Map

The simplest chaotic system:

```
x[n+1] = r * x[n] * (1 - x[n])     x in [0,1], r in [0,4]
```

**Behavior by r:**

| r | Behavior | Audio character |
|---|---|---|
| 0-1 | Decays to 0 | Silence |
| 1-3 | Fixed point | DC |
| 3.0-3.449 | Period-2 oscillation | Sub-audio tone |
| 3.449-3.544 | Period-4, 8... | Complex sub-audio |
| 3.57-4.0 | Chaos (with stability islands) | Colored noise |
| 4.0 | Fully chaotic, ergodic | Near-white noise on [0,1] |

**Audio output:** `audio[n] = 2*x[n] - 1`

**Sweeping r** from 3.57 to 4.0 transitions from tonal to chaotic — a continuous morph between a complex tone and noise. The transition passes through period-doubling cascades (audible as sub-octave descents).

```cpp
float logistic(float& x, float r) {
    x = r * x * (1.0f - x);
    return 2.0f * x - 1.0f;
}
```

### 4.2 Lorenz Attractor

Three-dimensional continuous-time system. Famous butterfly attractor.

**Differential equations:**

```
dx/dt = sigma * (y - x)
dy/dt = x * (rho - z) - y
dz/dt = x * y - beta * z
```

**Classic chaotic parameters:** `sigma = 10, rho = 28, beta = 8/3`

**RK4 integration (recommended over Euler for stability at audio rates):**

```cpp
struct LorenzState { float x, y, z; };

LorenzState lorenz_deriv(LorenzState s, float sigma, float rho, float beta) {
    return {
        sigma * (s.y - s.x),
        s.x * (rho - s.z) - s.y,
        s.x * s.y - beta * s.z
    };
}

void lorenz_step(LorenzState& s, float dt, float sigma, float rho, float beta) {
    auto k1 = lorenz_deriv(s, sigma, rho, beta);
    LorenzState s2 = { s.x+0.5f*dt*k1.x, s.y+0.5f*dt*k1.y, s.z+0.5f*dt*k1.z };
    auto k2 = lorenz_deriv(s2, sigma, rho, beta);
    LorenzState s3 = { s.x+0.5f*dt*k2.x, s.y+0.5f*dt*k2.y, s.z+0.5f*dt*k2.z };
    auto k3 = lorenz_deriv(s3, sigma, rho, beta);
    LorenzState s4 = { s.x+dt*k3.x, s.y+dt*k3.y, s.z+dt*k3.z };
    auto k4 = lorenz_deriv(s4, sigma, rho, beta);
    s.x += dt/6.0f * (k1.x + 2*k2.x + 2*k3.x + k4.x);
    s.y += dt/6.0f * (k1.y + 2*k2.y + 2*k3.y + k4.y);
    s.z += dt/6.0f * (k1.z + 2*k2.z + 2*k3.z + k4.z);
}
```

**Audio output:** use `x`, `y`, or `z` normalized by ~20 (typical coordinate range). `z` has more low-frequency energy; `x` and `y` are more complex.

**dt parameter:** smaller dt = slower, lower-pitched oscillation. Typical range: 0.001-0.05 per sample. At `dt = 0.01` and 44.1 kHz the Lorenz fundamental maps to roughly 300-800 Hz.

**Modulating rho:** sweeping `rho` between 24 (near bifurcation at 24.74) and 35 morphs between ordered spiraling and full butterfly chaos.

### 4.3 Duffing Oscillator

Forced nonlinear spring. Produces intermittent chaos — the signal oscillates semi-regularly for a while then bursts into noise, then returns to order. This intermittency is effective for horror textures.

**Equations (as two first-order ODEs):**

```
dx/dt = v
dv/dt = -delta*v - alpha*x - beta*x^3 + gamma*cos(omega*t)
```

**Chaotic parameter set:**

```
alpha = -1,  beta = 1,  delta = 0.3,  gamma = 0.50,  omega = 1.2
```

Audio output: `x` normalized by expected range (~1.5 for above parameters).

### 4.4 Chua's Circuit

Three-dimensional system with a piecewise-linear nonlinearity. Produces the double-scroll attractor — audible as intermittent pitch-switching between two lobes.

**Equations:**

```
dx/dt = alpha * (y - x - f(x))
dy/dt = x - y + z
dz/dt = -beta * y

f(x) = m1*x + 0.5*(m0-m1)*(|x+1| - |x-1|)
```

**Double-scroll attractor parameters:** `alpha = 9, beta = 100/7, m0 = -1/7, m1 = 2/7`

```cpp
float chua_f(float x, float m0, float m1) {
    return m1*x + 0.5f*(m0 - m1)*(std::abs(x + 1.0f) - std::abs(x - 1.0f));
}
```

Varying `alpha` (5-12) shapes the attractor and controls spectral density.

---

## 6. Spectral Domain Techniques

All techniques below operate in the STFT domain: FFT -> modify bins -> IFFT with overlap-add. Standard parameters: N = 1024-4096, hop = N/4, Hann window.

### 5.1 Phase Randomization

Preserves spectral magnitude; destroys temporal coherence. Produces a smeared, whisper-like version of the input.

```
X[k,m] = |X[k,m]| * exp(j * (original_phase + rand_uniform(0, 2*pi) * depth))
```

- `depth = 0`: unmodified
- `depth = 0.1-0.5`: ghostly smearing, transients lose sharpness
- `depth = 1.0`: full randomization — spectral freeze / whisper effect

### 5.2 Spectral Smearing

Blur each bin's magnitude over neighboring bins with a Gaussian kernel:

```
|X_smeared[k]| = sum_{n=-W}^{W} |X[k+n]| * gauss[n+W]
```

Width `W` = 3-30 bins. At W=20, N=2048, 44.1 kHz: ~430 Hz of spectral blurring per bin. Destroys pitch cues; combine with phase randomization for maximum incoherence. Larger W = less identifiable source; useful to transform recognizable sounds into uncanny noise.

### 5.3 Per-Bin Noise Injection

Add complex noise to individual frequency bins:

```cpp
// Add noise to all bins:
X[k] += noise_level * (rand_gaussian() + j * rand_gaussian());

// Add noise only above a frequency threshold (e.g. inject hiss above 3 kHz):
if (bin_to_hz(k) > 3000.0f)
    X[k] *= (1.0f + noise_level * rand_uniform(-1.0f, 1.0f));
```

Preserves spectral envelope while adding stochastic variation frame-to-frame.

### 5.4 Spectral Freeze and Morph

Hold a frozen spectral frame and interpolate toward new frames over time:

```cpp
// alpha ramps 0->1 over morph_time seconds
X_output[k] = (1.0f - alpha) * X_frozen[k] + alpha * X_current[k];
```

For horror atmospheres: freeze a room tone, then morph toward colored noise — the space gradually loses its acoustic identity. Crossfade multiple frozen frames out of phase with each other to create slow spectral churning.

### 5.5 Spectral Convolution with Noise Profiles

Replace the magnitude spectrum of the input with a target noise profile while preserving phase:

```
|X_shaped[k]| = target_envelope[k]
phase_shaped[k] = phase_of_original[k]
```

Target envelope can be: measured room noise floor, pink noise (`1/sqrt(k)`), or any desired noise color. The result sounds like the noise filtered through the phase structure of the original signal.

---

## 7. Feedback Synthesis and Exploited Instability

### 6.1 Feedback Comb Filter

```
y[n] = x[n] + g * y[n - M]
```

Resonant peaks at harmonics of `fs/M` Hz. Stable for `|g| < 1`. For noise synthesis, push toward instability with a saturator in the loop:

```cpp
y[n] = x[n] + g * std::tanh(k * y[n - M]);
```

With `g = 0.999, k = 2`: near-unstable dense resonance that never blows up.
With `g = 1.02, k = 1`: true self-oscillation locked by saturation — generates a pitched, saturated drone.

### 6.2 Allpass Chain Feedback

A cascade of N Schroeder allpass sections (each: flat magnitude, dispersive phase):

```
v[n] = x[n] - a * v[n - L]
y[n] = a * v[n] + v[n - L]
```

Cascade 6-8 sections with mutually prime `L` values spanning 1-100 ms. Feed output back into input with `g_feedback` slightly below 1.0. The result is a dense reverberant texture. Modulating `a` coefficients with slow LFOs (0.1-2 Hz) breaks up periodic modal buildup.

**Instability burst:** temporarily push `g_feedback` above 1.0 (up to ~1.05) for a short burst to create a growing, unstable swell — tanh clamp prevents runaway while allowing dramatic energy build.

### 6.3 Resonant Filter Self-Oscillation

An SVF oscillates when Q is pushed past the stability boundary (poles on or outside the unit circle). Drive resonance past that boundary while injecting small-amplitude noise to sustain it:

```
Pole radius r = exp(-pi * fc / (Q * fs))

r >= 1.0  ->  self-oscillation
```

The SVF topology is more robust for this use case than direct-form biquads — less susceptible to quantization overflow at extreme resonance. A tiny noise injection (`< -80 dBFS`) keeps the self-oscillation active without adding audible noise.

### 6.4 Feedback Delay Network (FDN) Noise

N comb filters cross-coupled through a unitary matrix — same structure as algorithmic reverb:

```
state[n] = A * state[n - L_vec] + b * x[n]
y[n] = c^T * state[n]
```

With `|eigenvalues(A)| = 1` (unitary/Hadamard), the network sustains indefinitely. Insert `tanh(k * .)` inside the loop to create bounded self-oscillation. The Hadamard matrix maximally decorrelates delay lines, generating very dense noise-like texture.

---

## 8. Xenakis Stochastic Synthesis (GENDYN)

### 7.1 Core Concept

GENDYN treats the waveform as a polygonal line of N breakpoints connected by linear interpolation. At each cycle, every breakpoint undergoes an independent random walk in both time and amplitude. The waveform shape itself evolves stochastically, producing sounds that are perpetually in flux — neither noise nor tone but something between them.

### 7.2 Breakpoint Perturbation

```cpp
// State: N_bp breakpoints, each with time_offset[i] and amplitude[i]
for (int i = 0; i < N_bp; ++i) {
    float delta_t = sample_distribution(dist_t, step_size_t);
    float delta_a = sample_distribution(dist_a, step_size_a);

    time_offset[i] = reflect_barrier(time_offset[i] + delta_t, 0.0f, max_period_samples);
    amplitude[i]   = reflect_barrier(amplitude[i]   + delta_a, -1.0f, 1.0f);
}

// Reflect at barrier (bounces back, not absorbed):
float reflect_barrier(float x, float lo, float hi) {
    while (x < lo) x = 2.0f * lo - x;
    while (x > hi) x = 2.0f * hi - x;
    return x;
}
```

### 7.3 Waveform Output

Between waveform cycles, interpolate linearly between sorted breakpoints:

```cpp
// Within one period — find segment and interpolate:
int seg = find_segment(current_phase, time_offset, N_bp);
float t_frac = (current_phase - time_offset[seg]) /
               (time_offset[seg+1] - time_offset[seg]);
float y = amplitude[seg] + t_frac * (amplitude[seg+1] - amplitude[seg]);
```

### 7.4 Probability Distributions

Xenakis found that barrier positions dominated sonic character more than distribution choice. However, heavy-tailed distributions produce more dramatic variation:

**Cauchy distribution (heavy-tailed — rare large jumps occur frequently):**

```cpp
float cauchy_sample(float scale) {
    float u = rand_uniform(0.0f, 1.0f);
    return scale * std::tan(float(M_PI) * (u - 0.5f));
}
// Clamp to prevent extreme outliers:
return std::clamp(cauchy_sample(scale), -max_step, max_step);
```

**Gaussian:** smoother evolution, fewer abrupt changes.
**Arcsine:** bimodal — values tend toward the extremes of their range.

### 7.5 Key Parameters

| Parameter | Effect |
|---|---|
| `N_bp` (breakpoints per period) | More = lower fundamental pitch; Xenakis used 12-30 |
| `step_size_t` | Controls pitch stability; larger = more chaotic period variation |
| `step_size_a` | Controls spectral variation rate; larger = more timbre mutation |
| Distribution type | Cauchy = abrupt changes; Gaussian = gradual morphing |

**GENDY3 variant** (second-order random walk — walking the walk):

```cpp
delta_t_walk[i] += sample_distribution(d_meta, step_meta);
delta_t[i] += delta_t_walk[i];
time_offset[i] = reflect_barrier(time_offset[i] + delta_t[i], 0.0f, max_t);
```

---

## 9. Physical Modeling for Horror

### 8.1 Modal Synthesis — Creaking and Scraping

A rigid body's sound is the sum of its vibrational modes, each modeled as a damped oscillator. In real-time, each mode is a resonant biquad:

```cpp
// Mode at frequency f_k with decay time T60_k:
float r_k     = std::exp(-6.908f / (T60_k * fs));     // pole radius
float omega_k = 2.0f * float(M_PI) * f_k / fs;
float b0      = 1.0f - r_k;
float a1      = -2.0f * r_k * std::cos(omega_k);
float a2      = r_k * r_k;

// Per-sample:
y[n] = b0 * x[n] - a1 * y[n-1] - a2 * y[n-2];
```

**For horror:** use inharmonic mode frequencies. A metal bar's modes follow:

```
f_k = f_1 * k^2 * sqrt(1 + B * k^2)
```

`B = 0.001-0.01` (inharmonicity constant) makes upper modes progressively sharp — immediately unsettling. For piano wire: `B ~= 0.0002`. For a taut metal cable near breaking: `B ~= 0.01-0.05`.

### 8.2 Stick-Slip Friction for Creaking

Drive the mode bank with friction-generated noise rather than an impulse:

```cpp
float stick_slip_friction(float velocity, float normal_force,
                          float mu_s, float mu_d) {
    const float vel_threshold = 0.001f;
    if (velocity < vel_threshold)
        return mu_s * normal_force * velocity / vel_threshold;  // sticking
    else
        return mu_d * normal_force * std::tanh(10.0f * velocity);  // sliding
}
```

Transitions between stick and slip states produce the characteristic chattering/creaking texture. Modulate `velocity` slowly (0.1-2 Hz) to create recurring creak events. At very low velocities, intermittent stick-slip produces horror-genre "settling" sounds.

### 8.3 Inharmonic Karplus-Strong

Standard Karplus-Strong with modifications for tension and dread:

**Standard:**
```
y[n] = 0.5 * (delay[n-D] + delay[n-D-1])    // averaging LP in loop
delay[n] = y[n]
```

**Inharmonicity via dispersive allpass in loop:**

```cpp
// Stiffness allpass: detunes upper harmonics progressively upward
// a controls the amount of inharmonicity
float allpass_stiffness(float x, float& y_prev, float& x_prev, float a) {
    float y = a * (x - y_prev) + x_prev;
    x_prev = x;
    y_prev = y;
    return y;
}
```

**Coupled detuned strings for beating:**

```cpp
// Two slightly detuned Karplus-Strong loops with cross-coupling:
y1[n] = 0.49f * y1[n-D1] + 0.49f * y1[n-D1-1] + 0.02f * y2[n-D2];
y2[n] = 0.49f * y2[n-D2] + 0.49f * y2[n-D2-1] + 0.02f * y1[n-D1];
```

The cross-coupling produces beating that evolves over time — neither periodically nor randomly, but in a complex, organic pattern.

### 8.4 Shepard-Risset Glissando (Endless Descent)

Place pitched signals at octave intervals; fade them in/out as they approach spectral extremes. The percept is an endlessly descending tone with no actual change in frequency range.

```cpp
const int N_oct = 8;
const float ramp_rate = -0.02f;  // descend; positive = ascend

float ramp = 0.0f;  // advances continuously

// Per-sample:
float output = 0.0f;
for (int k = 0; k < N_oct; ++k) {
    float octave_pos = std::fmod(k + ramp, float(N_oct));
    float f_k        = f_base * std::pow(2.0f, octave_pos);
    float env_k      = gaussian_bell(octave_pos / N_oct, 0.5f, 0.15f);  // centre bell
    output += env_k * oscillator(f_k);
}
ramp += ramp_rate * dt;
```

Combine with inharmonic modal resonance for maximum dread. A descending glissando with increasing roughness and inharmonicity is a highly effective horror cue.

---

## 10. Convolution-Based Industrial Soundscapes

### 9.1 Synthesizing Industrial Impulse Responses

Algorithmically generate the acoustic signature of industrial spaces:

```cpp
// Sparse resonant IR of a metal chamber:
for (int t = 0; t < ir_length; ++t) {
    float ir_t = 0.0f;
    for (int k = 0; k < N_modes; ++k) {
        ir_t += A[k] * std::exp(-decay[k] * t / fs)
              * std::sin(2.0f * float(M_PI) * freq[k] * t / fs);
    }
    ir[t] = ir_t + noise_floor * std::exp(-t / (total_decay * fs)) * rand_gaussian();
}
```

**Inharmonic resonance frequency sets:**

| Source | Frequency pattern |
|---|---|
| Pipe (length L) | `f_k = k * c / (2*L)`, c = 343 m/s |
| Metal plate (modes m,n) | `f_mn proportional to (m^2 + n^2)` |
| Spring/coil | Near-equal spacing + small random deviations |
| Decaying room with flutter | Short exponential + delayed comb echo |

### 9.2 Real-Time Convolution (Overlap-Add)

```cpp
// Per block, size B:
auto X = FFT(input_block, 2*B);           // zero-padded
auto Y = X * H_fft;                        // complex multiply with pre-FFT'd IR
auto y_block = IFFT(Y);
output[0..B-1]    += y_block[0..B-1];
overlap[0..B-1]    = y_block[B..2*B-1];  // save for next block
```

For long IRs (multi-second industrial reverbs) use non-uniform partitioning: first partition = 64 samples (low latency), subsequent partitions double in size (128, 256, 512 ... 16384).

### 9.3 Evolving Convolution for Dread

Modify the frequency-domain IR between blocks to create slow spectral evolution:

```cpp
// Drift phase of each bin slowly — creates spectral smearing over time:
phase_drift[k] += drift_rate * k / N;     // high frequencies drift faster
H_modified[k]   = H[k] * std::polar(1.0f, phase_drift[k]);
```

---

## 11. Psychoacoustic Unease Techniques

### 10.1 Critical Bandwidth and Roughness

Roughness is perceived when beating rates fall in the range 20-200 Hz (within one critical bandwidth). The Plomp-Levelt / Sethares formula for dissonance between two pure tones at frequencies `f1, f2`:

```
s    = 0.24 / (0.021 * min(f1,f2) + 19)
d    = exp(-3.51 * s * |f2-f1|) - exp(-5.75 * s * |f2-f1|)
```

`d = 0` = consonant (unison or octave); `d = max` = maximum roughness at approximately 25% of the critical bandwidth above the lower frequency.

**Peak roughness beat rates by frequency:**

| Lower tone | Critical BW | Peak roughness at |
|---|---|---|
| 100 Hz | ~100 Hz | f2 = 125 Hz (+25 Hz) |
| 500 Hz | ~105 Hz | f2 = 526 Hz (+26 Hz) |
| 1000 Hz | ~130 Hz | f2 = 1033 Hz (+33 Hz) |
| 4000 Hz | ~700 Hz | f2 = 4175 Hz (+175 Hz) |

**Dissonance calculation over a spectrum (Sethares):**

```cpp
float dissonance(const std::vector<float>& freqs, const std::vector<float>& amps) {
    float D = 0.0f;
    for (size_t i = 0; i < freqs.size(); ++i) {
        for (size_t j = i+1; j < freqs.size(); ++j) {
            float fmin  = std::min(freqs[i], freqs[j]);
            float fdif  = std::abs(freqs[j] - freqs[i]);
            float s     = 0.24f / (0.021f * fmin + 19.0f);
            float sfdif = s * fdif;
            float a     = std::min(amps[i], amps[j]);
            D += a * (5.0f * std::exp(-3.51f * sfdif) - 5.0f * std::exp(-5.75f * sfdif));
        }
    }
    return D;
}
```

### 10.2 Infrasound and Near-Infrasound Modulation

Frequencies below ~20 Hz are felt rather than heard. Research confirms that infrasound combined with audio is rated more unpleasant than either alone. Gaspar Noe used 27 Hz in *Irreversible* to induce nausea.

**Near-infrasound implementation:**

```cpp
// Amplitude modulation at infrasonic rate (6-19 Hz):
float lfo = std::sin(2.0f * float(M_PI) * f_infra * n / fs);  // f_infra = 6-19 Hz
y[n] = x[n] * (1.0f + depth * lfo);   // depth 0.5-1.0 = strong pumping

// Pitch modulation (vibrato at infrasonic rate):
float delay_mod = D_base * (1.0f + 0.01f * lfo);  // 1% depth = ~±2 cents at fundamental
y[n] = interpolated_delay_read(delay_mod);
```

Strongest effect through headphones or a subwoofer. 18 Hz at moderate amplitude creates powerful physical unease.

### 10.3 Missing Fundamental

Generate harmonics 2f, 3f, 4f, 5f without the fundamental f. The auditory system infers f0 even though it is absent — creates the sensation of impossibly deep frequencies:

```cpp
float missing_fundamental(float f0, float t) {
    return std::sin(2.0f * float(M_PI) * 2*f0 * t)
         + 0.70f * std::sin(2.0f * float(M_PI) * 3*f0 * t)
         + 0.50f * std::sin(2.0f * float(M_PI) * 4*f0 * t)
         + 0.40f * std::sin(2.0f * float(M_PI) * 5*f0 * t);
}
```

With `f0 = 30-40 Hz`, systems unable to reproduce those frequencies will still imply them through the harmonic series. Combine with subharmonic generation (octave divider circuit) for maximum perceived sub-bass.

### 10.4 Ring Modulation for Controlled Roughness

Ring modulation at low carrier frequencies (20-200 Hz) directly produces roughness within the critical bandwidth:

```cpp
y[n] = x[n] * std::sin(2.0f * float(M_PI) * fc * n / fs);  // fc = 30-150 Hz
```

For a drone at f0, ring modulation adds sidebands at f0±fc. With fc = 30 Hz, all sidebands are within one critical bandwidth of their origin — maximum roughness at a predictable, controlled frequency. More controllable than additive beating.

### 10.5 Auditory Masking Exploitation

**Spectral masking for startle response:**
1. Sustain a dense broadband noise at 1-4 kHz (peak hearing sensitivity) for 10-30 seconds
2. Cut suddenly to near-silence
3. The silence feels physically overwhelming — the auditory system takes 200-500 ms to re-calibrate sensitivity

**Temporal pre-masking:**
A loud burst (< 5 ms) followed 10-15 ms later by a softer target makes the target louder than it is — use for jump-scare enhancement. The cochlea has a ~20 ms pre-masking window within which prior energy affects perception of subsequent sounds.

**Build-release cycle (dread arc):**
- 0-30s: build dense, mid-frequency masking
- 30s: sudden cut to silence
- 30-31s: silence perceived as overwhelming presence
- Use recursively for sustained unease

### 10.6 Bark Scale and Critical Band Shaping

The Bark scale maps frequency to perceptual roughness bandwidth. Designing distortion that adds harmonics spaced at exactly 1 critical bandwidth apart within 1-4 kHz maximizes perceived roughness:

```cpp
// Bark scale (Traunmuller 1990 approximation):
float hz_to_bark(float f) {
    return 26.81f * f / (1960.0f + f) - 0.53f;
}

// ERB (Equivalent Rectangular Bandwidth) -- Moore & Glasberg:
float erb(float f) {
    return 24.7f * (4.37f * f / 1000.0f + 1.0f);
}

// Frequency that is exactly 1 ERB above f:
float one_erb_above(float f) {
    return f + erb(f);
}
```

A spectrum with components separated by exactly 1 ERB in the 1-4 kHz range produces the harshest possible roughness at each critical band.

---

## 12. Velvet Noise

Velvet noise is a sparse sequence of +1, 0, -1 where each time window of length `M = fs/density` samples contains exactly one non-zero value.

**Generation:**

```cpp
void generate_velvet_noise(float* vn, int length, float density, float fs) {
    int M = int(fs / density);  // window length in samples
    std::fill(vn, vn + length, 0.0f);
    for (int m = 0; m * M < length; ++m) {
        int pos  = m * M + rand_int(0, M - 1);
        float s  = (rand_uniform(0.0f, 1.0f) > 0.5f) ? 1.0f : -1.0f;
        if (pos < length) vn[pos] = s;
    }
}
```

Convolving audio with a velvet noise IR reduces to additions and sign-flips only (no multiplications per non-zero pulse). With density 2000-6000 pulses/sec, the convolution produces smooth, spectrally flat late reverberation. Modulate pulse signs over time for an evolving texture.

**Colored velvet noise:** apply a spectral shaping filter (single-pole LP or HP) to the impulse positions to create frequency-weighted late reverb:

```
VN_colored[k] = VN[k] * H_spectral[k]    // in frequency domain
```

**Dark velvet noise:** replace each unit impulse with a running-sum block of length L (a rectangular window). This creates a lowpass-colored texture without multiplication.

---

## 13. Input-Source Interaction Guide

This section is specific to the kaos-engine context: noise as an effect on live instrument signals (guitar, synth, bass) rather than as a standalone generator. Three interaction patterns exist, and each technique fits primarily into one.

---

### Pattern A — Driven by Input

The input signal **controls or gates** the noise: its amplitude, envelope, frequency content, or moment-to-moment waveform determines what the noise does. The noise itself is generated independently but its character is modulated by what is playing.

| Technique | How input drives it | Sonic result on guitar/synth |
|---|---|---|
| **Follow / Gated mode** (§1) | Input envelope opens a gate on any noise type; Follow scales amplitude proportionally | Noise follows the instrument's dynamics — quiet passages are clean, loud notes bring in texture. On guitar: adds air or grit only when strings are struck |
| **Logistic map coupled chaos** (§5) | Input level sets the chaos parameter `r`; louder input = deeper chaos | Notes played at different dynamics produce different noise characters — a quiet chord produces near-periodic texture; a loud chord produces full chaos |
| **Lorenz / Duffing driven** (§5) | Use input envelope to modulate `rho` (Lorenz) or `gamma` (Duffing); transitions attractor between ordered and chaotic regimes | Playing louder pushes the attractor into chaos; release lets it fall back to ordered oscillation — noise has a musical "decay arc" that follows the note |
| **Stick-slip friction model** (§9) | Input amplitude maps to bow velocity in the friction model; dynamics control stick/slip ratio | Soft playing = smooth resonance; hard playing = chattering creak. Highly responsive to pick attack on guitar |
| **Resonant noise bank with envelope** (§1.5) | Input envelope modulates gain of a filter bank tuned to inharmonic frequencies | The instrument's playing intensity shapes how much of an industrial resonant texture bleeds in |
| **Infrasonic AM driven by envelope** (§11) | Input RMS envelope scales the infrasonic modulation depth | Loud sustained notes create physical unease; silence removes it. Attack transients trigger sudden modulation onset |

**Implementation pattern:**
```cpp
// Envelope follower driving noise amount
env  = alpha_r * env + (1.0f - alpha_r) * std::abs(input);  // smooth
env  = (input > threshold) ? alpha_a * env + ... : ...;       // attack/release
noise_gain = std::pow(env / ref_level, drive_curve);          // nonlinear mapping
output = input + noise_gain * noise_source;
```

---

### Pattern B — Injected Into Input

The input signal **passes through** the noise process: the noise is applied to the signal's waveform, spectrum, or amplitude, transforming the input itself rather than running in parallel.

| Technique | How it transforms the input | Sonic result on guitar/synth |
|---|---|---|
| **Add blend** (kaos-engine) | `out = (1-w)*input + w*noise` | Parallel layer — at low MIX adds air; at high MIX submerges the instrument in noise. Clean and controllable |
| **AM blend** (kaos-engine) | `out = input * (1 + mix*noise)` | Noise amplitude-modulates the instrument. Every noise sample multiplied into the signal adds sideband texture to every harmonic. Very effective on sustained synth pads |
| **Saturate blend** (kaos-engine) | `out = tanh(input + mod*noise)` | Noise drives a soft clipper on the instrument — adds harmonics whose character changes with the noise. On guitar: adds organic, non-periodic grit |
| **Spectral blend / per-bin noise injection** (kaos-engine / §6) | Each FFT bin's magnitude scaled by `1 + mod*n_k` | Per-frequency-band amplitude modulation. Creates a shimmering, constantly shifting spectral texture on the instrument without changing its overall frequency balance |
| **Waveshaping with noise input** (§3) | Noise is added before a waveshaper stage in the instrument's signal chain | The waveshaper's operating point shifts each sample, producing harmonics whose character is noise-modulated. Less predictable than clean distortion |
| **Ring modulation at noise rate** (§11) | Carrier is a noise-derived signal rather than a sine | Adds sideband content whose position varies with the noise — sounds like a badly tuned ring mod but with organic randomness. Effective on metallic synth timbres |
| **Spectral phase randomization** (§6) | FFT of instrument -> randomize bin phases -> IFFT | Smears transients; creates a ghostly, reverberant quality without adding energy. At `depth = 0.3` the instrument is still recognizable but texturally transformed |

**Implementation pattern:**
```cpp
// AM injection — most musically useful for synth/guitar
float noise  = generate_noise();                    // any source from §1-§5
float mod    = noise * gain * gate_envelope;        // gated/shaped
output = input * (1.0f + mix * mod);               // AM: sidebands on every harmonic
```

---

### Pattern C — Mutual Interaction

The input signal and the noise source **affect each other**: the signal seeds, excites, or entangles with the noise in a feedback or convolution relationship. The boundary between "instrument" and "noise" dissolves.

| Technique | Interaction mechanism | Sonic result on guitar/synth |
|---|---|---|
| **Residual noise** (kaos-engine) | Input high-pass residual `n = x - LP(x)` IS the noise | The noise is literally extracted from the instrument — attack transients, pick noise, bow rosin become the noise source. The instrument and noise share the same identity |
| **Diffuse allpass of input** (kaos-engine) | Schroeder allpass cascade applied to input; same spectrum, smeared in time | The instrument's own sound creates a ghost of itself, time-smeared. On guitar: each note creates a diffuse shimmer that lingers |
| **Coupled logistic chaos** (kaos-engine) | Input drives a chaotic map; map output is mixed back | The instrument entangles with a chaotic system — different notes produce different chaos behaviors. Fast passages may settle into chaos; slow notes may find period-2 stability |
| **Modal resonator bank excited by input** (§9) | Input is the excitation signal for a resonant body | The instrument plays a fictional physical object — a metal plate, a stretched wire. The resonator adds inharmonic overtones coherent with the playing but with their own decay |
| **Convolution with industrial IR** (§10) | Input convolved with a synthesized industrial impulse response | The guitar/synth sounds as if it is being played inside a metal tank, pipe, or spring. The room responds to every articulation |
| **Feedback comb network seeded by input** (§7) | Input feeds into a near-unstable feedback network | The instrument excites a resonant noise system that feeds back on itself. Hard playing pushes the network toward instability; soft playing keeps it just below resonance |
| **GENDYN breakpoints driven by input** (§8) | Input envelope modulates `step_size` parameters; louder input = faster mutation | Playing dynamics control how fast the stochastic waveform mutates — loud notes accelerate the GENDYN evolution toward noise; quiet notes settle it toward quasi-periodic tones |
| **Simplex noise with input-mapped coordinates** (§2) | Input RMS or fundamental frequency navigates through the 2D noise space | Different notes or playing intensities map to different regions of the noise space, producing coherent but unpredictably different textures per note |

**Implementation pattern:**
```cpp
// Input-excited modal resonator bank
float excite = input;                             // instrument is the excitation
float output_modal = 0.0f;
for (int k = 0; k < N_modes; ++k) {
    // resonant biquad at inharmonic frequency f_k:
    float y = b0*excite - a1*y1[k] - a2*y2[k];
    y2[k] = y1[k]; y1[k] = y;
    output_modal += mode_gain[k] * y;
}
output = (1.0f - mix) * input + mix * output_modal;
```

---

### Suitability Summary for Guitar and Synth

| Technique | Guitar | Synth pads | Synth leads | Best pattern |
|---|---|---|---|---|
| Follow/Gated mode + any noise | Excellent | Good | Good | A |
| Residual noise | Excellent | Poor | Moderate | C |
| Diffuse allpass | Excellent | Excellent | Moderate | C |
| AM blend | Good | Excellent | Good | B |
| Saturate blend | Excellent | Good | Excellent | B |
| Spectral blend (per-bin) | Good | Excellent | Good | B |
| Coupled logistic chaos | Excellent | Good | Excellent | C |
| Modal resonator excitation | Excellent | Good | Good | C |
| Convolution with industrial IR | Excellent | Excellent | Good | C |
| Lorenz driven by envelope | Good | Excellent | Moderate | A |
| Infrasonic AM driven | Good | Excellent | Good | A |
| Simplex noise mapped to note | Good | Excellent | Excellent | C |
| Feedback comb seeded | Excellent | Moderate | Good | C |
| Ring mod at noise rate | Good | Good | Excellent | B |
| Spectral phase randomization | Moderate | Excellent | Moderate | B |

---

## 14. Implementation Priority Reference

| Technique | CPU cost | Complexity | Experimental impact | Horror impact |
|---|---|---|---|---|
| Logistic map chaos | Very low | Very low | Medium | Medium |
| Infrasonic AM | Very low | Very low | Low | Very high |
| Missing fundamental | Low | Low | Low | Medium |
| Roughness beating | Very low | Low | Medium | High |
| GENDYN stochastic | Low | Moderate | Very high | Very high |
| Feedback comb noise | Low | Low | High | High |
| Lorenz / Duffing | Moderate | Moderate | High | High |
| Granular noise cloud | Moderate | Moderate | Very high | High |
| HNW feedback chain | Moderate | Moderate | Very high | Very high |
| Modal friction scraping | Moderate | High | High | Very high |
| Phase vocoder smear | High | High | Very high | High |
| Convolution (long IR) | High | Moderate | High | Very high |

---

## References and Further Reading

**Stochastic synthesis:**
- Xenakis, I. *Formalized Music* (1971/1992) — foundational text for GENDYN and granular synthesis
- Collins, N. "Implementing Stochastic Synthesis" (ICMC 2005)
- Luque, S. "Stochastic Synthesis: Origins and Extensions" (Institute of Sonology, 2006)

**Physical modeling:**
- Smith, J.O. *Physical Audio Signal Processing* — CCRMA online textbook (waveguide, modal)
- Smith, J.O. *Spectral Audio Signal Processing* — CCRMA online textbook (phase vocoder, STFT)
- Rocchesso & Scalcon, "Object-based Synthesis of Scraping and Rolling Sounds" (arXiv 2021)

**Psychoacoustics:**
- Sethares, W. *Tuning, Timbre, Spectrum, Scale* (Springer, 2005) — dissonance curves, roughness
- Plomp & Levelt, "Tonal consonance and critical bandwidth" (JASA, 1965)
- Noe, G. *Irreversible* (film, 2002) — infrasound in cinema

**Feedback and chaos:**
- Chowdhury, J. "Nonlinear Feedback Delay Networks" (GitHub)
- Smith, J.O. *Introduction to Digital Filters* — Schroeder allpass, comb filters
- Kennedy & Chua, "Chaos from a time-delayed Chua's circuit" (IEEE Trans., 1993)

**Noise and distortion DSP:**
- Zolzer, U. *DAFX: Digital Audio Effects* (2nd ed., 2011) — waveshapers, spectral processing
- Parker, J. et al. "Antiderivative Antialiasing for Waveshapers" (DAFx-2016)
- Schlecht, S. FDNTB: Feedback Delay Network Toolbox (MATLAB, GitHub)

**Velvet noise:**
- Valimaki et al. "Velvet-Noise Reverberator" (IEEE Signal Processing Letters, 2007)
- Jarvelainen & Valimaki, "Reverberation with Filtered Velvet Noise" (MDPI Applied Sciences, 2017)
