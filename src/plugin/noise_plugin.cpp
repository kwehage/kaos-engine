#include "noise_plugin.h"
#include "noise_editor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {

static constexpr auto kType      = "noise_type";
static constexpr auto kMode      = "noise_mode";
static constexpr auto kBlend     = "noise_blend";
static constexpr auto kMod       = "noise_mod";
static constexpr auto kGain      = "gain";
static constexpr auto kGrainSize = "grain_size";
static constexpr auto kDensity   = "density";
static constexpr auto kThreshold = "threshold";
static constexpr auto kAttack    = "attack";
static constexpr auto kRelease   = "release";
static constexpr auto kMix       = "mix";
static constexpr auto kOutput    = "output";

// ── Parameter layout ───────────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout
NoisePlugin::make_params()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kType, 1}, "Noise Type",
        StringArray{"White", "Pink", "Blue", "Brown",
                    "Granular", "FeedbackComb", "Simplex",
                    "Lorenz", "Duffing", "Gendyn", "HarshWall", "Chua",
                    "Residual", "Coupled", "Diffuse", "Modal", "SimplexDriven"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kMode, 1}, "Mode",
        StringArray{"Follow", "Gated", "Always On"}, 0));

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID{kBlend, 1}, "Blend",
        StringArray{"Add", "AM", "Saturate", "Spectral", "Phase Random", "Ring Mod", "Infrasonic AM", "Roughness"}, 0));

    {
        NormalisableRange<float> r(0.0f, 1.0f, 0.001f);
        r.setSkewForCentre(0.1f);  // most useful range is 0-0.3; skew puts it centre-knob
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kMod, 1}, "Mod", r, 0.03f));
    }

    {
        NormalisableRange<float> r(0.0f, 1.0f, 0.001f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kGain, 1}, "Gain", r, 0.5f));
    }

    {
        NormalisableRange<float> r(5.0f, 500.0f, 1.0f);
        r.setSkewForCentre(50.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kGrainSize, 1}, "Grain Size", r, 50.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kDensity, 1}, "Density",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    {
        NormalisableRange<float> r(-60.0f, 0.0f, 0.1f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kThreshold, 1}, "Threshold", r, -40.0f,
            AudioParameterFloatAttributes().withLabel("dB")));
    }

    {
        NormalisableRange<float> r(0.1f, 500.0f, 0.1f);
        r.setSkewForCentre(10.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kAttack, 1}, "Attack", r, 10.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }
    {
        NormalisableRange<float> r(1.0f, 5000.0f, 1.0f);
        r.setSkewForCentre(300.0f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kRelease, 1}, "Release", r, 300.0f,
            AudioParameterFloatAttributes().withLabel("ms")));
    }

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{kMix, 1}, "Mix",
        NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));

    {
        NormalisableRange<float> r(-20.0f, 6.0f, 0.1f);
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{kOutput, 1}, "Output", r, 0.0f,
            AudioParameterFloatAttributes().withLabel("dB")));
    }

    return {p.begin(), p.end()};
}

// ── Constructor ────────────────────────────────────────────────────────────────

NoisePlugin::NoisePlugin()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "KE_NOISE", make_params())
{
    p_type_       = apvts_.getRawParameterValue(kType);
    p_mode_       = apvts_.getRawParameterValue(kMode);
    p_blend_      = apvts_.getRawParameterValue(kBlend);
    p_mod_        = apvts_.getRawParameterValue(kMod);
    p_gain_       = apvts_.getRawParameterValue(kGain);
    p_grain_size_ = apvts_.getRawParameterValue(kGrainSize);
    p_density_    = apvts_.getRawParameterValue(kDensity);
    p_threshold_  = apvts_.getRawParameterValue(kThreshold);
    p_attack_     = apvts_.getRawParameterValue(kAttack);
    p_release_    = apvts_.getRawParameterValue(kRelease);
    p_mix_        = apvts_.getRawParameterValue(kMix);
    p_output_     = apvts_.getRawParameterValue(kOutput);
}

// ── Bus layout ────────────────────────────────────────────────────────────────

bool NoisePlugin::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ── Lifecycle ──────────────────────────────────────────────────────────────────

void NoisePlugin::prepareToPlay(double sr, int bs)
{
    dsp_.prepare(sr, bs);

    // Pre-compute Hann window for OLA analysis.
    for (int i = 0; i < kSpectSize; ++i)
        hann_win_[i] = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / kSpectSize));

    spect_noise_l_.seed(0x12345678u);
    spect_noise_r_.seed(0xabcdef01u);
    reset_ola();
}

