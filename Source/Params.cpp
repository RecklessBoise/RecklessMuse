#include "Params.h"

namespace rm
{
namespace
{
using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;
using Range = juce::NormalisableRange<float>;

Range logRange (float lo, float hi)
{
    return Range (lo, hi,
                  [] (float start, float end, float norm) { return start * std::pow (end / start, norm); },
                  [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); });
}

Range bipolarRange (float skew = 0.5f)
{
    Range r (-1.0f, 1.0f);
    r.setSkewForCentre (0.0f);
    r.skew = skew;
    r.symmetricSkew = true;
    return r;
}

juce::String hzText (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                        : juce::String (v, v < 10.0f ? 2 : (v < 100.0f ? 1 : 0)) + " Hz";
}

juce::String timeText (float v, int)
{
    return v >= 1.0f ? juce::String (v, 2) + " s" : juce::String (v * 1000.0f, v < 0.01f ? 1 : 0) + " ms";
}

juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::String bipolarText (float v, int)
{
    auto i = juce::roundToInt (v * 100.0f);
    return (i > 0 ? "+" : "") + juce::String (i) + " %";
}

juce::String semiText (float v, int)
{
    return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " st";
}

juce::String panText (float v, int)
{
    auto i = juce::roundToInt (v * 100.0f);
    if (i == 0) return "C";
    return i < 0 ? "L" + juce::String (-i) : "R" + juce::String (i);
}

juce::String noteText (int v, int)
{
    return juce::MidiMessage::getMidiNoteName (v, true, true, 3);
}

struct Builder
{
    Layout& layout;
    juce::String prefix;
    juce::String namePrefix;

    void flt (const juce::String& key, const juce::String& name, Range range, float def,
              std::function<juce::String (float, int)> text = pctText)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { prefix + key, 1 }, namePrefix + name, range, def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
    }

    void choice (const juce::String& key, const juce::String& name, const juce::StringArray& items, int def)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { prefix + key, 1 },
                                                                  namePrefix + name, items, def));
    }

    void boolean (const juce::String& key, const juce::String& name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { prefix + key, 1 },
                                                                namePrefix + name, def));
    }

    void integer (const juce::String& key, const juce::String& name, int lo, int hi, int def,
                  std::function<juce::String (int, int)> text = nullptr)
    {
        auto attrs = juce::AudioParameterIntAttributes();
        if (text != nullptr)
            attrs = attrs.withStringFromValueFunction (std::move (text));
        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { prefix + key, 1 },
                                                               namePrefix + name, lo, hi, def, attrs));
    }
};

