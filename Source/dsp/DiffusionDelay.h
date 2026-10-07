#pragma once

#include "DspUtil.h"
#include <array>
#include <vector>

namespace rm::dsp
{
// Stereo "diffusion delay": two independent delay lines whose feedback path runs
// through a chain of allpass diffusers (smearing repeats into a reverb-like wash),
// a damping low-pass, a low-cut and a soft saturator. "Character" darkens the
// repeats and adds tape-style wow.
class DiffusionDelay
{
public:
    struct Settings
    {
        float timeL = 0.375f, timeR = 0.5f; // seconds
        float feedback = 0.35f;             // 0..1
        float character = 0.4f;             // 0..1
        float diffusion = 0.3f;             // 0..1
        float mix = 0.25f;                  // 0..1
    };

    void prepare (double sr)
    {
        sampleRate = (float) sr;
        const auto size = (size_t) (sr * 2.2) + 8;
        for (auto& line : lines)
        {
            line.buffer.assign (size, 0.0f);
            line.write = 0;
            line.smoothedDelay = 0.3f * sampleRate;
            line.lp = line.hpState = 0.0f;
            static constexpr int lengths[2][4] { { 142, 379, 107, 277 }, { 151, 353, 113, 263 } };
            const int li = (int) (&line - lines.data());
            for (int i = 0; i < 4; ++i)
            {
                auto len = (size_t) std::max (8.0f, (float) lengths[li][i] * sampleRate / 44100.0f);
                line.allpass[(size_t) i].buffer.assign (len, 0.0f);
                line.allpass[(size_t) i].index = 0;
            }
        }
        wowPhase = 0.0f;
    }

    void reset()
    {
        for (auto& line : lines)
        {
            std::fill (line.buffer.begin(), line.buffer.end(), 0.0f);
            for (auto& ap : line.allpass)
                std::fill (ap.buffer.begin(), ap.buffer.end(), 0.0f);
            line.lp = line.hpState = 0.0f;
        }
    }

    void process (float* left, float* right, int numSamples, const Settings& s) noexcept
    {
        const float fb = std::min (s.feedback, 1.0f) * 0.98f;
        const float apGain = 0.72f * s.diffusion;
        const float dampHz = 14000.0f * std::pow (0.1f, s.character);
        const float dampCoef = 1.0f - std::exp (-kTwoPi * dampHz / sampleRate);
        const float hpCoef = 1.0f - std::exp (-kTwoPi * 80.0f / sampleRate);
        const float wowDepth = s.character * 0.0012f * sampleRate;
        const float wowInc = 0.55f / sampleRate;
        const float timeSmooth = 1.0f - std::exp (-1.0f / (0.12f * sampleRate));
        const float wet = s.mix;
        const float dry = 1.0f - 0.5f * s.mix;
        const float targets[2] { s.timeL * sampleRate, s.timeR * sampleRate };

        for (int n = 0; n < numSamples; ++n)
        {
            wowPhase += wowInc;
            if (wowPhase >= 1.0f) wowPhase -= 1.0f;
            const float wow = std::sin (kTwoPi * wowPhase);

            float* io[2] { left + n, right + n };
            for (size_t c = 0; c < 2; ++c)
            {
                auto& line = lines[c];
                line.smoothedDelay += (targets[c] - line.smoothedDelay) * timeSmooth;
                const float wowOffset = (c == 0 ? wow : -wow) * wowDepth;
                const float delayed = read (line, line.smoothedDelay + wowOffset);

                // Feedback conditioning: diffuse -> damp -> low cut -> saturate.
                float x = delayed;
                for (auto& ap : line.allpass)
                    x = ap.process (x, apGain);
                line.lp += (x - line.lp) * dampCoef;
                line.hpState += (line.lp - line.hpState) * hpCoef;
                const float conditioned = line.lp - line.hpState;

                const float input = *io[c];
                line.buffer[line.write] = fastTanh (input + conditioned * fb);
                if (++line.write >= line.buffer.size()) line.write = 0;

                *io[c] = input * dry + conditioned * wet;
            }
        }
    }

private:
    struct Allpass
    {
        std::vector<float> buffer;
        size_t index = 0;

        float process (float x, float g) noexcept
        {
            const float delayed = buffer[index];
            const float v = x + g * delayed;
            buffer[index] = v;
            if (++index >= buffer.size()) index = 0;
            return delayed - g * v;
        }
    };

    struct Line
    {
        std::vector<float> buffer;
        size_t write = 0;
        float smoothedDelay = 0.0f, lp = 0.0f, hpState = 0.0f;
        std::array<Allpass, 4> allpass;
    };

    static float read (const Line& line, float delaySamples) noexcept
    {
        const auto size = (float) line.buffer.size();
        delaySamples = std::clamp (delaySamples, 1.0f, size - 4.0f);
        float pos = (float) line.write - delaySamples;
        if (pos < 0.0f) pos += size;
        const auto i0 = (size_t) pos;
        const float f = pos - (float) i0;
        const auto i1 = i0 + 1 < line.buffer.size() ? i0 + 1 : 0;
        return line.buffer[i0] + f * (line.buffer[i1] - line.buffer[i0]);
    }

    std::array<Line, 2> lines;
    float sampleRate = 44100.0f, wowPhase = 0.0f;
};
} // namespace rm::dsp
