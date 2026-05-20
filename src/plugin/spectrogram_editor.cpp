#include "spectrogram_editor.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace kaos_engine {
using namespace juce;

// ── Color map ─────────────────────────────────────────────────────────────────
// Maps [0,1] level to a color. Five control points chosen to match the project
// palette: background black -> dark accent -> cadmium red -> hot orange-white.

Colour SpectrogramEditor::map_colour(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);

    struct CP { float v; uint8_t r, g, b; };
    static constexpr CP pts[] = {
        { 0.00f,  20,  20,  20 },   // background #141414
        { 0.20f,  60,  10,  10 },   // very dark red
        { 0.45f, 100,  20,  20 },   // dark red
        { 0.70f, 210,  43,  43 },   // cadmium red #D22B2B (accent)
        { 0.88f, 255, 120,  30 },   // orange
        { 1.00f, 255, 230, 180 },   // near-white peak
    };
    constexpr int N = 6;

    for (int i = 0; i < N - 1; ++i) {
        if (v <= pts[i + 1].v) {
            const float t = (v - pts[i].v) / (pts[i+1].v - pts[i].v);
            return Colour(
                uint8_t(pts[i].r + t * float(pts[i+1].r - pts[i].r)),
                uint8_t(pts[i].g + t * float(pts[i+1].g - pts[i].g)),
                uint8_t(pts[i].b + t * float(pts[i+1].b - pts[i].b)));
        }
    }
    return Colour(pts[N-1].r, pts[N-1].g, pts[N-1].b);
}

// ── Construction ───────────────────────────────────────────────────────────────

SpectrogramEditor::SpectrogramEditor(SpectrogramPlugin& plugin)
    : AudioProcessorEditor(plugin), plugin_(plugin)
{
    setLookAndFeel(&laf_);
    setSize(kWidth, kHeight);
    setResizable(false, false);
    tooltip_window_ = std::make_unique<TooltipWindow>(this, 600);

    build_colour_lut();

    // Initialise spectrogram image to background color.
    spectrogram_img_ = Image(Image::ARGB, kDispW, kDispH, true);
    {
        Graphics g(spectrogram_img_);
        g.fillAll(Colour(laf_.background()));
    }

    startTimerHz(30);
}

void SpectrogramEditor::build_colour_lut()
{
    for (int i = 0; i < 256; ++i) {
        const Colour c = map_colour(float(i) / 255.0f);
        colour_lut_[i] = c.getARGB();
    }
}

void SpectrogramEditor::build_freq_lut()
{
    const float sr      = float(std::max(plugin_.get_sample_rate(), 1.0));
    const float max_bin = float(kFftSize / 2 - 1);
    for (int x = 0; x < kDispW; ++x) {
        const float freq = 20.0f * std::pow(1000.0f, float(x) / float(kDispW - 1));
        freq_bin_lut_[x] = std::clamp(freq * float(kFftSize) / sr, 0.0f, max_bin);
    }
    lut_built_ = true;
}

SpectrogramEditor::~SpectrogramEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ── Timer ──────────────────────────────────────────────────────────────────────

void SpectrogramEditor::timerCallback()
{
    // Build LUT once the sample rate is known (first tick after audio starts).
    if (!lut_built_ && plugin_.get_sample_rate() > 0.0)
        build_freq_lut();

    // Process at most one FFT block per tick — identical to the EQ pattern.
    // The while-loop variant caused CPU spikes that starved the audio thread.
    if (plugin_.pull_fft_block(fft_data_)) {
        update_spectrogram();
        repaint(0, 0, kWidth, kHeight);
    }
}

void SpectrogramEditor::update_spectrogram()
{
    if (!lut_built_) return;

    // Apply window and run FFT.
    window_.multiplyWithWindowingTable(fft_data_, kFftSize);
    fft_.performFrequencyOnlyForwardTransform(fft_data_);

    const float norm_dB = Decibels::gainToDecibels(float(kFftSize));

    // Scroll image up one row via a single memmove on the raw pixel buffer.
    Image::BitmapData bdata(spectrogram_img_, Image::BitmapData::readWrite);
    const int stride = bdata.lineStride;
    std::memmove(bdata.data, bdata.data + stride, size_t(stride) * size_t(kDispH - 1));

    // Write new bottom row using direct uint32 pixel writes — avoids the
    // per-pixel colour-space conversion overhead of setPixelColour().
    auto* row = reinterpret_cast<uint32_t*>(bdata.data + (kDispH - 1) * stride);
    for (int x = 0; x < kDispW; ++x) {
        const float fbin  = freq_bin_lut_[x];
        const int   b0    = int(fbin);
        const int   b1    = std::min(b0 + 1, kFftSize / 2 - 1);
        const float t     = fbin - float(b0);
        const float mag   = fft_data_[b0] * (1.0f - t) + fft_data_[b1] * t;
        const float db    = Decibels::gainToDecibels(std::max(mag, 1e-10f)) - norm_dB;
        const int   idx   = jlimit(0, 255, int(jmap(db, -90.0f, 0.0f, 0.0f, 255.0f)));
        row[x] = colour_lut_[idx];
    }
}

