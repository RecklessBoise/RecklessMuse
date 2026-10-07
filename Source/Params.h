#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Parameter identifiers and layout.
//
// Every sound-shaping parameter exists twice, once per timbre ("a_" and "b_" prefixes).
// Performance / global parameters (delay, clock, arp, sequencer, chord, output) exist once.

namespace rm
{
constexpr int kNumTimbres = 2;
constexpr int kNumMatrixSlots = 8;
constexpr int kVoicesPerTimbre = 8;
constexpr int kMaxSeqSteps = 64;

inline juce::String timbrePrefix (int timbre) { return timbre == 0 ? "a_" : "b_"; }
inline juce::String tid (int timbre, const juce::String& key) { return timbrePrefix (timbre) + key; }
inline juce::String mmKey (int slot, const char* field) { return "mm" + juce::String (slot + 1) + field; }

// X-macro of every per-timbre parameter key (matrix slots are handled separately).
#define RM_TIMBRE_PARAMS(X) \
    X(o1Oct) X(o1Freq) X(o1Wave) \
    X(o2Oct) X(o2Freq) X(o2Wave) X(o2Sync) X(o2KbTrack) \
    X(fmAmt) X(fmSrc) \
    X(moscFreq) X(moscWave) X(moscKbTrack) X(moscUnipolar) X(moscKeyReset) \
    X(moscPitchAmt) X(moscPitchDest) X(moscFilterAmt) X(moscFilterDest) X(moscPwmAmt) X(moscVcaAmt) \
    X(mixO1) X(mixRing) X(mixO2) X(mixMod) X(mixNoise) X(mixOverload) \
    X(f1Cutoff) X(f1Res) X(f1Env) X(f1Kb) X(f1Mode) \
    X(f2Cutoff) X(f2Res) X(f2Env) X(f2Kb) \
    X(fOrder) X(fRouting) X(fLink) \
    X(feA) X(feD) X(feS) X(feR) X(feLoop) X(feVel) \
    X(aeA) X(aeD) X(aeS) X(aeR) X(aeLoop) X(aeVel) \
    X(vcaLevel) X(vcaPan) X(vcaSpread) \
    X(l1Rate) X(l1Sync) X(l1Div) X(l1Wave) X(l1Amp) X(l1Reset) \
    X(l2Rate) X(l2Sync) X(l2Div) X(l2Wave) X(l2Amp) X(l2Reset) \
    X(plRate) X(plWave) X(plAmt) X(plDest) X(plWheel) \
    X(polyMode) X(uniVoices) X(uniDetune) X(glide) X(glideLegato) X(bendRange) X(notePrio)

#define RM_GLOBAL_PARAMS(X) \
    X(timbreMode) X(splitNote) X(kbOctave) X(kbHold) X(vintage) X(quality) X(masterVol) \
    X(dlyOn) X(dlyTimeL) X(dlyTimeR) X(dlySync) X(dlyDivL) X(dlyDivR) X(dlyFb) X(dlyChar) X(dlyDiff) X(dlyMix) \
    X(macro1) X(macro2) X(tempo) \
    X(arpOn) X(arpDiv) X(arpMode) X(arpOct) X(arpGate) X(arpLatch) \
    X(seqPlay) X(seqRec) X(seqDiv) X(seqLength) X(seqGate) X(seqTranspose) \
    X(chordOn) X(chordLearn)

namespace choices
{
    inline const juce::StringArray octave { "32'", "16'", "8'", "4'", "2'" };
    inline const juce::StringArray fmSrc { "OSC 1>2", "MOD>1", "MOD>2", "MOD>1+2" };
    inline const juce::StringArray modWave { "Sine", "Triangle", "Saw", "Ramp", "Square", "S&H", "Noise" };
    inline const juce::StringArray oscDest { "Osc 1", "Osc 2", "Both" };
    inline const juce::StringArray filterDest { "Filter 1", "Filter 2", "Both" };
    inline const juce::StringArray kbTrack { "Off", "1/2", "Full" };
    inline const juce::StringArray f1Mode { "Low Pass", "High Pass" };
    inline const juce::StringArray order { "6 dB", "12 dB", "18 dB", "24 dB" };
    inline const juce::StringArray routing { "Series", "Parallel", "Stereo" };
    inline const juce::StringArray lfoWave { "Sine", "Triangle", "Saw", "Ramp", "Square", "S&H", "Smooth" };
    inline const juce::StringArray pitchLfoWave { "Sine", "Triangle", "Square", "S&H" };
    inline const juce::StringArray polyMode { "Poly", "Mono", "Unison" };
    inline const juce::StringArray notePrio { "Last", "Low", "High" };
    inline const juce::StringArray timbreMode { "Single", "Split", "Stack" };
    inline const juce::StringArray quality { "Eco", "High" };
    inline const juce::StringArray arpMode { "Up", "Down", "Up/Down", "Order", "Random" };
    inline const juce::StringArray divisions { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                        "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1", "4/1" };

