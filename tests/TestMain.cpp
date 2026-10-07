// RecklessMuse test runner: DSP sanity checks + offline render of every factory preset.
// Exit code 0 = all good.

#include "Params.h"
#include "dsp/LadderFilter.h"
#include "dsp/Oscillators.h"
#include "engine/NoteGenerator.h"
#include "engine/SynthEngine.h"
#include "presets/PresetManager.h"
#include "PresetData.h"

#include <iostream>
#include <map>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    if (! ok)
    {
        ++failures;
        std::cout << "  FAIL: " << what << std::endl;
    }
}

// Minimal processor that only owns the parameter tree.
struct ParamHost : juce::AudioProcessor
{
    ParamHost() : juce::AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo())),
                  apvts (*this, nullptr, "RecklessMuse", rm::createParameterLayout()) {}

    const juce::String getName() const override { return "host"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

struct Rig
{
    static constexpr double kRate = 48000.0;
    static constexpr int kBlock = 256;

    ParamHost host;
    rm::SequenceData seq;
    rm::ChordData chords;
    rm::PresetManager presets { host.apvts, seq, chords };
    rm::TimbreParamPtrs timbre[2];
    rm::GlobalParamPtrs globals;
    rm::SynthEngine engine;
    rm::NoteGenerator generator { seq, chords };

    Rig()
    {
        timbre[0].attach (host.apvts, 0);
        timbre[1].attach (host.apvts, 1);
        globals.attach (host.apvts);
        engine.setParameters (&timbre[0], &timbre[1], &globals);
        engine.prepare (kRate, kBlock);
        generator.prepare (kRate);
    }

    struct Stats { float peak = 0.0f, rms = 0.0f; bool finite = true; };

    // Holds the notes for holdSeconds, then releases and renders tailSeconds more.
    Stats render (std::initializer_list<int> notes, double holdSeconds, double tailSeconds)
    {
        engine.reset();
        generator.reset();
        Stats st;
        double sumSq = 0.0;
        long count = 0;
        const int holdBlocks = (int) (holdSeconds * kRate / kBlock);
        const int totalBlocks = holdBlocks + (int) (tailSeconds * kRate / kBlock);
        juce::AudioBuffer<float> buffer (2, kBlock);
        rm::HostTime time;
        time.bpm = 120.0;

        rm::NoteGenerator::Settings ns;
        ns.arpOn = rm::asBool (globals.arpOn);
        ns.arpMode = rm::asInt (globals.arpMode);
        ns.arpOctaves = rm::asInt (globals.arpOct);
        ns.arpGate = rm::asFloat (globals.arpGate);
        ns.arpDivBeats = rm::choices::divisionBeats[rm::asInt (globals.arpDiv)];
        ns.chordOn = rm::asBool (globals.chordOn);

        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int n : notes) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            if (b == holdBlocks)
                for (int n : notes) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            generator.process (midi, kBlock, ns, time);
            engine.process (buffer, midi, time.bpm);
            for (int c = 0; c < 2; ++c)
            {
                const float* d = buffer.getReadPointer (c);
                for (int i = 0; i < kBlock; ++i)
                {
                    if (! std::isfinite (d[i])) st.finite = false;
                    st.peak = std::max (st.peak, std::abs (d[i]));
                    sumSq += (double) d[i] * d[i];
                    ++count;
                }
            }
        }
        st.rms = (float) std::sqrt (sumSq / (double) std::max (1L, count));
        return st;
    }
};

void testOscillator()
{
    std::cout << "[osc] band-limited morph oscillator" << std::endl;
    for (float wave : { 0.0f, 0.33f, 0.66f, 1.0f })
    {
        rm::dsp::MorphOscillator osc;
        osc.reset (0.0f);
        double sum = 0.0;
        float peak = 0.0f;
        const float inc = 1000.0f / 48000.0f;
        const int n = 48000;
        for (int i = 0; i < n; ++i)
        {
            const float v = osc.process (inc, wave, 0.0f, nullptr, nullptr);
            sum += v;
            peak = std::max (peak, std::abs (v));
        }
        check (std::abs (sum / n) < 0.02, "osc DC offset, wave " + juce::String (wave));
        check (peak < 2.0f && peak > 0.5f, "osc peak range, wave " + juce::String (wave) + " = " + juce::String (peak));
    }
}