// ── Layout ─────────────────────────────────────────────────────────────────────

void SpectrogramEditor::resized() {}

// ── Painting ───────────────────────────────────────────────────────────────────

void SpectrogramEditor::paint(Graphics& g)
{
    g.fillAll(Colour(laf_.background()));
    draw_spectrogram(g, Rectangle<int>(kDispX, kDispY, kDispW, kDispH));
}

void SpectrogramEditor::draw_spectrogram(Graphics& g, Rectangle<int> area)
{
    // Draw the scrolling image.
    g.drawImage(spectrogram_img_, area.toFloat());

    // ── Frequency axis labels (below the image) ────────────────────────────────
    auto freq_to_x = [&](float f) -> float {
        return float(area.getX()) + float(area.getWidth()) *
               std::log(f / 20.0f) / std::log(1000.0f);
    };

    struct FreqMark { float hz; const char* label; };
    static constexpr FreqMark marks[] = {
        { 50.0f,    "50"  },
        { 100.0f,   "100" },
        { 200.0f,   "200" },
        { 500.0f,   "500" },
        { 1000.0f,  "1k"  },
        { 2000.0f,  "2k"  },
        { 5000.0f,  "5k"  },
        { 10000.0f, "10k" },
        { 20000.0f, "20k" },
    };

    g.setFont(Font(8.5f));
    for (auto& m : marks) {
        const float px = freq_to_x(m.hz);
        // Tick mark
        g.setColour(Colour(laf_.border()).withAlpha(0.6f));
        g.drawLine(px, float(area.getBottom()), px, float(area.getBottom()) + 4.0f, 1.0f);
        // Faint vertical gridline in the spectrogram
        g.setColour(Colour(laf_.border()).withAlpha(0.15f));
        g.drawLine(px, float(area.getY()), px, float(area.getBottom()), 1.0f);
        // Label
        g.setColour(Colour(laf_.text_muted()));
        g.drawText(m.label,
                   int(px) - 14, kAxisY + 5, 28, 10,
                   Justification::centred);
    }

    // Frequency axis title
    g.setFont(Font(7.5f));
    g.setColour(Colour(laf_.text_muted()).withAlpha(0.6f));
    g.drawText("Hz", kWidth - 20, kAxisY + 5, 18, 10, Justification::centredLeft);

    // ── Color scale legend (top-right corner, inside the image) ───────────────
    {
        const int lw = 12, lh = 80;
        const int lx = kDispW - lw - 6;
        const int ly = 6;

        for (int i = 0; i < lh; ++i) {
            const float v = 1.0f - float(i) / float(lh - 1);
            g.setColour(map_colour(v));
            g.fillRect(lx, ly + i, lw, 1);
        }
        g.setColour(Colour(laf_.border()));
        g.drawRect(lx, ly, lw, lh, 1);

        g.setFont(Font(7.0f));
        g.setColour(Colour(laf_.text_muted()));
        g.drawText("0 dB",  lx - 28, ly - 1,          26, 9, Justification::centredRight);
        g.drawText("-90",   lx - 28, ly + lh - 5,      26, 9, Justification::centredRight);
    }

    // ── Time axis label (left edge, rotated note) ─────────────────────────────
    {
        g.saveState();
        g.setFont(Font(7.5f));
        g.setColour(Colour(laf_.text_muted()).withAlpha(0.6f));
        // Draw "TIME" vertically, rotated -90 degrees
        juce::AffineTransform rot = juce::AffineTransform::rotation(
            -float(M_PI) * 0.5f,
            6.0f, float(kDispH) / 2.0f);
        g.addTransform(rot);
        g.drawText("time ->", int(-float(kDispH) / 2.0f) - 20, -2, 80, 9,
                   Justification::centred);
        g.restoreState();
    }

    // ── Border around the image area ──────────────────────────────────────────
    g.setColour(Colour(laf_.border()));
    g.drawRect(area.toFloat(), 1.0f);

    // ── Footer ────────────────────────────────────────────────────────────────
    g.setFont(Font(12.0f));
    g.setColour(Colour(laf_.accent_colour()));
    g.drawText("kaos-engine::spectrogram",
               kDispX + 4, kFooterY + 2, 260, kFooterH,
               Justification::centredLeft);

}

} // namespace kaos_engine
