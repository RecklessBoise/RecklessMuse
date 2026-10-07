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

// Renders `seconds` with the given note events; returns left/right RMS of the last `measure` seconds.
std::pair<double, double> play (Rig& rig, const rm::NoteGenerator::Settings& ns, rm::HostTime& time,
                                std::initializer_list<int> notes, double holdFor, double seconds, double measure,
                                double stopTransportAt = -1.0)
{
    juce::AudioBuffer<float> buffer (2, Rig::kBlock);
    const int total = (int) (seconds * Rig::kRate / Rig::kBlock);
    const int holdBlocks = (int) (holdFor * Rig::kRate / Rig::kBlock);
    const int measureFrom = total - (int) (measure * Rig::kRate / Rig::kBlock);
    const int stopBlock = stopTransportAt < 0 ? -1 : (int) (stopTransportAt * Rig::kRate / Rig::kBlock);
    double l = 0, r = 0;
    for (int b = 0; b < total; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) for (int n : notes) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
        if (b == holdBlocks) for (int n : notes) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        if (b == stopBlock) time.playing = false;
        rig.generator.process (midi, Rig::kBlock, ns, time);
        rig.engine.process (buffer, midi, time.bpm);
        if (time.playing) time.ppq += Rig::kBlock / Rig::kRate * time.bpm / 60.0;
        if (b >= measureFrom)
            for (int k = 0; k < Rig::kBlock; ++k)
            {
                l += (double) buffer.getSample (0, k) * buffer.getSample (0, k);
                r += (double) buffer.getSample (1, k) * buffer.getSample (1, k);
            }
    }
    const double n = (double) (total - measureFrom) * Rig::kBlock;
    return { std::sqrt (l / n), std::sqrt (r / n) };
}

rm::NoteGenerator::Settings settingsFrom (Rig& rig)
{
    rm::NoteGenerator::Settings ns;
    ns.arpOn = rm::asBool (rig.globals.arpOn);
    ns.arpLatch = rm::asBool (rig.globals.arpLatch);
    ns.arpMode = rm::asInt (rig.globals.arpMode);
    ns.arpOctaves = rm::asInt (rig.globals.arpOct);
    ns.arpGate = rm::asFloat (rig.globals.arpGate);
    ns.arpDivBeats = rm::choices::divisionBeats[rm::asInt (rig.globals.arpDiv)];
    ns.seqLength = 16;
    return ns;
}

