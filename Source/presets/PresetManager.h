#pragma once

#include "../engine/NoteGenerator.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace rm
{
// Factory presets are embedded XML files; user presets live in ~/Music/RecklessMuse/Presets.
//
// Preset format:
//   <RecklessMusePreset name="..." category="..." author="...">
//     <Params a_o1Wave="0.33" ... />          plain (not normalised) values; missing = default
//     <Sequence notes="48,55,..." on="1,1,0,..."/>
//     <Chord intervals="0,4,7"/>
//   </RecklessMusePreset>
class PresetManager
{
public:
    struct Preset
    {
        juce::String name, category, author;
        juce::String xml;
        bool isFactory = true;
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState& apvts, SequenceData& seq, ChordData& chord);

    static const juce::StringArray& categoryOrder();
    static juce::File getUserPresetFolder();

    int getNumPresets() const noexcept { return (int) presets.size(); }
    const Preset& getPreset (int index) const { return presets[(size_t) index]; }
    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentName() const;
    juce::String getCurrentCategory() const;

    bool loadPreset (int index);
    void loadInit();
    void next (int delta);
    void nextCategory (int delta);
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();

    // Applies a preset XML to the parameters. Returns false if the XML isn't a preset.
    bool applyPresetXml (const juce::XmlElement& xml);
    std::unique_ptr<juce::XmlElement> createPresetXml (const juce::String& name, const juce::String& category) const;

    // For state restore
    void setCurrentByName (const juce::String& name);

private:
    void loadFactoryPresets();
    void sortPresets();

    juce::AudioProcessorValueTreeState& apvts;
    SequenceData& sequence;
    ChordData& chords;
    std::vector<Preset> presets;
    int currentIndex = -1;
    juce::String currentName { "Init" }, currentCategory { "Init" };
};
} // namespace rm
