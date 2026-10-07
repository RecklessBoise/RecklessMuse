#include "PluginProcessor.h"
#include "PluginEditor.h"

RecklessMuseProcessor::RecklessMuseProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RecklessMuse", rm::createParameterLayout()),
      presets (apvts, sequence, chords)
{
    for (int t = 0; t < rm::kNumTimbres; ++t)
        timbreParams[t].attach (apvts, t);
    globalParams.attach (apvts);
    engine.setParameters (&timbreParams[0], &timbreParams[1], &globalParams);

    // Start on the first factory preset so the plug-in sounds great immediately.
    int startPreset = 0;
    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (presets.getPreset (i).name == "Model D Low End")
            startPreset = i;
    if (presets.getNumPresets() > 0)
        presets.loadPreset (startPreset);
}

RecklessMuseProcessor::~RecklessMuseProcessor()
{
    cancelPendingUpdate();
}

bool RecklessMuseProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RecklessMuseProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    noteGenerator.prepare (sampleRate);
    keyboardState.reset();
}

void RecklessMuseProcessor::releaseResources() {}

void RecklessMuseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // Host tempo / transport
    rm::HostTime host;
    host.bpm = rm::asFloat (globalParams.tempo);
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                if (*bpm > 1.0)
                    host.bpm = *bpm;
            host.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
            {
                host.hasPpq = true;
                host.ppq = *ppq;
            }
        }
    currentBpm = host.bpm;
    hostSynced = host.playing && host.hasPpq;

    // On-screen keyboard + wheels
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    const float bend = uiPitchBend.load();
    if (std::abs (bend - lastUiBend) > 1.0e-4f)
    {
        lastUiBend = bend;
        midi.addEvent (juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, (int) ((bend + 1.0f) * 8192.0f))), 0);
    }
    const float wheel = uiModWheel.load();
    if (std::abs (wheel - lastUiWheel) > 1.0e-4f)
    {
        lastUiWheel = wheel;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, juce::roundToInt (wheel * 127.0f)), 0);
    }
    // HOLD acts as a sustain pedal, except with the arp / sequencer where it latches them instead.
    const bool hold = rm::asBool (globalParams.kbHold) && ! rm::asBool (globalParams.arpOn)
                      && ! rm::asBool (globalParams.seqPlay);
    if (hold != lastHold)
    {
        lastHold = hold;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, hold ? 127 : 0), 0);
    }
    if (panicRequest.exchange (false))
    {
        engine.allNotesOff();
        noteGenerator.reset();
    }

    // Keyboard -> chord -> arp/seq
    rm::NoteGenerator::Settings ns;
    ns.kbOctave = rm::asInt (globalParams.kbOctave);
    ns.chordOn = rm::asBool (globalParams.chordOn);
    ns.chordLearn = rm::asBool (globalParams.chordLearn);
    ns.arpOn = rm::asBool (globalParams.arpOn);
    ns.arpLatch = rm::asBool (globalParams.arpLatch);
    ns.arpMode = rm::asInt (globalParams.arpMode);
    ns.arpOctaves = rm::asInt (globalParams.arpOct);
    ns.arpDivBeats = rm::choices::divisionBeats[juce::jlimit (0, 15, rm::asInt (globalParams.arpDiv))];
    ns.arpGate = rm::asFloat (globalParams.arpGate);
    ns.seqPlay = rm::asBool (globalParams.seqPlay);
    ns.seqRec = rm::asBool (globalParams.seqRec);
    ns.seqTranspose = rm::asBool (globalParams.seqTranspose);
    ns.seqLength = rm::asInt (globalParams.seqLength);
    ns.seqDivBeats = rm::choices::divisionBeats[juce::jlimit (0, 15, rm::asInt (globalParams.seqDiv))];
    ns.seqGate = rm::asFloat (globalParams.seqGate);
    ns.hold = rm::asBool (globalParams.kbHold);
    noteGenerator.process (midi, numSamples, ns, host);

    if (chords.learnFinished.load())
        triggerAsyncUpdate();

    engine.process (buffer, midi, host.bpm);
    activeVoices = engine.getActiveVoiceCount();
    midi.clear();
}

