// Offline checks for the Subterra engine: pitch tracking, every parameter,
// presets, A/B, state and numerical safety. Exit code is the number of failures.

#include "PluginProcessor.h"

#include <cstdio>
#include <functional>
#include <random>
#include <thread>

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
    expect (juce::exactlyEqual (quiet.getFrequency(), 0.0f), "silence gives no pitch");

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
    expect (juce::exactlyEqual (p->detectedFrequency.load(), 0.0f), "note readout clears after the note ends");
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

    expect (presets.getNumFactoryPresets() >= 12, "has factory presets");
    expect (presets.unknownFactoryIds.isEmpty(), "factory presets only set real sound parameters: " + presets.unknownFactoryIds.joinIntoString (", "));
    expect (presets.getCurrentName() == "Bedrock", "starts on Bedrock");

    juce::StringArray seen;
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        presets.loadPreset (i);
        auto sound = presets.captureSound();
        sound.removeProperty ("name", nullptr);
        seen.add (sound.toXmlString());
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

// ---------------------------------------------------------------------------
// Effects

struct FakePlayHead final : juce::AudioPlayHead
{
    double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        return info;
    }
};

// The dry bass on its own through the effects: Mix 0 (no synth), routing Synth + Bass.
std::unique_ptr<SubterraProcessor> fxProcessor (double rate = testRate, int block = testBlock)
{
    auto p = makeProcessor (rate, block);
    setParam (*p, ParamIDs::mix, 0.0f);
    setParam (*p, ParamIDs::fxRouting, 1.0f);
    render (*p, silence (0.05, rate), block);   // let the routing settle
    return p;
}

std::vector<float> impulses (double seconds, std::initializer_list<double> times, double rate = testRate)
{
    auto x = silence (seconds, rate);
    for (auto t : times)
        x[(size_t) (t * rate)] = 0.5f;
    return x;
}

// Sum of samples in a window: one-pole filtering keeps it, so it measures a repeat's level.
float windowSum (const std::vector<float>& x, size_t centre, size_t half = 60)
{
    double sum = 0.0;
    for (size_t i = centre - half; i < centre + half && i < x.size(); ++i)
        sum += x[i];
    return (float) sum;
}

size_t peakIndex (const std::vector<float>& x, size_t from, size_t to)
{
    size_t best = from;
    for (size_t i = from; i < std::min (to, x.size()); ++i)
        if (std::abs (x[i]) > std::abs (x[best]))
            best = i;
    return best;
}

