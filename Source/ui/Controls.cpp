#include "Controls.h"

namespace rm::ui
{
namespace
{
void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, float size = 9.5f,
                float minScale = 0.8f)
{
    if (text.isEmpty())
        return;
    g.setColour (colours::text);
    g.setFont (labelFont (size));
    g.drawFittedText (text.toUpperCase(), area, juce::Justification::centredTop, 2, minScale);
}

void drawLed (juce::Graphics& g, juce::Point<float> centre, float radius, bool on)
{
    if (on)
    {
        g.setColour (colours::led.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (radius * 4.0f, radius * 4.0f).withCentre (centre));
    }
    g.setColour (on ? colours::led : colours::ledOff);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    if (on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.8f, radius * 0.8f).withCentre (centre.translated (-radius * 0.3f, -radius * 0.3f)));
    }
}

juce::RangedAudioParameter* findParam (APVTS& state, const juce::String& id)
{
    auto* p = state.getParameter (id);
    jassert (p != nullptr);
    return p;
}
} // namespace

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------
Knob::Knob (juce::String labelText, Style s) : label (std::move (labelText)), style (s)
{
    if (style == Style::Rotary)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::degreesToRadians (-140.0f), juce::degreesToRadians (140.0f), true);
        slider.setMouseDragSensitivity (240);
    }
    else
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
    }
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setVelocityModeParameters (0.6, 1, 0.05, true, juce::ModifierKeys::shiftModifier);
    slider.onDragStart = [this] { notifyTouch (param); };
    slider.onValueChange = [this]
    {
        if (slider.isMouseButtonDown())
            notifyTouch (param);
    };
    addAndMakeVisible (slider);
}

void Knob::bind (APVTS& state, const juce::String& paramId)
{
    attachment.reset();
    param = findParam (state, paramId);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::unbind()
{
    attachment.reset();
    param = nullptr;
}

void Knob::resized()
{
    auto b = getLocalBounds();
    if (style == Style::Rotary)
    {
        auto top = b.removeFromTop (b.getHeight() - 13);
        const int size = std::min (top.getWidth(), top.getHeight());
        slider.setBounds (top.withSizeKeepingCentre (size, size));
    }
    else
    {
        slider.setBounds (b.removeFromTop (b.getHeight() - 22));
    }
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    if (style == Style::Rotary)
        drawLabel (g, label, b.removeFromBottom (13).expanded (12, 0));
    else
        drawLabel (g, label, b.removeFromBottom (21).withTrimmedTop (2).expanded (2, 0), 9.0f, 0.55f);
}

// ---------------------------------------------------------------------------
// LedSwitch
// ---------------------------------------------------------------------------
LedSwitch::LedSwitch (juce::String labelText) : juce::Button (labelText), label (std::move (labelText))
{
    setClickingTogglesState (true);
}

void LedSwitch::bind (APVTS& state, const juce::String& paramId)
{
    attachment.reset();
    param = findParam (state, paramId);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

void LedSwitch::unbind()
{
    attachment.reset();
    param = nullptr;
}

void LedSwitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    drawLed (g, { cx, 4.5f }, 2.6f, getToggleState());

    auto cap = juce::Rectangle<float> (std::min (b.getWidth() - 4.0f, 22.0f), 11.0f).withCentre ({ cx, 16.0f });
    if (down) cap = cap.translated (0.0f, 0.8f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (cap.expanded (1.0f), 2.0f);
    juce::ColourGradient grad (juce::Colour (highlighted ? 0xff47484c : 0xff3a3b3f), cap.getX(), cap.getY(),
                               juce::Colour (0xff17181a), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 1.5f);

    drawLabel (g, label, getLocalBounds().withTrimmedTop (24).expanded (10, 0), 9.0f);
}

// ---------------------------------------------------------------------------
// KeyCap
// ---------------------------------------------------------------------------
KeyCap::KeyCap (juce::String labelText, juce::Colour capColour, bool led)
    : juce::Button (labelText), label (std::move (labelText)), colour (capColour), showLed (led)
{
}

void KeyCap::bind (APVTS& state, const juce::String& paramId)
{
    attachment.reset();
    param = findParam (state, paramId);
    setClickingTogglesState (true);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
}

void KeyCap::unbind()
{
    attachment.reset();
    param = nullptr;
}

void KeyCap::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat();
    auto labelArea = labelBelow && label.isNotEmpty() ? b.removeFromBottom (13.0f) : juce::Rectangle<float>();
    auto cap = b.reduced (1.0f);
    if (down) cap = cap.translated (0.0f, 1.0f);

    const bool on = getToggleState() || lit;
    const bool isWhite = colour.getSaturation() < 0.15f;
    juce::Colour base = isWhite ? (on ? colours::ivory : colours::ivory.darker (0.25f))
                                : (on ? colour.brighter (0.15f) : colour.darker (0.55f).withMultipliedSaturation (0.75f));
    if (highlighted) base = base.brighter (0.08f);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 2.5f);
    juce::ColourGradient grad (base.brighter (0.12f), cap.getX(), cap.getY(), base.darker (0.18f), cap.getX(),
                               cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.5f);

    if (on && ! isWhite)
    {
        g.setColour (base.withAlpha (0.35f));
        g.drawRoundedRectangle (cap.expanded (1.5f), 3.5f, 2.0f);
    }

    if (showLed && isWhite)
        drawLed (g, { cap.getCentreX(), cap.getY() + 5.0f }, 2.2f, on);

    if (capText.isNotEmpty())
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.setFont (labelFont (std::min (13.0f, cap.getHeight() * 0.55f)));
        g.drawFittedText (capText, cap.toNearestInt(), juce::Justification::centred, 1);
    }

    if (! labelArea.isEmpty())
        drawLabel (g, label, labelArea.toNearestInt().expanded (10, 0), 9.0f);
}

