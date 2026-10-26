// Offline checks for the Subterra engine: pitch tracking, every parameter,
// presets, A/B, state and numerical safety. Exit code is the number of failures.

#include "PluginProcessor.h"

#include <cstdio>
#include <functional>
#include <random>

using namespace subterra;

namespace
{
int failures = 0;
int checks = 0;

void expect (bool condition, const juce::String& what)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::printf ("  FAIL: %s\n", what.toRawUTF8());
    }
}

void section (const char* name) { std::printf ("%s\n", name); }

constexpr double testRate = 48000.0;
constexpr int testBlock = 256;

// A plucked bass-like tone: harmonics rolling off, exponential decay.
std::vector<float> bassNote (float hz, double seconds, float peak = 0.5f, double rate = testRate, float decay = 1.5f)
{
    const int n = (int) (seconds * rate);
    std::vector<float> out ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / rate;
        double s = 0.0;
        for (int h = 1; h <= 8; ++h)
            s += std::sin (2.0 * juce::MathConstants<double>::pi * hz * h * t) / (h * h * 0.6 + 0.4);
        const double envelope = std::min (1.0, t * 200.0) * std::exp (-t * decay);
        out[(size_t) i] = (float) (s * 0.55 * envelope * peak);
    }
    return out;
}

std::vector<float> silence (double seconds, double rate = testRate) { return std::vector<float> ((size_t) (seconds * rate), 0.0f); }

std::vector<float> concat (std::vector<float> a, const std::vector<float>& b)
{
    a.insert (a.end(), b.begin(), b.end());
    return a;
}

struct Render
{
    std::vector<float> left, right;
};

void setParam (SubterraProcessor& p, const char* id, float realValue)
{
    auto* param = p.getState().getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (realValue));
}

float getParam (SubterraProcessor& p, const char* id)
{
    auto* param = p.getState().getParameter (id);
    return param->convertFrom0to1 (param->getValue());
}

Render render (SubterraProcessor& p, const std::vector<float>& input, int block = testBlock,
               const std::function<void (int)>& perBlock = {})
{
    Render r;
    r.left.reserve (input.size());
    r.right.reserve (input.size());
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;

    for (size_t pos = 0, blockIndex = 0; pos < input.size(); pos += (size_t) block, ++blockIndex)
    {
        if (perBlock)
            perBlock ((int) blockIndex);

        const int n = (int) std::min ((size_t) block, input.size() - pos);
        buffer.setSize (2, n, false, false, true);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                buffer.setSample (ch, i, input[pos + (size_t) i]);

        p.processBlock (buffer, midi);

        for (int i = 0; i < n; ++i)
        {
            r.left.push_back (buffer.getSample (0, i));
            r.right.push_back (buffer.getSample (1, i));
        }
    }
    return r;
}

std::unique_ptr<SubterraProcessor> makeProcessor (double rate = testRate, int block = testBlock)
{
    auto p = std::make_unique<SubterraProcessor>();
    p->setPlayConfigDetails (2, 2, rate, block);
    p->prepareToPlay (rate, block);
    return p;
}

float rms (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double sum = 0.0;
    for (size_t i = from; i < to; ++i)
        sum += (double) x[i] * x[i];
    return to > from ? (float) std::sqrt (sum / (double) (to - from)) : 0.0f;
}

float difference (const std::vector<float>& a, const std::vector<float>& b)
{
    std::vector<float> d (std::min (a.size(), b.size()));
    for (size_t i = 0; i < d.size(); ++i)
        d[i] = a[i] - b[i];
    return rms (d);
}

// Fundamental estimate from a section of audio: autocorrelation peak with interpolation.
float estimatePitch (const std::vector<float>& x, size_t from, size_t to, float minHz = 12.0f, float maxHz = 1200.0f,
                     double rate = testRate)
{
    const int maxLag = (int) (rate / minHz), minLag = (int) (rate / maxHz);
    const int n = (int) (to - from) - maxLag;
    if (n <= 0)
        return 0.0f;

    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double sum = 0.0;
        for (int i = 0; i < n; i += 2)
            sum += (double) x[from + (size_t) i] * x[from + (size_t) (i + lag)];
        ac[(size_t) lag] = sum;
    }

    // First peak that reaches 85% of the largest one (avoids picking sub-multiples).
    double best = 0.0;
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        best = std::max (best, ac[(size_t) lag]);

    for (int lag = minLag + 1; lag <= maxLag; ++lag)
    {
        if (ac[(size_t) lag] >= 0.85 * best && ac[(size_t) lag] >= ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1])
        {
            const double a = ac[(size_t) lag - 1], b = ac[(size_t) lag], c = ac[(size_t) lag + 1];
            const double denom = a - 2.0 * b + c;
            const double refined = lag + (std::abs (denom) > 1e-12 ? 0.5 * (a - c) / denom : 0.0);
            return (float) (rate / refined);
        }
    }
    return 0.0f;
}

