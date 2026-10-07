#pragma once

#include "TimbreEngine.h"
#include "../dsp/DiffusionDelay.h"
#include <juce_dsp/juce_dsp.h>

namespace rm
{
// Complete sound engine: two timbres rendered at 2x (or 1x in Eco mode), split/stack
// routing, diffusion delay and the output stage. Owns no parameters itself; it reads
// the atomics handed to it in setParameters().
class SynthEngine
{
public:
    void setParameters (const TimbreParamPtrs* timbreA, const TimbreParamPtrs* timbreB, const GlobalParamPtrs* globals);
    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    // Renders the block. midi must already contain the final notes (after arp/seq/chord).
    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double bpm);

    void allNotesOff();
    int getActiveVoiceCount() const noexcept;
    TimbreEngine& getTimbre (int t) noexcept { return timbres[(size_t) t]; }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int num, const juce::MidiBuffer& midi, double bpm);
    void handleMidi (const juce::MidiMessage& m);
    void renderVoices (float* left, float* right, int numSamples);
    void configureRate (int oversampleFactor);
    static float safetyClip (float x) noexcept;

    const TimbreParamPtrs* params[2] {};
    const GlobalParamPtrs* globals = nullptr;

    std::array<TimbreEngine, 2> timbres;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> scratch;
    dsp::DiffusionDelay delay;
    dsp::DcBlocker dcL, dcR;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> masterGain;

    double sampleRate = 44100.0;
    int maxBlock = 512;
    int currentFactor = 0; // oversampling factor actually in use (1 or 2)
    std::array<uint8_t, 128> noteRouting {}; // bit 0 = timbre A, bit 1 = timbre B
};
} // namespace rm
