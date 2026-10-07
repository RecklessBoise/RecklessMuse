#include "PluginEditor.h"

namespace
{
constexpr int kBaseW = rm::ui::MainPanel::kWidth;
constexpr int kBaseH = rm::ui::MainPanel::kHeight;
constexpr float kMinScale = 0.5f, kMaxScale = 2.0f;
} // namespace

RecklessMuseEditor::RecklessMuseEditor (RecklessMuseProcessor& p)
    : AudioProcessorEditor (p), museProcessor (p), panel (p)
{
    // Read the saved size first: setting the limits below triggers resized().
    const float scale = juce::jlimit (kMinScale, kMaxScale, museProcessor.editorScale.load());
    addAndMakeVisible (panel);
    panel.onSizeMenu = [this] (juce::Component& anchor) { showSizeMenu (anchor); };

    setResizable (true, true);
    setResizeLimits ((int) (kBaseW * kMinScale), (int) (kBaseH * kMinScale),
                     (int) (kBaseW * kMaxScale), (int) (kBaseH * kMaxScale));
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);

    setSize (juce::roundToInt (kBaseW * scale), juce::roundToInt (kBaseH * scale));
}

RecklessMuseEditor::~RecklessMuseEditor() = default;

void RecklessMuseEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void RecklessMuseEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    panel.setBounds (0, 0, kBaseW, kBaseH);
    panel.setTransform (juce::AffineTransform::scale (scale));
    museProcessor.editorScale = scale;
}

void RecklessMuseEditor::showSizeMenu (juce::Component& anchor)
{
    juce::PopupMenu menu;
    const int sizes[] { 50, 60, 75, 85, 100, 125, 150, 175, 200 };
    const int current = juce::roundToInt (museProcessor.editorScale.load() * 100.0f);
    for (auto s : sizes)
        menu.addItem (s, juce::String (s) + " %", true, std::abs (current - s) <= 1);

    juce::Component::SafePointer<RecklessMuseEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&anchor), [safe] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        const float scale = (float) result / 100.0f;
        safe->setSize (juce::roundToInt (kBaseW * scale), juce::roundToInt (kBaseH * scale));
    });
}