void testEffects()
{
    section ("Effects");
    const auto note = concat (bassNote (55.0f, 0.8, 0.5f, testRate, 0.8f), silence (1.2));

    auto with = [] (std::initializer_list<std::pair<const char*, float>> values)
    {
        return [values] (SubterraProcessor& p) { for (auto& [id, v] : values) setParam (p, id, v); };
    };

    // Every effect control changes the sound when its effect is on.
    expectAffects (ParamIDs::chorusOn, 0.0f, 1.0f, note, 1.0e-3f, with ({ { ParamIDs::chorusMix, 1.0f } }));
    expectAffects (ParamIDs::chorusRate, 0.1f, 4.0f, note, 1.0e-4f, with ({ { ParamIDs::chorusOn, 1.0f }, { ParamIDs::chorusMix, 1.0f } }));
    expectAffects (ParamIDs::chorusDepth, 0.0f, 1.0f, note, 1.0e-4f, with ({ { ParamIDs::chorusOn, 1.0f }, { ParamIDs::chorusMix, 1.0f } }));
    expectAffects (ParamIDs::chorusMix, 0.0f, 1.0f, note, 1.0e-3f, with ({ { ParamIDs::chorusOn, 1.0f } }));
    expectAffects (ParamIDs::delayOn, 0.0f, 1.0f, note, 1.0e-3f, with ({}));
    expectAffects (ParamIDs::delayTime, 120.0f, 600.0f, note, 1.0e-3f, with ({ { ParamIDs::delayOn, 1.0f }, { ParamIDs::delaySync, 0.0f } }));
    expectAffects (ParamIDs::delayDivision, 2.0f, 5.0f, note, 1.0e-3f, with ({ { ParamIDs::delayOn, 1.0f } }));
    expectAffects (ParamIDs::delaySync, 0.0f, 1.0f, note, 1.0e-3f, with ({ { ParamIDs::delayOn, 1.0f }, { ParamIDs::delayTime, 120.0f } }));
    expectAffects (ParamIDs::delayFeedback, 0.0f, 0.8f, note, 1.0e-3f, with ({ { ParamIDs::delayOn, 1.0f } }));
    expectAffects (ParamIDs::delayTone, 500.0f, 12000.0f, note, 1.0e-4f, with ({ { ParamIDs::delayOn, 1.0f }, { ParamIDs::delayFeedback, 0.7f } }));
    expectAffects (ParamIDs::delayMix, 0.1f, 1.0f, note, 1.0e-3f, with ({ { ParamIDs::delayOn, 1.0f } }));
    expectAffects (ParamIDs::reverbOn, 0.0f, 1.0f, note, 1.0e-3f, with ({}));
    expectAffects (ParamIDs::reverbSize, 0.0f, 1.0f, note, 1.0e-4f, with ({ { ParamIDs::reverbOn, 1.0f }, { ParamIDs::reverbMix, 1.0f } }));
    expectAffects (ParamIDs::reverbTone, 0.0f, 1.0f, note, 1.0e-4f, with ({ { ParamIDs::reverbOn, 1.0f }, { ParamIDs::reverbMix, 1.0f } }));
    expectAffects (ParamIDs::reverbMix, 0.1f, 1.0f, note, 1.0e-3f, with ({ { ParamIDs::reverbOn, 1.0f } }));
    expectAffects (ParamIDs::fxRouting, 0.0f, 1.0f, note, 1.0e-3f, with ({ { ParamIDs::reverbOn, 1.0f }, { ParamIDs::reverbMix, 1.0f } }));

    // Effects that are off leave the sound exactly as it was.
    {
        auto plain = makeProcessor();
        auto tweaked = makeProcessor();
        for (auto [id, v] : { std::pair { ParamIDs::chorusMix, 1.0f }, { ParamIDs::delayMix, 1.0f }, { ParamIDs::delayFeedback, 0.9f },
                              { ParamIDs::reverbMix, 1.0f }, { ParamIDs::reverbSize, 1.0f }, { ParamIDs::fxRouting, 1.0f } })
            setParam (*tweaked, id, v);
        expect (difference (render (*plain, note).left, render (*tweaked, note).left) < 1.0e-6f, "effect knobs do nothing while the effects are off");
    }

    // Routing "Synth": with Mix at 0 there's no synth, so the effects add nothing to the dry bass.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::mix, 0.0f);
        for (auto* id : { ParamIDs::chorusOn, ParamIDs::delayOn, ParamIDs::reverbOn })
            setParam (*p, id, 1.0f);
        p->prepareToPlay (testRate, testBlock);
        expect (difference (render (*p, note).left, note) < 1.0e-4f, "routing Synth keeps the dry bass out of the effects");
    }

    // Delay timing, free.
    {
        auto p = fxProcessor();
        setParam (*p, ParamIDs::delayOn, 1.0f);
        setParam (*p, ParamIDs::delaySync, 0.0f);
        setParam (*p, ParamIDs::delayTime, 300.0f);
        setParam (*p, ParamIDs::delayFeedback, 0.5f);
        setParam (*p, ParamIDs::delayTone, 16000.0f);
        setParam (*p, ParamIDs::delayMix, 1.0f);
        auto out = render (*p, impulses (1.2, { 0.1 })).left;
        const size_t hit = (size_t) (0.1 * testRate);
        const size_t first = peakIndex (out, hit + 100, hit + (size_t) (0.5 * testRate));
        const double firstMs = (double) (first - hit) * 1000.0 / testRate;
        expect (std::abs (firstMs - 300.0) < 0.5, "Delay Time 300 ms repeats after " + juce::String (firstMs, 2) + " ms");
        const float r1 = windowSum (out, hit + 14400), r2 = windowSum (out, hit + 28800);
        expect (std::abs (r1 - 0.5f) < 0.01f, "first repeat at full level with Mix 100% (" + juce::String (r1, 3) + ")");
        expect (std::abs (r2 / r1 - 0.5f) < 0.03f, "Feedback 50% halves each repeat (" + juce::String (r2 / r1, 3) + ")");
    }

    // Delay timing, synced to the host tempo.
    for (auto [bpm, division, beats] : { std::tuple { 100.0, 5, 1.0 }, std::tuple { 100.0, 3, 0.75 }, std::tuple { 140.0, 2, 0.5 },
                                         std::tuple { 90.0, 1, 1.0 / 3.0 } })
    {
        FakePlayHead head;
        head.bpm = bpm;
        auto p = fxProcessor();
        p->setPlayHead (&head);
        setParam (*p, ParamIDs::delayOn, 1.0f);
        setParam (*p, ParamIDs::delaySync, 1.0f);
        setParam (*p, ParamIDs::delayDivision, (float) division);
        setParam (*p, ParamIDs::delayFeedback, 0.0f);
        setParam (*p, ParamIDs::delayMix, 1.0f);
        render (*p, silence (0.3));   // the delay time glides to the new tempo like tape
        auto out = render (*p, impulses (1.5, { 0.1 })).left;
        const size_t hit = (size_t) (0.1 * testRate);
        const double ms = (double) (peakIndex (out, hit + 100, out.size()) - hit) * 1000.0 / testRate;
        const double expected = beats * 60000.0 / bpm;
        expect (std::abs (ms - expected) < 1.0, "synced delay at " + juce::String (bpm) + " BPM, division " + juce::String (division)
                                                  + ": " + juce::String (ms, 1) + " ms (expected " + juce::String (expected, 1) + ")");
        expect (std::abs (p->hostTempo.load() - (float) bpm) < 0.01f, "host tempo is read from the play head");
        p->setPlayHead (nullptr);
    }

    // Very slow tempo with the longest division stays within the delay memory.
    {
        FakePlayHead head;
        head.bpm = 20.0;
        auto p = fxProcessor();
        p->setPlayHead (&head);
        setParam (*p, ParamIDs::delayOn, 1.0f);
        setParam (*p, ParamIDs::delayDivision, 7.0f);
        setParam (*p, ParamIDs::delayFeedback, 0.95f);
        setParam (*p, ParamIDs::delayMix, 1.0f);
        auto out = render (*p, concat (note, note));
        expect (allFinite (out), "6 s synced delay at 20 BPM stays finite");
        p->setPlayHead (nullptr);
    }

    // Trails: switching the delay off stops new input but lets the repeats ring out.
    {
        auto p = fxProcessor();
        setParam (*p, ParamIDs::delayOn, 1.0f);
        setParam (*p, ParamIDs::delaySync, 0.0f);
        setParam (*p, ParamIDs::delayTime, 200.0f);
        setParam (*p, ParamIDs::delayFeedback, 0.6f);
        setParam (*p, ParamIDs::delayTone, 16000.0f);
        setParam (*p, ParamIDs::delayMix, 1.0f);
        const auto input = concat (impulses (1.0, { 0.1, 0.45 }), silence (12.0));
        const int offBlock = (int) (0.35 * testRate / testBlock);
        auto out = render (*p, input, testBlock, [&] (int b) { if (b == offBlock) setParam (*p, ParamIDs::delayOn, 0.0f); }).left;
        const size_t hit = (size_t) (0.1 * testRate), d = (size_t) (0.2 * testRate);
        expect (windowSum (out, hit + 2 * d) > 0.15f, "delay repeats keep going after it's switched off");
        expect (std::abs (windowSum (out, (size_t) (0.45 * testRate) + d)) < 0.02f, "a switched-off delay takes no new input");
        expect (rms (out, out.size() - (size_t) testRate) < 1.0e-7f, "the repeats die away completely");
    }

    // Reverb tail, its length set by Size, and trails.
    {
        auto tail = [] (float size)
        {
            auto p = fxProcessor();
            setParam (*p, ParamIDs::reverbOn, 1.0f);
            setParam (*p, ParamIDs::reverbSize, size);
            setParam (*p, ParamIDs::reverbMix, 1.0f);
            return render (*p, impulses (3.0, { 0.1 })).left;
        };
        const auto small = tail (0.0f), large = tail (1.0f);
        const float smallLate = rms (small, (size_t) (1.0 * testRate), (size_t) (1.5 * testRate));
        const float largeLate = rms (large, (size_t) (1.0 * testRate), (size_t) (1.5 * testRate));
        expect (rms (large, (size_t) (0.2 * testRate), (size_t) (0.6 * testRate)) > 1.0e-3f, "reverb produces a tail");
        expect (largeLate > smallLate * 4.0f, "a bigger Size rings longer (" + juce::String (smallLate, 6) + " vs " + juce::String (largeLate, 6) + ")");

        auto p = fxProcessor();
        setParam (*p, ParamIDs::reverbOn, 1.0f);
        setParam (*p, ParamIDs::reverbSize, 0.8f);
        setParam (*p, ParamIDs::reverbMix, 1.0f);
        const int offBlock = (int) (0.2 * testRate / testBlock);
        auto out = render (*p, concat (impulses (1.0, { 0.1 }), silence (25.0)), testBlock,
                           [&] (int b) { if (b == offBlock) setParam (*p, ParamIDs::reverbOn, 0.0f); }).left;
        expect (rms (out, (size_t) (0.3 * testRate), (size_t) (0.6 * testRate)) > 1.0e-3f, "reverb tail rings out after switching off");
        expect (rms (out, out.size() - (size_t) testRate) < 1.0e-7f, "the reverb tail dies away completely");
    }

    // Chorus is stereo: the left and right sides differ, while without it they match.
    {
        auto p = fxProcessor();
        setParam (*p, ParamIDs::chorusOn, 1.0f);
        setParam (*p, ParamIDs::chorusMix, 1.0f);
        setParam (*p, ParamIDs::chorusDepth, 1.0f);
        auto out = render (*p, note);
        expect (difference (out.left, out.right) > 1.0e-3f, "chorus widens to stereo");

        auto q = fxProcessor();
        auto mono = render (*q, note);
        expect (difference (mono.left, mono.right) < 1.0e-7f, "without effects a mono input stays centred");
    }

    // Runaway feedback is kept in check.
    {
        auto p = fxProcessor();
        setParam (*p, ParamIDs::inputGain, 24.0f);
        setParam (*p, ParamIDs::delayOn, 1.0f);
        setParam (*p, ParamIDs::delaySync, 0.0f);
        setParam (*p, ParamIDs::delayTime, 10.0f);
        setParam (*p, ParamIDs::delayFeedback, 0.95f);
        setParam (*p, ParamIDs::delayTone, 16000.0f);
        setParam (*p, ParamIDs::delayMix, 1.0f);
        setParam (*p, ParamIDs::reverbOn, 1.0f);
        setParam (*p, ParamIDs::reverbMix, 1.0f);
        setParam (*p, ParamIDs::reverbSize, 1.0f);
        auto out = render (*p, concat (note, note));
        float peak = 0.0f;
        for (auto v : out.left)
            peak = std::max (peak, std::abs (v));
        expect (allFinite (out) && peak < 40.0f, "maximum feedback stays bounded (peak " + juce::String (peak, 2) + ")");
    }

    // Changing sample rate keeps delay times right.
    {
        auto p = fxProcessor (44100.0);
        p->prepareToPlay (96000.0, 512);
        setParam (*p, ParamIDs::delayOn, 1.0f);
        setParam (*p, ParamIDs::delaySync, 0.0f);
        setParam (*p, ParamIDs::delayTime, 250.0f);
        setParam (*p, ParamIDs::delayFeedback, 0.0f);
        setParam (*p, ParamIDs::delayMix, 1.0f);
        render (*p, silence (0.1, 96000.0), 512);
        auto out = render (*p, impulses (0.6, { 0.1 }, 96000.0), 512).left;
        const size_t hit = (size_t) (0.1 * 96000.0);
        const double ms = (double) (peakIndex (out, hit + 100, out.size()) - hit) * 1000.0 / 96000.0;
        expect (std::abs (ms - 250.0) < 0.5, "delay time is right after a sample-rate change (" + juce::String (ms, 2) + " ms)");
    }
}

