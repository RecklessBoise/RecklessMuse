#pragma once

#include "MuseLookAndFeel.h"

namespace rm::ui
{
// Blue vacuum-fluorescent style display of the PROGRAMMER section.
class Display : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Display();

    void setPreset (const juce::String& category, const juce::String& name, int index, int total);
    void setStatus (const juce::String& left, const juce::String& right);
    void showParameter (const juce::String& name, const juce::String& value);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

    std::function<void()> onClick; // opens the preset browser menu

private:
    void timerCallback() override;

    juce::String category, name, statusLeft, statusRight, paramName, paramValue;
    int index = 0, total = 0;
    juce::int64 paramShownAt = 0;
};
} // namespace rm::ui
