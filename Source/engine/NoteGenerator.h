#pragma once

#include "../Params.h"
#include "../dsp/DspUtil.h"
#include <array>
#include <atomic>

namespace rm
{
// Step data edited from the UI and played on the audio thread.
struct SequenceData
{
    SequenceData()
    {
        for (int i = 0; i < kMaxSeqSteps; ++i)
        {
            note[(size_t) i] = 48 + (i % 4 == 0 ? 0 : (i % 4) * 7 % 12);
            on[(size_t) i] = true;
        }
    }

    std::array<std::atomic<int>, kMaxSeqSteps> note {};
    std::array<std::atomic<bool>, kMaxSeqSteps> on {};
    std::atomic<int> recordIndex { 0 };
    std::atomic<int> playIndex { -1 };
    std::atomic<bool> resetRequest { false };
    std::atomic<bool> advanceRequest { false };
};

struct ChordData
{
    static constexpr int kMaxNotes = 8;
    std::array<std::atomic<int>, kMaxNotes> intervals {};
    std::atomic<int> count { 0 };
    std::atomic<bool> learnFinished { false };
};

struct HostTime
{
    double bpm = 120.0;
    bool playing = false;
    bool hasPpq = false;
    double ppq = 0.0;
};

// Turns the incoming keyboard MIDI into the notes the synth should play:
// keyboard octave -> chord memory -> (sequencer | arpeggiator | direct).
class NoteGenerator
{
public:
    struct Settings
    {
        int kbOctave = 0;
        bool chordOn = false, chordLearn = false;
        bool arpOn = false, arpLatch = false;
        int arpMode = 0, arpOctaves = 1;
        double arpDivBeats = 0.25;
        float arpGate = 0.5f;
        bool seqPlay = false, seqRec = false, seqTranspose = true;
        int seqLength = 16;
        double seqDivBeats = 0.25;
        float seqGate = 0.5f;
    };

    NoteGenerator (SequenceData& seq, ChordData& chord) : sequence (seq), chords (chord) {}

    void prepare (double sampleRate);
    void reset();

    // Replaces the content of midi with the generated note stream.
    void process (juce::MidiBuffer& midi, int numSamples, const Settings& s, const HostTime& host);

private:
    struct StepClock
    {
        double stepSamples = 1.0, ppqStart = 0.0, ppqPerSample = 0.0, divBeats = 0.25;
        double samplesToNext = 0.0;
        int64_t lastIndex = -1;
        bool hostMode = false;

        void begin (const HostTime& h, double div, double sampleRate);
        void restart() { samplesToNext = 0.0; lastIndex = -1; }
        bool due (int t);
        int samplesUntilNext (int t) const;
        void advance (int samples) { if (! hostMode) samplesToNext -= samples; }
        int64_t index() const { return lastIndex; }
    };

    void handleInput (const juce::MidiMessage& m, int t, const Settings& s);
    void routeNote (bool isOn, int note, float velocity, int t, const Settings& s);
    void emit (bool isOn, int note, float velocity, int t);
    void arpStep (int t, const Settings& s);
    void seqStep (int t, const Settings& s);
    void stopArpNote (int t);
    void stopSeqNote (int t);
    void releaseDirectNotes (int t);

    SequenceData& sequence;
    ChordData& chords;
    juce::MidiBuffer output;
    double sampleRate = 44100.0;
    dsp::Random rng { 0x51A7u };

    // Mapping of input key -> notes actually generated (octave + chord)
    std::array<std::array<int8_t, ChordData::kMaxNotes>, 128> generated {};
    std::array<uint8_t, 128> generatedCount {};

    // Direct (non-arp, non-seq) sounding notes
    std::array<bool, 128> directOn {};

    // Arpeggiator
    struct ArpNote { int note; float velocity; };
    std::array<ArpNote, 32> arpNotes {};
    int numArpNotes = 0, physicalHeld = 0, arpIndex = -1;
    int arpPlaying = -1, arpGateLeft = 0;
    StepClock arpClock;

    // Sequencer
    std::array<int, 16> seqHeld {};
    int numSeqHeld = 0, seqIndex = -1;
    int seqPlaying = -1, seqGateLeft = 0;
    StepClock seqClock;

    // Chord learn
    std::array<int, ChordData::kMaxNotes> learnNotes {};
    int numLearn = 0, learnHeld = 0;

    bool wasArpOn = false, wasSeqPlay = false;
};
} // namespace rm
