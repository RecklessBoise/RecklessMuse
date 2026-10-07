#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rm::ui
{
namespace colours
{
    inline const juce::Colour panel { 0xff2a2b2e };
    inline const juce::Colour panelLight { 0xff34363a };
    inline const juce::Colour panelDark { 0xff1c1d1f };
    inline const juce::Colour line { 0xff5d5f63 };
    inline const juce::Colour text { 0xffe9e7e2 };
    inline const juce::Colour textDim { 0xffa9a7a2 };
    inline const juce::Colour led { 0xffff3b2f };
    inline const juce::Colour ledOff { 0xff4a1e1c };
    inline const juce::Colour orange { 0xfff08a24 };
    inline const juce::Colour yellow { 0xfff3c534 };
    inline const juce::Colour cyan { 0xff63d5f5 };
    inline const juce::Colour ivory { 0xffece8df };
    inline const juce::Colour vfdBg { 0xff07123a };
    inline const juce::Colour vfdText { 0xff8fc4ff };
    inline const juce::Colour woodDark { 0xff6e3313 };
    inline const juce::Colour woodLight { 0xffb5652c };
}

juce::Font labelFont (float height, bool bold = true);

class MuseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MuseLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};
} // namespace rm::ui
