#include "MuseLookAndFeel.h"

namespace rm::ui
{
juce::Font labelFont (float height, bool bold)
{
    return juce::Font (juce::FontOptions ("DIN Alternate", height, bold ? juce::Font::bold : juce::Font::plain));
}

MuseLookAndFeel::MuseLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::panelDark);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::orange);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, colours::panelDark);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::outlineColourId, colours::line);
    setColour (juce::ComboBox::arrowColourId, colours::textDim);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panelDark);
    setColour (juce::TooltipWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::panelDark);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void MuseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (1.0f);
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = size * 0.5f;

    // Scale ticks around the knob
    g.setColour (colours::textDim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const float r1 = radius * 0.94f, r2 = radius * (i % 5 == 0 ? 1.0f : 0.98f);
        g.drawLine (centre.x + r1 * std::sin (a), centre.y - r1 * std::cos (a),
                    centre.x + r2 * std::sin (a), centre.y - r2 * std::cos (a), i % 5 == 0 ? 1.2f : 0.8f);
    }

    // Skirt
    const float skirtR = radius * 0.86f;
    juce::ColourGradient skirt (juce::Colour (0xff4a4b4f), centre.x, centre.y - skirtR,
                                juce::Colour (0xff0d0d0e), centre.x, centre.y + skirtR, false);
    g.setGradientFill (skirt);
    g.fillEllipse (centre.x - skirtR, centre.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);

    // Knurled cap
    const float capR = radius * 0.66f;
    juce::ColourGradient cap (juce::Colour (0xff3b3c40), centre.x - capR * 0.4f, centre.y - capR,
                              juce::Colour (0xff111112), centre.x + capR * 0.3f, centre.y + capR, true);
    g.setGradientFill (cap);
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 24; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
        g.drawLine (centre.x + capR * 0.88f * std::sin (a), centre.y - capR * 0.88f * std::cos (a),
                    centre.x + capR * std::sin (a), centre.y - capR * std::cos (a), 0.8f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f, 0.8f);

    // Pointer
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    juce::Path pointer;
    const float pw = std::max (1.6f, size * 0.05f);
    pointer.addRoundedRectangle (-pw * 0.5f, -skirtR * 0.98f, pw, skirtR * 0.55f, pw * 0.5f);
    g.setColour (colours::ivory);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void MuseLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float cx = b.getCentreX();

    // Slot
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (cx - 2.0f, b.getY() + 2.0f, 4.0f, b.getHeight() - 4.0f, 2.0f);

    // Scale
    g.setColour (colours::textDim.withAlpha (0.5f));
    for (int i = 0; i <= 10; ++i)
    {
        const float yy = b.getY() + 6.0f + (b.getHeight() - 12.0f) * (float) i / 10.0f;
        const float len = i % 5 == 0 ? 5.0f : 3.0f;
        g.drawLine (cx - 7.0f - len, yy, cx - 7.0f, yy, 0.8f);
        g.drawLine (cx + 7.0f, yy, cx + 7.0f + len, yy, 0.8f);
    }

    // Cap
    const float capW = std::min (b.getWidth() - 2.0f, 18.0f), capH = 12.0f;
    const auto cap = juce::Rectangle<float> (capW, capH).withCentre ({ cx, sliderPos });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.0f);
    juce::ColourGradient grad (juce::Colour (0xff3d3e42), cap.getX(), cap.getY(),
                               juce::Colour (0xff121213), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (colours::ivory);
    g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 0.75f, cap.getWidth() - 4.0f, 1.5f);
}

void MuseLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                    juce::ComboBox& box)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (b, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) width - 10.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

juce::Font MuseLookAndFeel::getComboBoxFont (juce::ComboBox&) { return labelFont (13.0f); }
juce::Font MuseLookAndFeel::getPopupMenuFont() { return labelFont (14.0f, false); }

void MuseLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::panelDark);
    g.setColour (colours::line);
    g.drawRect (0, 0, width, height);
}
} // namespace rm::ui
