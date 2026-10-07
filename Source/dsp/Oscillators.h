#pragma once

#include "DspUtil.h"

namespace rm::dsp
{
// Information passed from a master oscillator to a hard-synced slave.
struct SyncEvent
{
    bool happened = false;
    float frac = 0.0f; // time elapsed since the master wrapped, in samples (0..1)
};

// Continuously variable "VCO" waveform: Triangle -> Saw -> Square -> narrow Pulse.
//
// Band-limited with 2-point polyBLEP (steps) and polyBLAMP (slope changes). The
// corrections are spread over the previous and the current sample, so the output
// runs one sample late. That is invisible musically and keeps the maths exact.
class MorphOscillator
{
public:
    void reset (float startPhase)
    {
        phase = startPhase - std::floor (startPhase);
        held = 0.0f;
    }

    float getPhase() const noexcept { return phase; }

    // inc: cycles per sample. wave: 0..1 morph. pwm: -1..1 pulse width modulation.
    float process (float inc, float wave, float pwm, const SyncEvent* syncIn, SyncEvent* syncOut) noexcept
    {
        inc = std::clamp (inc, 1.0e-6f, 0.45f);
        computeWeights (wave, pwm);

        if (syncOut != nullptr)
            syncOut->happened = false;

        const float phPrev = phase;
        float raw = phPrev + inc;
        float corr = 0.0f;

        if (syncIn != nullptr && syncIn->happened)
        {
            // Advance to the sync instant, jump to phase 0, then run the remaining time.
            const float d = std::clamp (syncIn->frac, 0.0f, 1.0f);
            float atEvent = phPrev + inc * (1.0f - d);
            atEvent -= std::floor (atEvent);
            const float h = value (0.0f) - value (atEvent);
            held += 0.5f * h * d * d;
            corr -= 0.5f * h * (1.0f - d) * (1.0f - d);
            phase = inc * d;
            const float out = held;
            held = value (phase) + corr;
            return out;
        }

        // Pulse edge (falling) and triangle peak before the wrap.
        if (wSqr > 0.0f && phPrev < pw && raw >= pw)
            addStep (-2.0f * wSqr, (raw - pw) / inc, corr);
        if (wTri > 0.0f && phPrev < 0.5f && raw >= 0.5f)
            addRamp (-8.0f * inc * wTri, (raw - 0.5f) / inc, corr);

        if (raw >= 1.0f)
        {
            raw -= 1.0f;
            const float d = raw / inc;
            // Saw drops by 2, square rises by 2, triangle slope flips upward.
            addStep (-2.0f * wSaw + 2.0f * wSqr, d, corr);
            if (wTri > 0.0f)
                addRamp (8.0f * inc * wTri, d, corr);

            if (wSqr > 0.0f && raw >= pw)
                addStep (-2.0f * wSqr, (raw - pw) / inc, corr);
            if (wTri > 0.0f && raw >= 0.5f)
                addRamp (-8.0f * inc * wTri, (raw - 0.5f) / inc, corr);

            if (syncOut != nullptr)
            {
                syncOut->happened = true;
                syncOut->frac = d;
            }
        }

        phase = raw;
        const float out = held;
        held = value (phase) + corr;
        return out;
    }

private:
    void computeWeights (float wave, float pwm) noexcept
    {
        const float w = std::clamp (wave, 0.0f, 1.0f) * 3.0f;
        wTri = wSaw = wSqr = 0.0f;
        float width = 0.5f;
        if (w < 1.0f)      { wTri = 1.0f - w; wSaw = w; }
        else if (w < 2.0f) { wSaw = 2.0f - w; wSqr = w - 1.0f; }
        else               { wSqr = 1.0f; width = 0.5f - 0.42f * (w - 2.0f); }
        pw = std::clamp (width - 0.4f * pwm, 0.05f, 0.95f);
        sqrDc = 2.0f * pw - 1.0f; // remove the DC of asymmetric pulses
    }

    float value (float p) const noexcept
    {
        const float tri = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
        const float saw = 2.0f * p - 1.0f;
        const float sqr = (p < pw ? 1.0f : -1.0f) - sqrDc;
        return wTri * tri + wSaw * saw + wSqr * sqr;
    }

    // Step of height h that happened d samples before the current sample.
    void addStep (float h, float d, float& corr) noexcept
    {
        held += 0.5f * h * d * d;
        corr -= 0.5f * h * (1.0f - d) * (1.0f - d);
    }

    // Slope change of m (per sample) that happened d samples before the current sample.
    void addRamp (float m, float d, float& corr) noexcept
    {
        held += m * d * d * d * (1.0f / 6.0f);
        const float e = 1.0f - d;
        corr += m * e * e * e * (1.0f / 6.0f);
    }

    float phase = 0.0f, held = 0.0f;
    float wTri = 0.0f, wSaw = 1.0f, wSqr = 0.0f, pw = 0.5f, sqrDc = 0.0f;
};

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Modulation oscillator / LFO core. Works from sub-audio up to audio rate.
class ShapeOscillator
{
public:
    enum Shape { Sine, Triangle, Saw, Ramp, Square, SampleHold, Noise, Smooth };

    void reset (float startPhase, uint32_t seed)
    {
        phase = startPhase - std::floor (startPhase);
        rng = Random (seed);
        held = rng.bipolar();
        target = rng.bipolar();
        prevTarget = held;
    }

    void resetPhase() noexcept { phase = 0.0f; }

    float process (float inc, int shape) noexcept
    {
        inc = std::clamp (inc, 0.0f, 0.45f);
        float out = 0.0f;
        const float t = phase;
        switch (shape)
        {
            case Sine:     out = std::sin (kTwoPi * t); break;
            case Triangle: out = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t; break;
            case Saw:      out = 1.0f - 2.0f * t + polyBlep (t, inc); break;  // falling saw
            case Ramp:     out = 2.0f * t - 1.0f - polyBlep (t, inc); break;  // rising ramp
            case Square:
            {
                float sq = t < 0.5f ? 1.0f : -1.0f;
                sq += polyBlep (t, inc);
                float t2 = t + 0.5f;
                t2 -= std::floor (t2);
                sq -= polyBlep (t2, inc);
                out = sq;
                break;
            }
            case SampleHold: out = held; break;
            case Noise:      out = rng.bipolar(); break;
            case Smooth:
            {
                const float s = t * t * (3.0f - 2.0f * t);
                out = prevTarget + (target - prevTarget) * s;
                break;
            }
            default: break;
        }

        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rng.bipolar();
            prevTarget = target;
            target = rng.bipolar();
        }
        return out;
    }

    float phase = 0.0f;

private:
    Random rng;
    float held = 0.0f, target = 0.0f, prevTarget = 0.0f;
};

// White + gently pinked noise source.
class NoiseSource
{
public:
    void reset (uint32_t seed) { rng = Random (seed); b0 = b1 = b2 = 0.0f; }

    float process() noexcept
    {
        const float w = rng.bipolar();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        return 0.5f * w + 0.25f * (b0 + b1 + b2 + w * 0.1848f);
    }

private:
    Random rng;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
};
} // namespace rm::dsp
