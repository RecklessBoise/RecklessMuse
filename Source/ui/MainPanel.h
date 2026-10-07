#pragma once

#include "Controls.h"
#include "Display.h"
#include "MatrixOverlay.h"
#include <juce_audio_utils/juce_audio_utils.h>

class RecklessMuseProcessor;

namespace rm::ui
{
// The whole instrument face, laid out at a fixed design size and scaled by the editor.
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 1600;
    static constexpr int kHeight = 780;

    explicit MainPanel (RecklessMuseProcessor& processor);
    ~MainPanel() override;

    std::function<void (juce::Component& anchor)> onSizeMenu;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    template <class T, class... Args>
    T& make (Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        addAndMakeVisible (ref);
        owned.push_back (std::move (c));
        return ref;
    }

    juce::Rectangle<int> section (const juce::String& title) const;
    void addSection (const juce::String& title, juce::Rectangle<int> bounds);
    static void place (juce::Component& c, juce::Rectangle<int> sec, int cx, int cy, int w, int h);

    Knob& knob (const juce::String& label, juce::Rectangle<int> sec, int cx, int cy, int diameter);
    Knob& fader (const juce::String& label, juce::Rectangle<int> sec, int cx, int top, int height);

    void timbreParam (Bindable& control, const juce::String& key);
    void globalParam (Bindable& control, const juce::String& id);
    void bindTimbre (int timbre);

    void buildTopRow();
    void buildBottomRow();
    void buildPerformance();
    void timerCallback() override;
    void showParameter (const juce::String& name, const juce::String& value);
    void savePresetDialog();
    void selectStep (int step);

    RecklessMuseProcessor& processor;
    APVTS& state;
    MuseLookAndFeel lnf;

    std::vector<Section> sections;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::pair<Bindable*, juce::String>> timbreBindings;
    std::vector<Bindable*> allBindables;

    Display* display = nullptr;
    KeyCap* timbreButtons[2] {};
    KeyCap* matrixButton = nullptr;
    std::array<KeyCap*, 16> stepButtons {};
    std::array<KeyCap*, 4> pageButtons {};
    Knob* stepValue = nullptr;
    Knob *lfoRate[2] {}, *lfoDiv[2] {};
    Knob *delayTime[2] {}, *delayDiv[2] {};
    KeyCap* sizeButton = nullptr;
    Wheel *pitchWheel = nullptr, *modWheel = nullptr;
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    std::unique_ptr<MatrixOverlay> matrix;

    int editTimbre = 0;
    int stepPage = 0;
    int selectedStep = 0;
    bool clockLedOn = false;
    juce::String chordText;
    int kbOctave = 0;
};
} // namespace rm::ui
