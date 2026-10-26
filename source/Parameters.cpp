#include "Parameters.h"

namespace subterra
{
namespace
{
using Range = juce::NormalisableRange<float>;
using FloatAttributes = juce::AudioParameterFloatAttributes;

Range logRange (float min, float max)
{
    return Range (min, max,
                  [] (float start, float end, float x) { return start * std::pow (end / start, x); },
                  [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
                  [] (float start, float end, float v) { return juce::jlimit (start, end, v); });
}

juce::String hzText (float hz, int)
{
    return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                         : juce::String (juce::roundToInt (hz)) + " Hz";
}

float hzValue (const juce::String& text)
{
    const auto value = text.getFloatValue();
    return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
}

FloatAttributes percent()
{
    return FloatAttributes().withLabel ("%")
        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; });
}

FloatAttributes decibels()
{
    return FloatAttributes().withLabel ("dB")
        .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });
}

FloatAttributes hertz()
{
    return FloatAttributes().withLabel ("Hz")
        .withStringFromValueFunction (hzText)
        .withValueFromStringFunction (hzValue);
}

template <typename... Args>
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const char* name, Args&&... args)
{
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, std::forward<Args> (args)...);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const char* name, bool def,
                                                    const char* onText = "On", const char* offText = "Off")
{
    return std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, 1 }, name, def,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction (
            [onText, offText] (bool v, int) { return juce::String (v ? onText : offText); }));
}
}

juce::String waveName (float wave)
{
    static const char* names[] = { "Sine", "Saw", "Square", "Triangle" };
    const float position = juce::jlimit (0.0f, 1.0f, wave) * 3.0f;
    const int nearest = juce::roundToInt (position);

    if (std::abs (position - (float) nearest) < 0.08f)
        return names[nearest];

    const int lower = (int) position;
    const int amount = juce::roundToInt ((position - (float) lower) * 100.0f);
    return juce::String (names[lower]) + " > " + names[lower + 1] + " " + juce::String (amount) + "%";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat (inputGain, "Input", Range (-24.0f, 24.0f, 0.1f), 0.0f, decibels()));
    layout.add (makeFloat (outputGain, "Output", Range (-24.0f, 24.0f, 0.1f), 0.0f, decibels()));

    layout.add (makeBool (inFilterOn, "In Filters", false));
    layout.add (makeFloat (inHighPass, "In High-Pass", logRange (20.0f, 1000.0f), 20.0f, hertz()));
    layout.add (makeFloat (inLowPass, "In Low-Pass", logRange (500.0f, 20000.0f), 20000.0f, hertz()));
    layout.add (makeBool (outFilterOn, "Out Filters", false));
    layout.add (makeFloat (outHighPass, "Out High-Pass", logRange (20.0f, 1000.0f), 20.0f, hertz()));
    layout.add (makeFloat (outLowPass, "Out Low-Pass", logRange (500.0f, 20000.0f), 20000.0f, hertz()));

    layout.add (makeFloat (ParamIDs::wave, "Wave", Range (0.0f, 1.0f), 1.0f / 3.0f,
                           FloatAttributes().withStringFromValueFunction ([] (float v, int) { return waveName (v); })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { octave, 1 }, "Octave",
        juce::StringArray { "-2 oct", "-1 oct", "0 oct", "+1 oct", "+2 oct" }, defaultOctaveIndex));

    layout.add (makeFloat (filter, "Filter", logRange (40.0f, 12000.0f), 600.0f, hertz()));
    layout.add (makeFloat (reso, "Reso", Range (0.0f, 1.0f), 0.3f, percent()));
    layout.add (makeFloat (env, "Env", Range (-1.0f, 1.0f), 0.5f,
                           FloatAttributes().withLabel ("%")
                               .withStringFromValueFunction ([] (float v, int) {
                                   const int p = juce::roundToInt (v * 100.0f);
                                   return (p > 0 ? "+" : "") + juce::String (p) + "%";
                               })
                               .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; })));
    layout.add (makeFloat (drive, "Drive", Range (0.0f, 1.0f), 0.2f, percent()));
    layout.add (makeFloat (mix, "Mix", Range (0.0f, 1.0f), 0.7f, percent()));

    layout.add (makeBool (bypass, "Bypass", false, "Bypassed", "Active"));
    layout.add (makeBool (hold, "Hold", false));
    layout.add (makeBool (tuner, "Tuner", false));

    layout.add (makeFloat (tracking, "Tracking", Range (0.0f, 1.0f), 0.6f, percent()));
    layout.add (makeFloat (glide, "Glide", logRange (1.0f, 1000.0f), 20.0f,
                           FloatAttributes().withLabel ("ms")
                               .withStringFromValueFunction ([] (float v, int) {
                                   return v <= 1.05f ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " ms";
                               })
                               .withValueFromStringFunction ([] (const juce::String& t) { return juce::jmax (1.0f, t.getFloatValue()); })));

    return layout;
}
}