// Regression: arp / sequencer must stop when the notes end; mono sounds must be centred.
void testNotesStop (Rig& rig)
{
    std::cout << "[notes] arp / sequencer stop when the MIDI notes end" << std::endl;
    int checked = 0;
    for (int i = 0; i < rig.presets.getNumPresets(); ++i)
    {
        const auto& preset = rig.presets.getPreset (i);
        if (preset.category != "Sequence")
            continue;
        rig.presets.loadPreset (i);
        rig.engine.reset();
        rig.generator.reset();
        rm::HostTime time;
        time.playing = true;
        time.hasPpq = true;
        auto [l, r] = play (rig, settingsFrom (rig), time, { 48, 55, 60 }, 2.0, 10.0, 2.0);
        check (l < 1.0e-3 && r < 1.0e-3, preset.name + " keeps sounding after note-off (rms " + juce::String (l, 4) + ")");
        ++checked;
    }

    // Built-in sequencer: silent without keys, plays while a key is held, stops on release
    rig.presets.loadInit();
    rig.engine.reset();
    rig.generator.reset();
    auto ns = settingsFrom (rig);
    ns.seqPlay = true;
    rm::HostTime time;
    auto idle = play (rig, ns, time, {}, 0.0, 2.0, 2.0);
    check (idle.first < 1.0e-4, "sequencer must not play without a key");
    rig.generator.reset();
    auto held = play (rig, ns, time, { 48 }, 3.0, 3.0, 2.0);
    check (held.first > 0.01, "sequencer plays while a key is held");
    rig.engine.reset();
    rig.generator.reset();
    auto released = play (rig, ns, time, { 48 }, 1.0, 8.0, 2.0);
    check (released.first < 1.0e-3, "sequencer stops when the key is released");

    // Latch keeps the arp running, but stopping the transport stops it
    rig.presets.loadInit();
    rig.engine.reset();
    rig.generator.reset();
    ns = settingsFrom (rig);
    ns.arpOn = ns.arpLatch = true;
    time = {};
    time.playing = true;
    time.hasPpq = true;
    auto latched = play (rig, ns, time, { 48, 52 }, 0.5, 4.0, 1.0);
    check (latched.first > 0.01, "latched arp keeps playing after release");
    rig.engine.reset();
    rig.generator.reset();
    time = {};
    time.playing = true;
    time.hasPpq = true;
    auto stopped = play (rig, ns, time, { 48, 52 }, 0.5, 9.0, 2.0, 2.0);
    check (stopped.first < 1.0e-3, "transport stop silences a latched arp");

    // Mono patches sit in the centre
    for (const char* name : { "Model D Low End", "Acid Ladder", "Lucky Lead" })
        for (int i = 0; i < rig.presets.getNumPresets(); ++i)
            if (rig.presets.getPreset (i).name == name)
            {
                rig.presets.loadPreset (i);
                rig.host.apvts.getParameter ("dlyOn")->setValueNotifyingHost (0.0f); // L/R delay times differ on purpose
                rig.engine.reset();
                rig.generator.reset();
                rm::HostTime t;
                auto [l, r] = play (rig, settingsFrom (rig), t, { 48 }, 2.0, 2.0, 1.5);
                const double db = 20.0 * std::log10 ((l + 1e-9) / (r + 1e-9));
                check (std::abs (db) < 0.5, juce::String (name) + " is off-centre by " + juce::String (db, 2) + " dB");
            }
    std::cout << "  " << checked << " sequence presets stop cleanly" << std::endl;
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
// Simulates a DAW: transport running, notes held then released, long render.
// Prints left/right RMS per 2-second window for each Sequence preset.
void diagnoseSequences (Rig& rig)
{
    for (int i = 0; i < rig.presets.getNumPresets(); ++i)
    {
        const auto& preset = rig.presets.getPreset (i);
        if (preset.category != "Sequence")
            continue;
        rig.presets.loadPreset (i);
        rig.engine.reset();
        rig.generator.reset();

        rm::NoteGenerator::Settings ns;
        ns.arpOn = rm::asBool (rig.globals.arpOn);
        ns.arpLatch = rm::asBool (rig.globals.arpLatch);
        ns.arpMode = rm::asInt (rig.globals.arpMode);
        ns.arpOctaves = rm::asInt (rig.globals.arpOct);
        ns.arpGate = rm::asFloat (rig.globals.arpGate);
        ns.arpDivBeats = rm::choices::divisionBeats[rm::asInt (rig.globals.arpDiv)];

        rm::HostTime time;
        time.bpm = 120.0;
        time.playing = true;
        time.hasPpq = true;
        juce::AudioBuffer<float> buffer (2, Rig::kBlock);
        const int blocksPerWindow = (int) (2.0 * Rig::kRate / Rig::kBlock);
        juce::String line = preset.name.paddedRight (' ', 24);
        for (int w = 0; w < 10; ++w)
        {
            double l = 0, r = 0;
            bool finite = true;
            for (int b = 0; b < blocksPerWindow; ++b)
            {
                juce::MidiBuffer midi;
                if (w == 0 && b == 0)
                    for (int n : { 48, 55, 60 }) midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
                if (w == 1 && b == 0)
                    for (int n : { 48, 55, 60 }) midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
                rig.generator.process (midi, Rig::kBlock, ns, time);
                rig.engine.process (buffer, midi, time.bpm);
                time.ppq += Rig::kBlock / Rig::kRate * time.bpm / 60.0;
                for (int k = 0; k < Rig::kBlock; ++k)
                {
                    const float a = buffer.getSample (0, k), c = buffer.getSample (1, k);
                    finite = finite && std::isfinite (a) && std::isfinite (c);
                    l += a * a;
                    r += c * c;
                }
            }
            const double n = blocksPerWindow * Rig::kBlock;
            line << juce::String (std::sqrt (l / n), 3) << "/" << juce::String (std::sqrt (r / n), 3) << (finite ? " " : "NaN ");
        }
        std::cout << line << std::endl;
    }
}
// DAW-like stress: 44.1 kHz, random block sizes (some larger than prepared), looping transport,
// random notes. Reports any non-finite output or a channel that goes silent while the other plays.
void stressAllPresets()
{
    ParamHost host;
    rm::SequenceData seq;
    rm::ChordData chords;
    rm::PresetManager presets { host.apvts, seq, chords };
    rm::TimbreParamPtrs timbre[2];
    rm::GlobalParamPtrs globals;
    timbre[0].attach (host.apvts, 0);
    timbre[1].attach (host.apvts, 1);
    globals.attach (host.apvts);
    rm::SynthEngine engine;
    engine.setParameters (&timbre[0], &timbre[1], &globals);
    const double rate = 44100.0;
    engine.prepare (rate, 512);
    rm::NoteGenerator gen { seq, chords };
    gen.prepare (rate);
    juce::Random rnd (7);
    juce::AudioBuffer<float> buffer (2, 4096);

    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        presets.loadPreset (i);
        engine.reset();
        gen.reset();
        rm::NoteGenerator::Settings ns;
        ns.arpOn = rm::asBool (globals.arpOn);
        ns.arpLatch = rm::asBool (globals.arpLatch);
        ns.arpMode = rm::asInt (globals.arpMode);
        ns.arpOctaves = rm::asInt (globals.arpOct);
        ns.arpGate = rm::asFloat (globals.arpGate);
        ns.arpDivBeats = rm::choices::divisionBeats[rm::asInt (globals.arpDiv)];
        rm::HostTime time;
        time.bpm = 128.0;
        time.playing = true;
        time.hasPpq = true;
        int done = 0, firstBad = -1;
        double l = 0, r = 0;
        std::array<bool, 128> on {};
        while (done < (int) (12.0 * rate))
        {
            const int n = 1 + rnd.nextInt (rnd.nextBool() ? 4096 : 600);
            buffer.setSize (2, n, false, false, true);
            juce::MidiBuffer midi;
            for (int e = 0; e < 3; ++e)
                if (rnd.nextInt (8) == 0)
                {
                    const int note = 36 + rnd.nextInt (48);
                    const int pos = rnd.nextInt (n);
                    if (on[(size_t) note]) midi.addEvent (juce::MidiMessage::noteOff (1, note), pos);
                    else midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.3f + 0.7f * rnd.nextFloat()), pos);
                    on[(size_t) note] = ! on[(size_t) note];
                }
            gen.process (midi, n, ns, time);
            engine.process (buffer, midi, time.bpm);
            time.ppq += n / rate * time.bpm / 60.0;
            if (time.ppq > 16.0) time.ppq -= 16.0; // 4-bar loop
            for (int k = 0; k < n; ++k)
            {
                const float a = buffer.getSample (0, k), c = buffer.getSample (1, k);
                if ((! std::isfinite (a) || ! std::isfinite (c)) && firstBad < 0) firstBad = done + k;
                if (done > (int) (8.0 * rate)) { l += a * a; r += c * c; }
            }
            done += n;
        }
        const auto& p = presets.getPreset (i);
        const bool lopsided = (l > 1.0 && r < l * 0.05) || (r > 1.0 && l < r * 0.05);
        check (firstBad < 0, p.category + "/" + p.name + ": non-finite output at sample " + juce::String (firstBad));
        check (! lopsided, p.category + "/" + p.name + ": one channel dead L=" + juce::String (l) + " R=" + juce::String (r));
    }
    std::cout << "stress done" << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--stress")
    {
        stressAllPresets();
        std::cout << failures << " failure(s)" << std::endl;
        return failures == 0 ? 0 : 1;
    }
    if (argc > 1 && juce::String (argv[1]) == "--diag")
    {
        Rig rig;
        diagnoseSequences (rig);
        return 0;
    }
    testOscillator();
    testLadder();
    Rig rig;
    testParameterIds (rig);
    testInitPatch (rig);
    testAllPresets (rig);
    testNotesStop (rig);
    testPerformance (rig);

    std::cout << (failures == 0 ? "ALL TESTS PASSED" : juce::String (failures) + " FAILURE(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
