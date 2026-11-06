#pragma once

#include "SynthVoice.h"

#include <algorithm>
#include <cmath>

namespace subterra
{
// The synth voice, locked to the bass waveform itself rather than to a pitch estimate.
//
// A low-passed copy of the input is watched for zero crossings (with hysteresis). Every rising
// crossing starts a new cycle of the voice, so the voice follows the string cycle by cycle with no
// analysis window and no lookahead: the same principle as an analog octave divider. Counting cycles
// gives the sub-octaves (one or two flip-flop stages), and running the phase two or four times per
// cycle gives the octaves up. The waveform is band-limited (PolyBLEP).
//
// The voice can also run free at a given frequency (for Glide and Hold); it then ignores the crossings.
// The level follower holds the peak of each half cycle, so it rises at once and has no ripple on low notes.
class LockedVoice
{
public:
    static constexpr float minFrequency = 25.0f;
    static constexpr float maxFrequency = 1100.0f;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        dcCoef = (float) std::exp (-twoPi * 12.0 / sampleRate);
        levelAttack = 1.0f - (float) std::exp (-1.0 / (sampleRate * 0.0003));
        levelRelease = 1.0f - (float) std::exp (-1.0 / (sampleRate * 0.010));
        levelDecay = (float) std::exp (-1.0 / (sampleRate * 0.025));
        thresholdRelease = (float) std::exp (-1.0 / (sampleRate * 0.05));
        cutoffGlide = 1.0f - (float) std::exp (-(double) controlInterval / (sampleRate * 0.015));
        reset();
    }

    void reset() noexcept
    {
        dcIn = dcOut = 0.0f;
        ic1 = ic2 = 0.0f;
        setCutoff (restingReference * 1.3f);
        logCutoff = std::log2 (cutoff);
        lastReference = restingReference;
        controlCountdown = 0;
        filtered = 0.0f;
        filteredPeak = 0.0f;
        armedLow = armedHigh = false;
        now = 0.0;
        lastRise = lastFall = -1.0e9;
        lastCrossing = -1.0e9;
        periodSamples = 0.0;
        lastGoodPeriod = sampleRate / 80.0;
        measuredHz = 0.0f;
        phase = 0.0;
        lead = riseDelay = 0.0;
        cycle = 0;
        wrappedSinceSync = false;
        started = false;
        currentMax = heldMax = level = 0.0f;
    }

