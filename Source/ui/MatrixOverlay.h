#pragma once

#include "Controls.h"
#include "../Params.h"

namespace rm::ui
{
// 8-slot modulation matrix for the timbre being edited: Source x Via -> Destination, Amount.
class MatrixOverlay : public juce::Component
{
public:
    MatrixOverlay();

    void bindTimbre (APVTS& state, int timbre);
    std::function<void()> onClose;
    TouchCallback onTouch;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        juce::ComboBox source, via, dest;
        juce::Slider amount;
        std::unique_ptr<APVTS::ComboBoxAttachment> sourceAtt, viaAtt, destAtt;
        std::unique_ptr<APVTS::SliderAttachment> amountAtt;
    };

    std::array<Row, kNumMatrixSlots> rows;
    juce::TextButton closeButton { "CLOSE" };
    int timbre = 0;
};
} // namespace rm::ui