// ---------------------------------------------------------------------------
// Playing like a real bass: plucked strings with noise and hum, and legato lines.

std::vector<float> pluckedString (float hz, double seconds, std::mt19937& rng, float peak = 0.6f)
{
    // Karplus-Strong: a softened noise burst circulating in a lossy delay loop one period long.
    const int n = (int) (seconds * testRate);
    const auto len = (size_t) std::floor (testRate / hz);
    std::uniform_real_distribution<float> burst (-1.0f, 1.0f);
    std::vector<float> loop (len);
    for (auto& v : loop)
        v = burst (rng);
    for (int pass = 0; pass < 3; ++pass)
        for (size_t i = 1; i < len; ++i)
            loop[i] = 0.5f * (loop[i] + loop[i - 1]);

    std::vector<float> out ((size_t) n);
    float prev = 0.0f;
    for (size_t i = 0, pos = 0; i < (size_t) n; ++i, pos = (pos + 1) % len)
    {
        const float x = loop[pos];
        loop[pos] = 0.997f * 0.5f * (x + prev);
        prev = x;
        out[i] = x * peak * std::min (1.0f, (float) i / 48.0f);
    }
    return out;
}

void addNoiseAndHum (std::vector<float>& x, std::mt19937& rng)
{
    // Pickup noise around -66 dBFS and 60 Hz mains hum at -60 dBFS, like a single-coil bass into an interface.
    std::normal_distribution<float> noise (0.0f, 0.0005f);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] += noise (rng) + 0.001f * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * (double) i / testRate);
}

