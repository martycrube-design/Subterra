#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace subterra
{
// Monophonic pitch tracker for bass. The input is low-passed, decimated to
// about 8 kHz and analysed with the YIN algorithm every hop.
class PitchTracker
{
public:
    static constexpr float minFrequency = 28.0f;    // a little under B0 (30.87 Hz)
    static constexpr float maxFrequency = 1000.0f;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        decimation = std::max (1, (int) (sampleRate / 8000.0));
        analysisRate = sampleRate / decimation;

        tauMin = std::max (2, (int) std::floor (analysisRate / maxFrequency));
        tauMax = (int) std::ceil (analysisRate / minFrequency);
        windowSize = tauMax + 8;
        bufferSize = windowSize + tauMax + 2;
        hopSize = std::max (16, (int) (analysisRate * 0.006));

        ring.assign ((size_t) bufferSize, 0.0f);
        frame.assign ((size_t) bufferSize, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        rawDiff.assign ((size_t) tauMax + 2, 0.0f);

        setLowPass (std::min (1200.0, analysisRate * 0.4));
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = 0;
        decimCounter = 0;
        hopCounter = 0;
        for (auto& s : lpState)
            s = {};
        frequency = 0.0f;
        confidence = 0.0f;
        history = { 0.0f, 0.0f, 0.0f };
        historyCount = 0;
    }

    // Feeds samples; returns true if a new estimate was made in this call.
    bool process (const float* input, int numSamples)
    {
        bool updated = false;

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = lowPass (input[i]);

            if (++decimCounter < decimation)
                continue;

            decimCounter = 0;
            ring[(size_t) writePos] = x;
            writePos = (writePos + 1) % bufferSize;

            if (++hopCounter >= hopSize)
            {
                hopCounter = 0;
                analyse();
                updated = true;
            }
        }

        return updated;
    }

    // Detected fundamental in Hz, or 0 when no clear pitch is present.
    float getFrequency() const noexcept   { return frequency; }
    float getConfidence() const noexcept  { return confidence; }
    double getAnalysisRate() const noexcept { return analysisRate; }

private:
    struct Biquad { float z1 = 0, z2 = 0; };

    void setLowPass (double cutoff)
    {
        // Two cascaded Butterworth sections (24 dB/oct) as anti-alias and
        // harmonic reduction before decimation.
        const double w0 = 2.0 * 3.141592653589793 * cutoff / sampleRate;
        const double cosw = std::cos (w0), sinw = std::sin (w0);
        const double qs[2] = { 0.5412, 1.3066 };

        for (int s = 0; s < 2; ++s)
        {
            const double alpha = sinw / (2.0 * qs[s]);
            const double a0 = 1.0 + alpha;
            coeffs[s][0] = (float) (((1.0 - cosw) * 0.5) / a0);
            coeffs[s][1] = (float) ((1.0 - cosw) / a0);
            coeffs[s][2] = coeffs[s][0];
            coeffs[s][3] = (float) ((-2.0 * cosw) / a0);
            coeffs[s][4] = (float) ((1.0 - alpha) / a0);
        }
    }

    float lowPass (float x)
    {
        for (int s = 0; s < 2; ++s)
        {
            auto& c = coeffs[s];
            auto& st = lpState[s];
            const float y = c[0] * x + st.z1;
            st.z1 = c[1] * x - c[3] * y + st.z2;
            st.z2 = c[2] * x - c[4] * y;
            x = y;
        }
        return x;
    }

    void analyse()
    {
        const int n = windowSize + tauMax;

        // Unroll the ring buffer, oldest sample first.
        int readPos = (writePos - n + bufferSize) % bufferSize;
        double energy = 0.0;
        for (int i = 0; i < n; ++i)
        {
            frame[(size_t) i] = ring[(size_t) readPos];
            energy += (double) frame[(size_t) i] * frame[(size_t) i];
            readPos = (readPos + 1) % bufferSize;
        }

        const double rms = std::sqrt (energy / n);
        if (rms < 3.0e-4) // about -70 dBFS: nothing to track
        {
            setUnvoiced();
            return;
        }

        // YIN difference function and cumulative mean normalised difference.
        diff[0] = 1.0f;
        double runningSum = 0.0;
        const float* x = frame.data();

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double d = 0.0;
            for (int j = 0; j < windowSize; ++j)
            {
                const float delta = x[j] - x[j + tau];
                d += (double) delta * delta;
            }
            runningSum += d;
            rawDiff[(size_t) tau] = (float) d;
            diff[(size_t) tau] = runningSum > 0.0 ? (float) (d * tau / runningSum) : 1.0f;
        }

        constexpr float threshold = 0.15f;
        int bestTau = -1;

        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (diff[(size_t) tau] < threshold)
            {
                while (tau + 1 <= tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                    ++tau;
                bestTau = tau;
                break;
            }
        }

        if (bestTau < 0)
        {
            // No dip under the threshold: accept the global minimum only if it is clear enough.
            float best = 1.0f;
            for (int tau = tauMin; tau <= tauMax; ++tau)
            {
                if (diff[(size_t) tau] < best)
                {
                    best = diff[(size_t) tau];
                    bestTau = tau;
                }
            }

            if (best > 0.35f)
            {
                setUnvoiced();
                return;
            }
        }

        // Parabolic interpolation on the raw difference function, which is less biased than the normalised one.
        double refined = bestTau;
        if (bestTau > tauMin && bestTau < tauMax)
        {
            const double a = rawDiff[(size_t) bestTau - 1];
            const double b = rawDiff[(size_t) bestTau];
            const double c = rawDiff[(size_t) bestTau + 1];
            const double denom = a - 2.0 * b + c;
            if (std::abs (denom) > 1.0e-9)
                refined += 0.5 * (a - c) / denom;
        }

        const float estimate = (float) (analysisRate / refined);
        if (estimate < minFrequency * 0.97f || estimate > maxFrequency)
        {
            setUnvoiced();
            return;
        }

        confidence = 1.0f - std::clamp (diff[(size_t) bestTau], 0.0f, 1.0f);
        pushEstimate (estimate);
    }

    void pushEstimate (float estimate)
    {
        history[2] = history[1];
        history[1] = history[0];
        history[0] = estimate;
        historyCount = std::min (historyCount + 1, 3);

        if (historyCount < 3)
        {
            frequency = estimate;
            return;
        }

        // Median of the last three estimates removes single-frame octave slips.
        auto a = history[0], b = history[1], c = history[2];
        frequency = std::max (std::min (a, b), std::min (std::max (a, b), c));
    }

    void setUnvoiced()
    {
        frequency = 0.0f;
        confidence = 0.0f;
        historyCount = 0;
    }

    double sampleRate = 44100.0, analysisRate = 8820.0;
    int decimation = 5, tauMin = 8, tauMax = 315, windowSize = 323, bufferSize = 640, hopSize = 52;
    int writePos = 0, decimCounter = 0, hopCounter = 0;
    std::vector<float> ring, frame, diff, rawDiff;
    float coeffs[2][5] {};
    Biquad lpState[2];
    float frequency = 0.0f, confidence = 0.0f;
    std::array<float, 3> history { 0.0f, 0.0f, 0.0f };
    int historyCount = 0;
};
}