// Frequency of a sine from its rising zero crossings (exact for the sine wave setting).
float zeroCrossingPitch (const std::vector<float>& x, size_t from, size_t to, double rate = testRate)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (size_t i = from + 1; i < to; ++i)
    {
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    }
    return crossings > 1 ? (float) ((crossings - 1) * rate / (last - first)) : 0.0f;
}

bool allFinite (const Render& r)
{
    for (size_t i = 0; i < r.left.size(); ++i)
        if (! std::isfinite (r.left[i]) || ! std::isfinite (r.right[i]) || std::abs (r.left[i]) > 1.0e4f)
            return false;
    return true;
}

float centsBetween (float a, float b) { return 1200.0f * std::log2 (a / b); }

// ---------------------------------------------------------------------------

void testPitchTracker()
{
    section ("Pitch tracker");
    const float notes[] = { 30.87f, 41.20f, 49.0f, 55.0f, 73.42f, 98.0f, 146.8f, 196.0f, 261.6f, 392.0f };

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        for (float hz : notes)
        {
            PitchTracker tracker;
            tracker.prepare (rate);
            auto signal = bassNote (hz, 0.8, 0.5f, rate);

            std::vector<float> estimates;
            for (size_t pos = 0; pos < signal.size(); pos += 64)
            {
                const int n = (int) std::min<size_t> (64, signal.size() - pos);
                tracker.process (signal.data() + pos, n);
                if (pos > (size_t) (0.25 * rate))
                    estimates.push_back (tracker.getFrequency());
            }

            int good = 0;
            double sumCents = 0.0;
            for (float e : estimates)
                if (e > 0.0f)
                    sumCents += centsBetween (e, hz);
            if (std::getenv ("SUBTERRA_DEBUG"))
                std::printf ("    %.2f Hz @ %.0f: mean error %.2f cents\n", hz, rate, sumCents / (double) estimates.size());
            for (float e : estimates)
                good += (e > 0.0f && std::abs (centsBetween (e, hz)) < 10.0f) ? 1 : 0;

            const float ratio = (float) good / (float) estimates.size();
            expect (ratio > 0.97f, "tracks " + juce::String (hz) + " Hz at " + juce::String (rate) + " (" + juce::String (ratio * 100.0f, 1) + "% within 10 cents)");
        }
    }

    // Time to first stable estimate for a low E.
    PitchTracker tracker;
    tracker.prepare (testRate);
    auto signal = bassNote (41.2f, 0.5f);
    int firstGood = -1;
    for (size_t pos = 0; pos < signal.size() && firstGood < 0; pos += 32)
    {
        tracker.process (signal.data() + pos, 32);
        if (tracker.getFrequency() > 0.0f && std::abs (centsBetween (tracker.getFrequency(), 41.2f)) < 15.0f)
            firstGood = (int) pos;
    }
    const float latencyMs = (float) (firstGood * 1000.0 / testRate);
    std::printf ("  low E locks after %.1f ms\n", latencyMs);
    expect (firstGood > 0 && latencyMs < 90.0f, "low E locks within 90 ms");

    PitchTracker quiet;
    quiet.prepare (testRate);
    auto none = silence (0.3);
    quiet.process (none.data(), (int) none.size());
    expect (quiet.getFrequency() == 0.0f, "silence gives no pitch");

    std::mt19937 rng (1);
    std::normal_distribution<float> noise (0.0f, 0.2f);
    std::vector<float> n ((size_t) testRate / 2);
    for (auto& s : n)
        s = noise (rng);
    PitchTracker noisy;
    noisy.prepare (testRate);
    int voiced = 0, total = 0;
    for (size_t pos = 0; pos < n.size(); pos += 64)
    {
        noisy.process (n.data() + pos, 64);
        ++total;
        voiced += noisy.getFrequency() > 0.0f ? 1 : 0;
    }
    expect (voiced < total / 4, "white noise is mostly unvoiced (" + juce::String (voiced) + "/" + juce::String (total) + ")");
}