void testRealPlaying()
{
    section ("Playing like a real bass");
    std::mt19937 rng (11);

    // Each note of the four open strings and some fretted notes, plucked with noise and mains hum.
    const float notes[] = { 41.2f, 55.0f, 73.42f, 98.0f, 49.0f, 65.41f, 87.31f, 130.8f };
    std::vector<float> line;
    for (float hz : notes)
        line = concat (line, pluckedString (hz, 0.6, rng));
    line = concat (line, silence (0.4));
    addNoiseAndHum (line, rng);

    auto p = makeProcessor();
    std::vector<float> readings;
    render (*p, line, testBlock, [&] (int) { readings.push_back (p->detectedFrequency.load()); });

    const double blockSeconds = (double) testBlock / testRate;
    for (size_t n = 0; n < std::size (notes); ++n)
    {
        // Reading 0.3 s into each note (the length of string's own tuning is approximate, hence 15 cents).
        const size_t block = (size_t) (((double) n * 0.6 + 0.3) / blockSeconds);
        const float heard = readings[block];
        const float tuneHz = (float) (testRate / std::floor (testRate / notes[n]));   // pitch the integer string length gives
        expect (heard > 0.0f && std::abs (centsBetween (heard, tuneHz)) < 15.0f,
                "plucked " + juce::String (notes[n]) + " Hz read as " + juce::String (heard, 2) + " Hz");
    }
    expect (juce::exactlyEqual (readings.back(), 0.0f), "readout clears after the line ends, with only noise and hum left ("
                                                         + juce::String (readings.back(), 2) + " Hz)");

    // A very noisy instrument (hum at -40 dBFS) doesn't trigger the synth on its own once Tracking is turned down.
    {
        auto q = makeProcessor();
        setParam (*q, ParamIDs::mix, 1.0f);
        setParam (*q, ParamIDs::tracking, 0.3f);
        std::vector<float> hum ((size_t) (2.0 * testRate));
        for (size_t i = 0; i < hum.size(); ++i)
            hum[i] = 0.01f * (float) std::sin (juce::MathConstants<double>::twoPi * 60.0 * (double) i / testRate);
        auto out = render (*q, hum);
        expect (rms (out.left, (size_t) testRate) < 1.0e-4f, "hum alone doesn't start the synth with Tracking at 30%");
    }

    // The synth plays the right note.
    {
        auto q = makeProcessor();
        setParam (*q, ParamIDs::mix, 1.0f);
        setParam (*q, ParamIDs::wave, 0.0f);
        setParam (*q, ParamIDs::filter, 12000.0f);
        setParam (*q, ParamIDs::env, 0.0f);
        setParam (*q, ParamIDs::glide, 1.0f);
        auto pluck = pluckedString (55.0f, 1.0, rng);
        addNoiseAndHum (pluck, rng);
        auto out = render (*q, pluck);
        const float synthHz = zeroCrossingPitch (out.left, (size_t) (0.3 * testRate), (size_t) (0.7 * testRate));
        const float tuneHz = (float) (testRate / std::floor (testRate / 55.0));
        expect (std::abs (centsBetween (synthHz, tuneHz)) < 10.0f, "synth plays a plucked A at " + juce::String (synthHz, 2) + " Hz");
    }

    // Legato: notes run into each other without gaps (hammer-ons, slides between frets).
    {
        const float legato[] = { 55.0f, 61.74f, 65.41f, 73.42f, 82.41f, 73.42f, 65.41f, 55.0f };
        std::vector<float> run;
        for (float hz : legato)
            run = concat (run, bassNote (hz, 0.35, 0.45f, testRate, 0.4f));
        addNoiseAndHum (run, rng);
        auto q = makeProcessor();
        std::vector<float> heard;
        render (*q, run, testBlock, [&] (int) { heard.push_back (q->detectedFrequency.load()); });
        int good = 0;
        for (size_t n = 0; n < std::size (legato); ++n)
        {
            const size_t block = (size_t) (((double) n * 0.35 + 0.25) / blockSeconds);
            if (std::abs (centsBetween (heard[block], legato[n])) < 20.0f)
                ++good;
            else
                std::printf ("    legato note %d: %.2f Hz read as %.2f\n", (int) n, legato[n], heard[block]);
        }
        expect (good == (int) std::size (legato), "legato line tracked note for note (" + juce::String (good) + "/8)");
    }
}

