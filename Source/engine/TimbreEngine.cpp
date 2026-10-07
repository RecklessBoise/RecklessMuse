#include "TimbreEngine.h"

namespace rm
{
void TimbreEngine::prepare (double internalSampleRate, int timbreIndex)
{
    for (int i = 0; i < kVoicesPerTimbre; ++i)
    {
        voices[(size_t) i].prepare (internalSampleRate, i + timbreIndex * kVoicesPerTimbre);
    }
    numHeld = 0;
    keyDown.fill (false);
    sustained.fill (false);
    sustainPedal = false;
}

void TimbreEngine::updateState (const TimbreParamPtrs& p, float vintage, double bpm)
{
    auto& s = state;
    s.o1Oct = asInt (p.o1Oct);
    s.o1Freq = asFloat (p.o1Freq);
    s.o1Wave = asFloat (p.o1Wave);
    s.o2Oct = asInt (p.o2Oct);
    s.o2Freq = asFloat (p.o2Freq);
    s.o2Wave = asFloat (p.o2Wave);
    s.o2Sync = asBool (p.o2Sync);
    s.o2KbTrack = asBool (p.o2KbTrack);
    s.fmAmt = asFloat (p.fmAmt);
    s.fmSrc = asInt (p.fmSrc);

    s.moFreq = asFloat (p.moscFreq);
    s.moWave = asInt (p.moscWave);
    s.moKbTrack = asBool (p.moscKbTrack);
    s.moUnipolar = asBool (p.moscUnipolar);
    s.moKeyReset = asBool (p.moscKeyReset);
    s.moPitchAmt = asFloat (p.moscPitchAmt);
    s.moPitchDest = asInt (p.moscPitchDest);
    s.moFilterAmt = asFloat (p.moscFilterAmt);
    s.moFilterDest = asInt (p.moscFilterDest);
    s.moPwmAmt = asFloat (p.moscPwmAmt);
    s.moVcaAmt = asFloat (p.moscVcaAmt);

    s.mixO1 = asFloat (p.mixO1);
    s.mixRing = asFloat (p.mixRing);
    s.mixO2 = asFloat (p.mixO2);
    s.mixMod = asFloat (p.mixMod);
    s.mixNoise = asFloat (p.mixNoise);
    s.mixOverload = asFloat (p.mixOverload);

    static constexpr float kbAmounts[] { 0.0f, 0.5f, 1.0f };
    s.f1Cutoff = asFloat (p.f1Cutoff);
    s.f1Res = asFloat (p.f1Res);
    s.f1Env = asFloat (p.f1Env);
    s.f1Kb = kbAmounts[std::clamp (asInt (p.f1Kb), 0, 2)];
    s.f1Mode = asInt (p.f1Mode);
    s.f2Cutoff = asFloat (p.f2Cutoff);
    s.f2Res = asFloat (p.f2Res);
    s.f2Env = asFloat (p.f2Env);
    s.f2Kb = kbAmounts[std::clamp (asInt (p.f2Kb), 0, 2)];
    s.fOrder = asInt (p.fOrder) + 1;
    s.fRouting = asInt (p.fRouting);
    s.fLink = asBool (p.fLink);

    s.feA = asFloat (p.feA);
    s.feD = asFloat (p.feD);
    s.feS = asFloat (p.feS);
    s.feR = asFloat (p.feR);
    s.feLoop = asBool (p.feLoop);
    s.feVel = asFloat (p.feVel);
    s.aeA = asFloat (p.aeA);
    s.aeD = asFloat (p.aeD);
    s.aeS = asFloat (p.aeS);
    s.aeR = asFloat (p.aeR);
    s.aeLoop = asBool (p.aeLoop);
    s.aeVel = asFloat (p.aeVel);

    s.vcaLevel = asFloat (p.vcaLevel);
    s.vcaPan = asFloat (p.vcaPan);
    s.vcaSpread = asFloat (p.vcaSpread);

    auto syncedHz = [bpm] (int division)
    {
        const double beats = choices::divisionBeats[std::clamp (division, 0, 15)];
        return (float) (bpm / 60.0 / beats);
    };
    s.l1Hz = asBool (p.l1Sync) ? syncedHz (asInt (p.l1Div)) : asFloat (p.l1Rate);
    s.l2Hz = asBool (p.l2Sync) ? syncedHz (asInt (p.l2Div)) : asFloat (p.l2Rate);
    s.l1Wave = asInt (p.l1Wave);
    s.l2Wave = asInt (p.l2Wave);
    s.l1Amp = asFloat (p.l1Amp);
    s.l2Amp = asFloat (p.l2Amp);
    s.l1Reset = asBool (p.l1Reset);
    s.l2Reset = asBool (p.l2Reset);

    s.plHz = asFloat (p.plRate);
    s.plWave = asInt (p.plWave);
    s.plAmt = asFloat (p.plAmt);
    s.plDest = asInt (p.plDest);
    s.plWheel = asBool (p.plWheel);

    s.polyMode = asInt (p.polyMode);
    s.uniVoices = asInt (p.uniVoices);
    s.uniDetune = asFloat (p.uniDetune);
    s.glide = asFloat (p.glide);
    s.glideLegato = asBool (p.glideLegato);
    s.bendRange = (float) asInt (p.bendRange);
    s.notePrio = asInt (p.notePrio);

    for (int i = 0; i < kNumMatrixSlots; ++i)
    {
        s.mmSrc[i] = asInt (p.mmSrc[i]);
        s.mmVia[i] = asInt (p.mmVia[i]);
        s.mmDst[i] = asInt (p.mmDst[i]);
        s.mmAmt[i] = asFloat (p.mmAmt[i]);
    }

    s.vintage = vintage;

    if (s.polyMode != lastPolyMode)
    {
        allNotesOff();
        lastPolyMode = s.polyMode;
    }
    configureStack();
}

void TimbreEngine::configureStack()
{
    // Pan spread only spreads polyphonic voices (alternating left/right, wider with the voice
    // number). Mono and unison stay centred: the unison voices get their own symmetric spread.
    for (int i = 0; i < kVoicesPerTimbre; ++i)
    {
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        voices[(size_t) i].setSpreadPosition (state.polyMode == 0 ? side * (0.35f + 0.65f * (float) (i / 2) / 3.0f) : 0.0f);
    }

    if (state.polyMode == 2)
    {
        const int n = numStackVoices();
        for (int i = 0; i < kVoicesPerTimbre; ++i)
        {
            const float pos = n > 1 ? (2.0f * (float) i / (float) (n - 1) - 1.0f) : 0.0f;
            voices[(size_t) i].setStackPosition (i < n ? pos * state.uniDetune * 0.35f : 0.0f,
                                                 i < n ? pos * 0.8f : 0.0f);
        }
    }
    else
    {
        // In poly/mono mode the detune knob adds a subtle per-voice spread (analog "slop").
        for (int i = 0; i < kVoicesPerTimbre; ++i)
        {
            const float pos = ((float) i / (float) (kVoicesPerTimbre - 1)) * 2.0f - 1.0f;
            voices[(size_t) i].setStackPosition (pos * state.uniDetune * 0.06f, 0.0f);
        }
    }
}

bool TimbreEngine::anyGateOn() const noexcept
{
    for (auto& v : voices)
        if (v.isActive() && v.isGateOn())
            return true;
    return false;
}

void TimbreEngine::noteOn (int note, float velocity)
{
    note = std::clamp (note, 0, 127);
    keyDown[(size_t) note] = true;
    sustained[(size_t) note] = false;

    if (state.polyMode == 0)
        polyNoteOn (note, velocity);
    else
        monoNoteOn (note, velocity);
}

void TimbreEngine::polyNoteOn (int note, float velocity)
{
    const bool legato = anyGateOn();
    const bool useGlide = state.glide > 0.0005f && (! state.glideLegato || legato);

    Voice* chosen = nullptr;

    // 1) Same note still sounding: retrigger it
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == note)
        {
            chosen = &v;
            break;
        }

