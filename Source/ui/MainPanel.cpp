#include "MainPanel.h"
#include "../PluginProcessor.h"

namespace rm::ui
{
namespace
{
constexpr int kRow1Y = 46, kRow1H = 290;
constexpr int kRow2Y = 342, kRow2H = 210;
constexpr int kPerfY = 560;
constexpr int kLeft = 26;
} // namespace

// ---------------------------------------------------------------------------
// Construction helpers
// ---------------------------------------------------------------------------
void MainPanel::addSection (const juce::String& title, juce::Rectangle<int> bounds)
{
    sections.push_back ({ title, bounds });
}

juce::Rectangle<int> MainPanel::section (const juce::String& title) const
{
    for (auto& s : sections)
        if (s.title == title)
            return s.bounds;
    jassertfalse;
    return {};
}

void MainPanel::place (juce::Component& c, juce::Rectangle<int> sec, int cx, int cy, int w, int h)
{
    c.setBounds (sec.getX() + cx - w / 2, sec.getY() + cy - h / 2, w, h);
}

Knob& MainPanel::knob (const juce::String& label, juce::Rectangle<int> sec, int cx, int cy, int diameter)
{
    auto& k = make<Knob> (label, Knob::Style::Rotary);
    const int w = diameter + 20, h = diameter + 13;
    k.setBounds (sec.getX() + cx - w / 2, sec.getY() + cy - diameter / 2, w, h);
    return k;
}

Knob& MainPanel::fader (const juce::String& label, juce::Rectangle<int> sec, int cx, int top, int height)
{
    auto& f = make<Knob> (label, Knob::Style::Fader);
    f.setBounds (sec.getX() + cx - 14, sec.getY() + top, 28, height);
    return f;
}

void MainPanel::timbreParam (Bindable& control, const juce::String& key)
{
    timbreBindings.emplace_back (&control, key);
    allBindables.push_back (&control);
    control.onTouch = [this] (const juce::String& n, const juce::String& v) { showParameter (n, v); };
}

void MainPanel::globalParam (Bindable& control, const juce::String& id)
{
    control.bind (state, id);
    allBindables.push_back (&control);
    control.onTouch = [this] (const juce::String& n, const juce::String& v) { showParameter (n, v); };
}

void MainPanel::bindTimbre (int timbre)
{
    editTimbre = timbre;
    processor.editTimbre = timbre;
    for (auto& [control, key] : timbreBindings)
        control->bind (state, tid (timbre, key));
    if (matrix != nullptr)
        matrix->bindTimbre (state, timbre);
    for (int t = 0; t < 2; ++t)
        if (timbreButtons[t] != nullptr)
            timbreButtons[t]->setLit (t == timbre);
    repaint();
}

void MainPanel::showParameter (const juce::String& name, const juce::String& value)
{
    if (display != nullptr)
        display->showParameter (name, value);
}

// ---------------------------------------------------------------------------
// MainPanel
// ---------------------------------------------------------------------------
MainPanel::MainPanel (RecklessMuseProcessor& p) : processor (p), state (p.apvts)
{
    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    // Section map -----------------------------------------------------------
    int x = kLeft;
    auto row1 = [&x] (int w) { auto r = juce::Rectangle<int> (x, kRow1Y, w, kRow1H); x += w; return r; };
    auto lfo = row1 (110);
    addSection ("LFO 1", lfo.withHeight (kRow1H / 2));
    addSection ("LFO 2", lfo.withTrimmedTop (kRow1H / 2));
    addSection ("MODULATION OSCILLATOR", row1 (168));
    auto osc = row1 (250);
    addSection ("OSCILLATOR 1", osc.withWidth (125));
    addSection ("OSCILLATOR 2", osc.withTrimmedLeft (125));
    auto mixer = row1 (178);
    addSection ("BRAND", mixer.withHeight (52));
    addSection ("MIXER", mixer.withTrimmedTop (52));
    auto filters = row1 (290);
    addSection ("FILTER 1  HIGH / LOW PASS", filters.withWidth (145));
    addSection ("FILTER 2  LOW PASS", filters.withTrimmedLeft (145));
    auto envs = row1 (196);
    addSection ("FILTER ENVELOPE", envs.withHeight (kRow1H / 2));
    addSection ("VCA ENVELOPE", envs.withTrimmedTop (kRow1H / 2));
    addSection ("VCA", row1 (88));
    addSection ("DIFFUSION DELAY", row1 (170));
    addSection ("OUTPUT", row1 (98));

    x = kLeft;
    auto row2 = [&x] (int w) { auto r = juce::Rectangle<int> (x, kRow2Y, w, kRow2H); x += w; return r; };
    addSection ("PITCH LFO", row2 (168));
    addSection ("ASSIGNABLE CONTROLLERS", row2 (150));
    addSection ("CLOCK", row2 (108));
    addSection ("ARPEGGIATOR", row2 (216));
    addSection ("SEQUENCER", row2 (164));
    addSection ("PROGRAMMER", row2 (420));
    addSection ("VOICE CONTROL", row2 (210));
    addSection ("CHORD", row2 (112));

    addSection ("PERFORMANCE", { kLeft, kPerfY, 186, kHeight - kPerfY - 6 });

    buildTopRow();
    buildBottomRow();
    buildPerformance();

    matrix = std::make_unique<MatrixOverlay>();
    matrix->onClose = [this]
    {
        matrix->setVisible (false);
        if (matrixButton != nullptr) matrixButton->setLit (false);
    };
    addChildComponent (*matrix);

    bindTimbre (juce::jlimit (0, 1, processor.editTimbre.load()));
    resized();
    timerCallback();
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    for (auto* b : allBindables)
        b->unbind();
    keyboard.reset();
    setLookAndFeel (nullptr);
}

void MainPanel::buildTopRow()
{
    // LFO 1 / LFO 2
    for (int i = 0; i < 2; ++i)
    {
        const auto s = section (i == 0 ? "LFO 1" : "LFO 2");
        const juce::String k = "l" + juce::String (i + 1);
        lfoRate[i] = &knob ("Rate", s, 30, 48, 34);
        timbreParam (*lfoRate[i], k + "Rate");
        lfoDiv[i] = &knob ("Rate (sync)", s, 30, 48, 34);
        timbreParam (*lfoDiv[i], k + "Div");
        timbreParam (knob ("Amplitude", s, 80, 48, 34), k + "Amp");
        auto& wave = make<Selector> ("Waveform");
        place (wave, s, 30, 112, 46, 34);
        timbreParam (wave, k + "Wave");
        auto& sync = make<LedSwitch> ("Sync");
        place (sync, s, 67, 113, 34, 38);
        timbreParam (sync, k + "Sync");
        auto& reset = make<LedSwitch> ("Key Rst");
        place (reset, s, 94, 113, 30, 38);
        timbreParam (reset, k + "Reset");
    }

    // Modulation oscillator
    {
        const auto s = section ("MODULATION OSCILLATOR");
        timbreParam (knob ("Frequency", s, 42, 54, 48), "moscFreq");
        auto& wave = make<Selector> ("Waveform");
        place (wave, s, 124, 44, 66, 36);
        timbreParam (wave, "moscWave");
        auto& kb = make<LedSwitch> ("KB Trk");
        place (kb, s, 96, 98, 30, 38);
        timbreParam (kb, "moscKbTrack");
        auto& uni = make<LedSwitch> ("Unipol");
        place (uni, s, 125, 98, 30, 38);
        timbreParam (uni, "moscUnipolar");
        auto& rst = make<LedSwitch> ("Key Rst");
        place (rst, s, 154, 98, 30, 38);
        timbreParam (rst, "moscKeyReset");

        timbreParam (knob ("Pitch Amt", s, 32, 154, 36), "moscPitchAmt");
        auto& pd = make<LedChoice> ("", juce::StringArray { "OSC 1", "OSC 2", "BOTH" }, true);
        place (pd, s, 82, 160, 44, 46);
        timbreParam (pd, "moscPitchDest");
        timbreParam (knob ("PWM Amt", s, 136, 154, 36), "moscPwmAmt");

        timbreParam (knob ("Filter Amt", s, 32, 232, 36), "moscFilterAmt");
        auto& fd = make<LedChoice> ("", juce::StringArray { "FILT 1", "FILT 2", "BOTH" }, true);
        place (fd, s, 82, 238, 44, 46);
        timbreParam (fd, "moscFilterDest");
        timbreParam (knob ("VCA Amt", s, 136, 232, 36), "moscVcaAmt");
    }

    // Oscillators
    for (int i = 0; i < 2; ++i)
    {
        const auto s = section (i == 0 ? "OSCILLATOR 1" : "OSCILLATOR 2");
        const juce::String k = "o" + juce::String (i + 1);
        timbreParam (knob ("Frequency", s, 62, 52, 48), k + "Freq");
        auto& oct = make<LedChoice> ("Octave", juce::StringArray { "32'", "16'", "8'", "4'", "2'" }, true);
        place (oct, s, 26, 148, 40, 88);
        timbreParam (oct, k + "Oct");
        timbreParam (knob ("Wave", s, 86, 132, 42), k + "Wave");
        if (i == 0)
        {
            timbreParam (knob ("FM Amount", s, 34, 236, 34), "fmAmt");
            auto& fmSrc = make<Selector> ("FM Source");
            place (fmSrc, s, 92, 240, 56, 34);
            timbreParam (fmSrc, "fmSrc");
        }
        else
        {
            auto& sync = make<LedSwitch> ("Sync 2>1");
            place (sync, s, 38, 240, 40, 38);
            timbreParam (sync, "o2Sync");
            auto& kb = make<LedSwitch> ("KB Track");
            place (kb, s, 88, 240, 40, 38);
            timbreParam (kb, "o2KbTrack");
        }
    }

    // Mixer
    {
        const auto s = section ("MIXER");
        const juce::StringArray labels { "Osc 1", "Ring\nMod", "Osc 2", "Mod\nOsc", "Noise", "Over\nLoad" };
        const juce::StringArray keys { "mixO1", "mixRing", "mixO2", "mixMod", "mixNoise", "mixOverload" };
        for (int i = 0; i < 6; ++i)
            timbreParam (fader (labels[i], s, 18 + (int) std::round (i * 28.4), 22, 210), keys[i]);
    }

    // Filters
    {
        const auto s = section ("FILTER 1  HIGH / LOW PASS");
        timbreParam (knob ("Cutoff", s, 50, 68, 60), "f1Cutoff");
        timbreParam (knob ("Resonance", s, 116, 50, 38), "f1Res");
        auto& mode = make<LedChoice> ("", juce::StringArray { "LOW", "HIGH" }, false);
        place (mode, s, 116, 108, 56, 24);
        timbreParam (mode, "f1Mode");
        auto& kb = make<LedChoice> ("KB Tracking", juce::StringArray { "OFF", "1/2", "1" }, false);
        place (kb, s, 50, 146, 76, 38);
        timbreParam (kb, "f1Kb");
        timbreParam (knob ("Env Amount", s, 50, 212, 40), "f1Env");
        auto& order = make<LedChoice> ("Order", juce::StringArray { "6 dB", "12 dB", "18 dB", "24 dB" }, true);
        place (order, s, 116, 214, 50, 78);
        timbreParam (order, "fOrder");
    }
    {
        const auto s = section ("FILTER 2  LOW PASS");
        timbreParam (knob ("Cutoff", s, 95, 68, 60), "f2Cutoff");
        timbreParam (knob ("Resonance", s, 29, 50, 38), "f2Res");
        auto& link = make<LedSwitch> ("Link");
        place (link, s, 29, 112, 34, 38);
        timbreParam (link, "fLink");
        auto& kb = make<LedChoice> ("KB Tracking", juce::StringArray { "OFF", "1/2", "1" }, false);
        place (kb, s, 95, 146, 76, 38);
        timbreParam (kb, "f2Kb");
        timbreParam (knob ("Env Amount", s, 95, 212, 40), "f2Env");
        auto& routing = make<LedChoice> ("Routing", juce::StringArray { "SERIES", "PARALLEL", "STEREO" }, true);
        place (routing, s, 32, 214, 58, 66);
        timbreParam (routing, "fRouting");
    }

    // Envelopes
    for (int i = 0; i < 2; ++i)
    {
        const auto s = section (i == 0 ? "FILTER ENVELOPE" : "VCA ENVELOPE");
        const juce::String k = i == 0 ? "fe" : "ae";
        const juce::StringArray labels { "Attack", "Decay", "Sustain", "Release" };
        const juce::StringArray keys { "A", "D", "S", "R" };
        for (int j = 0; j < 4; ++j)
            timbreParam (fader (labels[j], s, 22 + j * 32, 20, 120), k + keys[j]);
        auto& loop = make<LedSwitch> ("Loop");
        place (loop, s, 162, 44, 34, 38);
        timbreParam (loop, k + "Loop");
        timbreParam (knob ("Velocity", s, 162, 100, 30), k + "Vel");
    }

    // VCA
    {
        const auto s = section ("VCA");
        timbreParam (knob ("VCA Level", s, 44, 56, 44), "vcaLevel");
        timbreParam (knob ("Pan", s, 44, 144, 34), "vcaPan");
        timbreParam (knob ("Pan Spread", s, 44, 226, 34), "vcaSpread");
    }

    // Diffusion delay (global)
    {
        const auto s = section ("DIFFUSION DELAY");
        delayTime[0] = &knob ("Time L", s, 36, 54, 40);
        globalParam (*delayTime[0], "dlyTimeL");
        delayDiv[0] = &knob ("Time L", s, 36, 54, 40);
        globalParam (*delayDiv[0], "dlyDivL");
        delayTime[1] = &knob ("Time R", s, 134, 54, 40);
        globalParam (*delayTime[1], "dlyTimeR");
        delayDiv[1] = &knob ("Time R", s, 134, 54, 40);
        globalParam (*delayDiv[1], "dlyDivR");
        auto& sync = make<LedSwitch> ("Clock Sync");
        place (sync, s, 85, 50, 44, 38);
        globalParam (sync, "dlySync");
        auto& on = make<KeyCap> ("On", colours::orange);
        place (on, s, 85, 106, 30, 40);
        globalParam (on, "dlyOn");
        globalParam (knob ("Feedback", s, 36, 140, 36), "dlyFb");
        globalParam (knob ("Character", s, 134, 140, 36), "dlyChar");
        globalParam (knob ("Diffusion", s, 36, 222, 36), "dlyDiff");
        globalParam (knob ("Mix", s, 134, 222, 36), "dlyMix");
    }

    // Output (global)
    {
        const auto s = section ("OUTPUT");
        globalParam (knob ("Main Out", s, 49, 56, 46), "masterVol");
        globalParam (knob ("Vintage", s, 49, 144, 36), "vintage");
        auto& q = make<LedChoice> ("Quality", juce::StringArray { "ECO", "HIGH" }, false);
        place (q, s, 49, 240, 70, 38);
        globalParam (q, "quality");
    }
}

void MainPanel::buildBottomRow()
{
    // Pitch LFO
    {
        const auto s = section ("PITCH LFO");
        timbreParam (knob ("Rate", s, 32, 52, 38), "plRate");
        auto& shape = make<Selector> ("Shape");
        place (shape, s, 84, 52, 52, 34);
        timbreParam (shape, "plWave");
        timbreParam (knob ("Amount", s, 136, 52, 38), "plAmt");
        auto& dest = make<LedChoice> ("Destination", juce::StringArray { "OSC 1", "OSC 2", "BOTH" }, false);
        place (dest, s, 60, 150, 104, 38);
        timbreParam (dest, "plDest");
        auto& wheel = make<LedSwitch> ("Mod Wheel");
        place (wheel, s, 138, 151, 44, 38);
        timbreParam (wheel, "plWheel");
    }

    // Assignable controllers
    {
        const auto s = section ("ASSIGNABLE CONTROLLERS");
        globalParam (knob ("Macro 1", s, 40, 56, 44), "macro1");
        globalParam (knob ("Macro 2", s, 110, 56, 44), "macro2");
        auto& mm = make<KeyCap> ("", colours::orange, false);
        mm.setCapText ("MOD MATRIX");
        mm.setLabelBelow (false);
        place (mm, s, 75, 150, 112, 30);
        mm.onClick = [this, &mm]
        {
            const bool show = ! matrix->isVisible();
            matrix->setVisible (show);
            mm.setLit (show);
            if (show) matrix->toFront (false);
        };
        matrixButton = &mm;
    }

    // Clock
    {
        const auto s = section ("CLOCK");
        globalParam (knob ("Tempo", s, 54, 56, 46), "tempo");
        auto& tap = make<KeyCap> ("Tap Tempo", colours::ivory, false);
        place (tap, s, 54, 152, 44, 44);
        tap.onClick = [this] { processor.tapTempo(); };
    }

    // Arpeggiator
    {
        const auto s = section ("ARPEGGIATOR");
        globalParam (knob ("Clock Div", s, 34, 52, 38), "arpDiv");
        auto& on = make<KeyCap> ("On", colours::yellow);
        place (on, s, 84, 54, 34, 44);
        globalParam (on, "arpOn");
        auto& latch = make<KeyCap> ("Latch", colours::ivory);
        place (latch, s, 126, 54, 34, 44);
        globalParam (latch, "arpLatch");
        auto& mode = make<LedChoice> ("Mode", juce::StringArray { "UP", "DOWN", "UP/DN", "ORDER", "RAND" }, true);
        place (mode, s, 182, 82, 56, 108);
        globalParam (mode, "arpMode");
        globalParam (knob ("Gate", s, 34, 146, 34), "arpGate");
        auto& oct = make<LedChoice> ("Octave Range", juce::StringArray { "1", "2", "3", "4" }, false);
        place (oct, s, 104, 158, 80, 38);
        globalParam (oct, "arpOct");
    }

    // Sequencer
    {
        const auto s = section ("SEQUENCER");
        globalParam (knob ("Clock Div", s, 28, 50, 32), "seqDiv");
        globalParam (knob ("Length", s, 82, 50, 32), "seqLength");
        globalParam (knob ("Gate", s, 136, 50, 32), "seqGate");
        auto& play = make<KeyCap> ("Play", colours::orange);
        place (play, s, 23, 128, 32, 44);
        globalParam (play, "seqPlay");
        auto& rec = make<KeyCap> ("Rec", juce::Colour (0xffe8502c));
        place (rec, s, 62, 128, 32, 44);
        globalParam (rec, "seqRec");
        auto& adv = make<KeyCap> ("Adv", colours::ivory, false);
        place (adv, s, 101, 128, 32, 44);
        adv.onClick = [this] { processor.sequence.advanceRequest = true; };
        auto& reset = make<KeyCap> ("Reset", colours::ivory, false);
        place (reset, s, 140, 128, 32, 44);
        reset.onClick = [this] { processor.sequence.resetRequest = true; };
        auto& transpose = make<LedSwitch> ("Key Transpose");
        place (transpose, s, 82, 184, 70, 36);
        globalParam (transpose, "seqTranspose");
    }

    // Programmer
    {
        const auto s = section ("PROGRAMMER");
        display = &make<Display>();
        display->setBounds (s.getX() + 66, s.getY() + 22, 286, 82);

        auto& prev = make<KeyCap> ("", colours::ivory, false);
        prev.setCapText ("<");
        prev.setLabelBelow (false);
        place (prev, s, 32, 44, 40, 30);
        prev.onClick = [this] { processor.presets.next (-1); };
        auto& next = make<KeyCap> ("", colours::ivory, false);
        next.setCapText (">");
        next.setLabelBelow (false);
        place (next, s, 32, 84, 40, 30);
        next.onClick = [this] { processor.presets.next (1); };

        const juce::StringArray actions { "BANK", "SAVE", "INIT" };
        for (int i = 0; i < 3; ++i)
        {
            auto& b = make<KeyCap> ("", colours::ivory, false);
            b.setCapText (actions[i]);
            b.setLabelBelow (false);
            place (b, s, 388, 34 + i * 28, 50, 22);
            if (i == 0) b.onClick = [this] { processor.presets.nextCategory (1); };
            if (i == 1) b.onClick = [this] { savePresetDialog(); };
            if (i == 2) b.onClick = [this] { processor.presets.loadInit(); };
        }

        // Step pages and step note value
        for (int i = 0; i < 4; ++i)
        {
            auto& b = make<KeyCap> ("", colours::ivory, false);
            b.setCapText (juce::String (i * 16 + 1) + "-" + juce::String (i * 16 + 16));
            b.setLabelBelow (false);
            place (b, s, 92 + i * 52, 124, 46, 18);
            b.onClick = [this, i] { stepPage = i; selectStep (i * 16); };
            pageButtons[(size_t) i] = &b;
        }
        stepValue = &make<Knob> ("Step Note", Knob::Style::Rotary);
        stepValue->setBounds (s.getX() + 318, s.getY() + 106, 44, 41);
        stepValue->slider.setRange (0.0, 127.0, 1.0);
        stepValue->slider.setMouseDragSensitivity (400);
        stepValue->slider.onValueChange = [this]
        {
            const int note = (int) stepValue->slider.getValue();
            processor.sequence.note[(size_t) selectedStep] = note;
            showParameter ("Step " + juce::String (selectedStep + 1), juce::MidiMessage::getMidiNoteName (note, true, true, 3));
        };

        for (int i = 0; i < 16; ++i)
        {
            auto& b = make<KeyCap> (juce::String (i + 1), colours::ivory, true);
            b.setBounds (s.getX() + 14 + (int) std::round (i * 25.5), s.getY() + 150, 22, 46);
            b.onClick = [this, i]
            {
                const int step = stepPage * 16 + i;
                if (selectedStep == step)
                    processor.sequence.on[(size_t) step] = ! processor.sequence.on[(size_t) step].load();
                selectStep (step);
            };
            stepButtons[(size_t) i] = &b;
        }
    }

    // Voice control
    {
        const auto s = section ("VOICE CONTROL");
        for (int t = 0; t < 2; ++t)
        {
            auto& b = make<KeyCap> (t == 0 ? "Edit A" : "Edit B", colours::cyan, false);
            b.setCapText (t == 0 ? "A" : "B");
            place (b, s, 26 + t * 40, 46, 34, 46);
            b.onClick = [this, t] { bindTimbre (t); };
            timbreButtons[t] = &b;
        }
        auto& tm = make<LedChoice> ("Timbres", juce::StringArray { "SINGLE", "SPLIT", "STACK" }, false);
        place (tm, s, 152, 46, 104, 38);
        globalParam (tm, "timbreMode");

        auto& vm = make<LedChoice> ("Voice Mode", juce::StringArray { "POLY", "MONO", "UNISON" }, false);
        place (vm, s, 56, 118, 104, 38);
        timbreParam (vm, "polyMode");
        timbreParam (knob ("Voices", s, 132, 104, 30), "uniVoices");
        timbreParam (knob ("Detune", s, 182, 104, 30), "uniDetune");

        auto& split = knob ("Split Point", s, 32, 160, 28);
        globalParam (split, "splitNote");
        auto& prio = make<Selector> ("Note Prio");
        place (prio, s, 96, 174, 52, 32);
        timbreParam (prio, "notePrio");
        timbreParam (knob ("Bend Range", s, 164, 160, 28), "bendRange");
    }

    // Chord
    {
        const auto s = section ("CHORD");
        auto& on = make<KeyCap> ("On", colours::yellow);
        place (on, s, 32, 50, 34, 44);
        globalParam (on, "chordOn");
        auto& learn = make<KeyCap> ("Learn", colours::ivory);
        place (learn, s, 80, 50, 34, 44);
        globalParam (learn, "chordLearn");
    }
}

void MainPanel::buildPerformance()
{
    const auto s = section ("PERFORMANCE");

    timbreParam (knob ("Glide", s, 34, 22, 34), "glide");
    auto& legato = make<LedSwitch> ("Legato");
    place (legato, s, 34, 84, 40, 38);
    timbreParam (legato, "glideLegato");

    auto& down = make<KeyCap> ("Oct -", colours::ivory, false);
    place (down, s, 92, 26, 30, 40);
    down.onClick = [this]
    {
        if (auto* p = state.getParameter ("kbOctave"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jmax (-2, kbOctave - 1)));
    };
    auto& up = make<KeyCap> ("Oct +", colours::ivory, false);
    place (up, s, 126, 26, 30, 40);
    up.onClick = [this]
    {
        if (auto* p = state.getParameter ("kbOctave"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jmin (2, kbOctave + 1)));
    };
    auto& hold = make<KeyCap> ("Hold", colours::ivory);
    place (hold, s, 162, 26, 30, 40);
    globalParam (hold, "kbHold");

    auto& panic = make<KeyCap> ("", colours::ivory, false);
    panic.setCapText ("PANIC");
    panic.setLabelBelow (false);
    place (panic, s, 34, 160, 50, 20);
    panic.onClick = [this] { processor.panicRequest = true; };

    pitchWheel = &make<Wheel> ("Pitch", true);
    pitchWheel->setBounds (s.getX() + 80, s.getY() + 62, 34, 146);
    pitchWheel->onChange = [this] (float v) { processor.uiPitchBend = v; };
    modWheel = &make<Wheel> ("Mod", false);
    modWheel->setBounds (s.getX() + 128, s.getY() + 62, 34, 146);
    modWheel->onChange = [this] (float v) { processor.uiModWheel = v; };
    modWheel->setValue (processor.uiModWheel.load(), false);

    keyboard = std::make_unique<juce::MidiKeyboardComponent> (processor.keyboardState,
                                                              juce::MidiKeyboardComponent::horizontalKeyboard);
    keyboard->setAvailableRange (36, 96);
    keyboard->setOctaveForMiddleC (4);
    keyboard->setScrollButtonsVisible (false);
    keyboard->setWantsKeyboardFocus (false);
    keyboard->setColour (juce::MidiKeyboardComponent::whiteNoteColourId, colours::ivory);
    keyboard->setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff151516));
    keyboard->setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff8d8a83));
    keyboard->setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::orange.withAlpha (0.55f));
    keyboard->setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::orange.withAlpha (0.18f));
    keyboard->setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.5f));
    addAndMakeVisible (*keyboard);

    sizeButton = &make<KeyCap> ("", colours::panelLight, false);
    sizeButton->setCapText ("SIZE");
    sizeButton->setLabelBelow (false);
    sizeButton->setBounds (kWidth - 96, 10, 56, 20);
    sizeButton->onClick = [this] { if (onSizeMenu) onSizeMenu (*sizeButton); };
}

