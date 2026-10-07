#pragma once

#include "Voice.h"
#include <array>

namespace rm
{
// One timbre (A or B): 8-voice pool, voice allocation, mono/unison note stack, sustain pedal.
class TimbreEngine
{
public:
    void prepare (double internalSampleRate, int timbreIndex);

    // Reads the parameter atomics into the plain-value state used by the voices.
    void updateState (const TimbreParamPtrs& p, float vintage, double bpm);
    const TimbreState& getState() const noexcept { return state; }
    TimbreState& getStateForTesting() noexcept { return state; }

    void noteOn (int note, float velocity);
    void noteOff (int note);
    void setSustain (bool down);
    void allNotesOff();
    void killAll();

    PerformanceState perf;

    void render (float* left, float* right, int numSamples);
    int getActiveVoiceCount() const noexcept;

private:
    void polyNoteOn (int note, float velocity);
    void monoNoteOn (int note, float velocity);
    void monoNoteOff (int note);
    int pickMonoNote() const noexcept;
    int numStackVoices() const noexcept { return state.polyMode == 2 ? std::clamp (state.uniVoices, 2, kVoicesPerTimbre) : 1; }
    bool anyGateOn() const noexcept;
    void configureStack();

    std::array<Voice, kVoicesPerTimbre> voices;
    TimbreState state;
    int lastPolyMode = 0;
    int nextVoice = 0;
    float lastPitch = 60.0f;

    // Held keys (for mono priority and the sustain pedal)
    std::array<int, 32> heldNotes {};
    std::array<float, 32> heldVelocities {};
    int numHeld = 0;
    std::array<bool, 128> keyDown {};
    std::array<bool, 128> sustained {};
    bool sustainPedal = false;
    float monoVelocity = 1.0f;
};
} // namespace rm