void addTimbreParams (Layout& layout, int t)
{
    Builder b { layout, timbrePrefix (t), juce::String (t == 0 ? "A " : "B ") };

    // Oscillators
    b.choice ("o1Oct", "Osc 1 Octave", choices::octave, 2);
    b.flt ("o1Freq", "Osc 1 Frequency", Range (-7.0f, 7.0f), 0.0f, semiText);
    b.flt ("o1Wave", "Osc 1 Wave", Range (0.0f, 1.0f), 1.0f / 3.0f);
    b.choice ("o2Oct", "Osc 2 Octave", choices::octave, 2);
    b.flt ("o2Freq", "Osc 2 Frequency", Range (-7.0f, 7.0f), 0.07f, semiText);
    b.flt ("o2Wave", "Osc 2 Wave", Range (0.0f, 1.0f), 1.0f / 3.0f);
    b.boolean ("o2Sync", "Osc 2 Sync", false);
    b.boolean ("o2KbTrack", "Osc 2 KB Track", true);
    b.flt ("fmAmt", "FM Amount", Range (0.0f, 1.0f, 0.0f, 0.5f), 0.0f);
    b.choice ("fmSrc", "FM Source", choices::fmSrc, 0);

    // Modulation oscillator
    b.flt ("moscFreq", "Mod Osc Frequency", logRange (0.02f, 2000.0f), 5.0f, hzText);
    b.choice ("moscWave", "Mod Osc Wave", choices::modWave, 1);
    b.boolean ("moscKbTrack", "Mod Osc KB Track", false);
    b.boolean ("moscUnipolar", "Mod Osc Unipolar", false);
    b.boolean ("moscKeyReset", "Mod Osc Key Reset", false);
    b.flt ("moscPitchAmt", "Mod Osc Pitch Amount", bipolarRange (0.4f), 0.0f, bipolarText);
    b.choice ("moscPitchDest", "Mod Osc Pitch Dest", choices::oscDest, 2);
    b.flt ("moscFilterAmt", "Mod Osc Filter Amount", bipolarRange (0.5f), 0.0f, bipolarText);
    b.choice ("moscFilterDest", "Mod Osc Filter Dest", choices::filterDest, 2);
    b.flt ("moscPwmAmt", "Mod Osc PWM Amount", Range (0.0f, 1.0f), 0.0f);
    b.flt ("moscVcaAmt", "Mod Osc VCA Amount", Range (0.0f, 1.0f), 0.0f);

    // Mixer
    b.flt ("mixO1", "Mixer Osc 1", Range (0.0f, 1.0f), 0.8f);
    b.flt ("mixRing", "Mixer Ring Mod", Range (0.0f, 1.0f), 0.0f);
    b.flt ("mixO2", "Mixer Osc 2", Range (0.0f, 1.0f), 0.65f);
    b.flt ("mixMod", "Mixer Mod Osc", Range (0.0f, 1.0f), 0.0f);
    b.flt ("mixNoise", "Mixer Noise", Range (0.0f, 1.0f), 0.0f);
    b.flt ("mixOverload", "Mixer Overload", Range (0.0f, 1.0f), 0.2f);

    // Filters
    b.flt ("f1Cutoff", "Filter 1 Cutoff", logRange (20.0f, 20000.0f), 20.0f, hzText);
    b.flt ("f1Res", "Filter 1 Resonance", Range (0.0f, 1.0f), 0.0f);
    b.flt ("f1Env", "Filter 1 Env Amount", bipolarRange(), 0.0f, bipolarText);
    b.choice ("f1Kb", "Filter 1 KB Track", choices::kbTrack, 0);
    b.choice ("f1Mode", "Filter 1 Mode", choices::f1Mode, 1);
    b.flt ("f2Cutoff", "Filter 2 Cutoff", logRange (20.0f, 20000.0f), 1400.0f, hzText);
    b.flt ("f2Res", "Filter 2 Resonance", Range (0.0f, 1.0f), 0.15f);
    b.flt ("f2Env", "Filter 2 Env Amount", bipolarRange(), 0.35f, bipolarText);
    b.choice ("f2Kb", "Filter 2 KB Track", choices::kbTrack, 1);
    b.choice ("fOrder", "Filter Order", choices::order, 3);
    b.choice ("fRouting", "Filter Routing", choices::routing, 0);
    b.boolean ("fLink", "Link Filters", false);

    // Envelopes
    auto attack = logRange (0.0005f, 20.0f);
    auto decay = logRange (0.002f, 30.0f);
    b.flt ("feA", "Filter Env Attack", attack, 0.002f, timeText);
    b.flt ("feD", "Filter Env Decay", decay, 0.45f, timeText);
    b.flt ("feS", "Filter Env Sustain", Range (0.0f, 1.0f), 0.3f);
    b.flt ("feR", "Filter Env Release", decay, 0.3f, timeText);
    b.boolean ("feLoop", "Filter Env Loop", false);
    b.flt ("feVel", "Filter Env Velocity", Range (0.0f, 1.0f), 0.3f);
    b.flt ("aeA", "VCA Env Attack", attack, 0.002f, timeText);
    b.flt ("aeD", "VCA Env Decay", decay, 0.6f, timeText);
    b.flt ("aeS", "VCA Env Sustain", Range (0.0f, 1.0f), 0.85f);
    b.flt ("aeR", "VCA Env Release", decay, 0.25f, timeText);
    b.boolean ("aeLoop", "VCA Env Loop", false);
    b.flt ("aeVel", "VCA Env Velocity", Range (0.0f, 1.0f), 0.3f);

    // VCA
    b.flt ("vcaLevel", "VCA Level", Range (0.0f, 1.0f), 0.75f);
    b.flt ("vcaPan", "Pan", Range (-1.0f, 1.0f), 0.0f, panText);
    b.flt ("vcaSpread", "Pan Spread", Range (0.0f, 1.0f), 0.3f);

    // LFOs
    for (int i = 1; i <= 2; ++i)
    {
        juce::String k = "l" + juce::String (i), n = "LFO " + juce::String (i) + " ";
        b.flt (k + "Rate", n + "Rate", logRange (0.02f, 60.0f), i == 1 ? 4.0f : 0.5f, hzText);
        b.boolean (k + "Sync", n + "Sync", false);
        b.choice (k + "Div", n + "Division", choices::divisions, 10);
        b.choice (k + "Wave", n + "Wave", choices::lfoWave, i == 1 ? 1 : 0);
        b.flt (k + "Amp", n + "Amplitude", Range (0.0f, 1.0f), 1.0f);
        b.boolean (k + "Reset", n + "Key Reset", false);
    }

    // Pitch LFO (classic mod-wheel vibrato)
    b.flt ("plRate", "Pitch LFO Rate", logRange (0.05f, 30.0f), 5.5f, hzText);
    b.choice ("plWave", "Pitch LFO Wave", choices::pitchLfoWave, 1);
    b.flt ("plAmt", "Pitch LFO Amount", Range (0.0f, 1.0f, 0.0f, 0.5f), 0.25f);
    b.choice ("plDest", "Pitch LFO Dest", choices::oscDest, 2);
    b.boolean ("plWheel", "Pitch LFO Mod Wheel", true);

    // Voice control
    b.choice ("polyMode", "Voice Mode", choices::polyMode, 0);
    b.integer ("uniVoices", "Unison Voices", 2, 8, 4);
    b.flt ("uniDetune", "Detune", Range (0.0f, 1.0f), 0.25f);
    b.flt ("glide", "Glide Time", Range (0.0f, 5.0f, 0.0f, 0.3f), 0.0f, timeText);
    b.boolean ("glideLegato", "Glide Legato", false);
    b.integer ("bendRange", "Bend Range", 1, 24, 2);
    b.choice ("notePrio", "Note Priority", choices::notePrio, 0);

    // Modulation matrix
    for (int s = 0; s < kNumMatrixSlots; ++s)
    {
        juce::String n = "Matrix " + juce::String (s + 1) + " ";
        b.choice (mmKey (s, "Src"), n + "Source", choices::modSources, 0);
        b.choice (mmKey (s, "Via"), n + "Via", choices::modSources, 0);
        b.choice (mmKey (s, "Dst"), n + "Destination", choices::modDests, 0);
        b.flt (mmKey (s, "Amt"), n + "Amount", bipolarRange (0.5f), 0.0f, bipolarText);
    }
}