void testSynthFollowsBass()
{
    section ("Synth follows the bass");
    const float notes[] = { 41.2f, 55.0f, 82.4f, 110.0f, 196.0f };
    const int steps[] = { -2, -1, 0, 1, 2 };

    for (float hz : notes)
    {
        for (int stepIndex = 0; stepIndex < 5; ++stepIndex)
        {
            auto p = makeProcessor();
            setParam (*p, ParamIDs::mix, 1.0f);
            setParam (*p, ParamIDs::wave, 0.0f);       // sine, so the output pitch is unambiguous
            setParam (*p, ParamIDs::filter, 12000.0f);
            setParam (*p, ParamIDs::env, 0.0f);
            setParam (*p, ParamIDs::drive, 0.0f);
            setParam (*p, ParamIDs::reso, 0.0f);
            setParam (*p, ParamIDs::octave, (float) stepIndex);

            auto out = render (*p, bassNote (hz, 1.0, 0.5f, testRate, 0.5f));
            const float expected = hz * std::pow (2.0f, (float) steps[stepIndex]);
            const float measured = zeroCrossingPitch (out.left, (size_t) (0.4 * testRate), (size_t) (0.95 * testRate));
            expect (measured > 0.0f && std::abs (centsBetween (measured, expected)) < 8.0f,
                    juce::String (hz) + " Hz at octave " + juce::String (steps[stepIndex]) + ": expected "
                        + juce::String (expected, 2) + ", got " + juce::String (measured, 2));
        }
    }

    // Dynamics: a louder note gives a louder synth.
    auto level = [] (float peak)
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::mix, 1.0f);
        auto out = render (*p, bassNote (55.0f, 0.6, peak, testRate, 0.5f));
        return rms (out.left, (size_t) (0.2 * testRate), (size_t) (0.5 * testRate));
    };
    const float soft = level (0.1f), loud = level (0.6f);
    expect (loud > soft * 3.0f, "synth level follows playing dynamics (" + juce::String (soft, 4) + " vs " + juce::String (loud, 4) + ")");

    // Note release: after the bass stops, the synth stops.
    auto p = makeProcessor();
    setParam (*p, ParamIDs::mix, 1.0f);
    auto out = render (*p, concat (bassNote (55.0f, 0.5, 0.5f, testRate, 0.5f), silence (0.8)));
    const float during = rms (out.left, (size_t) (0.2 * testRate), (size_t) (0.45 * testRate));
    const float after = rms (out.left, (size_t) (1.0 * testRate), (size_t) (1.3 * testRate));
    expect (during > 0.02f, "synth sounds while playing");
    expect (after < during * 0.001f, "synth goes quiet after the note ends");
    expect (p->detectedFrequency.load() == 0.0f, "note readout clears after the note ends");
}

// Renders the same input twice, with a parameter at two values, and checks the outputs differ.
void expectAffects (const char* id, float a, float b, const std::vector<float>& input, float minDiff = 1.0e-3f,
                    const std::function<void (SubterraProcessor&)>& setup = {})
{
    auto run = [&] (float value)
    {
        auto p = makeProcessor();
        if (setup)
            setup (*p);
        setParam (*p, id, value);
        return render (*p, input);
    };

    const auto ra = run (a), rb = run (b);
    const float d = difference (ra.left, rb.left);
    expect (d > minDiff, juce::String (id) + " changes the sound (difference " + juce::String (d, 5) + ")");
    expect (allFinite (ra) && allFinite (rb), juce::String (id) + " output stays finite");
}