// ---------------------------------------------------------------------------
// No clicks: switches and knob jumps during a sustained note.

float maxStep (const std::vector<float>& x, size_t from, size_t to)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, x.size()); ++i)
        m = std::max (m, std::abs (x[i] - x[i - 1]));
    return m;
}

void testNoClicks()
{
    section ("No clicks or zipper noise");
    // A long, steady note so any jump stands out.
    std::vector<float> held ((size_t) (2.5 * testRate));
    for (size_t i = 0; i < held.size(); ++i)
        held[i] = 0.4f * (float) std::sin (juce::MathConstants<double>::twoPi * 55.0 * (double) i / testRate);

    struct Change { const char* id; float from, to; };
    const Change changes[] = {
        { ParamIDs::bypass, 0, 1 }, { ParamIDs::hold, 0, 1 }, { ParamIDs::tuner, 0, 1 },
        { ParamIDs::inFilterOn, 0, 1 }, { ParamIDs::outFilterOn, 0, 1 }, { ParamIDs::chorusOn, 0, 1 },
        { ParamIDs::delayOn, 0, 1 }, { ParamIDs::delayOn, 1, 0 }, { ParamIDs::reverbOn, 0, 1 }, { ParamIDs::reverbOn, 1, 0 },
        { ParamIDs::fxRouting, 0, 1 }, { ParamIDs::mix, 0.0f, 1.0f }, { ParamIDs::mix, 1.0f, 0.0f },
        { ParamIDs::outputGain, -12.0f, 12.0f }, { ParamIDs::inputGain, -12.0f, 12.0f }, { ParamIDs::filter, 200.0f, 8000.0f },
        { ParamIDs::drive, 0.0f, 1.0f }, { ParamIDs::reso, 0.0f, 1.0f }, { ParamIDs::chorusMix, 0.0f, 1.0f },
        { ParamIDs::delayMix, 0.0f, 1.0f }, { ParamIDs::reverbMix, 0.0f, 1.0f }, { ParamIDs::outLowPass, 20000.0f, 600.0f },
    };

    for (const auto& c : changes)
    {
        for (bool fxRouted : { false, true })
        {
            auto p = makeProcessor();
            setParam (*p, ParamIDs::wave, 0.0f);
            setParam (*p, ParamIDs::env, 0.0f);
            setParam (*p, ParamIDs::mix, 0.6f);
            setParam (*p, ParamIDs::fxRouting, fxRouted ? 1.0f : 0.0f);
            setParam (*p, ParamIDs::outFilterOn, 1.0f);
            if (c.id != ParamIDs::delayOn && c.id != ParamIDs::reverbOn)
            {
                setParam (*p, ParamIDs::delayOn, 1.0f);
                setParam (*p, ParamIDs::reverbOn, 1.0f);
            }
            if (std::string (c.id) == ParamIDs::outFilterOn)
                setParam (*p, ParamIDs::outLowPass, 800.0f);
            setParam (*p, c.id, c.from);
            p->prepareToPlay (testRate, testBlock);

            const int changeBlock = (int) (1.5 * testRate / testBlock) ;
            const size_t at = (size_t) changeBlock * testBlock + 100;   // mid-block via a 100-sample offset render
            // Shift the input so the change lands mid-way through the processor's internal chunk.
            auto out = render (*p, held, testBlock, [&] (int b) { if (b == changeBlock) setParam (*p, c.id, c.to); }).left;
            // A click is a jump bigger than anything in the steady sound before or after the change.
            const float before = maxStep (out, (size_t) (1.2 * testRate), at - 200);
            const float after = maxStep (out, at + (size_t) (0.1 * testRate), at + (size_t) (0.4 * testRate));
            const float during = maxStep (out, at - 120, at + (size_t) (0.1 * testRate));
            const float limit = std::max ({ before, after, 0.003f }) * 1.5f + 0.003f;
            expect (during < limit, juce::String (c.id) + " " + juce::String (c.from) + " -> " + juce::String (c.to)
                                        + (fxRouted ? " (synth + bass)" : "") + " is smooth (step " + juce::String (during, 4)
                                        + ", steady " + juce::String (before, 4) + " / " + juce::String (after, 4) + ")");
        }
    }

    // A fast filter sweep, as automation: no zipper steps.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::wave, 0.0f);
        setParam (*p, ParamIDs::mix, 1.0f);
        const int blocks = (int) (held.size() / 64);
        auto out = render (*p, held, 64, [&] (int b) { setParam (*p, ParamIDs::filter, 40.0f * std::pow (300.0f, (float) b / (float) blocks)); }).left;
        expect (maxStep (out, (size_t) (0.3 * testRate), out.size()) < 0.05f, "filter automation sweep is smooth");
    }
}