void addGlobalParams (Layout& layout)
{
    Builder b { layout, {}, {} };

    b.choice ("timbreMode", "Timbre Mode", choices::timbreMode, 0);
    b.integer ("splitNote", "Split Point", 24, 96, 60, noteText);
    b.integer ("kbOctave", "Keyboard Octave", -2, 2, 0);
    b.boolean ("kbHold", "Hold", false);
    b.flt ("vintage", "Vintage", Range (0.0f, 1.0f), 0.35f);
    b.choice ("quality", "Quality", choices::quality, 1);
    b.flt ("masterVol", "Main Out", Range (-48.0f, 6.0f, 0.0f, 2.0f), -6.0f,
           [] (float v, int) { return juce::String (v, 1) + " dB"; });

    b.boolean ("dlyOn", "Delay On", false);
    b.flt ("dlyTimeL", "Delay Time L", logRange (0.001f, 2.0f), 0.375f, timeText);
    b.flt ("dlyTimeR", "Delay Time R", logRange (0.001f, 2.0f), 0.5f, timeText);
    b.boolean ("dlySync", "Delay Sync", true);
    b.choice ("dlyDivL", "Delay Division L", choices::divisions, 8);
    b.choice ("dlyDivR", "Delay Division R", choices::divisions, 10);
    b.flt ("dlyFb", "Delay Feedback", Range (0.0f, 1.0f), 0.35f);
    b.flt ("dlyChar", "Delay Character", Range (0.0f, 1.0f), 0.4f);
    b.flt ("dlyDiff", "Delay Diffusion", Range (0.0f, 1.0f), 0.3f);
    b.flt ("dlyMix", "Delay Mix", Range (0.0f, 1.0f), 0.25f);

    b.flt ("macro1", "Macro 1", Range (0.0f, 1.0f), 0.0f);
    b.flt ("macro2", "Macro 2", Range (0.0f, 1.0f), 0.0f);
    b.flt ("tempo", "Tempo", Range (30.0f, 300.0f, 0.1f), 120.0f,
           [] (float v, int) { return juce::String (v, 1) + " BPM"; });

    b.boolean ("arpOn", "Arp On", false);
    b.choice ("arpDiv", "Arp Division", choices::divisions, 4);
    b.choice ("arpMode", "Arp Mode", choices::arpMode, 0);
    b.integer ("arpOct", "Arp Octaves", 1, 4, 1);
    b.flt ("arpGate", "Arp Gate", Range (0.05f, 1.0f), 0.5f);
    b.boolean ("arpLatch", "Arp Latch", false);

    b.boolean ("seqPlay", "Seq Play", false);
    b.boolean ("seqRec", "Seq Record", false);
    b.choice ("seqDiv", "Seq Division", choices::divisions, 4);
    b.integer ("seqLength", "Seq Length", 1, kMaxSeqSteps, 16);
    b.flt ("seqGate", "Seq Gate", Range (0.05f, 1.0f), 0.5f);
    b.boolean ("seqTranspose", "Seq Key Transpose", true);

    b.boolean ("chordOn", "Chord On", false);
    b.boolean ("chordLearn", "Chord Learn", false);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    Layout layout;
    addGlobalParams (layout);
    for (int t = 0; t < kNumTimbres; ++t)
        addTimbreParams (layout, t);
    return layout;
}

void TimbreParamPtrs::attach (juce::AudioProcessorValueTreeState& apvts, int timbre)
{
    auto get = [&] (const juce::String& key)
    {
        auto* p = apvts.getRawParameterValue (tid (timbre, key));
        jassert (p != nullptr);
        return p;
    };
#define RM_ATTACH_PTR(name) name = get (#name);
    RM_TIMBRE_PARAMS (RM_ATTACH_PTR)
#undef RM_ATTACH_PTR
    for (int s = 0; s < kNumMatrixSlots; ++s)
    {
        mmSrc[s] = get (mmKey (s, "Src"));
        mmVia[s] = get (mmKey (s, "Via"));
        mmDst[s] = get (mmKey (s, "Dst"));
        mmAmt[s] = get (mmKey (s, "Amt"));
    }
}

void GlobalParamPtrs::attach (juce::AudioProcessorValueTreeState& apvts)
{
#define RM_ATTACH_PTR(name) name = apvts.getRawParameterValue (#name); jassert (name != nullptr);
    RM_GLOBAL_PARAMS (RM_ATTACH_PTR)
#undef RM_ATTACH_PTR
}
} // namespace rm