void RecklessMuseProcessor::handleAsyncUpdate()
{
    if (chords.learnFinished.exchange (false))
    {
        if (auto* learn = apvts.getParameter ("chordLearn"))
            learn->setValueNotifyingHost (0.0f);
        if (auto* on = apvts.getParameter ("chordOn"))
            on->setValueNotifyingHost (1.0f);
    }
}

void RecklessMuseProcessor::tapTempo()
{
    const auto now = juce::Time::currentTimeMillis();
    const auto delta = now - lastTapMs;
    lastTapMs = now;
    if (delta > 2000)
    {
        tapIntervals.clear();
        return;
    }
    tapIntervals.add ((double) delta);
    while (tapIntervals.size() > 4)
        tapIntervals.remove (0);
    double sum = 0.0;
    for (auto d : tapIntervals)
        sum += d;
    const double bpm = 60000.0 / (sum / tapIntervals.size());
    if (auto* p = apvts.getParameter ("tempo"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (30.0, 300.0, bpm)));
}

int RecklessMuseProcessor::getNumPrograms() { return std::max (1, presets.getNumPresets()); }
int RecklessMuseProcessor::getCurrentProgram() { return std::max (0, presets.getCurrentIndex()); }

void RecklessMuseProcessor::setCurrentProgram (int index)
{
    if (index != presets.getCurrentIndex())
        presets.loadPreset (index);
}

const juce::String RecklessMuseProcessor::getProgramName (int index)
{
    if (index < 0 || index >= presets.getNumPresets())
        return "Init";
    const auto& p = presets.getPreset (index);
    return p.category + ": " + p.name;
}

void RecklessMuseProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    xml->setAttribute ("presetName", presets.getCurrentName());
    xml->setAttribute ("editorScale", (double) editorScale.load());
    xml->setAttribute ("editTimbre", editTimbre.load());
    xml->setAttribute ("favoritesBank", presets.isFavoritesMode());

    // Sequence and chord memory travel with the session
    if (auto extra = presets.createPresetXml (presets.getCurrentName(), presets.getCurrentCategory()))
    {
        if (auto* seq = extra->getChildByName ("Sequence"))
            xml->addChildElement (new juce::XmlElement (*seq));
        if (auto* chord = extra->getChildByName ("Chord"))
            xml->addChildElement (new juce::XmlElement (*chord));
    }
    copyXmlToBinary (*xml, destData);
}

void RecklessMuseProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    editorScale = (float) xml->getDoubleAttribute ("editorScale", 0.85);
    editTimbre = xml->getIntAttribute ("editTimbre", 0);
    const auto presetName = xml->getStringAttribute ("presetName", "Init");

    // Rebuild a preset-like element for the sequence / chord data
    juce::XmlElement extra ("RecklessMusePreset");
    if (auto* seq = xml->getChildByName ("Sequence"))
        extra.addChildElement (new juce::XmlElement (*seq));
    if (auto* chord = xml->getChildByName ("Chord"))
        extra.addChildElement (new juce::XmlElement (*chord));
    xml->deleteAllChildElementsWithTagName ("Sequence");
    xml->deleteAllChildElementsWithTagName ("Chord");

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (auto* seq = extra.getChildByName ("Sequence"))
    {
        juce::StringArray notes, ons;
        notes.addTokens (seq->getStringAttribute ("notes"), ",", {});
        ons.addTokens (seq->getStringAttribute ("on"), ",", {});
        for (int i = 0; i < rm::kMaxSeqSteps && i < notes.size(); ++i)
        {
            sequence.note[(size_t) i] = juce::jlimit (0, 127, notes[i].getIntValue());
            sequence.on[(size_t) i] = i < ons.size() ? ons[i].getIntValue() != 0 : true;
        }
    }
    if (auto* chord = extra.getChildByName ("Chord"))
    {
        juce::StringArray iv;
        iv.addTokens (chord->getStringAttribute ("intervals"), ",", {});
        const int count = std::min (iv.size(), rm::ChordData::kMaxNotes);
        for (int i = 0; i < count; ++i)
            chords.intervals[(size_t) i] = iv[i].getIntValue();
        chords.count = count;
    }
    else
    {
        chords.count = 0;
    }
    presets.setCurrentByName (presetName);
    presets.setFavoritesMode (xml->getBoolAttribute ("favoritesBank", false));
}

juce::AudioProcessorEditor* RecklessMuseProcessor::createEditor()
{
    return new RecklessMuseEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessMuseProcessor();
}