// ---------------------------------------------------------------------------
// LedChoice
// ---------------------------------------------------------------------------
LedChoice::LedChoice (juce::String t, juce::StringArray shortLabels, bool isVertical)
    : title (std::move (t)), items (std::move (shortLabels)), vertical (isVertical)
{
}

void LedChoice::bind (APVTS& state, const juce::String& paramId)
{
    attachment.reset();
    param = findParam (state, paramId);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        selected = juce::roundToInt (v);
        repaint();
    });
    attachment->sendInitialUpdate();
}

void LedChoice::unbind()
{
    attachment.reset();
    param = nullptr;
}

juce::Rectangle<float> LedChoice::itemBounds (int index) const
{
    auto b = getLocalBounds().toFloat();
    if (title.isNotEmpty())
        b.removeFromBottom (13.0f);
    const int n = std::max (1, items.size());
    if (vertical)
    {
        const float h = b.getHeight() / (float) n;
        return { b.getX(), b.getY() + h * (float) index, b.getWidth(), h };
    }
    const float w = b.getWidth() / (float) n;
    return { b.getX() + w * (float) index, b.getY(), w, b.getHeight() };
}

void LedChoice::paint (juce::Graphics& g)
{
    for (int i = 0; i < items.size(); ++i)
    {
        auto r = itemBounds (i);
        if (vertical)
        {
            drawLed (g, { r.getX() + 5.0f, r.getCentreY() }, 2.4f, i == selected);
            g.setColour (i == selected ? colours::text : colours::textDim);
            g.setFont (labelFont (9.0f));
            g.drawText (items[i], r.withTrimmedLeft (11.0f), juce::Justification::centredLeft, false);
        }
        else
        {
            drawLed (g, { r.getCentreX(), r.getY() + 4.5f }, 2.4f, i == selected);
            g.setColour (i == selected ? colours::text : colours::textDim);
            g.setFont (labelFont (9.0f));
            g.drawFittedText (items[i], r.withTrimmedTop (10.0f).toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
        }
    }
    if (title.isNotEmpty())
        drawLabel (g, title, getLocalBounds().removeFromBottom (13).expanded (10, 0), 9.0f);
}

void LedChoice::mouseDown (const juce::MouseEvent& e)
{
    if (attachment == nullptr)
        return;
    for (int i = 0; i < items.size(); ++i)
        if (itemBounds (i).contains (e.position))
        {
            attachment->setValueAsCompleteGesture ((float) i);
            notifyTouch (param);
            return;
        }
    // Click on the title cycles
    attachment->setValueAsCompleteGesture ((float) ((selected + 1) % std::max (1, items.size())));
    notifyTouch (param);
}

// ---------------------------------------------------------------------------
// Selector
// ---------------------------------------------------------------------------
Selector::Selector (juce::String labelText) : label (std::move (labelText)) {}

void Selector::bind (APVTS& state, const juce::String& paramId)
{
    attachment.reset();
    param = findParam (state, paramId);
    items.clear();
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        items = choice->choices;
    else
    {
        const auto& range = param->getNormalisableRange();
        for (int v = (int) range.start; v <= (int) range.end; ++v)
            items.add (param->getText (param->convertTo0to1 ((float) v), 16));
    }
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        selected = juce::roundToInt (v - param->getNormalisableRange().start);
        repaint();
    });
    attachment->sendInitialUpdate();
}