void MainPanel::resized()
{
    if (keyboard != nullptr)
    {
        const auto area = juce::Rectangle<int> (kLeft + 192, kPerfY + 4, kWidth - 2 * kLeft - 192, kHeight - kPerfY - 10);
        keyboard->setBounds (area);
        keyboard->setKeyWidth ((float) area.getWidth() / 36.0f);
    }
    if (matrix != nullptr)
        matrix->setBounds (kLeft + 120, kRow1Y + 6, 1300, kRow1H - 12);
}

void MainPanel::selectStep (int step)
{
    selectedStep = juce::jlimit (0, kMaxSeqSteps - 1, step);
    stepPage = selectedStep / 16;
    const int note = processor.sequence.note[(size_t) selectedStep].load();
    stepValue->slider.setValue (note, juce::dontSendNotification);
    showParameter ("Step " + juce::String (selectedStep + 1) + (processor.sequence.on[(size_t) selectedStep] ? "" : " (rest)"),
                   juce::MidiMessage::getMidiNoteName (note, true, true, 3));
}

void MainPanel::savePresetDialog()
{
    auto* window = new juce::AlertWindow ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("name", processor.presets.getCurrentName());
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainPanel> safe (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safe, window] (int result)
    {
        if (safe != nullptr && result == 1)
        {
            const auto name = window->getTextEditorContents ("name");
            if (! safe->processor.presets.saveUserPreset (name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                        "Could not write to " + PresetManager::getUserPresetFolder().getFullPathName());
        }
    }), true);
}