void NoisePlugin::reset_ola()
{
    ola_l_ = {};  ola_l_.out_write_ = kSpectHop;
    ola_r_ = {};  ola_r_.out_write_ = kSpectHop;
}

void NoisePlugin::releaseResources() { dsp_.reset(); reset_ola(); }

// ── Processing ────────────────────────────────────────────────────────────────

void NoisePlugin::processBlock(juce::AudioBuffer<float>& buf,
                                juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals ndn;

    dsp_.set_type       (static_cast<NoiseType> (juce::roundToInt(p_type_ ->load())));
    dsp_.set_mode       (static_cast<NoiseMode> (juce::roundToInt(p_mode_ ->load())));
    dsp_.set_blend      (static_cast<NoiseBlend>(juce::roundToInt(p_blend_->load())));
    dsp_.set_mod        (p_mod_->load());
    dsp_.set_gain       (p_gain_      ->load());
    dsp_.set_grain_size_ms(p_grain_size_->load());
    dsp_.set_grain_density(p_density_  ->load());
    dsp_.set_threshold_db (p_threshold_->load());
    dsp_.set_attack_ms    (p_attack_   ->load());
    dsp_.set_release_ms   (p_release_  ->load());
    dsp_.set_mix        (p_mix_       ->load());
    dsp_.set_output     (p_output_    ->load());

    const NoiseBlend blend = static_cast<NoiseBlend>(juce::roundToInt(p_blend_->load()));

    float* left  = buf.getWritePointer(0);
    float* right = buf.getWritePointer(1);
    const int ns = buf.getNumSamples();

    // Capture dry (input) before in-place processing.
    if (ns > 0) {
        dry_sample_.store(0.5f * (left[ns - 1] + right[ns - 1]),
                          std::memory_order_relaxed);
    }

    if (blend == NoiseBlend::Spectral || blend == NoiseBlend::PhaseRandom) {
        const float mod          = p_mod_      ->load();
        const float mix          = p_mix_      ->load();
        const float output_lin   = std::pow(10.0f, p_output_->load() * 0.05f);
        const float threshold_lin= std::pow(10.0f, p_threshold_->load() * 0.05f);
        const float sr           = float(std::max(getSampleRate(), 1.0));
        const float atk_ms       = p_attack_ ->load();
        const float rel_ms       = p_release_->load();
        const float alpha_a      = 1.0f - std::exp(-1.0f / (atk_ms * 0.001f * sr));
        const float alpha_r      = 1.0f - std::exp(-1.0f / (rel_ms * 0.001f * sr));
        const NoiseMode mode     = static_cast<NoiseMode>(juce::roundToInt(p_mode_->load()));

        for (int i = 0; i < ns; ++i) {
            left [i] = process_ola_sample(ola_l_, spect_noise_l_, left [i],
                                          mod, mix, threshold_lin,
                                          alpha_a, alpha_r, mode, output_lin, blend);
            right[i] = process_ola_sample(ola_r_, spect_noise_r_, right[i],
                                          mod, mix, threshold_lin,
                                          alpha_a, alpha_r, mode, output_lin, blend);
        }
    } else {
        dsp_.process(left, right, ns);
    }

    // Expose last output sample for the editor strip chart (30 Hz read).
    if (ns > 0) {
        const float mono = 0.5f * (left[ns - 1] + right[ns - 1]);
        output_sample_.store(mono, std::memory_order_relaxed);
    }
}

// ── Spectral OLA ──────────────────────────────────────────────────────────────

