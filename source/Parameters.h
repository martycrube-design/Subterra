#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace subterra
{
namespace ParamIDs
{
    inline constexpr auto inputGain   = "inputGain";
    inline constexpr auto outputGain  = "outputGain";
    inline constexpr auto inFilterOn  = "inFilterOn";
    inline constexpr auto inHighPass  = "inHighPass";
    inline constexpr auto inLowPass   = "inLowPass";
    inline constexpr auto outFilterOn = "outFilterOn";
    inline constexpr auto outHighPass = "outHighPass";
    inline constexpr auto outLowPass  = "outLowPass";
    inline constexpr auto wave        = "wave";
    inline constexpr auto octave      = "octave";
    inline constexpr auto filter      = "filter";
    inline constexpr auto reso        = "reso";
    inline constexpr auto env         = "env";
    inline constexpr auto drive       = "drive";
    inline constexpr auto mix         = "mix";
    inline constexpr auto bypass      = "bypass";
    inline constexpr auto hold        = "hold";
    inline constexpr auto tracking    = "tracking";
    inline constexpr auto glide       = "glide";
    inline constexpr auto tuner       = "tuner";
}

// Octave knob steps, in octaves relative to the note being played.
inline constexpr int octaveSteps[] = { -2, -1, 0, 1, 2 };
inline constexpr int defaultOctaveIndex = 2;

// Parameters that are performance switches rather than part of a sound.
// Presets and A/B slots leave them alone.
inline bool isPerformanceParameter (const juce::String& id)
{
    return id == ParamIDs::bypass || id == ParamIDs::hold || id == ParamIDs::tuner;
}

juce::String waveName (float wave);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
