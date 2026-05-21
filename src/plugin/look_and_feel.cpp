#include "look_and_feel.h"

namespace kaos_engine {

using namespace juce;

// ── ColourPalette factories ────────────────────────────────────────────────────

ColourPalette ColourPalette::dark(juce::uint32 accent_colour)
{
    return {
        0xff141414,   // background
        0xff1e1e1e,   // surface
        0xff2e2e2e,   // border
        0xffe8e8e8,   // text_primary
        0xff808080,   // text_muted
        0xff303030,   // knob_track
        0xff4a4a4a,   // knob_fill
        0xffe8e8e8,   // knob_dot
        accent_colour,// accent
    };
}

ColourPalette ColourPalette::dark_with_bg(juce::uint32 background, juce::uint32 accent_colour)
{
    const Colour bg  = Colour(background);
    const Colour srf = bg.brighter(0.12f);
    const Colour brd = bg.brighter(0.25f);
    return {
        background,
        srf.getARGB(),
        brd.getARGB(),
        0xffe8e8e8,                     // text_primary: light for readability
        0xff909090,                     // text_muted: mid-grey
        brd.getARGB(),                  // knob_track
        srf.brighter(0.15f).getARGB(), // knob_fill
        0xffe8e8e8,                     // knob_dot
        accent_colour,
    };
}

ColourPalette ColourPalette::inverted(juce::uint32 bg_colour, juce::uint32 dark_colour)
{
    // Derive surface and border as slightly darker/lighter shades of bg_colour.
    const Colour bg  = Colour(bg_colour);
    const Colour srf = bg.darker(0.12f);
    const Colour brd = bg.darker(0.25f);
    const Colour txt = Colour(dark_colour).brighter(0.05f);
    const Colour mut = Colour(dark_colour).brighter(0.55f);

    return {
        bg_colour,
        srf.getARGB(),
        brd.getARGB(),
        txt.getARGB(),          // text_primary: near-dark
        mut.getARGB(),          // text_muted: medium dark
        brd.getARGB(),          // knob_track: darker blue ring
        srf.getARGB(),          // knob_fill: slightly darker blue body
        txt.getARGB(),          // knob_dot: dark indicator line
        dark_colour,            // accent: the original "dark" colour, now the accent
    };
}

// ── Constructor ────────────────────────────────────────────────────────────────

NullEngineLookAndFeel::NullEngineLookAndFeel(ColourPalette palette)
    : p_(palette)
{
    setColour(ResizableWindow::backgroundColourId,      Colour(p_.background));
    setColour(Slider::rotarySliderFillColourId,         Colour(p_.accent));
    setColour(Slider::rotarySliderOutlineColourId,      Colour(p_.knob_track));
    setColour(Slider::thumbColourId,                    Colour(p_.knob_dot));
    setColour(Slider::textBoxTextColourId,              Colour(p_.text_primary));
    setColour(Slider::textBoxBackgroundColourId,        Colour(p_.surface));
    setColour(Slider::textBoxOutlineColourId,           Colour(0x00000000));
    setColour(Label::backgroundColourId,                Colour(0x00000000));  // transparent — editor fills bg
    setColour(Label::textColourId,                      Colour(p_.text_muted));
    setColour(ComboBox::backgroundColourId,             Colour(p_.surface));
    setColour(ComboBox::outlineColourId,                Colour(p_.border));
    setColour(ComboBox::textColourId,                   Colour(p_.text_primary));
    setColour(ComboBox::arrowColourId,                  Colour(p_.text_muted));
    setColour(PopupMenu::backgroundColourId,            Colour(p_.surface));
    setColour(PopupMenu::textColourId,                  Colour(p_.text_primary));
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(p_.accent).withAlpha(0.22f));
    setColour(PopupMenu::highlightedTextColourId,       Colour(p_.text_primary));

    plugin_font_ = Font(12.0f);
}