void testEveryControl()
{
    section ("Every control changes the sound");
    const auto input = bassNote (55.0f, 0.8, 0.5f, testRate, 0.8f);
    auto synthOnly = [] (SubterraProcessor& p) { setParam (p, ParamIDs::mix, 1.0f); };
    auto inFiltersOn = [] (SubterraProcessor& p) { setParam (p, ParamIDs::inFilterOn, 1.0f); };
    auto outFiltersOn = [] (SubterraProcessor& p) { setParam (p, ParamIDs::outFilterOn, 1.0f); };

    expectAffects (ParamIDs::inputGain, 0.0f, 12.0f, input);
    expectAffects (ParamIDs::outputGain, 0.0f, -12.0f, input);
    expectAffects (ParamIDs::inFilterOn, 0.0f, 1.0f, input, 1.0e-3f,
                   [] (SubterraProcessor& p) { setParam (p, ParamIDs::inLowPass, 600.0f); });
    expectAffects (ParamIDs::inHighPass, 20.0f, 400.0f, input, 1.0e-3f, inFiltersOn);
    expectAffects (ParamIDs::inLowPass, 20000.0f, 600.0f, input, 1.0e-3f, inFiltersOn);
    expectAffects (ParamIDs::outFilterOn, 0.0f, 1.0f, input, 1.0e-3f,
                   [] (SubterraProcessor& p) { setParam (p, ParamIDs::outHighPass, 300.0f); });
    expectAffects (ParamIDs::outHighPass, 20.0f, 400.0f, input, 1.0e-3f, outFiltersOn);
    expectAffects (ParamIDs::outLowPass, 20000.0f, 600.0f, input, 1.0e-3f, outFiltersOn);
    expectAffects (ParamIDs::wave, 0.0f, 0.667f, input, 1.0e-3f, synthOnly);
    expectAffects (ParamIDs::octave, 2.0f, 1.0f, input, 1.0e-3f, synthOnly);
    expectAffects (ParamIDs::filter, 200.0f, 4000.0f, input, 1.0e-3f, synthOnly);
    expectAffects (ParamIDs::reso, 0.0f, 0.9f, input, 1.0e-3f, synthOnly);
    expectAffects (ParamIDs::env, 0.0f, 1.0f, input, 1.0e-3f, synthOnly);
    expectAffects (ParamIDs::env, 0.0f, -1.0f, input, 1.0e-3f, synthOnly);
    expectAffects (ParamIDs::drive, 0.0f, 1.0f, input, 1.0e-3f,
                   [] (SubterraProcessor& p) { setParam (p, ParamIDs::mix, 1.0f); setParam (p, ParamIDs::wave, 0.0f); });
    expectAffects (ParamIDs::mix, 0.0f, 1.0f, input);
    expectAffects (ParamIDs::bypass, 0.0f, 1.0f, input);
    expectAffects (ParamIDs::tuner, 0.0f, 1.0f, input);

    // Mix extremes: 0 is the dry bass only, 1 is synth only.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::mix, 0.0f);
        p->prepareToPlay (testRate, testBlock);
        auto out = render (*p, input);
        expect (difference (out.left, input) < 1.0e-4f, "Mix at 0 passes the dry bass");
    }

    // Bypass is bit-exact once the crossfade is done.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::bypass, 1.0f);
        setParam (*p, ParamIDs::inputGain, 12.0f);
        setParam (*p, ParamIDs::outputGain, -12.0f);
        p->prepareToPlay (testRate, testBlock);
        auto out = render (*p, input);
        float maxErr = 0.0f;
        for (size_t i = 0; i < input.size(); ++i)
            maxErr = std::max (maxErr, std::abs (out.left[i] - input[i]));
        expect (maxErr < 1.0e-6f, "Bypass passes the input untouched (max error " + juce::String (maxErr) + ")");
    }

    // Tuner mutes while on, and passes audio when muting is turned off in settings.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::tuner, 1.0f);
        auto out = render (*p, input);
        expect (rms (out.left, 4096) < 1.0e-5f, "Tuner mutes the output");
        expect (p->detectedFrequency.load() > 50.0f && p->detectedFrequency.load() < 60.0f,
                "Tuner reads the note (" + juce::String (p->detectedFrequency.load()) + " Hz)");

        auto q = makeProcessor();
        q->tunerMutesOutput = false;
        setParam (*q, ParamIDs::tuner, 1.0f);
        auto open = render (*q, input);
        expect (rms (open.left, 4096) > 0.01f, "Tuner without muting passes audio");
    }

    // Input gain moves the input meter, output gain the output meter.
    {
        auto meters = [&] (float inDb, float outDb)
        {
            auto p = makeProcessor();
            setParam (*p, ParamIDs::inputGain, inDb);
            setParam (*p, ParamIDs::outputGain, outDb);
            render (*p, silence (0.1));
            p->inputMeters[0].take();
            p->outputMeters[0].take();
            render (*p, input);
            return std::pair { p->inputMeters[0].take(), p->outputMeters[0].take() };
        };
        const auto [inDefault, outDefault] = meters (0.0f, 0.0f);
        const auto [inBoosted, unused1] = meters (6.0f, 0.0f);
        const auto [unused2, outCut] = meters (0.0f, -6.0f);
        juce::ignoreUnused (unused1, unused2);
        expect (inDefault > 0.1f, "input meter shows signal");
        expect (inBoosted > inDefault * 1.8f, "input meter follows Input gain");
        expect (outDefault > 0.05f, "output meter shows signal");
        expect (std::abs (outCut / outDefault - 0.5f) < 0.03f, "output meter follows Output gain (" + juce::String (outDefault) + " -> " + juce::String (outCut) + ")");
    }

    // Tracking: a quiet note only triggers the synth with high sensitivity.
    {
        const auto quiet = bassNote (55.0f, 0.8, 0.012f, testRate, 0.3f);   // about -45 dBFS
        auto level = [&] (float tracking)
        {
            auto p = makeProcessor();
            setParam (*p, ParamIDs::mix, 1.0f);
            setParam (*p, ParamIDs::tracking, tracking);
            auto out = render (*p, quiet);
            return rms (out.left, (size_t) (0.3 * testRate));
        };
        const float low = level (0.0f), high = level (1.0f);
        expect (low < 1.0e-6f, "Tracking at 0 ignores a quiet note");
        expect (high > 1.0e-4f, "Tracking at 100% picks up a quiet note");
    }

    // Glide: time to move between two legato notes.
    {
        auto transition = [] (float glideMs)
        {
            auto p = makeProcessor();
            setParam (*p, ParamIDs::mix, 1.0f);
            setParam (*p, ParamIDs::wave, 0.0f);
            setParam (*p, ParamIDs::filter, 12000.0f);
            setParam (*p, ParamIDs::env, 0.0f);
            setParam (*p, ParamIDs::glide, glideMs);
            auto legato = concat (bassNote (55.0f, 0.6, 0.5f, testRate, 0.2f), bassNote (110.0f, 0.8, 0.5f, testRate, 0.2f));
            auto out = render (*p, legato);
            // Pitch 120 ms after the change
            const size_t at = (size_t) (0.72 * testRate);
            if (std::getenv ("SUBTERRA_DEBUG"))
                for (double t = 0.5; t < 1.3; t += 0.04)
                    std::printf ("    glide %.0f t=%.2f pitch=%.1f rms=%.3f\n", glideMs, t,
                                 estimatePitch (out.left, (size_t) (t * testRate), (size_t) ((t + 0.06) * testRate), 30.0f, 200.0f),
                                 rms (out.left, (size_t) (t * testRate), (size_t) ((t + 0.04) * testRate)));
            return estimatePitch (out.left, at, at + (size_t) (0.06 * testRate), 40.0f, 200.0f);
        };
        const float fast = transition (1.0f), slow = transition (1000.0f);
        std::printf ("  pitch 120 ms after a 55 -> 110 Hz change: glide off %.1f Hz, glide 1 s %.1f Hz\n", fast, slow);
        expect (std::abs (centsBetween (fast, 110.0f)) < 25.0f, "Glide off reaches the new note quickly");
        expect (slow > 50.0f && slow < 100.0f, "Long glide is still on its way to the new note");
    }

    // Hold: the synth keeps sounding after the bass stops.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::mix, 1.0f);
        const auto note = concat (bassNote (55.0f, 0.5, 0.5f, testRate, 0.5f), silence (1.0));
        const int engageBlock = (int) (0.3 * testRate / testBlock);
        auto out = render (*p, note, testBlock, [&] (int b) { if (b == engageBlock) setParam (*p, ParamIDs::hold, 1.0f); });
        const float held = rms (out.left, (size_t) (1.1 * testRate), (size_t) (1.4 * testRate));
        expect (held > 0.02f, "Hold sustains the synth after the bass stops (" + juce::String (held, 4) + ")");
        expect (p->holdEngaged.load(), "Hold LED state is on");
        const float heldPitch = estimatePitch (out.left, (size_t) (1.1 * testRate), (size_t) (1.4 * testRate), 30.0f, 120.0f);
        expect (std::abs (centsBetween (heldPitch, 55.0f)) < 10.0f, "Hold keeps the note's pitch (" + juce::String (heldPitch) + ")");

        setParam (*p, ParamIDs::hold, 0.0f);
        auto released = render (*p, silence (0.5));
        expect (rms (released.left, (size_t) (0.2 * testRate)) < 1.0e-4f, "releasing Hold lets the synth stop");
        expect (! p->holdEngaged.load(), "Hold LED state is off");
    }
}