    // Length of each division in quarter notes.
    constexpr double divisionBeats[] { 0.0625, 1.0 / 12.0, 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5,
                                       0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };

    enum ModSource { srcOff, srcLfo1, srcLfo2, srcModOsc, srcFiltEnv, srcVcaEnv, srcVelocity, srcModWheel,
                     srcAftertouch, srcKeyTrack, srcRandom, srcMacro1, srcMacro2, numModSources };
    inline const juce::StringArray modSources { "Off", "LFO 1", "LFO 2", "Mod Osc", "Filter Env", "VCA Env", "Velocity",
                                         "Mod Wheel", "Aftertouch", "Key Track", "Random", "Macro 1", "Macro 2" };

    enum ModDest { dstOff, dstPitch, dstO1Pitch, dstO2Pitch, dstO1Wave, dstO2Wave, dstFm, dstO1Level, dstO2Level,
                   dstNoise, dstModRate, dstCutoff, dstF1Cutoff, dstF2Cutoff, dstF1Res, dstF2Res, dstOverload,
                   dstVca, dstPan, dstL1Rate, dstL2Rate, numModDests };
    inline const juce::StringArray modDests { "Off", "Pitch", "Osc 1 Pitch", "Osc 2 Pitch", "Osc 1 Wave", "Osc 2 Wave",
                                       "FM Amount", "Osc 1 Level", "Osc 2 Level", "Noise Level", "Mod Osc Rate",
                                       "Cutoff 1+2", "Cutoff 1", "Cutoff 2", "Resonance 1", "Resonance 2",
                                       "Overload", "VCA Level", "Pan", "LFO 1 Rate", "LFO 2 Rate" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Raw atomic pointers for lock-free access from the audio thread.
struct TimbreParamPtrs
{
#define RM_DECLARE_PTR(name) std::atomic<float>* name = nullptr;
    RM_TIMBRE_PARAMS (RM_DECLARE_PTR)
#undef RM_DECLARE_PTR
    std::atomic<float>* mmSrc[kNumMatrixSlots] {};
    std::atomic<float>* mmVia[kNumMatrixSlots] {};
    std::atomic<float>* mmDst[kNumMatrixSlots] {};
    std::atomic<float>* mmAmt[kNumMatrixSlots] {};

    void attach (juce::AudioProcessorValueTreeState& apvts, int timbre);
};

struct GlobalParamPtrs
{
#define RM_DECLARE_PTR(name) std::atomic<float>* name = nullptr;
    RM_GLOBAL_PARAMS (RM_DECLARE_PTR)
#undef RM_DECLARE_PTR

    void attach (juce::AudioProcessorValueTreeState& apvts);
};

inline int asInt (const std::atomic<float>* p) { return (int) std::lround (p->load (std::memory_order_relaxed)); }
inline bool asBool (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed) >= 0.5f; }
inline float asFloat (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); }
} // namespace rm
