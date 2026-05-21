#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace kaos_engine {

// ── Colour palette ─────────────────────────────────────────────────────────────
// All colours a plugin needs. Construct via the static factory methods so each
// plugin just picks a preset rather than filling in every field by hand.
struct ColourPalette {
    juce::uint32 background;
    juce::uint32 surface;
    juce::uint32 border;
    juce::uint32 text_primary;
    juce::uint32 text_muted;
    juce::uint32 knob_track;
    juce::uint32 knob_fill;
    juce::uint32 knob_dot;
    juce::uint32 accent;

    // ── Presets ───────────────────────────────────────────────────────────────
    // Standard dark background (#141414) with a per-plugin accent colour.
    static ColourPalette dark(juce::uint32 accent = 0xffd4cfc6);

    // Custom background colour with accent. Surface and border are derived by
    // brightening the background. Use when the plugin has a unique background
    // colour distinct from the standard #141414 (e.g. reverb with #343a27).
    static ColourPalette dark_with_bg(juce::uint32 background, juce::uint32 accent);

    // Inverted: accent colour becomes the background, original background
    // becomes the accent. Used by the delay plugin (steel blue bg, dark knobs).
    static ColourPalette inverted(juce::uint32 bg_colour   = 0xff6ca8c8,
                                  juce::uint32 dark_colour = 0xff141414);
};

// ── Look and feel ──────────────────────────────────────────────────────────────
class NullEngineLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    explicit NullEngineLookAndFeel(ColourPalette palette = ColourPalette::dark());

    // Colour accessors for use in editor paint() methods.
    juce::uint32 background()   const { return p_.background;   }
    juce::uint32 surface()      const { return p_.surface;      }
    juce::uint32 border()       const { return p_.border;       }
    juce::uint32 text_primary() const { return p_.text_primary; }
    juce::uint32 text_muted()   const { return p_.text_muted;   }
    juce::uint32 accent_colour()const { return p_.accent;       }

    // ── LookAndFeel overrides ─────────────────────────────────────────────────
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

    void drawComboBox(juce::Graphics&, int w, int h, bool isDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;

    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getLabelFont(juce::Label&) override;

    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKey,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

    void drawTooltip(juce::Graphics&, const juce::String& text,
                     int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText,
                                          juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;

    // ── Shared knob layout constants (use in all plugin editors) ─────────────
    static constexpr int   kKnobSize      = 54;
    static constexpr int   kKnobTextBoxH  = 13;
    static constexpr int   kKnobLabelH    = 13;
    static constexpr float kKnobLabelFont = 8.5f;

private:
    ColourPalette p_;
    juce::Font    plugin_font_;
};

} // namespace kaos_engine
