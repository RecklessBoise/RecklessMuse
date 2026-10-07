#include "SynthEngine.h"

namespace rm
{
void SynthEngine::setParameters (const TimbreParamPtrs* timbreA, const TimbreParamPtrs* timbreB,
                                 const GlobalParamPtrs* globalParams)
{
    params[0] = timbreA;
    params[1] = timbreB;
    globals = globalParams;
}

void SynthEngine::prepare (double newSampleRate, int maximumBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = std::max (1, maximumBlockSize);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) maxBlock);
    scratch.setSize (2, maxBlock * 2);

    delay.prepare (sampleRate);
    dcL.setup (sampleRate);
    dcR.setup (sampleRate);
    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (0.5f);

    currentFactor = 0;
    configureRate (globals != nullptr && asInt (globals->quality) == 0 ? 1 : 2);
    noteRouting.fill (0);
}

void SynthEngine::configureRate (int factor)
{
    if (factor == currentFactor)
        return;
    currentFactor = factor;
    for (int t = 0; t < 2; ++t)
        timbres[(size_t) t].prepare (sampleRate * factor, t);
    if (oversampler != nullptr)
        oversampler->reset();
}

void SynthEngine::reset()
{
    for (auto& t : timbres)
        t.killAll();
    delay.reset();
    dcL.reset();
    dcR.reset();
    if (oversampler != nullptr)
        oversampler->reset();
    noteRouting.fill (0);
}

void SynthEngine::allNotesOff()
{
    for (auto& t : timbres)
        t.allNotesOff();
    noteRouting.fill (0);
}

int SynthEngine::getActiveVoiceCount() const noexcept
{
    return timbres[0].getActiveVoiceCount() + timbres[1].getActiveVoiceCount();
}

void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        const float vel = m.getFloatVelocity();
        const int mode = asInt (globals->timbreMode);
        uint8_t route = 1;
        if (mode == 1)
            route = note < asInt (globals->splitNote) ? 2 : 1;
        else if (mode == 2)
            route = 3;

        // A note re-struck while still held on another route: release it there first.
        const uint8_t old = noteRouting[(size_t) note];
        for (int t = 0; t < 2; ++t)
            if ((old & (1 << t)) != 0 && (route & (1 << t)) == 0)
                timbres[(size_t) t].noteOff (note);

        noteRouting[(size_t) note] = route;
        for (int t = 0; t < 2; ++t)
            if ((route & (1 << t)) != 0)
                timbres[(size_t) t].noteOn (note, vel);
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        const uint8_t route = noteRouting[(size_t) note];
        for (int t = 0; t < 2; ++t)
            if ((route & (1 << t)) != 0)
                timbres[(size_t) t].noteOff (note);
        noteRouting[(size_t) note] = 0;
    }
    else if (m.isPitchWheel())
    {
        const float bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        for (auto& t : timbres) t.perf.pitchBend = std::clamp (bend, -1.0f, 1.0f);
    }
    else if (m.isChannelPressure())
    {
        for (auto& t : timbres) t.perf.aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    }
    else if (m.isAftertouch())
    {
        for (auto& t : timbres) t.perf.aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber();
        const float v = (float) m.getControllerValue() / 127.0f;
        if (cc == 1)
            for (auto& t : timbres) t.perf.modWheel = v;
        else if (cc == 64)
            for (auto& t : timbres) t.setSustain (v >= 0.5f);
        else if (cc == 120 || cc == 123)
            allNotesOff();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        allNotesOff();
    }
}

void SynthEngine::renderVoices (float* left, float* right, int numSamples)
{
    for (auto& t : timbres)
        t.render (left, right, numSamples);
}

float SynthEngine::safetyClip (float x) noexcept
{
    // Transparent up to 0.85, then a soft knee that never exceeds 1.0.
    const float a = std::abs (x) - 0.85f;
    if (a <= 0.0f)
        return x;
    const float shaped = 0.85f + a / (1.0f + a * (1.0f / 0.15f));
    return x < 0.0f ? -shaped : shaped;
}

void SynthEngine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double bpm)
{
    jassert (globals != nullptr && params[0] != nullptr && params[1] != nullptr);
    buffer.clear();

    configureRate (asInt (globals->quality) == 0 ? 1 : 2);

    const float vintage = asFloat (globals->vintage);
    for (int t = 0; t < 2; ++t)
    {
        auto& timbre = timbres[(size_t) t];
        timbre.updateState (*params[t], vintage, bpm);
        timbre.perf.macro1 = asFloat (globals->macro1);
        timbre.perf.macro2 = asFloat (globals->macro2);
    }

    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += maxBlock)
        processChunk (buffer, start, std::min (maxBlock, total - start), midi, bpm);
}

void SynthEngine::processChunk (juce::AudioBuffer<float>& buffer, int start, int num,
                                const juce::MidiBuffer& midi, double bpm)
{
    const int factor = currentFactor;
    float* outL = buffer.getWritePointer (0, start);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, start) : nullptr;

    float* renderL;
    float* renderR;
    juce::dsp::AudioBlock<float> baseBlock;
    if (factor == 2)
    {
        float* chans[2] { outL, outR != nullptr ? outR : scratch.getWritePointer (1) };
        baseBlock = juce::dsp::AudioBlock<float> (chans, 2, (size_t) num);
        baseBlock.clear();
        auto osBlock = oversampler->processSamplesUp (baseBlock);
        osBlock.clear();
        renderL = osBlock.getChannelPointer (0);
        renderR = osBlock.getChannelPointer (1);
    }
    else
    {
        scratch.clear();
        renderL = scratch.getWritePointer (0);
        renderR = scratch.getWritePointer (1);
    }

    // Sample-accurate MIDI: render between events.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = meta.samplePosition - start;
        if (at < 0) continue;
        if (at >= num) break;
        if (at > pos)
        {
            renderVoices (renderL + pos * factor, renderR + pos * factor, (at - pos) * factor);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < num)
        renderVoices (renderL + pos * factor, renderR + pos * factor, (num - pos) * factor);

    float* right = outR != nullptr ? outR : scratch.getWritePointer (1);
    if (factor == 2)
    {
        oversampler->processSamplesDown (baseBlock);
    }
    else
    {
        std::copy (renderL, renderL + num, outL);
        std::copy (renderR, renderR + num, right);
    }

    // Diffusion delay
    if (asBool (globals->dlyOn))
    {
        dsp::DiffusionDelay::Settings ds;
        auto divSeconds = [bpm] (int division)
        { return (float) (choices::divisionBeats[std::clamp (division, 0, 15)] * 60.0 / bpm); };
        const bool synced = asBool (globals->dlySync);
        ds.timeL = std::min (2.0f, synced ? divSeconds (asInt (globals->dlyDivL)) : asFloat (globals->dlyTimeL));
        ds.timeR = std::min (2.0f, synced ? divSeconds (asInt (globals->dlyDivR)) : asFloat (globals->dlyTimeR));
        ds.feedback = asFloat (globals->dlyFb);
        ds.character = asFloat (globals->dlyChar);
        ds.diffusion = asFloat (globals->dlyDiff);
        ds.mix = asFloat (globals->dlyMix);
        delay.process (outL, right, num, ds);
    }

    // Output stage
    masterGain.setTargetValue (juce::Decibels::decibelsToGain (asFloat (globals->masterVol), -60.0f) + 1.0e-6f);
    for (int n = 0; n < num; ++n)
    {
        const float g = masterGain.getNextValue();
        outL[n] = safetyClip (dcL.process (outL[n]) * g);
        right[n] = safetyClip (dcR.process (right[n]) * g);
    }
}
} // namespace rm