void MainPanel::mouseDown (const juce::MouseEvent&) {}

void MainPanel::timerCallback()
{
    auto& pm = processor.presets;
    if (display != nullptr)
    {
        display->setPreset (pm.getCurrentCategory(), pm.getCurrentName(), pm.getCurrentIndex(), pm.getNumPresets());
        const int mode = asInt (state.getRawParameterValue ("timbreMode"));
        const juce::String timbre = juce::String ("EDIT ") + (editTimbre == 0 ? "A" : "B") + "  "
                                    + choices::timbreMode[mode].toUpperCase();
        const juce::String right = juce::String (processor.currentBpm.load(), 1) + (processor.hostSynced ? " BPM HOST" : " BPM")
                                   + "  V" + juce::String (processor.activeVoices.load());
        display->setStatus (timbre, right);
    }

    // Tempo-synced knobs swap in place
    for (int i = 0; i < 2; ++i)
    {
        const bool sync = asBool (state.getRawParameterValue (tid (editTimbre, "l" + juce::String (i + 1) + "Sync")));
        lfoRate[i]->setVisible (! sync);
        lfoDiv[i]->setVisible (sync);
    }
    const bool dSync = asBool (state.getRawParameterValue ("dlySync"));
    for (int i = 0; i < 2; ++i)
    {
        delayTime[i]->setVisible (! dSync);
        delayDiv[i]->setVisible (dSync);
    }

    // Sequencer step lights
    const int playIndex = processor.sequence.playIndex.load();
    const int length = asInt (state.getRawParameterValue ("seqLength"));
    for (int i = 0; i < 16; ++i)
    {
        const int step = stepPage * 16 + i;
        auto* b = stepButtons[(size_t) i];
        b->setLit (processor.sequence.on[(size_t) step].load() && step < length);
        b->setCapColour (step == playIndex ? colours::orange : (step == selectedStep ? juce::Colour (0xffd8e4f0) : colours::ivory));
        b->setAlpha (step < length ? 1.0f : 0.45f);
    }
    for (int i = 0; i < 4; ++i)
        pageButtons[(size_t) i]->setLit (i == stepPage);

    // Clock LED, chord memory, keyboard octave
    const double bpm = std::max (1.0, processor.currentBpm.load());
    const auto ms = juce::Time::getMillisecondCounterHiRes();
    const bool led = std::fmod (ms / (60000.0 / bpm), 1.0) < 0.2;
    juce::String chord;
    const int count = processor.chords.count.load();
    for (int i = 0; i < count; ++i)
        chord << (i > 0 ? " " : "") << processor.chords.intervals[(size_t) i].load();
    const int oct = asInt (state.getRawParameterValue ("kbOctave"));
    if (led != clockLedOn || chord != chordText || oct != kbOctave)
    {
        clockLedOn = led;
        chordText = chord;
        kbOctave = oct;
        repaint (section ("CLOCK"));
        repaint (section ("CHORD"));
        repaint (section ("PERFORMANCE"));
    }
    if (processor.editTimbre.load() != editTimbre)
        bindTimbre (processor.editTimbre.load());
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
void MainPanel::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();

    // Wooden end cheeks
    auto drawWood = [&g] (juce::Rectangle<float> r)
    {
        juce::ColourGradient wood (colours::woodLight, r.getX(), r.getY(), colours::woodDark, r.getRight(), r.getY(), false);
        wood.addColour (0.5, colours::woodLight.darker (0.15f));
        g.setGradientFill (wood);
        g.fillRect (r);
        juce::Random rng (42);
        for (int i = 0; i < 26; ++i)
        {
            const float x = r.getX() + rng.nextFloat() * r.getWidth();
            g.setColour (colours::woodDark.withAlpha (0.25f + 0.25f * rng.nextFloat()));
            g.drawLine (x, r.getY(), x + rng.nextFloat() * 3.0f - 1.5f, r.getBottom(), 0.7f + rng.nextFloat());
        }
    };
    drawWood ({ 0.0f, 0.0f, 22.0f, full.getHeight() });
    drawWood ({ full.getWidth() - 22.0f, 0.0f, 22.0f, full.getHeight() });

    // Panel
    auto panel = full.reduced (22.0f, 0.0f);
    juce::ColourGradient grad (colours::panelLight, 0.0f, 0.0f, colours::panel, 0.0f, (float) kPerfY, false);
    g.setGradientFill (grad);
    g.fillRect (panel);

    // Top bar with speaker grilles and wordmark
    auto top = panel.removeFromTop (40.0f);
    g.setColour (colours::panelDark);
    g.fillRect (top);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    for (int i = 0; i < 46; ++i)
    {
        const float gx = top.getX() + 210.0f + (float) i * 5.0f;
        g.fillRect (gx, top.getY() + 12.0f, 2.0f, 16.0f);
        g.fillRect (top.getRight() - 230.0f - (float) i * 5.0f, top.getY() + 12.0f, 2.0f, 16.0f);
    }
    g.setColour (colours::text);
    g.setFont (labelFont (26.0f, false).withExtraKerningFactor (0.08f));
    g.drawText ("reckless", top.withWidth (top.getWidth() * 0.5f - 6.0f), juce::Justification::centredRight);
    g.setFont (labelFont (26.0f, true).withExtraKerningFactor (0.08f));
    g.drawText ("muse", top.withTrimmedLeft (top.getWidth() * 0.5f + 6.0f), juce::Justification::centredLeft);
    g.setFont (labelFont (9.0f));
    g.setColour (colours::textDim);
    g.drawText ("POWER", juce::Rectangle<float> (top.getX() + 14.0f, top.getY() + 26.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
    g.setColour (colours::led);
    g.fillEllipse (top.getX() + 24.0f, top.getY() + 12.0f, 7.0f, 7.0f);

    // Sections
    for (auto& s : sections)
    {
        if (s.title == "BRAND")
        {
            auto r = s.bounds.toFloat().reduced (8.0f, 6.0f);
            g.setColour (colours::text);
            g.setFont (labelFont (25.0f, false).withExtraKerningFactor (0.18f));
            g.drawText ("MUSE", r.removeFromLeft (84.0f), juce::Justification::centredLeft);
            g.setFont (labelFont (8.5f));
            g.drawFittedText ("RECKLESS\n8-VOICE BITIMBRAL\nANALOG MODELED SYNTH", r.toNearestInt(), juce::Justification::centredLeft, 3, 0.8f);
            g.setColour (colours::line);
            g.drawRect (s.bounds.toFloat(), 1.0f);
            continue;
        }
        g.setColour (colours::line);
        g.drawRect (s.bounds.toFloat(), 1.0f);
        if (s.title != "PERFORMANCE")
        {
            g.setColour (colours::text);
            g.setFont (labelFont (10.0f));
            g.drawText (s.title, s.bounds.withHeight (16).reduced (4, 0), juce::Justification::centred);
        }
    }

    // Waveform glyphs around the oscillator WAVE knobs (triangle, saw, square, pulse)
    for (auto* name : { "OSCILLATOR 1", "OSCILLATOR 2" })
    {
        const auto s = section (name);
        const juce::Point<float> c ((float) s.getX() + 86.0f, (float) s.getY() + 132.0f + 21.0f);
        const float angles[] { -140.0f, -47.0f, 47.0f, 140.0f };
        for (int i = 0; i < 4; ++i)
        {
            const float a = juce::degreesToRadians (angles[i]);
            const juce::Point<float> p (c.x + 33.0f * std::sin (a), c.y - 33.0f * std::cos (a));
            juce::Path glyph;
            const float w = 5.0f, h = 3.5f;
            if (i == 0) { glyph.startNewSubPath (p.x - w, p.y + h); glyph.lineTo (p.x - w * 0.5f, p.y - h); glyph.lineTo (p.x, p.y + h); glyph.lineTo (p.x + w * 0.5f, p.y - h); glyph.lineTo (p.x + w, p.y + h); }
            if (i == 1) { glyph.startNewSubPath (p.x - w, p.y + h); glyph.lineTo (p.x, p.y - h); glyph.lineTo (p.x, p.y + h); glyph.lineTo (p.x + w, p.y - h); glyph.lineTo (p.x + w, p.y + h); }
            if (i == 2) { glyph.startNewSubPath (p.x - w, p.y + h); glyph.lineTo (p.x - w, p.y - h); glyph.lineTo (p.x, p.y - h); glyph.lineTo (p.x, p.y + h); glyph.lineTo (p.x + w, p.y + h); glyph.lineTo (p.x + w, p.y - h); }
            if (i == 3) { glyph.startNewSubPath (p.x - w, p.y + h); glyph.lineTo (p.x - w, p.y - h); glyph.lineTo (p.x - w * 0.6f, p.y - h); glyph.lineTo (p.x - w * 0.6f, p.y + h); glyph.lineTo (p.x + w, p.y + h); glyph.lineTo (p.x + w, p.y - h); }
            g.setColour (colours::text);
            g.strokePath (glyph, juce::PathStrokeType (1.0f));
        }
    }

    // Programmer framing & labels
    {
        const auto s = section ("PROGRAMMER");
        g.setColour (colours::textDim);
        g.setFont (labelFont (9.0f));
        g.drawText ("PRESET", s.getX() + 12, s.getY() + 104, 42, 12, juce::Justification::centred);
        g.drawText ("STEP PAGE", s.getX() + 14, s.getY() + 118, 52, 12, juce::Justification::centredLeft);
    }

    // Clock LED
    {
        const auto s = section ("CLOCK");
        const juce::Point<float> c ((float) s.getRight() - 14.0f, (float) s.getY() + 26.0f);
        g.setColour (clockLedOn ? colours::led : colours::ledOff);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));
    }

    // Chord memory read-out
    {
        const auto s = section ("CHORD");
        auto r = juce::Rectangle<int> (s.getX() + 10, s.getY() + 104, s.getWidth() - 20, 40);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        g.setColour (colours::led);
        g.setFont (labelFont (12.0f));
        g.drawFittedText (chordText.isEmpty() ? juce::String ("NO CHORD") : chordText, r.reduced (4), juce::Justification::centred, 2);
        g.setColour (colours::textDim);
        g.setFont (labelFont (8.5f, false));
        g.drawFittedText ("Hold LEARN, play a chord,\nrelease all keys.", juce::Rectangle<int> (s.getX() + 4, s.getY() + 150, s.getWidth() - 8, 40),
                          juce::Justification::centred, 3);
    }

    // Keyboard octave LEDs
    {
        const auto s = section ("PERFORMANCE");
        for (int i = -2; i <= 2; ++i)
        {
            const juce::Point<float> c ((float) s.getX() + 109.0f + (float) i * 7.0f, (float) s.getY() + 52.0f);
            g.setColour (i == kbOctave ? colours::led : colours::ledOff);
            g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (c));
        }
    }

    // Shadow line above the keyboard
    g.setColour (juce::Colours::black);
    g.fillRect (22.0f, (float) kPerfY - 2.0f, full.getWidth() - 44.0f, 3.0f);
}
} // namespace rm::ui