    // Analyses one input sample and moves the voice on by one sample.
    // referenceHz: the pitch tracker's reading (0 if none), used only to reject crossings from strong
    // harmonics and to place the trigger filter before the first cycle. sync: follow the bass cycle by cycle; otherwise run free
    // at freeIncrement (cycles per sample of the fundamental).
    void advance (float x, float referenceHz, bool sync, double freeIncrement) noexcept
    {
        now += 1.0;

        // DC blocker, then the trigger low-pass (two-pole state-variable, Q 0.707).
        const float hp = x - dcIn + dcCoef * dcOut;
        dcIn = x;
        dcOut = hp;

        const float v3 = hp - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        const float previous = filtered;
        filtered = v2;

        // Hysteresis follows the filtered level: 30% of its recent peak, so small ripples from harmonics
        // never count as a new cycle.
        filteredPeak = std::max (std::abs (filtered), filteredPeak * thresholdRelease);
        const float threshold = 0.3f * filteredPeak + 1.0e-6f;
        if (filtered < -threshold)
            armedLow = true;
        if (filtered > threshold)
            armedHigh = true;

        currentMax = std::max (currentMax, std::abs (hp));

        const double referencePeriod = referenceHz > 0.0f ? sampleRate / referenceHz : 0.0;
        const double shortest = std::max (sampleRate / maxFrequency, 0.65 * referencePeriod);
        const double longest = sampleRate / minFrequency;

        bool rose = false, fell = false;
        double crossingTime = 0.0;

        if (armedLow && previous < 0.0f && filtered >= 0.0f)
        {
            crossingTime = now - 1.0 + (double) (previous / (previous - filtered));
            armedLow = false;
            const double interval = crossingTime - lastRise;
            if (interval >= shortest)
            {
                rose = true;
                if (interval <= longest)
                {
                    // The trigger filter delays each crossing by its phase delay at the note, which moves
                    // when the note or the filter moves. Take the change out of the interval, so the
                    // period is the bass's own.
                    double delay = filterDelaySamples (interval);
                    const double corrected = interval - (delay - riseDelay);
                    delay = filterDelaySamples (corrected);
                    setPeriod (std::clamp (interval - (delay - riseDelay), shortest, longest));
                    riseDelay = delay;
                }
                else
                {
                    if (lastFall > lastRise && crossingTime - lastFall <= 0.5 * longest)
                        setPeriod (2.0 * (crossingTime - lastFall));   // first full cycle after silence: use the half cycle
                    riseDelay = filterDelaySamples (periodSamples > 0.0 ? periodSamples : lastGoodPeriod);
                }
                lastRise = crossingTime;
            }
        }
        else if (armedHigh && previous > 0.0f && filtered <= 0.0f)
        {
            crossingTime = now - 1.0 + (double) (previous / (previous - filtered));
            armedHigh = false;
            if (crossingTime - lastFall >= shortest)
            {
                fell = true;
                const double half = crossingTime - lastRise;
                if (periodSamples <= 0.0 && half > 0.5 * shortest && half <= 0.5 * longest)
                    setPeriod (2.0 * half);
                lastFall = crossingTime;
            }
        }

        if (rose || fell)
        {
            heldMax = currentMax;
            currentMax = 0.0f;
            lastCrossing = crossingTime;
        }

        // Without crossings (silence, or a note dying into noise) the level and the period expire.
        const double sinceCrossing = now - lastCrossing;
        if (sinceCrossing > std::max (2.5 * lastGoodPeriod, 0.03 * sampleRate))
        {
            heldMax *= levelDecay;
            currentMax *= levelDecay;
        }
        if (sinceCrossing > longest * 1.5)
        {
            periodSamples = 0.0;
            measuredHz = 0.0f;
            started = false;
        }

        // The trigger filter sits a little above the fundamental the pitch tracker hears, gliding there over
        // about 15 ms so that it never jolts the crossing times. It never follows the voice's own
        // measurement, which could lock onto a harmonic. Without a reading it stays where it was during a
        // note and rests low between notes.
        if (referenceHz > 0.0f)
            lastReference = referenceHz;
        else if (! started)
            lastReference = restingReference;
        const float reference = lastReference;
        if (--controlCountdown <= 0)
        {
            controlCountdown = controlInterval;
            const float wanted = std::log2 (std::clamp (reference * 1.3f, 40.0f, 700.0f));
            logCutoff += (wanted - logCutoff) * cutoffGlide;
            if (std::abs (logCutoff - std::log2 (cutoff)) > 0.002f)
                setCutoff (std::exp2 (logCutoff));
        }

        const float target = std::max (heldMax, currentMax);
        level += (target - level) * (target > level ? levelAttack : levelRelease);

        // Phase.
        const double syncIncrement = 1.0 / (periodSamples > 0.0 ? periodSamples : lastGoodPeriod);
        increment = sync ? syncIncrement : std::clamp (freeIncrement, 0.0, 0.45);

        bool placed = false;
        if (sync && (rose || fell))
        {
            const double past = now - crossingTime;   // how far past the crossing this sample is, 0..1

            if (rose)
            {
                if (! (wrappedSinceSync && phase < 0.5))
                    ++cycle;
                phase = past * syncIncrement;
                // The trigger filter delays the fundamental: at this crossing the bass's own cycle is
                // already that far on, so the voice is rendered that far ahead.
                lead = riseDelay * syncIncrement;
                started = placed = true;
            }
            else if (! started || periodSamples <= 0.0 || now - lastRise > 1.5 * periodSamples)
            {
                // A falling crossing places the cycle only before the first rising one.
                phase = 0.5 + past * syncIncrement;
                started = placed = true;
            }
            if (placed)
                wrappedSinceSync = false;
        }

        if (! placed)
        {
            phase += increment;
            if (phase >= 1.0)
            {
                // While locked, a cycle ends at the next rising crossing; if the bass is a little flat of the
                // last period, the phase waits at the end of the cycle for it instead of starting another.
                const bool waiting = sync && periodSamples > 0.0 && now - lastRise < 1.5 * periodSamples;
                if (waiting)
                {
                    phase = 0.99999;
                }
                else
                {
                    phase -= 1.0;
                    ++cycle;
                    wrappedSinceSync = true;
                }
            }
        }
        cycle &= 3;
    }