    // 2) Free voice, round-robin (spreads the analog tolerances across notes)
    if (chosen == nullptr)
        for (int i = 0; i < kVoicesPerTimbre; ++i)
        {
            auto& v = voices[(size_t) ((nextVoice + i) % kVoicesPerTimbre)];
            if (! v.isActive())
            {
                chosen = &v;
                nextVoice = (nextVoice + i + 1) % kVoicesPerTimbre;
                break;
            }
        }

    // 3) Quietest releasing voice
    if (chosen == nullptr)
    {
        float lowest = 2.0f;
        for (auto& v : voices)
            if (v.isReleasing() && v.getLevel() < lowest)
            {
                lowest = v.getLevel();
                chosen = &v;
            }
    }

    // 4) Oldest voice
    if (chosen == nullptr)
    {
        chosen = &voices[0];
        for (auto& v : voices)
            if (v.getAge() < chosen->getAge())
                chosen = &v;
    }

    chosen->start (note, velocity, lastPitch, useGlide, state);
    lastPitch = (float) note;
}

int TimbreEngine::pickMonoNote() const noexcept
{
    if (numHeld == 0)
        return -1;
    if (state.notePrio == 1)
        return *std::min_element (heldNotes.begin(), heldNotes.begin() + numHeld);
    if (state.notePrio == 2)
        return *std::max_element (heldNotes.begin(), heldNotes.begin() + numHeld);
    return heldNotes[(size_t) numHeld - 1];
}

