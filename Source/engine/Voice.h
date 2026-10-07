#pragma once

#include "TimbreState.h"
#include "../dsp/Envelope.h"
#include "../dsp/LadderFilter.h"
#include "../dsp/Oscillators.h"

namespace rm
{
// One analog-modelled voice: 2 VCOs + mod osc + noise -> mixer/overload -> 2 ladders -> VCA.
class Voice
{
public:
    void prepare (double internalSampleRate, int voiceIndex);

    // Starts (or retriggers) the voice. startPitch is where glide begins.
    void start (int note, float velocity, float startPitch, bool useGlide, const TimbreState& s);
    // Moves a sounding voice to a new note without retriggering the envelopes (legato).
    void legatoTo (int note, bool useGlide);
    void release();
    void kill();

    bool isActive() const noexcept { return active; }
    bool isGateOn() const noexcept { return gate; }
    bool isReleasing() const noexcept { return active && ! gate; }
    int getNote() const noexcept { return note; }
    uint64_t getAge() const noexcept { return age; }
    float getLevel() const noexcept { return ampEnv.getLevel(); }

    void setStackPosition (float detuneSemis, float panOffset) noexcept
    {
        unisonDetune = detuneSemis;
        unisonPan = panOffset;
    }
    void setSpreadPosition (float p) noexcept { spreadPos = p; }

    // Adds this voice's output to left/right (internal sample rate).
    void render (float* left, float* right, int numSamples, const TimbreState& s, const PerformanceState& perf);

private:
    void updateControl (const TimbreState& s, const PerformanceState& perf);

    enum Ramp { rP1, rP2, rC1, rC2, rK1, rK2, rW1, rW2, rFm, rL1, rL2, rLR, rLM, rLN, rOvl, rGainL, rGainR, numRamps };

    static constexpr int kControlInterval = 16;

    float fs = 88200.0f;
    int index = 0;

    dsp::MorphOscillator osc1, osc2;
    dsp::ShapeOscillator modOsc, lfo1, lfo2, pitchLfo, drift;
    dsp::NoiseSource noise;
    dsp::LadderFilter filter1, filter2;
    dsp::AdsrEnvelope filterEnv, ampEnv;
    dsp::Random rng;

    bool active = false, gate = false, firstTick = true, glideThisNote = false;
    int note = 60;
    float velocity = 1.0f, randomValue = 0.0f;
    float pitch = 60.0f, targetPitch = 60.0f;
    uint64_t age = 0;
    static inline uint64_t ageCounter = 0;

    float unisonDetune = 0.0f, unisonPan = 0.0f, spreadPos = 0.0f;
    float tolTune1 = 0.0f, tolTune2 = 0.0f, tolCutoff = 0.0f; // "component tolerance"

    int ctrlCountdown = 0;
    float cur[numRamps] {}, step[numRamps] {};

    // Per-control-period values used by the per-sample loop
    float moInc = 0.0f, moPitchSemis = 0.0f, moFilterOct = 0.0f, moPwm = 0.0f, moVca = 0.0f;
    float env1Oct = 0.0f, env2Oct = 0.0f, filterVelScale = 1.0f, ampVelScale = 1.0f, vcaLevel = 1.0f;
    float overloadBias = 0.0f, overloadBiasOut = 0.0f;
    float lastModOsc = 0.0f, lastOsc1 = 0.0f, lfo1Value = 0.0f, lfo2Value = 0.0f;
    float lfo1RateMul = 1.0f, lfo2RateMul = 1.0f;
};
} // namespace rm
