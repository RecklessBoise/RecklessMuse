#pragma once

#include "MuseLookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace rm::ui
{
using APVTS = juce::AudioProcessorValueTreeState;
using TouchCallback = std::function<void (const juce::String& name, const juce::String& value)>;

// Every parameter-bound control can be re-attached to another parameter
// (used to switch the panel between timbre A and timbre B).
class Bindable
{
public:
    virtual ~Bindable() = default;
    virtual void bind (APVTS& state, const juce::String& paramId) = 0;
    virtual void unbind() = 0;
    TouchCallback onTouch;

protected:
    void notifyTouch (juce::RangedAudioParameter* p)
    {
        if (onTouch != nullptr && p != nullptr)
            onTouch (p->getName (40), p->getCurrentValueAsText());
    }
};

// Rotary knob or vertical fader with its label underneath.
class Knob : public juce::Component, public Bindable
{
public:
    enum class Style { Rotary, Fader };

    explicit Knob (juce::String labelText, Style style = Style::Rotary);

    void bind (APVTS& state, const juce::String& paramId) override;
    void unbind() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String label;
    Style style;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Small rectangular push switch with a red LED above it.
class LedSwitch : public juce::Button, public Bindable
{
public:
    explicit LedSwitch (juce::String labelText);

    void bind (APVTS& state, const juce::String& paramId) override;
    void unbind() override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override { if (isMouseOver (true)) notifyTouch (param); }

private:
    juce::String label;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Coloured keycap (Arp ON, Seq PLAY, step buttons, Timbre A/B ...). Can be a parameter
// toggle, a manual toggle, or a momentary action button.
class KeyCap : public juce::Button, public Bindable
{
public:
    KeyCap (juce::String labelText, juce::Colour capColour, bool showLed = true);

    void bind (APVTS& state, const juce::String& paramId) override;
    void unbind() override;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void clicked() override { if (isMouseOver (true)) notifyTouch (param); }

    void setLit (bool shouldBeLit) { if (lit != shouldBeLit) { lit = shouldBeLit; repaint(); } }
    void setCapText (juce::String t) { capText = std::move (t); repaint(); }
    void setCapColour (juce::Colour c) { if (c != colour) { colour = c; repaint(); } }
    void setLabelBelow (bool b) { labelBelow = b; repaint(); }

private:
    juce::String label, capText;
    juce::Colour colour;
    bool showLed, lit = false, labelBelow = true;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Row/column of LEDs with labels; click an entry to select it (choice parameters).
class LedChoice : public juce::Component, public Bindable
{
public:
    LedChoice (juce::String title, juce::StringArray shortLabels, bool vertical);

    void bind (APVTS& state, const juce::String& paramId) override;
    void unbind() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> itemBounds (int index) const;

    juce::String title;
    juce::StringArray items;
    bool vertical;
    int selected = 0;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Push button + red value read-out. Click = next value, right-click / shift-click = menu.
class Selector : public juce::Component, public Bindable
{
public:
    explicit Selector (juce::String labelText);

    void bind (APVTS& state, const juce::String& paramId) override;
    void unbind() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::String label;
    juce::StringArray items;
    int selected = 0;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Spring-loaded pitch wheel / mod wheel.
class Wheel : public juce::Component
{
public:
    Wheel (juce::String labelText, bool springBack);

    std::function<void (float)> onChange; // -1..1 (pitch) or 0..1 (mod)
    void setValue (float v, bool notify);
    float getValue() const noexcept { return value; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::String label;
    bool spring;
    float value = 0.0f, dragStartValue = 0.0f;
};
} // namespace rm::ui
