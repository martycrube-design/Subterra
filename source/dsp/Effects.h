#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace subterra
{
// Interpolating stereo delay line with a fixed maximum length, allocated in prepare().
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = juce::nextPowerOfTwo (maxSamples + 4);
        mask = size - 1;
        for (auto& b : buffer)
            b.assign ((size_t) size, 0.0f);
        writePos = 0;
    }

    void clear()
    {
        for (auto& b : buffer)
            std::fill (b.begin(), b.end(), 0.0f);
    }

    void push (float l, float r) noexcept
    {
        buffer[0][(size_t) writePos] = l;
        buffer[1][(size_t) writePos] = r;
        writePos = (writePos + 1) & mask;
    }

    // Reads `delay` samples behind the last pushed sample (delay >= 1), cubic interpolation.
    float read (int channel, float delay) const noexcept
    {
        delay = std::clamp (delay, 1.0f, (float) (size - 4));
        const float pos = (float) writePos - delay;
        const int i = (int) std::floor (pos);
        const float f = pos - (float) i;
        const auto& b = buffer[channel];
        const float y0 = b[(size_t) ((i - 1) & mask)];
        const float y1 = b[(size_t) (i & mask)];
        const float y2 = b[(size_t) ((i + 1) & mask)];
        const float y3 = b[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

private:
    std::vector<float> buffer[2];
    int size = 0, mask = 0, writePos = 0;
};

// Two-voice stereo chorus: LFOs 90 degrees apart sweep a short delay on each side.
class Chorus
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        line.prepare ((int) (sampleRate * 0.04));
        reset();
    }

    void reset()
    {
        line.clear();
        phase = 0.0;
    }

    // Keeps the delay line filled while the chorus is off, so switching it on fades in cleanly.
    void feed (float inL, float inR) noexcept { line.push (inL, inR); }

    // rate in Hz, depth 0..1. Returns the wet signal only.
    void process (float inL, float inR, float rate, float depth, float& outL, float& outR) noexcept
    {
        line.push (inL, inR);
        phase += rate / sampleRate;
        if (phase >= 1.0)
            phase -= 1.0;

        const float baseMs = 8.0f, sweepMs = 0.5f + 5.5f * depth;
        const float lfoL = (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        const float lfoR = (float) std::cos (juce::MathConstants<double>::twoPi * phase);
        const float msToSamples = (float) sampleRate * 0.001f;
        outL = line.read (0, (baseMs + sweepMs * lfoL) * msToSamples);
        outR = line.read (1, (baseMs + sweepMs * lfoR) * msToSamples);
    }

private:
    double sampleRate = 44100.0, phase = 0.0;
    DelayLine line;
};

// Stereo echo with filtered feedback. Time changes glide like a tape delay instead of clicking.
class Echo
{
public:
    static constexpr float maxSeconds = 2.5f;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        line.prepare ((int) (sampleRate * maxSeconds) + 8);
        timeCoef = 1.0f - (float) std::exp (-1.0 / (sampleRate * 0.12));
        lastToneHz = -1.0f;
        reset();
    }

    void reset()
    {
        line.clear();
        toneL = toneR = 0.0f;
        currentDelay = -1.0f;
    }

    void process (float inL, float inR, float seconds, float feedback, float toneHz,
                  float& outL, float& outR) noexcept
    {
        const float target = std::clamp (seconds, 0.001f, maxSeconds) * (float) sampleRate;
        if (currentDelay < 0.0f)
            currentDelay = target;
        currentDelay += (target - currentDelay) * timeCoef;

        float l = line.read (0, currentDelay);
        float r = line.read (1, currentDelay);

        // One-pole low-pass in the feedback path darkens each repeat.
        if (std::abs (toneHz - lastToneHz) > 0.01f)
        {
            lastToneHz = toneHz;
            toneCoef = 1.0f - (float) std::exp (-juce::MathConstants<double>::twoPi * toneHz / sampleRate);
        }
        const float a = toneCoef;
        toneL += (l - toneL) * a;
        toneR += (r - toneR) * a;

        const float fb = std::clamp (feedback, 0.0f, 0.95f);
        line.push (inL + softLimit (toneL * fb), inR + softLimit (toneR * fb));
        outL = l;
        outR = r;
    }

private:
    static float softLimit (float x) noexcept { return std::abs (x) < 1.0f ? x : std::tanh (x); }

    double sampleRate = 44100.0;
    DelayLine line;
    float toneL = 0.0f, toneR = 0.0f, currentDelay = -1.0f, timeCoef = 0.001f;
    float lastToneHz = -1.0f, toneCoef = 1.0f;
};

// Plate-style reverb built on JUCE's Freeverb, used 100% wet.
class Plate
{
public:
    void prepare (double sampleRate)
    {
        reverb.setSampleRate (sampleRate);
        reverb.reset();
    }

    void reset() { reverb.reset(); }

    void setParameters (float size, float tone)
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.30f + 0.68f * std::clamp (size, 0.0f, 1.0f);
        p.damping = 0.9f - 0.85f * std::clamp (tone, 0.0f, 1.0f);
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);
    }

    void process (float* left, float* right, int numSamples) { reverb.processStereo (left, right, numSamples); }

private:
    juce::Reverb reverb;
};
}