void testLadder()
{
    std::cout << "[filter] ladder self-oscillation and stability" << std::endl;
    rm::dsp::LadderFilter f;
    f.reset();
    const float fs = 96000.0f;
    const float g = rm::dsp::prewarp (rm::dsp::kPi * 1000.0f / fs);
    double sumSq = 0.0;
    int zeroCrossings = 0;
    float prev = 0.0f;
    const int n = (int) fs;
    for (int i = 0; i < n; ++i)
    {
        const float in = i == 0 ? 0.5f : 0.0f;
        const float y = f.process (in, g, 4.75f, 4, rm::dsp::LadderFilter::LowPass);
        if (i > n / 2)
        {
            sumSq += (double) y * y;
            if ((prev < 0.0f) != (y < 0.0f)) ++zeroCrossings;
        }
        prev = y;
    }
    const double rms = std::sqrt (sumSq / (n / 2));
    const double freq = zeroCrossings / 2.0 / 0.5;
    std::cout << "  self-osc rms " << rms << ", frequency ~" << freq << " Hz (cutoff 1000)" << std::endl;
    check (rms > 0.05, "ladder should self-oscillate at max resonance");
    check (freq > 700 && freq < 1300, "self-oscillation should track cutoff");

    // Driven hard with resonance: must stay bounded.
    f.reset();
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float in = (i / 40) % 2 == 0 ? 3.0f : -3.0f;
        peak = std::max (peak, std::abs (f.process (in, g, 4.0f, 4, rm::dsp::LadderFilter::LowPass)));
    }
    check (peak < 4.0f && std::isfinite (peak), "ladder bounded under heavy drive");
}

void testInitPatch (Rig& rig)
{
    std::cout << "[engine] init patch renders" << std::endl;
    rig.presets.loadInit();
    auto st = rig.render ({ 60 }, 1.0, 1.0);
    std::cout << "  peak " << st.peak << " rms " << st.rms << std::endl;
    check (st.finite, "init patch finite");
    check (st.peak > 0.05f, "init patch audible");
    check (st.peak <= 1.0f, "init patch within full scale");
}

void testAllPresets (Rig& rig)
{
    std::cout << "[presets] " << rig.presets.getNumPresets() << " factory presets" << std::endl;
    check (rig.presets.getNumPresets() >= 200, "at least 200 factory presets");

    juce::StringArray ids;
    for (auto* p : rig.host.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            ids.add (r->getParameterID());

    juce::StringArray names;
    int silent = 0;
    for (int i = 0; i < rig.presets.getNumPresets(); ++i)
    {
        const auto& preset = rig.presets.getPreset (i);
        if (! preset.isFactory)
            continue;
        const juce::String label = preset.category + "/" + preset.name;
        check (! names.contains (label), "duplicate preset name " + label);
        names.add (label);

        // Every attribute must be a real parameter
        if (auto xml = juce::parseXML (preset.xml))
            if (auto* values = xml->getChildByName ("Params"))
                for (int a = 0; a < values->getNumAttributes(); ++a)
                    check (ids.contains (values->getAttributeName (a)), label + ": unknown parameter " + values->getAttributeName (a));

        check (rig.presets.loadPreset (i), label + ": failed to load");
        auto st = rig.render ({ 48, 55, 60, 64 }, 2.5, 1.5);
        const bool ok = st.finite && st.peak > 0.01f && st.peak <= 1.0f;
        if (! ok)
        {
            ++silent;
            std::cout << "  " << label << ": peak " << st.peak << " rms " << st.rms << (st.finite ? "" : " NaN!") << std::endl;
        }
        check (st.finite, label + " produced NaN/inf");
        check (st.peak > 0.01f, label + " is silent");
    }
    std::cout << "  rendered " << names.size() << " presets, " << silent << " problems" << std::endl;
}

// AU and VST3 hosts identify parameters by a 31-bit hash of the ID string: it must be unique.
void testParameterIds (Rig& rig)
{
    std::cout << "[params] host parameter ID hashes" << std::endl;
    std::map<juce::uint32, juce::String> seen;
    int count = 0;
    for (auto* p : rig.host.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = r->getParameterID();
            const auto hash = (juce::uint32) id.hashCode() & 0x7fffffffu;
            const auto it = seen.find (hash);
            check (it == seen.end(), "hash collision: " + id + " / " + (it != seen.end() ? it->second : juce::String()));
            seen[hash] = id;
            ++count;
        }
    std::cout << "  " << count << " parameters, all IDs unique" << std::endl;
}

void testPerformance (Rig& rig)
{
    std::cout << "[perf] 16 voices (stack), 2x oversampling" << std::endl;
    rig.presets.loadInit();
    if (auto* p = rig.host.apvts.getParameter ("timbreMode")) p->setValueNotifyingHost (p->convertTo0to1 (2.0f));
    if (auto* p = rig.host.apvts.getParameter ("a_f1Mode")) p->setValueNotifyingHost (0.0f);
    const auto start = juce::Time::getMillisecondCounterHiRes();
    rig.render ({ 48, 52, 55, 59, 62, 65, 69, 72 }, 10.0, 0.0);
    const double ms = juce::Time::getMillisecondCounterHiRes() - start;
    const double cpu = ms / 10000.0 * 100.0;
    std::cout << "  10 s of audio rendered in " << ms << " ms  (" << cpu << " % of one core)" << std::endl;
    check (cpu < 60.0, "realtime performance");
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testOscillator();
    testLadder();
    Rig rig;
    testParameterIds (rig);
    testInitPatch (rig);
    testAllPresets (rig);
    testPerformance (rig);

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
