#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rm::dsp
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// Rational tanh approximation, exact at the clip points, smooth everywhere.
inline float fastTanh (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// 2^x, ~1e-5 relative error. Used for per-sample pitch / cutoff exponentials.
inline float fastExp2 (float x) noexcept
{
    x = std::clamp (x, -60.0f, 60.0f);
    const float fi = std::floor (x);
    const float f = x - fi;
    const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    return std::ldexp (p, (int) fi);
}

// Padé approximation of tan(x) for the bilinear pre-warp; x is clamped below pi/2.
inline float prewarp (float x) noexcept
{
    x = std::min (x, 1.40f);
    const float x2 = x * x;
    return x * (15.0f - x2) / (15.0f - 6.0f * x2);
}

inline float noteToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

struct Random
{
    explicit Random (uint32_t seed = 0x9E3779B9u) : state (seed != 0 ? seed : 1u) {}

    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float bipolar() noexcept { return (float) (next() >> 8) * (2.0f / 16777216.0f) - 1.0f; }
    float unipolar() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }

    uint32_t state;
};

// One-pole DC blocker.
struct DcBlocker
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.995f;

    void setup (double sampleRate, float hz = 12.0f) { r = 1.0f - (kTwoPi * hz / (float) sampleRate); }
    void reset() { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};
} // namespace rm::dsp
