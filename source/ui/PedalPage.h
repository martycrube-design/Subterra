#pragma once

#include "Controls.h"

namespace subterra
{
class NoteDisplay final : public juce::Component
{
public:
    explicit NoteDisplay (SubterraLookAndFeel&);
    void update (float frequency, bool tunerOn, float reference);
    void paint (juce::Graphics&) override;

private:
    SubterraLookAndFeel& lnf;
    float frequency = 0.0f, reference = 440.0f;
    bool tunerOn = false;
    Tween needle, modeFade;
    FrameDriver frames { *this, [this] { const bool a = needle.update(); const bool b = modeFade.update(); return a || b; } };
};

class PedalBody final : public juce::Component
{
public:
    explicit PedalBody (SubterraLookAndFeel&);
    void setLeds (bool active, bool hold);
    void paint (juce::Graphics&) override;

    Knob wave, octave, filter, reso, env, drive, mix;
    Footswitch bypassSwitch { "Bypass footswitch" }, holdSwitch { "Hold footswitch" };

private:
    SubterraLookAndFeel& lnf;
    Led activeLed { Colours::led }, holdLed { Colours::led };
    FrameDriver frames { *this, [this] { const bool a = activeLed.update(); const bool b = holdLed.update(); return a || b; } };
    bool ledsInitialised = false;
};

// Everything below the filter row on the main view: cables, pedal, Tracking/Glide, readout, Tuner.
class PedalPage final : public juce::Component
{
public:
    explicit PedalPage (SubterraLookAndFeel&);
    void paint (juce::Graphics&) override;

    PedalBody pedal;
    Knob tracking { KnobStyle::frameMedium, "Tracking", "sensitivity: how quietly you can play and still trigger the synth" };
    Knob glide { KnobStyle::frameMedium, "Glide", "portamento time between legato notes" };
    NoteDisplay noteDisplay;
    PillToggle tunerButton;

private:
    void paintCables (juce::Graphics&);
    SubterraLookAndFeel& lnf;
};

void paintPatchCable (juce::Graphics&, juce::Point<float> from, juce::Point<float> to, float sag);
}