float NoisePlugin::process_ola_sample(OlaChannel& ch, NoiseChannel& noise_ch,
                                       float in,  float mod,  float mix,
                                       float threshold_lin,
                                       float gate_alpha_a, float gate_alpha_r,
                                       NoiseMode mode, float output_lin, NoiseBlend blend)
{
    // Gate envelope (mirrors noise_processor.cpp logic, but per-OLA-channel).
    if (mode == NoiseMode::AlwaysOn) {
        ch.gate_smooth_ = 1.0f;
    } else {
        const float level  = std::abs(in);
        float target;
        if (mode == NoiseMode::Follow)
            target = (level > threshold_lin) ? level : 0.0f;
        else  // Gated
            target = (level > threshold_lin) ? 1.0f : 0.0f;
        const float alpha = (target > ch.gate_env_) ? gate_alpha_a : gate_alpha_r;
        ch.gate_env_ += alpha * (target - ch.gate_env_);
        ch.gate_smooth_ = ch.gate_env_;
    }

    // Effective injection depth scales with gate.
    const float eff_mod = mod * ch.gate_smooth_;

    // Dry delay: read-then-write so output is delayed exactly kSpectHop samples.
    const float dry = ch.dry_buf_[ch.dry_pos_];
    ch.dry_buf_[ch.dry_pos_] = in;
    ch.dry_pos_ = (ch.dry_pos_ + 1) % kSpectHop;

    // Input ring buffer.
    ch.in_buf_[ch.in_pos_] = in;
    ch.in_pos_ = (ch.in_pos_ + 1) % kSpectSize;
    ++ch.hop_count_;

    // Trigger FFT frame every kSpectHop input samples.
    if (ch.hop_count_ >= kSpectHop) {
        ch.hop_count_ = 0;
        process_ola_frame(ch, noise_ch, eff_mod, blend);
    }

    // Read OLA output sample and clear the slot for future overlap-add.
    const float wet = ch.out_buf_[ch.out_read_];
    ch.out_buf_[ch.out_read_] = 0.0f;
    ch.out_read_ = (ch.out_read_ + 1) % (kSpectSize * 2);

    return output_lin * ((1.0f - mix) * dry + mix * wet);
}

void NoisePlugin::process_ola_frame(OlaChannel& ch, NoiseChannel& noise_ch,
                                     float mod, NoiseBlend blend)
{
    for (int i = 0; i < kSpectSize; ++i) {
        const int ri = (ch.in_pos_ + i) % kSpectSize;
        ch.fft_buf_[i] = ch.in_buf_[ri] * hann_win_[i];
    }
    std::fill(ch.fft_buf_ + kSpectSize, ch.fft_buf_ + kSpectSize * 2, 0.0f);

    spect_fft_.performRealOnlyForwardTransform(ch.fft_buf_, false);

    if (blend == NoiseBlend::PhaseRandom) {
        // Rotate each bin's phase by a random amount scaled by mod.
        // Magnitudes are preserved; only temporal coherence is destroyed.
        // DC and Nyquist are real-only — randomly flip sign at full mod.
        if (noise_ch.randbi() < (mod * 2.0f - 1.0f)) ch.fft_buf_[0] = -ch.fft_buf_[0];
        if (noise_ch.randbi() < (mod * 2.0f - 1.0f)) ch.fft_buf_[1] = -ch.fft_buf_[1];
        for (int k = 1; k < kSpectSize / 2; ++k) {
            const float re  = ch.fft_buf_[2*k];
            const float im  = ch.fft_buf_[2*k+1];
            const float mag = std::sqrt(re*re + im*im);
            if (mag < 1e-10f) continue;
            const float phi = std::atan2(im, re) + mod * noise_ch.randbi() * float(M_PI);
            ch.fft_buf_[2*k]   = mag * std::cos(phi);
            ch.fft_buf_[2*k+1] = mag * std::sin(phi);
        }
    } else {
        // Spectral blend: scale each bin magnitude by (1 + mod * noise).
        ch.fft_buf_[0] *= (1.0f + mod * noise_ch.randbi());
        ch.fft_buf_[1] *= (1.0f + mod * noise_ch.randbi());
        for (int k = 1; k < kSpectSize / 2; ++k) {
            const float scale = 1.0f + mod * noise_ch.randbi();
            ch.fft_buf_[2*k]   *= scale;
            ch.fft_buf_[2*k+1] *= scale;
        }
    }

    // Inverse FFT: reconstructs conjugate half from [0..N] automatically.
    // JUCE normalises by 1/N so IFFT(FFT(x)) = x.
    spect_fft_.performRealOnlyInverseTransform(ch.fft_buf_);

    // OLA: overlap-add synthesised frame into output accumulator.
    // Analysis-window-only OLA: with Hann at 50% overlap,
    // sum of windows = 1 everywhere — no synthesis window or scaling needed.
    for (int i = 0; i < kSpectSize; ++i) {
        const int oi = (ch.out_write_ + i) % (kSpectSize * 2);
        ch.out_buf_[oi] += ch.fft_buf_[i];
    }
    ch.out_write_ = (ch.out_write_ + kSpectHop) % (kSpectSize * 2);
}

// ── State ──────────────────────────────────────────────────────────────────────

void NoisePlugin::getStateInformation(juce::MemoryBlock& dest)
{
    auto xml = apvts_.copyState().createXml();
    copyXmlToBinary(*xml, dest);
}

void NoisePlugin::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml && xml->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* NoisePlugin::createEditor()
{
    return new NoiseEditor(*this);
}

} // namespace kaos_engine

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kaos_engine::NoisePlugin();
}
