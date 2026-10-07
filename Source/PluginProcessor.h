#pragma once

#include "Params.h"
#include "engine/NoteGenerator.h"
#include "engine/SynthEngine.h"
#include "presets/PresetManager.h"
#include <juce_audio_utils/juce_audio_utils.h>

class RecklessMuseProcessor : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    RecklessMuseProcessor();
    ~RecklessMuseProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rm::SequenceData sequence;
    rm::ChordData chords;
    rm::PresetManager presets;
    juce::MidiKeyboardState keyboardState;

    // UI <-> audio communication
    std::atomic<float> uiPitchBend { 0.0f }, uiModWheel { 0.0f };
    std::atomic<bool> panicRequest { false };
    std::atomic<double> currentBpm { 120.0 };
    std::atomic<bool> hostSynced { false };
    std::atomic<int> activeVoices { 0 };

    // Editor size (persisted with the session)
    std::atomic<float> editorScale { 0.85f };
    // Timbre being edited in the UI (0 = A, 1 = B)
    std::atomic<int> editTimbre { 0 };

    void tapTempo();

private:
    void handleAsyncUpdate() override;

    rm::TimbreParamPtrs timbreParams[rm::kNumTimbres];
    rm::GlobalParamPtrs globalParams;
    rm::SynthEngine engine;
    rm::NoteGenerator noteGenerator { sequence, chords };

    float lastUiBend = 0.0f, lastUiWheel = 0.0f;
    bool lastHold = false;
    juce::int64 lastTapMs = 0;
    juce::Array<double> tapIntervals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessMuseProcessor)
};
