#pragma once

#include <algorithm>
#include <cmath>

namespace subterra
{
inline constexpr double twoPi = 6.283185307179586;

// Band-limited oscillator that morphs sine -> saw -> square -> triangle.
class MorphOscillator
{
public:
    void reset (double startPhase = 0.0) noexcept { phase = startPhase; }

    // increment = frequency / sampleRate, wave in [0, 1]
    float process (double increment, float wave) noexcept
    {
        increment = std::clamp (increment, 0.0, 0.45);
        const float t = (float) phase;
        const float dt = (float) increment;

        const float position = std::clamp (wave, 0.0f, 1.0f) * 3.0f;
        const int segment = std::min (2, (int) position);
        const float blend = position - (float) segment;

        const float a = shape (segment, t, dt);
        const float b = blend > 0.0f ? shape (segment + 1, t, dt) : 0.0f;

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;

        return a + (b - a) * blend;
    }

private:
    static float polyBlep (float t, float dt) noexcept
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

    static float shape (int index, float t, float dt) noexcept
    {
        switch (index)
        {
            case 0: return std::sin ((float) twoPi * t);
            case 1: return (2.0f * t - 1.0f) - polyBlep (t, dt);
            case 2:
            {
                float sq = t < 0.5f ? 1.0f : -1.0f;
                sq += polyBlep (t, dt);
                float t2 = t + 0.5f;
                if (t2 >= 1.0f)
                    t2 -= 1.0f;
                sq -= polyBlep (t2, dt);
                return sq * 0.8f; // matches the loudness of the saw
            }
            default: return 1.0f - 4.0f * std::abs (t - 0.5f);
        }
    }

    double phase = 0.0;
};

// Resonant low-pass state-variable filter (topology-preserving transform).
class ResonantLowPass
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate; reset(); }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float process (float x, float cutoff, float resonance) noexcept
    {
        cutoff = std::clamp (cutoff, 20.0f, (float) (sampleRate * 0.45));
        const float g = std::tan ((float) (3.141592653589793 * cutoff / sampleRate));
        const float k = 2.0f - 1.94f * std::clamp (resonance, 0.0f, 1.0f);
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

private:
    double sampleRate = 44100.0;
    float ic1 = 0.0f, ic2 = 0.0f;
};

// Saturation with constant peak level: drive 0 is clean, 1 is heavy.
inline float driveShape (float x, float drive) noexcept
{
    if (drive <= 0.001f)
        return x;

    const float gain = 1.0f + drive * 24.0f;
    return std::tanh (gain * x) / std::tanh (gain);
}

// Level follower for the bass: mean square through a two-pole low-pass, so the
// result rises and falls quickly but has little ripple even on a low B.
// Returns the peak level of an equivalent sine.
class EnvelopeFollower
{
public:
    void prepare (double sampleRate, float cutoffHz = 14.0f) noexcept
    {
        coeff = 1.0f - (float) std::exp (-6.283185307179586 * cutoffHz / sampleRate);
        reset();
    }

    void reset() noexcept { stage1 = stage2 = 0.0f; }

    float process (float x) noexcept
    {
        stage1 += (x * x - stage1) * coeff;
        stage2 += (stage1 - stage2) * coeff;
        return std::sqrt (2.0f * std::max (0.0f, stage2));
    }

private:
    float coeff = 0.0f, stage1 = 0.0f, stage2 = 0.0f;
};
}