    // The voice's waveform at the given octave step (-2 .. +2) and wave morph (0 sine .. 1 triangle).
    float render (int octaveStep, float wave) const noexcept
    {
        // Position in a four-cycle span: continuous across each new cycle, so the dividers never jump.
        const double u = (double) (cycle & 3) + phase + lead;
        double t = u, dt = increment;
        switch (octaveStep)
        {
            case -2: t = u * 0.25; dt *= 0.25; break;
            case -1: t = u * 0.5; dt *= 0.5; break;
            case 1:  t = u * 2.0; dt *= 2.0; break;
            case 2:  t = u * 4.0; dt *= 4.0; break;
            default: break;
        }
        t -= std::floor (t);
        return MorphOscillator::render ((float) t, (float) dt, wave);
    }

    // Peak level of the bass, held over each half cycle.
    float getLevel() const noexcept { return level; }
    // The voice has found the bass's cycle and can play.
    bool isActive() const noexcept { return started; }
    // The bass frequency from the last full cycle, 0 when unknown.
    float getMeasuredFrequency() const noexcept { return measuredHz; }
    bool hasPeriod() const noexcept { return periodSamples > 0.0; }

private:
    void setPeriod (double samples) noexcept
    {
        periodSamples = samples;
        lastGoodPeriod = samples;
        measuredHz = (float) (sampleRate / samples);
    }

    void setCutoff (float hz) noexcept
    {
        cutoff = hz;
        const float g = (float) std::tan (3.141592653589793 * std::min ((double) hz, sampleRate * 0.45) / sampleRate);
        constexpr float k = 1.41421356f;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // Phase delay, in samples, of the trigger filters (DC blocker and low-pass) for a period of `samples`.
    double filterDelaySamples (double samples) const noexcept
    {
        const double hz = sampleRate / samples;
        const double r = hz / cutoff;
        const double lowPassLag = std::atan2 (1.41421356 * r, 1.0 - r * r);
        const double dcLead = std::atan2 (12.0, hz);
        return (lowPassLag - dcLead) / twoPi * samples;
    }

    static constexpr float restingReference = 80.0f;

    double sampleRate = 44100.0;
    float dcCoef = 0.998f, dcIn = 0.0f, dcOut = 0.0f;
    static constexpr int controlInterval = 16;
    int controlCountdown = 0;
    float lastReference = restingReference;
    float logCutoff = 6.7f, cutoffGlide = 0.01f;
    float cutoff = 104.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    float filtered = 0.0f, filteredPeak = 0.0f, thresholdRelease = 0.999f;
    bool armedLow = false, armedHigh = false;

    double now = 0.0, lastRise = -1.0e9, lastFall = -1.0e9, lastCrossing = -1.0e9;
    double periodSamples = 0.0, lastGoodPeriod = 551.0;
    float measuredHz = 0.0f;

    double phase = 0.0, increment = 0.0, lead = 0.0, riseDelay = 0.0;
    int cycle = 0;
    bool wrappedSinceSync = false, started = false;

    float currentMax = 0.0f, heldMax = 0.0f, level = 0.0f;
    float levelAttack = 0.1f, levelRelease = 0.002f, levelDecay = 0.999f;
};
}
