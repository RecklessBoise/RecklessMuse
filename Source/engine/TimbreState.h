#pragma once

#include "../Params.h"

namespace rm
{
// Plain-value snapshot of one timbre's parameters, refreshed once per audio block.
struct TimbreState
{
    // Oscillators
    int o1Oct = 0, o2Oct = 0;
    float o1Freq = 0, o2Freq = 0, o1Wave = 0, o2Wave = 0;
    bool o2Sync = false, o2KbTrack = true;
    float fmAmt = 0;
    int fmSrc = 0;

    // Mod osc
    float moFreq = 5;
    int moWave = 0;
    bool moKbTrack = false, moUnipolar = false, moKeyReset = false;
    float moPitchAmt = 0, moFilterAmt = 0, moPwmAmt = 0, moVcaAmt = 0;
    int moPitchDest = 2, moFilterDest = 2;

    // Mixer
    float mixO1 = 0, mixRing = 0, mixO2 = 0, mixMod = 0, mixNoise = 0, mixOverload = 0;

    // Filters
    float f1Cutoff = 20, f1Res = 0, f1Env = 0, f2Cutoff = 1000, f2Res = 0, f2Env = 0;
    float f1Kb = 0, f2Kb = 0;
    int f1Mode = 1, fOrder = 4, fRouting = 0;
    bool fLink = false;

    // Envelopes
    float feA = 0, feD = 0, feS = 0, feR = 0, feVel = 0;
    float aeA = 0, aeD = 0, aeS = 0, aeR = 0, aeVel = 0;
    bool feLoop = false, aeLoop = false;

    // VCA
    float vcaLevel = 0.75f, vcaPan = 0, vcaSpread = 0;

    // LFOs (rate already resolved to Hz, tempo-synced if needed)
    float l1Hz = 1, l2Hz = 1, l1Amp = 1, l2Amp = 1;
    int l1Wave = 0, l2Wave = 0;
    bool l1Reset = false, l2Reset = false;

    float plHz = 5, plAmt = 0;
    int plWave = 0, plDest = 2;
    bool plWheel = true;

    // Voice control
    int polyMode = 0, uniVoices = 4, notePrio = 0;
    float uniDetune = 0, glide = 0, bendRange = 2;
    bool glideLegato = false;

    // Modulation matrix
    int mmSrc[kNumMatrixSlots] {}, mmVia[kNumMatrixSlots] {}, mmDst[kNumMatrixSlots] {};
    float mmAmt[kNumMatrixSlots] {};

    // Globals relevant to voices
    float vintage = 0.35f;
};

// Performance controllers shared by every voice of a timbre.
struct PerformanceState
{
    float pitchBend = 0.0f;  // -1..1
    float modWheel = 0.0f;   // 0..1
    float aftertouch = 0.0f; // 0..1
    float macro1 = 0.0f, macro2 = 0.0f;
};
} // namespace rm