// ---------------------------------------------------------------------------
// Host behaviour: block sizes, automation, threads, old sessions, CPU.

void testHostBehaviour()
{
    section ("Host behaviour");
    std::mt19937 rng (3);
    auto line = concat (concat (bassNote (41.2f, 0.6, 0.5f), bassNote (73.42f, 0.6, 0.5f)), silence (0.3));
    addNoiseAndHum (line, rng);

    // The host's block size doesn't change the sound.
    {
        auto renderAt = [&] (int block)
        {
            auto p = makeProcessor (testRate, block);
            for (auto* id : { ParamIDs::chorusOn, ParamIDs::delayOn, ParamIDs::reverbOn })
                setParam (*p, id, 1.0f);
            p->prepareToPlay (testRate, block);
            return render (*p, line, block).left;
        };
        const auto reference = renderAt (256);
        for (int block : { 1, 7, 64, 255, 257, 1000, 4096 })
        {
            const float d = difference (reference, renderAt (block));
            expect (d < 1.0e-5f, "block size " + juce::String (block) + " sounds the same as 256 (difference " + juce::String (d, 7) + ")");
        }
    }

    // Sample-accurate-ish automation at block size 1: every parameter moving every sample stays finite.
    {
        auto p = makeProcessor (testRate, 1);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        auto params = p->getParameters();
        auto out = render (*p, line, 1, [&] (int b)
        {
            if (b % 32 == 0)
                for (auto* param : params)
                    if (param != p->getBypassParameter())
                        param->setValueNotifyingHost (uni (rng));
        });
        expect (allFinite (out), "automating every parameter at block size 1 stays finite");
    }

    // Presets, A/B and state from the UI thread while audio runs.
    {
        auto p = makeProcessor();
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("subterra-thread-presets");
        temp.deleteRecursively();
        p->getPresets().setUserFolder (temp);

        std::atomic<bool> stop { false }, bad { false };
        std::atomic<int> blocks { 0 };
        std::thread audio ([&]
        {
            juce::AudioBuffer<float> buffer (2, testBlock);
            juce::MidiBuffer midi;
            size_t pos = 0;
            while (! stop.load())
            {
                for (int i = 0; i < testBlock; ++i, pos = (pos + 1) % line.size())
                    buffer.setSample (0, i, line[pos]), buffer.setSample (1, i, line[pos]);
                p->processBlock (buffer, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < testBlock; ++i)
                        if (! std::isfinite (buffer.getSample (ch, i)))
                            bad = true;
                ++blocks;
            }
        });

        auto& presets = p->getPresets();
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        for (int i = 0; i < 400; ++i)
        {
            presets.loadPreset (i % presets.getNumPresets());
            if (i % 3 == 0) p->switchSlot();
            if (i % 7 == 0) p->copyActiveSlotToOther();
            juce::MemoryBlock state;
            p->getStateInformation (state);
            if (i % 5 == 0) p->setStateInformation (state.getData(), (int) state.getSize());
            if (i % 40 == 0) presets.saveUserPreset ("Thread test " + juce::String (i / 40));
            if (i % 60 == 0) presets.rescanUserPresets();
            p->getParameters()[(int) (uni (rng) * 30.0f)]->setValueNotifyingHost (uni (rng));
            juce::ignoreUnused (p->getProgramName (i % p->getNumPrograms()));
        }
        stop = true;
        audio.join();
        expect (! bad.load() && blocks.load() > 10, "audio stays clean while presets, A/B and state change (" + juce::String (blocks.load()) + " blocks)");
        temp.deleteRecursively();
    }

    // A session saved by version 0.1 (no effect parameters) opens with the effects off.
    {
        auto old = makeProcessor();
        juce::MemoryBlock saved;
        old->getStateInformation (saved);
        auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), (int) saved.getSize());
        auto tree = juce::ValueTree::fromXml (*xml);
        for (int i = tree.getNumChildren(); --i >= 0;)
        {
            const auto id = tree.getChild (i).getProperty ("id").toString();
            if (id.startsWith ("chorus") || id.startsWith ("delay") || id.startsWith ("reverb") || id == ParamIDs::fxRouting)
                tree.removeChild (i, nullptr);
        }
        setParam (*old, ParamIDs::filter, 2500.0f);
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), legacy);

        auto p = makeProcessor();
        setParam (*p, ParamIDs::reverbOn, 1.0f);
        setParam (*p, ParamIDs::delayOn, 1.0f);
        p->setStateInformation (legacy.getData(), (int) legacy.getSize());
        expect (getParam (*p, ParamIDs::reverbOn) < 0.5f && getParam (*p, ParamIDs::delayOn) < 0.5f, "an older session opens with the effects off");
        expect (std::abs (getParam (*p, ParamIDs::filter) - getParam (*old, ParamIDs::filter)) > 1.0f
                    || std::abs (getParam (*p, ParamIDs::filter) - 600.0f) < 1.0f, "an older session keeps its own pedal settings");
    }

    // A user preset saved by version 0.1 loads with the effects off, not left over from the last sound.
    {
        auto p = makeProcessor();
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("subterra-legacy-presets");
        temp.deleteRecursively();
        temp.createDirectory();
        temp.getChildFile ("Old Sound.subterrapreset")
            .replaceWithText ("<Preset name=\"Old Sound\"><Param id=\"filter\" value=\"1234\"/><Param id=\"mix\" value=\"0.9\"/></Preset>");
        auto& presets = p->getPresets();
        presets.setUserFolder (temp);
        setParam (*p, ParamIDs::delayOn, 1.0f);
        for (int i = 0; i < presets.getNumPresets(); ++i)
            if (presets.getPreset (i).name == "Old Sound")
                presets.loadPreset (i);
        expect (std::abs (getParam (*p, ParamIDs::filter) - 1234.0f) < 1.0f, "older preset loads its values");
        expect (getParam (*p, ParamIDs::delayOn) < 0.5f, "older preset turns effects off");
        temp.deleteRecursively();
    }

    // Damaged session data is ignored.
    {
        auto p = makeProcessor();
        setParam (*p, ParamIDs::filter, 3000.0f);
        std::uniform_int_distribution<int> byte (0, 255);
        for (int attempt = 0; attempt < 50; ++attempt)
        {
            juce::MemoryBlock junk ((size_t) (1 + attempt * 37));
            for (size_t i = 0; i < junk.getSize(); ++i)
                junk[i] = (char) byte (rng);
            p->setStateInformation (junk.getData(), (int) junk.getSize());
        }
        p->setStateInformation (nullptr, 0);
        expect (std::abs (getParam (*p, ParamIDs::filter) - 3000.0f) < 1.0f, "damaged session data leaves the sound alone");
    }

    // CPU: 20 s of audio with everything on.
    {
        auto p = makeProcessor();
        for (auto* id : { ParamIDs::chorusOn, ParamIDs::delayOn, ParamIDs::reverbOn, ParamIDs::inFilterOn, ParamIDs::outFilterOn })
            setParam (*p, id, 1.0f);
        std::vector<float> long20;
        while (long20.size() < (size_t) (20.0 * testRate))
            long20 = concat (long20, line);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        render (*p, long20);
        const double seconds = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        const double load = seconds / ((double) long20.size() / testRate) * 100.0;
        std::printf ("  CPU with every effect on: %.2f%% of one core at 48 kHz\n", load);
        expect (load < 15.0, "CPU use is low (" + juce::String (load, 2) + "% of one core)");
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
    testEffects();
    testRealPlaying();
    testNoClicks();
    testHostBehaviour();

    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures;
}