void testPresetsAndState()
{
    section ("Presets, A/B and saved state");
    auto p = makeProcessor();
    auto& presets = p->getPresets();

    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("subterra-test-presets");
    temp.deleteRecursively();
    presets.setUserFolder (temp);

    expect (presets.getNumFactoryPresets() >= 6, "has factory presets");
    expect (presets.getCurrentName() == "Bedrock", "starts on Bedrock");

    juce::StringArray seen;
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        presets.loadPreset (i);
        seen.add (juce::String (getParam (*p, ParamIDs::filter)) + "/" + juce::String (getParam (*p, ParamIDs::wave)));
        expect (! presets.isModified(), "loading " + presets.getCurrentName() + " is not marked modified");
    }
    seen.removeDuplicates (false);
    expect (seen.size() == presets.getNumFactoryPresets(), "factory presets sound different from each other");

    presets.loadPreset (0);
    presets.loadNext();
    expect (presets.getCurrentIndex() == 1, "next preset");
    presets.loadPrevious();
    presets.loadPrevious();
    expect (presets.getCurrentIndex() == presets.getNumPresets() - 1, "previous wraps around");

    presets.loadPreset (2);
    setParam (*p, ParamIDs::filter, 3333.0f);
    expect (presets.isModified(), "editing a knob marks the preset modified");
    setParam (*p, ParamIDs::hold, 1.0f);
    setParam (*p, ParamIDs::hold, 0.0f);

    expect (presets.saveUserPreset ("My Growl"), "saves a user preset");
    expect (temp.getChildFile ("My Growl.subterrapreset").existsAsFile(), "user preset file written");
    expect (presets.getCurrentName() == "My Growl" && ! presets.isModified(), "saved preset becomes current");

    presets.loadPreset (0);
    expect (std::abs (getParam (*p, ParamIDs::filter) - 3333.0f) > 1.0f, "loading another preset changes the knob");
    int userIndex = -1;
    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (presets.getPreset (i).name == "My Growl")
            userIndex = i;
    expect (userIndex >= presets.getNumFactoryPresets(), "user preset is listed after the factory ones");
    presets.loadPreset (userIndex);
    expect (std::abs (getParam (*p, ParamIDs::filter) - 3333.0f) < 1.0f, "user preset restores its knobs");

    // Presets don't touch the performance switches.
    setParam (*p, ParamIDs::bypass, 1.0f);
    presets.loadPreset (1);
    expect (getParam (*p, ParamIDs::bypass) > 0.5f, "presets leave Bypass alone");
    setParam (*p, ParamIDs::bypass, 0.0f);

    // A/B
    presets.loadPreset (0);
    setParam (*p, ParamIDs::drive, 0.9f);
    expect (p->getActiveSlot() == 0, "starts on A");
    p->switchSlot();
    expect (p->getActiveSlot() == 1, "switches to B");
    expect (std::abs (getParam (*p, ParamIDs::drive) - 0.9f) < 1.0e-3f, "B starts as a copy of A");
    setParam (*p, ParamIDs::drive, 0.1f);
    p->switchSlot();
    expect (std::abs (getParam (*p, ParamIDs::drive) - 0.9f) < 1.0e-3f, "back on A restores A's knobs");
    p->switchSlot();
    expect (std::abs (getParam (*p, ParamIDs::drive) - 0.1f) < 1.0e-3f, "back on B restores B's knobs");
    p->copyActiveSlotToOther();
    p->switchSlot();
    expect (std::abs (getParam (*p, ParamIDs::drive) - 0.1f) < 1.0e-3f, "copy B to A");

    // Session state round trip.
    setParam (*p, ParamIDs::reso, 0.77f);
    setParam (*p, ParamIDs::octave, 0.0f);
    setParam (*p, ParamIDs::outFilterOn, 1.0f);
    p->tunerReference = 442.0f;
    p->tunerMutesOutput = false;
    juce::MemoryBlock saved;
    p->getStateInformation (saved);

    auto q = makeProcessor();
    q->setStateInformation (saved.getData(), (int) saved.getSize());
    expect (std::abs (getParam (*q, ParamIDs::reso) - 0.77f) < 1.0e-3f, "state restores Reso");
    expect ((int) getParam (*q, ParamIDs::octave) == 0, "state restores Octave");
    expect (getParam (*q, ParamIDs::outFilterOn) > 0.5f, "state restores Out Filters");
    expect (std::abs (q->tunerReference.load() - 442.0f) < 0.01f, "state restores tuner reference");
    expect (! q->tunerMutesOutput.load(), "state restores tuner mute setting");
    expect (q->getActiveSlot() == p->getActiveSlot(), "state restores the active A/B slot");
    expect (q->getPresets().getCurrentName() == p->getPresets().getCurrentName(), "state restores preset name");

    // Saving twice doesn't nest extra data.
    juce::MemoryBlock again;
    q->getStateInformation (again);
    auto r = makeProcessor();
    r->setStateInformation (again.getData(), (int) again.getSize());
    expect (std::abs (getParam (*r, ParamIDs::reso) - 0.77f) < 1.0e-3f, "state survives a second round trip");

    // Programs exposed to the host.
    expect (p->getNumPrograms() == presets.getNumFactoryPresets(), "host sees factory presets as programs");
    p->setCurrentProgram (3);
    expect (p->getCurrentProgram() == 3 && presets.getCurrentName() == p->getProgramName (3), "host can change program");

    temp.deleteRecursively();
}

