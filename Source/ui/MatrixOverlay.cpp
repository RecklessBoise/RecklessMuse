#include "MatrixOverlay.h"

namespace rm::ui
{
MatrixOverlay::MatrixOverlay()
{
    for (auto& row : rows)
    {
        for (int i = 0; i < choices::modSources.size(); ++i)
        {
            row.source.addItem (choices::modSources[i], i + 1);
            row.via.addItem (i == 0 ? juce::String ("(none)") : choices::modSources[i], i + 1);
        }
        for (int i = 0; i < choices::modDests.size(); ++i)
            row.dest.addItem (choices::modDests[i], i + 1);

        row.amount.setSliderStyle (juce::Slider::LinearBar);
        row.amount.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
        row.amount.setColour (juce::Slider::trackColourId, colours::orange.withAlpha (0.6f));
        row.amount.setColour (juce::Slider::backgroundColourId, colours::panelDark);
        row.amount.setColour (juce::Slider::textBoxOutlineColourId, colours::line);
        row.amount.setDoubleClickReturnValue (true, 0.0);

        addAndMakeVisible (row.source);
        addAndMakeVisible (row.via);
        addAndMakeVisible (row.dest);
        addAndMakeVisible (row.amount);
    }
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);
}

void MatrixOverlay::bindTimbre (APVTS& state, int t)
{
    timbre = t;
    for (int s = 0; s < kNumMatrixSlots; ++s)
    {
        auto& row = rows[(size_t) s];
        row.sourceAtt.reset();
        row.viaAtt.reset();
        row.destAtt.reset();
        row.amountAtt.reset();
        row.sourceAtt = std::make_unique<APVTS::ComboBoxAttachment> (state, tid (t, mmKey (s, "Src")), row.source);
        row.viaAtt = std::make_unique<APVTS::ComboBoxAttachment> (state, tid (t, mmKey (s, "Via")), row.via);
        row.destAtt = std::make_unique<APVTS::ComboBoxAttachment> (state, tid (t, mmKey (s, "Dst")), row.dest);
        row.amountAtt = std::make_unique<APVTS::SliderAttachment> (state, tid (t, mmKey (s, "Amt")), row.amount);
        row.amount.setTextValueSuffix (" ");
        row.amount.textFromValueFunction = [] (double v)
        {
            const int pct = juce::roundToInt (v * 100.0);
            return (pct > 0 ? "+" : "") + juce::String (pct) + "%";
        };
        row.amount.updateText();
    }
    repaint();
}

void MatrixOverlay::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::panelDark.withAlpha (0.97f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (colours::orange);
    g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 1.5f);

    g.setColour (colours::text);
    g.setFont (labelFont (16.0f));
    g.drawText ("MODULATION MATRIX  -  TIMBRE " + juce::String (timbre == 0 ? "A" : "B"),
                getLocalBounds().removeFromTop (34).withTrimmedLeft (18), juce::Justification::centredLeft);

    g.setFont (labelFont (11.0f));
    g.setColour (colours::textDim);
    const int x0 = 60;
    const int colW = (getWidth() - x0 - 30) / 4;
    const juce::StringArray headers { "SOURCE", "VIA (SCALER)", "DESTINATION", "AMOUNT" };
    for (int i = 0; i < 4; ++i)
        g.drawText (headers[i], x0 + i * colW, 38, colW - 10, 16, juce::Justification::centredLeft);

    for (int s = 0; s < kNumMatrixSlots; ++s)
    {
        const auto r = rows[(size_t) s].source.getBounds();
        g.setColour (colours::text);
        g.setFont (labelFont (14.0f));
        g.drawText (juce::String (s + 1), 18, r.getY(), 30, r.getHeight(), juce::Justification::centredLeft);
    }

    g.setFont (labelFont (10.0f, false));
    g.setColour (colours::textDim);
    g.drawText ("Amount is bipolar. VIA multiplies the source (e.g. LFO 1 via Mod Wheel = classic wheel vibrato).",
                getLocalBounds().removeFromBottom (24).withTrimmedLeft (18), juce::Justification::centredLeft);
}

void MatrixOverlay::resized()
{
    closeButton.setBounds (getWidth() - 90, 8, 76, 22);
    const int x0 = 60;
    const int colW = (getWidth() - x0 - 30) / 4;
    const int top = 58;
    const int rowH = (getHeight() - top - 30) / kNumMatrixSlots;
    for (int s = 0; s < kNumMatrixSlots; ++s)
    {
        auto& row = rows[(size_t) s];
        const int y = top + s * rowH;
        const int h = rowH - 6;
        row.source.setBounds (x0, y, colW - 10, h);
        row.via.setBounds (x0 + colW, y, colW - 10, h);
        row.dest.setBounds (x0 + 2 * colW, y, colW - 10, h);
        row.amount.setBounds (x0 + 3 * colW, y, colW - 10, h);
    }
}
} // namespace rm::ui
