#include "Display.h"

namespace rm::ui
{
namespace
{
void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Justification just,
               juce::Colour colour)
{
    g.setColour (colour.withAlpha (0.18f));
    for (auto d : { juce::Point<int> (1, 0), juce::Point<int> (-1, 0), juce::Point<int> (0, 1), juce::Point<int> (0, -1) })
        g.drawText (text, area.translated (d.x, d.y), just, true);
    g.setColour (colour);
    g.drawText (text, area, just, true);
}
} // namespace

Display::Display() { startTimerHz (10); }

void Display::setPreset (const juce::String& cat, const juce::String& n, int i, int t)
{
    if (cat == category && n == name && i == index && t == total)
        return;
    category = cat;
    name = n;
    index = i;
    total = t;
    repaint();
}

void Display::setStatus (const juce::String& left, const juce::String& right)
{
    if (left == statusLeft && right == statusRight)
        return;
    statusLeft = left;
    statusRight = right;
    repaint();
}

void Display::showParameter (const juce::String& n, const juce::String& v)
{
    paramName = n;
    paramValue = v;
    paramShownAt = juce::Time::currentTimeMillis();
    repaint();
}

void Display::timerCallback()
{
    if (paramShownAt != 0 && juce::Time::currentTimeMillis() - paramShownAt > 1800)
    {
        paramShownAt = 0;
        repaint();
    }
}

void Display::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Bezel and glass
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (b, 4.0f);
    auto glass = b.reduced (4.0f);
    juce::ColourGradient bg (colours::vfdBg.brighter (0.15f), glass.getX(), glass.getY(), colours::vfdBg.darker (0.4f),
                             glass.getX(), glass.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (glass, 2.0f);

    // Faint pixel grid, like a dot-matrix VFD
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float x = glass.getX(); x < glass.getRight(); x += 2.0f)
        g.drawVerticalLine ((int) x, glass.getY(), glass.getBottom());

    auto area = glass.reduced (8.0f, 4.0f).toNearestInt();
    const auto top = area.removeFromTop (area.getHeight() / 4);
    const auto bottom = area.removeFromBottom (area.getHeight() / 3);

    g.setFont (labelFont ((float) top.getHeight() * 0.9f, false));
    const auto number = index >= 0 ? juce::String (index + 1).paddedLeft ('0', 3) + "/" + juce::String (total) : juce::String ("---");
    glowText (g, category.toUpperCase(), top, juce::Justification::centredLeft, colours::vfdText.withAlpha (0.75f));
    glowText (g, number, top, juce::Justification::centredRight, colours::vfdText.withAlpha (0.75f));

    if (paramShownAt != 0)
    {
        g.setFont (labelFont ((float) area.getHeight() * 0.42f, true));
        glowText (g, paramName.toUpperCase(), area.removeFromTop (area.getHeight() / 2), juce::Justification::centredLeft,
                  colours::vfdText);
        g.setFont (labelFont ((float) area.getHeight() * 0.85f, true));
        glowText (g, paramValue, area, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.95f));
    }
    else
    {
        g.setFont (labelFont ((float) area.getHeight() * 0.72f, true));
        glowText (g, name.toUpperCase(), area, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.95f));
    }

    g.setFont (labelFont ((float) bottom.getHeight() * 0.8f, false));
    glowText (g, statusLeft, bottom, juce::Justification::centredLeft, colours::vfdText.withAlpha (0.8f));
    glowText (g, statusRight, bottom, juce::Justification::centredRight, colours::vfdText.withAlpha (0.8f));

    // Glass reflection
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (glass.withHeight (glass.getHeight() * 0.45f), 2.0f);
}
} // namespace rm::ui