void testRobustness()
{
    section ("Robustness");
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);

    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (int block : { 1, 17, 64, 512, 2048 })
        {
            auto p = makeProcessor (rate, block);
            for (auto* param : p->getParameters())
                param->setValueNotifyingHost (uni (rng));
            setParam (*p, ParamIDs::bypass, 0.0f);
            setParam (*p, ParamIDs::tuner, 0.0f);

            auto input = concat (bassNote (41.2f, 0.4, 0.9f, rate), silence (0.1, rate));
            std::normal_distribution<float> noise (0.0f, 0.5f);
            for (int i = 0; i < (int) (0.2 * rate); ++i)
                input.push_back (noise (rng));
            input.push_back (1.0f);
            input.push_back (-1.0f);

            auto out = render (*p, input, block);
            expect (allFinite (out), "finite output at " + juce::String (rate) + " Hz, block " + juce::String (block));
        }
    }

    // Extreme settings.
    auto p = makeProcessor();
    setParam (*p, ParamIDs::inputGain, 24.0f);
    setParam (*p, ParamIDs::outputGain, 24.0f);
    setParam (*p, ParamIDs::reso, 1.0f);
    setParam (*p, ParamIDs::drive, 1.0f);
    setParam (*p, ParamIDs::env, 1.0f);
    setParam (*p, ParamIDs::filter, 12000.0f);
    setParam (*p, ParamIDs::octave, 4.0f);
    setParam (*p, ParamIDs::mix, 1.0f);
    auto out = render (*p, bassNote (392.0f, 0.5, 1.0f));
    expect (allFinite (out), "finite output at extreme settings");

    // Mono in / stereo out and mono / mono layouts.
    for (auto [ins, outs] : { std::pair { 1, 2 }, std::pair { 1, 1 } })
    {
        SubterraProcessor m;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (ins == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        layout.outputBuses.add (outs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        expect (m.setBusesLayout (layout), "supports " + juce::String (ins) + " in / " + juce::String (outs) + " out");
        m.prepareToPlay (testRate, testBlock);
        juce::AudioBuffer<float> buffer (std::max (ins, outs), testBlock);
        auto note = bassNote (55.0f, 0.5);
        juce::MidiBuffer midi;
        float energy = 0.0f;
        for (size_t pos = 0; pos + testBlock <= note.size(); pos += testBlock)
        {
            buffer.clear();
            for (int i = 0; i < testBlock; ++i)
                buffer.setSample (0, i, note[pos + (size_t) i]);
            m.processBlock (buffer, midi);
            energy += buffer.getRMSLevel (0, 0, testBlock);
        }
        expect (energy > 0.1f && std::isfinite (energy), "audio flows with " + juce::String (ins) + " in / " + juce::String (outs) + " out");
    }

    SubterraProcessor s;
    juce::AudioProcessor::BusesLayout stereoToMono;
    stereoToMono.inputBuses.add (juce::AudioChannelSet::stereo());
    stereoToMono.outputBuses.add (juce::AudioChannelSet::mono());
    expect (! s.checkBusesLayoutSupported (stereoToMono), "rejects stereo in / mono out");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    testPitchTracker();
    testSynthFollowsBass();
    testEveryControl();
    testPresetsAndState();
    testRobustness();

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures;
}
