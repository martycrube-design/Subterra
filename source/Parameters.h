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

    inline constexpr auto fxRouting     = "fxRouting";
    inline constexpr auto chorusOn      = "chorusOn";
    inline constexpr auto chorusRate    = "chorusRate";
    inline constexpr auto chorusDepth   = "chorusDepth";
    inline constexpr auto chorusMix     = "chorusMix";
    inline constexpr auto delayOn       = "delayOn";
    inline constexpr auto delaySync     = "delaySync";
    inline constexpr auto delayTime     = "delayTime";
    inline constexpr auto delayDivision = "delayDivision";
    inline constexpr auto delayFeedback = "delayFeedback";
    inline constexpr auto delayTone     = "delayTone";
    inline constexpr auto delayMix      = "delayMix";
    inline constexpr auto reverbOn      = "reverbOn";
    inline constexpr auto reverbSize    = "reverbSize";
    inline constexpr auto reverbTone    = "reverbTone";
    inline constexpr auto reverbMix     = "reverbMix";
}

// Synced delay lengths, in quarter notes, matching the Division parameter's choices.
inline constexpr float delayDivisionBeats[] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f };
inline constexpr int defaultDelayDivision = 3;   // dotted eighth

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
