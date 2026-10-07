#include "NoteGenerator.h"

namespace rm
{
// ---------------------------------------------------------------------------
// StepClock
// ---------------------------------------------------------------------------
void NoteGenerator::StepClock::begin (const HostTime& h, double div, double sr)
{
    divBeats = div;
    const double bpm = std::max (1.0, h.bpm);
    stepSamples = std::max (1.0, div * 60.0 / bpm * sr);
    const bool host = h.playing && h.hasPpq;
    if (host != hostMode)
        restart();
    hostMode = host;
    ppqStart = h.ppq;
    ppqPerSample = bpm / 60.0 / sr;
}

bool NoteGenerator::StepClock::due (int t)
{
    if (hostMode)
    {
        const auto idx = (int64_t) std::floor ((ppqStart + t * ppqPerSample) / divBeats + 1.0e-7);
        if (idx != lastIndex)
        {
            lastIndex = idx;
            return true;
        }
        return false;
    }

    if (samplesToNext <= 0.0)
    {
        samplesToNext += stepSamples;
        if (samplesToNext <= 0.0) samplesToNext = stepSamples;
        ++lastIndex;
        return true;
    }
    return false;
}

int NoteGenerator::StepClock::samplesUntilNext (int t) const
{
    if (hostMode)
    {
        const double ppqNow = ppqStart + t * ppqPerSample;
        const double next = (double) (lastIndex + 1) * divBeats;
        return std::max (1, (int) std::ceil ((next - ppqNow) / ppqPerSample));
    }
    return std::max (1, (int) std::ceil (samplesToNext));
}

// ---------------------------------------------------------------------------
// NoteGenerator
// ---------------------------------------------------------------------------
void NoteGenerator::prepare (double sr)
{
    sampleRate = sr;
    output.ensureSize (8192);
    reset();
}

void NoteGenerator::reset()
{
    generatedCount.fill (0);
    directOn.fill (false);
    numArpNotes = physicalHeld = 0;
    arpIndex = -1;
    arpPlaying = seqPlaying = -1;
    numSeqHeld = seqPhysical = 0;
    seqIndex = -1;
    numLearn = learnHeld = 0;
    wasHostPlaying = false;
    arpClock.restart();
    seqClock.restart();
}

void NoteGenerator::emit (bool isOn, int note, float velocity, int t)
{
    if (note < 0 || note > 127)
        return;
    if (isOn)
        output.addEvent (juce::MidiMessage::noteOn (1, note, juce::jlimit (0.01f, 1.0f, velocity)), t);
    else
        output.addEvent (juce::MidiMessage::noteOff (1, note), t);
}

void NoteGenerator::releaseDirectNotes (int t)
{
    for (int n = 0; n < 128; ++n)
        if (directOn[(size_t) n])
        {
            emit (false, n, 0.0f, t);
            directOn[(size_t) n] = false;
        }
}

// Transport stop / All Notes Off: nothing generated may keep sounding.
void NoteGenerator::stopEverything (int t)
{
    stopArpNote (t);
    stopSeqNote (t);
    releaseDirectNotes (t);
    numArpNotes = physicalHeld = 0;
    numSeqHeld = seqPhysical = 0;
    arpIndex = seqIndex = -1;
    generatedCount.fill (0);
    sequence.playIndex = -1;
    arpClock.restart();
    seqClock.restart();
}

void NoteGenerator::stopArpNote (int t)
{
    if (arpPlaying >= 0)
        emit (false, arpPlaying, 0.0f, t);
    arpPlaying = -1;
    arpGateLeft = 0;
}

void NoteGenerator::stopSeqNote (int t)
{
    if (seqPlaying >= 0)
        emit (false, seqPlaying, 0.0f, t);
    seqPlaying = -1;
    seqGateLeft = 0;
}

void NoteGenerator::routeNote (bool isOn, int note, float velocity, int t, const Settings& s)
{
    if (s.seqPlay)
    {
        // The sequencer plays while a key is held (or latched); the key transposes it.
        if (isOn)
        {
            if (latched (s) && seqPhysical == 0)
                numSeqHeld = 0; // a fresh key replaces the latched one
            ++seqPhysical;
            if (numSeqHeld == 0)
            {
                seqClock.restart();
                seqIndex = -1;
            }
            if (numSeqHeld < (int) seqHeld.size())
                seqHeld[(size_t) numSeqHeld++] = note;
        }
        else
        {
            seqPhysical = std::max (0, seqPhysical - 1);
            if (! latched (s))
            {
                for (int i = 0; i < numSeqHeld; ++i)
                    if (seqHeld[(size_t) i] == note)
                    {
                        for (int j = i; j < numSeqHeld - 1; ++j)
                            seqHeld[(size_t) j] = seqHeld[(size_t) j + 1];
                        --numSeqHeld;
                        break;
                    }
                if (numSeqHeld == 0)
                {
                    stopSeqNote (t);
                    sequence.playIndex = -1;
                }
            }
        }
        return;
    }

    if (s.arpOn)
    {
        if (isOn)
        {
            if (latched (s) && physicalHeld == 0)
                numArpNotes = 0; // a fresh chord replaces the latched one
            ++physicalHeld;
            bool present = false;
            for (int i = 0; i < numArpNotes; ++i)
                present |= arpNotes[(size_t) i].note == note;
            if (! present && numArpNotes < (int) arpNotes.size())
            {
                if (numArpNotes == 0)
                {
                    arpClock.restart();
                    arpIndex = -1;
                }
                arpNotes[(size_t) numArpNotes++] = { note, velocity };
            }
        }
        else
        {
            physicalHeld = std::max (0, physicalHeld - 1);
            if (! latched (s))
            {
                for (int i = 0; i < numArpNotes; ++i)
                    if (arpNotes[(size_t) i].note == note)
                    {
                        for (int j = i; j < numArpNotes - 1; ++j)
                            arpNotes[(size_t) j] = arpNotes[(size_t) j + 1];
                        --numArpNotes;
                        break;
                    }
                if (numArpNotes == 0)
                    stopArpNote (t);
            }
        }
        return;
    }

    if (s.seqRec && isOn)
    {
        int idx = sequence.recordIndex.load();
        if (idx >= s.seqLength) idx = 0;
        sequence.note[(size_t) idx] = note;
        sequence.on[(size_t) idx] = true;
        sequence.recordIndex = (idx + 1) % std::max (1, s.seqLength);
    }

    emit (isOn, note, velocity, t);
    directOn[(size_t) note] = isOn;
}

void NoteGenerator::handleInput (const juce::MidiMessage& m, int t, const Settings& s)
{
    if (m.isAllNotesOff() || m.isAllSoundOff() || m.isResetAllControllers())
    {
        stopEverything (t);
        output.addEvent (m, t);
        return;
    }
    if (! (m.isNoteOn() || m.isNoteOff()))
    {
        output.addEvent (m, t);
        return;
    }

    const int key = m.getNoteNumber();

    if (m.isNoteOn())
    {
        const float vel = m.getFloatVelocity();
        const int base = key + 12 * s.kbOctave;

        // Chord learn: collect what's held, commit when every key is released.
        if (s.chordLearn)
        {
            ++learnHeld;
            if (numLearn < ChordData::kMaxNotes)
            {
                bool present = false;
                for (int i = 0; i < numLearn; ++i)
                    present |= learnNotes[(size_t) i] == base;
                if (! present)
                    learnNotes[(size_t) numLearn++] = base;
            }
        }

        auto& gen = generated[(size_t) key];
        auto& count = generatedCount[(size_t) key];

        // Same key struck again before its note-off: release the previous notes first.
        for (int i = 0; i < count; ++i)
            routeNote (false, gen[(size_t) i], 0.0f, t, s);
        count = 0;

        const int chordCount = chords.count.load();
        if (s.chordOn && ! s.chordLearn && chordCount > 0)
        {
            for (int i = 0; i < chordCount && i < ChordData::kMaxNotes; ++i)
            {
                const int n = base + chords.intervals[(size_t) i].load();
                if (n >= 0 && n <= 127)
                    gen[(size_t) count++] = (int8_t) n;
            }
        }
        else if (base >= 0 && base <= 127)
        {
            gen[(size_t) count++] = (int8_t) base;
        }

        for (int i = 0; i < count; ++i)
            routeNote (true, gen[(size_t) i], vel, t, s);
    }
    else
    {
        if (s.chordLearn && learnHeld > 0 && --learnHeld == 0 && numLearn > 0)
        {
            std::sort (learnNotes.begin(), learnNotes.begin() + numLearn);
            for (int i = 0; i < numLearn; ++i)
                chords.intervals[(size_t) i] = learnNotes[(size_t) i] - learnNotes[0];
            chords.count = numLearn;
            chords.learnFinished = true;
            numLearn = 0;
        }

        auto& gen = generated[(size_t) key];
        auto& count = generatedCount[(size_t) key];
        for (int i = 0; i < count; ++i)
            routeNote (false, gen[(size_t) i], 0.0f, t, s);
        count = 0;
    }
}

void NoteGenerator::arpStep (int t, const Settings& s)
{
    stopArpNote (t);
    if (numArpNotes == 0)
        return;

    // Build the pattern for this step.
    std::array<ArpNote, 32> sorted {};
    std::copy (arpNotes.begin(), arpNotes.begin() + numArpNotes, sorted.begin());
    if (s.arpMode != 3) // "Order" keeps the playing order
        std::sort (sorted.begin(), sorted.begin() + numArpNotes,
                   [] (const ArpNote& a, const ArpNote& b) { return a.note < b.note; });

    std::array<ArpNote, 256> pattern {}; // 32 notes x 4 octaves, doubled for up/down
    int len = 0;
    const int octaves = std::clamp (s.arpOctaves, 1, 4);
    for (int o = 0; o < octaves; ++o)
        for (int i = 0; i < numArpNotes; ++i)
            pattern[(size_t) len++] = { sorted[(size_t) i].note + 12 * o, sorted[(size_t) i].velocity };

    if (s.arpMode == 1)
    {
        std::reverse (pattern.begin(), pattern.begin() + len);
    }
    else if (s.arpMode == 2 && len > 2)
    {
        for (int i = len - 2; i >= 1; --i)
            pattern[(size_t) (len + (len - 2 - i))] = pattern[(size_t) i];
        len = len + len - 2;
    }

    ArpNote chosen;
    if (s.arpMode == 4)
        chosen = pattern[(size_t) (rng.next() % (uint32_t) len)];
    else
    {
        arpIndex = (arpIndex + 1) % len;
        chosen = pattern[(size_t) arpIndex];
    }

    if (chosen.note < 0 || chosen.note > 127)
        return;
    emit (true, chosen.note, chosen.velocity, t);
    arpPlaying = chosen.note;
    arpGateLeft = std::max (1, (int) (arpClock.stepSamples * s.arpGate));
}

void NoteGenerator::seqStep (int t, const Settings& s)
{
    stopSeqNote (t);
    const int length = std::clamp (s.seqLength, 1, kMaxSeqSteps);

    if (seqClock.hostMode)
        seqIndex = (int) (((seqClock.index() % length) + length) % length);
    else
        seqIndex = (seqIndex + 1) % length;
    sequence.playIndex = seqIndex;

    if (! sequence.on[(size_t) seqIndex].load())
        return;

    int note = sequence.note[(size_t) seqIndex].load();
    if (s.seqTranspose && numSeqHeld > 0)
        note += seqHeld[(size_t) numSeqHeld - 1] - sequence.note[0].load(); // key = step 1 plays as recorded
    if (note < 0 || note > 127)
        return;

    emit (true, note, 0.8f, t);
    seqPlaying = note;
    seqGateLeft = std::max (1, (int) (seqClock.stepSamples * s.seqGate));
}

void NoteGenerator::process (juce::MidiBuffer& midi, int numSamples, const Settings& s, const HostTime& host)
{
    output.clear();

    arpClock.begin (host, s.arpDivBeats, sampleRate);
    seqClock.begin (host, s.seqDivBeats, sampleRate);

    // Transport stopped in the host: stop arp / sequencer (the host releases its own notes).
    if (wasHostPlaying && ! host.playing)
        stopEverything (0);
    wasHostPlaying = host.playing;

    // Mode transitions
    if (s.seqPlay != wasSeqPlay)
    {
        if (s.seqPlay)
        {
            releaseDirectNotes (0);
            stopArpNote (0);
            seqClock.restart();
            seqIndex = -1;
        }
        else
        {
            stopSeqNote (0);
            sequence.playIndex = -1;
            numSeqHeld = seqPhysical = 0;
        }
        wasSeqPlay = s.seqPlay;
    }
    if (s.arpOn != wasArpOn)
    {
        if (s.arpOn)
            releaseDirectNotes (0);
        else
            stopArpNote (0);
        numArpNotes = physicalHeld = 0;
        arpIndex = -1;
        arpClock.restart();
        wasArpOn = s.arpOn;
    }
    if (! latched (s) && s.arpOn && physicalHeld == 0 && numArpNotes > 0)
    {
        numArpNotes = 0; // latch was switched off with nothing held
        stopArpNote (0);
    }
    if (! latched (s) && s.seqPlay && seqPhysical == 0 && numSeqHeld > 0)
    {
        numSeqHeld = 0;
        stopSeqNote (0);
        sequence.playIndex = -1;
    }

    if (sequence.resetRequest.exchange (false))
    {
        sequence.recordIndex = 0;
        seqClock.restart();
        seqIndex = -1;
    }
    if (sequence.advanceRequest.exchange (false))
    {
        const int len = std::max (1, s.seqLength);
        int idx = sequence.recordIndex.load() % len;
        sequence.on[(size_t) idx] = false;
        sequence.recordIndex = (idx + 1) % len;
    }

    // Walk the block in time order: input events, gate ends and clock steps.
    auto it = midi.cbegin();
    const auto end = midi.cend();
    int t = 0;
    while (t < numSamples)
    {
        while (it != end && (*it).samplePosition <= t)
        {
            handleInput ((*it).getMessage(), std::max (t, (*it).samplePosition), s);
            ++it;
        }

        if (arpPlaying >= 0 && arpGateLeft <= 0)
            stopArpNote (t);
        if (seqPlaying >= 0 && seqGateLeft <= 0)
            stopSeqNote (t);

        if (s.seqPlay && numSeqHeld > 0)
        {
            if (seqClock.due (t))
                seqStep (t, s);
        }
        else if (s.arpOn && numArpNotes > 0)
        {
            if (arpClock.due (t))
                arpStep (t, s);
        }

        int next = numSamples;
        if (it != end)
            next = std::min (next, (*it).samplePosition);
        if (arpPlaying >= 0)
            next = std::min (next, t + std::max (1, arpGateLeft));
        if (seqPlaying >= 0)
            next = std::min (next, t + std::max (1, seqGateLeft));
        if (s.seqPlay && numSeqHeld > 0)
            next = std::min (next, t + seqClock.samplesUntilNext (t));
        else if (s.arpOn && numArpNotes > 0)
            next = std::min (next, t + arpClock.samplesUntilNext (t));
        next = std::max (next, t + 1);
        next = std::min (next, numSamples);

        const int elapsed = next - t;
        arpGateLeft -= elapsed;
        seqGateLeft -= elapsed;
        if (s.seqPlay && numSeqHeld > 0)
            seqClock.advance (elapsed);
        else if (s.arpOn && numArpNotes > 0)
            arpClock.advance (elapsed);
        t = next;
    }
    // Events stamped beyond the block (shouldn't happen) are still delivered.
    for (; it != end; ++it)
        handleInput ((*it).getMessage(), numSamples - 1, s);

    midi.swapWith (output);
}
} // namespace rm
