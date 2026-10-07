#pragma once

#include "PluginProcessor.h"
#include "ui/MainPanel.h"

// Hosts the fixed-size MainPanel and scales it to whatever size the user picks:
// drag the bottom-right corner, or use the SIZE menu (50 % .. 200 %).
class RecklessMuseEditor : public juce::AudioProcessorEditor
{
public:
    explicit RecklessMuseEditor (RecklessMuseProcessor&);
    ~RecklessMuseEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showSizeMenu (juce::Component& anchor);

    RecklessMuseProcessor& museProcessor;
    rm::ui::MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessMuseEditor)
};
