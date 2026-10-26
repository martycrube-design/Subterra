#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/SubterraLookAndFeel.h"

namespace subterra
{
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

class Knob final : public juce::Slider
{
public:
    Knob (KnobStyle, const juce::String& title, const juce::String& tooltip);
};

// Pill switch drawn as "( ) OFF" / "(o) ON", optionally inside a labelled outer pill (the Tuner button).
class PillToggle final : public juce::Button
{
public:
    PillToggle (SubterraLookAndFeel&, const juce::String& title, const juce::String& outerLabel = {});
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    SubterraLookAndFeel& lnf;
    juce::String outerLabel;
};

// 32 px round button with an icon or short text.
class RoundButton final : public juce::Button
{
public:
    RoundButton (SubterraLookAndFeel&, const juce::String& title, juce::Path icon, bool iconFilled = false);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> drawContent;

private:
    SubterraLookAndFeel& lnf;
    juce::Path icon;
    bool filled;
};

// Text-only button for the preset arrows and the preset name.
class FlatTextButton final : public juce::Button
{
public:
    FlatTextButton (SubterraLookAndFeel&, const juce::String& title, float fontSize, int weight);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    SubterraLookAndFeel& lnf;
    float fontSize;
    int weight;
};

class Footswitch final : public juce::Button
{
public:
    explicit Footswitch (const juce::String& title);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class StereoMeter final : public juce::Component
{
public:
    void setLevels (float left, float right);
    void paint (juce::Graphics&) override;

private:
    float levels[2] {};
};

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
    void paintLed (juce::Graphics&, juce::Point<float> topLeft, bool lit);
    SubterraLookAndFeel& lnf;
    bool activeLed = true, holdLed = false;
};

class SubterraEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SubterraEditor (SubterraProcessor&);
    ~SubterraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Panel;

    void timerCallback() override;
    void showPresetMenu();
    void showSettingsMenu();
    void showSaveDialog();
    void showAbMenu();
    void refreshPresetName();

    SubterraProcessor& processor;
    SubterraLookAndFeel lnf;
    std::unique_ptr<Panel> panel;
    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::AlertWindow> saveDialog;

    float meterDisplay[4] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubterraEditor)
};
}