void Selector::unbind()
{
    attachment.reset();
    param = nullptr;
}

void Selector::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto labelArea = b.removeFromBottom (13.0f);
    auto box = b.reduced (1.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (box, 2.5f);
    g.setColour (colours::line.withAlpha (0.6f));
    g.drawRoundedRectangle (box, 2.5f, 0.8f);

    const auto text = items[selected].toUpperCase();
    g.setColour (colours::led.withAlpha (0.25f));
    g.setFont (labelFont (std::min (11.0f, box.getHeight() * 0.62f)));
    g.drawFittedText (text, box.toNearestInt().translated (0, 1), juce::Justification::centred, 1, 0.6f);
    g.setColour (colours::led);
    g.drawFittedText (text, box.toNearestInt(), juce::Justification::centred, 1, 0.6f);

    drawLabel (g, label, labelArea.toNearestInt().expanded (10, 0), 9.0f);
}

void Selector::mouseDown (const juce::MouseEvent& e)
{
    if (attachment == nullptr || items.isEmpty())
        return;
    const float start = param->getNormalisableRange().start;

    if (e.mods.isPopupMenu() || e.mods.isShiftDown())
    {
        juce::PopupMenu menu;
        for (int i = 0; i < items.size(); ++i)
            menu.addItem (i + 1, items[i], true, i == selected);
        juce::Component::SafePointer<Selector> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, start] (int result)
        {
            if (safe != nullptr && result > 0 && safe->attachment != nullptr)
            {
                safe->attachment->setValueAsCompleteGesture (start + (float) (result - 1));
                safe->notifyTouch (safe->param);
            }
        });
        return;
    }
    attachment->setValueAsCompleteGesture (start + (float) ((selected + 1) % items.size()));
    notifyTouch (param);
}

// ---------------------------------------------------------------------------
// Wheel
// ---------------------------------------------------------------------------
Wheel::Wheel (juce::String labelText, bool springBack) : label (std::move (labelText)), spring (springBack) {}

void Wheel::setValue (float v, bool notify)
{
    value = juce::jlimit (spring ? -1.0f : 0.0f, 1.0f, v);
    repaint();
    if (notify && onChange != nullptr)
        onChange (value);
}

void Wheel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto labelArea = b.removeFromBottom (13.0f);
    auto slot = b.reduced (2.0f, 0.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (slot, 4.0f);

    auto wheel = slot.reduced (3.0f, 4.0f);
    juce::ColourGradient grad (juce::Colour (0xff111112), wheel.getX(), wheel.getY(), juce::Colour (0xff111112),
                               wheel.getX(), wheel.getBottom(), false);
    grad.addColour (0.5, juce::Colour (0xff6b6c70));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (wheel, 3.0f);

    // Ribs scroll with the value
    const float norm = spring ? (value + 1.0f) * 0.5f : value;
    const float offset = (1.0f - norm) * 40.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    for (float y = wheel.getY() + std::fmod (offset, 6.0f); y < wheel.getBottom(); y += 6.0f)
        g.drawLine (wheel.getX() + 1.0f, y, wheel.getRight() - 1.0f, y, 1.0f);

    // Position marker
    const float markerY = wheel.getY() + 6.0f + (1.0f - norm) * (wheel.getHeight() - 12.0f);
    g.setColour (colours::ivory);
    g.fillRect (wheel.getX() + 2.0f, markerY - 1.5f, wheel.getWidth() - 4.0f, 3.0f);

    drawLabel (g, label, labelArea.toNearestInt().expanded (10, 0), 9.0f);
}

void Wheel::mouseDown (const juce::MouseEvent&) { dragStartValue = value; }

void Wheel::mouseDrag (const juce::MouseEvent& e)
{
    const float range = spring ? 2.0f : 1.0f;
    setValue (dragStartValue - (float) e.getDistanceFromDragStartY() / ((float) getHeight() * 0.8f) * range, true);
}

void Wheel::mouseUp (const juce::MouseEvent&)
{
    if (spring)
        setValue (0.0f, true);
}
} // namespace rm::ui