void TimbreEngine::monoNoteOn (int note, float velocity)
{
    // Move the note to the top of the stack
    for (int i = 0; i < numHeld; ++i)
        if (heldNotes[(size_t) i] == note)
        {
            for (int j = i; j < numHeld - 1; ++j)
            {
                heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
                heldVelocities[(size_t) j] = heldVelocities[(size_t) j + 1];
            }
            --numHeld;
            break;
        }
    if (numHeld == (int) heldNotes.size())
    {
        for (int j = 0; j < numHeld - 1; ++j)
            heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
        --numHeld;
    }
    heldNotes[(size_t) numHeld] = note;
    heldVelocities[(size_t) numHeld] = velocity;
    ++numHeld;

    const int target = pickMonoNote();
    const int n = numStackVoices();
    const bool legato = anyGateOn();

    if (legato)
    {
        const bool useGlide = state.glide > 0.0005f;
        for (int i = 0; i < n; ++i)
            voices[(size_t) i].legatoTo (target, useGlide);
    }
    else
    {
        monoVelocity = velocity;
        const bool useGlide = state.glide > 0.0005f && ! state.glideLegato;
        for (int i = 0; i < n; ++i)
            voices[(size_t) i].start (target, velocity, lastPitch, useGlide, state);
    }
    lastPitch = (float) target;
}

void TimbreEngine::monoNoteOff (int note)
{
    for (int i = 0; i < numHeld; ++i)
        if (heldNotes[(size_t) i] == note)
        {
            for (int j = i; j < numHeld - 1; ++j)
            {
                heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
                heldVelocities[(size_t) j] = heldVelocities[(size_t) j + 1];
            }
            --numHeld;
            break;
        }

    const int n = numStackVoices();
    if (numHeld == 0)
    {
        for (int i = 0; i < n; ++i)
            voices[(size_t) i].release();
        return;
    }

    const int target = pickMonoNote();
    if (target != voices[0].getNote())
    {
        for (int i = 0; i < n; ++i)
            voices[(size_t) i].legatoTo (target, state.glide > 0.0005f);
        lastPitch = (float) target;
    }
}

void TimbreEngine::noteOff (int note)
{
    note = std::clamp (note, 0, 127);
    keyDown[(size_t) note] = false;

    if (sustainPedal)
    {
        sustained[(size_t) note] = true;
        return;
    }

    if (state.polyMode == 0)
    {
        for (auto& v : voices)
            if (v.isActive() && v.isGateOn() && v.getNote() == note)
                v.release();
    }
    else
    {
        monoNoteOff (note);
    }
}

void TimbreEngine::setSustain (bool down)
{
    if (down == sustainPedal)
        return;
    sustainPedal = down;
    if (down)
        return;

    for (int n = 0; n < 128; ++n)
        if (sustained[(size_t) n])
        {
            sustained[(size_t) n] = false;
            if (! keyDown[(size_t) n])
                noteOff (n);
        }
}

void TimbreEngine::allNotesOff()
{
    numHeld = 0;
    keyDown.fill (false);
    sustained.fill (false);
    for (auto& v : voices)
        if (v.isActive())
            v.release();
}

void TimbreEngine::killAll()
{
    allNotesOff();
    for (auto& v : voices)
        v.kill();
}

void TimbreEngine::render (float* left, float* right, int numSamples)
{
    for (auto& v : voices)
        v.render (left, right, numSamples, state, perf);
}

int TimbreEngine::getActiveVoiceCount() const noexcept
{
    int n = 0;
    for (auto& v : voices)
        n += v.isActive() ? 1 : 0;
    return n;
}
} // namespace rm
