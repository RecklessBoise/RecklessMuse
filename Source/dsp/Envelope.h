#pragma once

#include "DspUtil.h"

namespace rm::dsp
{
// Analog-style ADSR: exponential RC segments, attack aims past 1.0 like a capacitor
// charging toward a higher rail, so it sounds punchy rather than linear.
// Optional looping (attack -> decay -> attack ...) while the gate is held.
// Retriggering starts from the current level, so voice stealing never clicks.
class AdsrEnvelope
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setSampleRate (double sr) { sampleRate = (float) sr; lastA = lastD = lastR = -1.0f; }

    void setParameters (float attackS, float decayS, float sustainLevel, float releaseS, bool shouldLoop) noexcept
    {
        sustain = std::clamp (sustainLevel, 0.0f, 1.0f);
        loop = shouldLoop;
        if (changed (attackS, lastA))
        {
            lastA = attackS;
            attackCoef = coef (attackS, attackRatio);
            attackBase = (1.0f + attackRatio) * (1.0f - attackCoef);
        }
        if (changed (decayS, lastD))
        {
            lastD = decayS;
            decayCoef = coef (decayS, decayRatio);
        }
        if (changed (releaseS, lastR))
        {
            lastR = releaseS;
            releaseCoef = coef (releaseS, decayRatio);
        }
    }

    void gateOn() noexcept { stage = Attack; }
    void gateOff() noexcept { if (stage != Idle) stage = Release; }
    void kill() noexcept { stage = Idle; level = 0.0f; }

    bool isActive() const noexcept { return stage != Idle; }
    bool isReleasing() const noexcept { return stage == Release; }
    float getLevel() const noexcept { return level; }

    float process() noexcept
    {
        switch (stage)
        {
            case Attack:
                level = attackBase + level * attackCoef;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Decay;
                }
                break;
            case Decay:
            {
                const float target = sustain - decayRatio * (1.0f - sustain);
                level = target + (level - target) * decayCoef;
                if (level <= sustain + 1.0e-4f)
                {
                    level = sustain;
                    stage = loop ? Attack : Sustain;
                }
                break;
            }
            case Sustain:
                level = sustain;
                if (loop) stage = Attack;
                break;
            case Release:
            {
                const float target = -decayRatio;
                level = target + (level - target) * releaseCoef;
                if (level <= 1.0e-5f)
                {
                    level = 0.0f;
                    stage = Idle;
                }
                break;
            }
            case Idle:
            default:
                break;
        }
        return level;
    }

private:
    static bool changed (float a, float b) noexcept { return std::abs (a - b) > 1.0e-7f; }

    float coef (float seconds, float ratio) const noexcept
    {
        const float samples = std::max (1.0f, seconds * sampleRate);
        return std::exp (-std::log ((1.0f + ratio) / ratio) / samples);
    }

    static constexpr float attackRatio = 0.35f;
    static constexpr float decayRatio = 0.0005f;

    float sampleRate = 44100.0f;
    float level = 0.0f, sustain = 1.0f;
    float attackCoef = 0.0f, attackBase = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f;
    float lastA = -1.0f, lastD = -1.0f, lastR = -1.0f;
    bool loop = false;
    Stage stage = Idle;
};
} // namespace rm::dsp