// ── Rotary knob ────────────────────────────────────────────────────────────────
void NullEngineLookAndFeel::drawRotarySlider(Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float startAngle, float endAngle,
    Slider& /*slider*/)
{
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float radius = std::min(w, h) * 0.5f - 4.0f;
    if (radius <= 0.0f) return;

    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Track arc
    {
        Path arc;
        arc.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour(Colour(p_.knob_track));
        g.strokePath(arc, PathStrokeType(2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    // Fill arc
    {
        Path arc;
        arc.addCentredArc(cx, cy, radius, radius, 0.0f, startAngle, angle, true);
        g.setColour(Colour(p_.accent));
        g.strokePath(arc, PathStrokeType(2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    // Knob body
    const float bodyR = radius - 5.0f;
    g.setColour(Colour(p_.knob_fill));
    g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour(Colour(p_.border));
    g.drawEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
    // Indicator
    const float sinA = std::sin(angle), cosA = -std::cos(angle);
    g.setColour(Colour(p_.knob_dot));
    g.drawLine(cx + sinA * bodyR * 0.28f, cy + cosA * bodyR * 0.28f,
               cx + sinA * bodyR * 0.82f, cy + cosA * bodyR * 0.82f, 2.0f);
}

// ── Label ──────────────────────────────────────────────────────────────────────
void NullEngineLookAndFeel::drawLabel(Graphics& g, Label& label)
{
    const Colour bg = label.findColour(Label::backgroundColourId);
    if (bg.getAlpha() > 0)
        g.fillAll(bg);
    if (!label.isBeingEdited()) {
        const Font font(getLabelFont(label));
        g.setColour(label.findColour(Label::textColourId));
        g.setFont(font);
        const auto area = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), area, label.getJustificationType(),
                         jmax(1, (int)(area.getHeight() / font.getHeight())),
                         label.getMinimumHorizontalScale());
    }
}

Font NullEngineLookAndFeel::getLabelFont(Label& label)
{
    return plugin_font_.withHeight(label.getFont().getHeight());
}

// ── ComboBox ───────────────────────────────────────────────────────────────────
void NullEngineLookAndFeel::drawComboBox(Graphics& g, int w, int h,
    bool /*isDown*/, int /*bx*/, int /*by*/, int /*bw*/, int /*bh*/, ComboBox& box)
{
    const Rectangle<float> bounds(0.0f, 0.0f, (float)w, (float)h);
    g.setColour(box.findColour(ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(box.findColour(ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    const float arrowX = w - 18.0f, arrowY = h * 0.5f;
    Path arrow;
    arrow.addTriangle(arrowX, arrowY - 3.0f, arrowX + 6.0f, arrowY - 3.0f, arrowX + 3.0f, arrowY + 3.0f);
    g.setColour(box.findColour(ComboBox::arrowColourId));
    g.fillPath(arrow);
}

void NullEngineLookAndFeel::positionComboBoxText(ComboBox& box, Label& label)
{
    label.setBounds(6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

Font NullEngineLookAndFeel::getComboBoxFont(ComboBox& box)
{
    return plugin_font_.withHeight(jmin(16.0f, (float)box.getHeight() * 0.7f));
}

// ── Popup menu ─────────────────────────────────────────────────────────────────
void NullEngineLookAndFeel::drawPopupMenuBackground(Graphics& g, int w, int h)
{
    g.setColour(findColour(PopupMenu::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float)w, (float)h, 3.0f);
    g.setColour(Colour(p_.border));
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 3.0f, 1.0f);
}

void NullEngineLookAndFeel::drawPopupMenuItem(Graphics& g,
    const Rectangle<int>& area, bool isSeparator, bool /*isActive*/,
    bool isHighlighted, bool isTicked, bool /*hasSubMenu*/,
    const String& text, const String& /*shortcut*/,
    const Drawable* /*icon*/, const Colour* /*textColour*/)
{
    if (isSeparator) {
        g.setColour(Colour(p_.border));
        g.fillRect(area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
        return;
    }
    if (isHighlighted) {
        g.setColour(findColour(PopupMenu::highlightedBackgroundColourId));
        g.fillRoundedRectangle(area.toFloat().reduced(2.0f, 1.0f), 2.0f);
    }
    g.setColour(isHighlighted ? findColour(PopupMenu::highlightedTextColourId)
                              : findColour(PopupMenu::textColourId));
    g.setFont(plugin_font_.withHeight(13.0f));
    g.drawFittedText(text, area.reduced(12, 0), Justification::centredLeft, 1);
    if (isTicked) {
        g.setColour(Colour(p_.accent));
        const float s = 5.0f;
        g.fillEllipse((float)area.getRight() - 14.0f, area.getCentreY() - s * 0.5f, s, s);
    }
}

// ── Tooltip ────────────────────────────────────────────────────────────────────

static juce::TextLayout layout_tip(const juce::String& text, juce::Font font,
                                   juce::Colour col, float maxW)
{
    juce::AttributedString s;
    s.setJustification(juce::Justification::topLeft);
    s.append(text, font, col);
    juce::TextLayout tl;
    tl.createLayout(s, maxW);
    return tl;
}

void NullEngineLookAndFeel::drawTooltip(Graphics& g, const String& text,
                                         int width, int height)
{
    const Rectangle<float> b(0.0f, 0.0f, (float)width, (float)height);
    g.setColour(Colour(p_.surface));
    g.fillRoundedRectangle(b, 3.0f);
    g.setColour(Colour(p_.border));
    g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);
    const auto tl = layout_tip(text, plugin_font_.withHeight(12.0f),
                                Colour(p_.text_primary), (float)width - 12.0f);
    tl.draw(g, b.reduced(6.0f, 4.0f));
}

juce::Rectangle<int> NullEngineLookAndFeel::getTooltipBounds(
    const String& tipText, Point<int> screenPos, Rectangle<int> parentArea)
{
    // Multi-line tooltips (algorithm descriptions) use a wide box.
    // Single-paragraph tooltips (knobs) use a narrow box closer to the JUCE default.
    const float kMaxW = tipText.containsChar('\n') ? 800.0f : 360.0f;
    const auto tl = layout_tip(tipText, plugin_font_.withHeight(12.0f),
                                Colour(p_.text_primary), kMaxW);
    const int w = jmin((int)(tl.getWidth() + 14.0f), (int)kMaxW + 14);
    const int h = (int)(tl.getHeight() + 10.0f);
    const int x = (screenPos.x > parentArea.getCentreX())
                  ? screenPos.x - (w + 12)
                  : screenPos.x + 24;
    const int y = jlimit(parentArea.getY(),
                         jmax(parentArea.getY(), parentArea.getBottom() - h),
                         screenPos.y - h / 2);
    return { x, y, w, h };
}

} // namespace kaos_engine
