#pragma once

#include "DspUtil.h"

namespace rm::dsp
{
// Four-pole transistor ladder in the spirit of the classic Moog 904A / Minimoog filter.
//
// Topology-preserving (zero-delay feedback) one-pole stages, with the global feedback
// solved for the linear part and then pushed through a tanh — the input differential
// pair of the real ladder. This gives the "Moog" behaviours that matter:
//   - smooth self-oscillation at the top of the resonance range, amplitude-limited
//   - the bass / passband thinning as resonance rises (only partly compensated)
//   - soft, warm compression when driven hard
// Each stage also has a gentle saturator on its state, emulating the transistor pairs.
class LadderFilter
{
public:
    enum Mode { LowPass, HighPass };

    void reset() noexcept { s[0] = s[1] = s[2] = s[3] = 0.0f; }

    // g = prewarped cutoff, k = feedback (0..~4.2), order = 1..4 poles.
    float process (float x, float g, float k, int order, Mode mode) noexcept
    {
        const float G = g / (1.0f + g);
        const float inv = 1.0f / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;

        // Partial passband gain compensation: a real ladder loses roughly 1/(1+k) of
        // its low end at high resonance; we give back about half of it.
        const float in = x * (1.0f + 0.5f * k);

        const float sigma = (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]) * inv;
        const float y4Est = (G4 * in + sigma) / (1.0f + k * G4);
        const float u = fastTanh (in - k * y4Est);

        float y[4];
        float stageIn = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (stageIn - s[i]) * G;
            y[i] = v + s[i];
            s[i] = softClip (y[i] + v);
            stageIn = y[i];
        }

        order = std::clamp (order, 1, 4);
        if (mode == LowPass)
            return y[order - 1];

        // High-pass outputs: binomial mixes of the ladder taps.
        switch (order)
        {
            case 1:  return u - y[0];
            case 2:  return u - 2.0f * y[0] + y[1];
            case 3:  return u - 3.0f * y[0] + 3.0f * y[1] - y[2];
            default: return u - 4.0f * y[0] + 6.0f * y[1] - 4.0f * y[2] + y[3];
        }
    }

private:
    // Transparent below 0.7, then a continuous soft knee that saturates toward 1.6.
    static float softClip (float v) noexcept
    {
        const float a = std::abs (v) - 0.7f;
        if (a <= 0.0f)
            return v;
        const float shaped = 0.7f + a / (1.0f + a * (1.0f / 0.9f));
        return v < 0.0f ? -shaped : shaped;
    }

    float s[4] {};
};
} // namespace rm::dsp
