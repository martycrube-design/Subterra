#pragma once

#include "Controls.h"
#include "../Parameters.h"

namespace subterra
{
namespace Accent
{
    inline const juce::Colour chorus { 0xff5ec4c9 };
    inline const juce::Colour delay  { 0xffffb347 };
    inline const juce::Colour reverb { 0xffb59cff };
}

// One effect in the chain, drawn as a hardware unit: title, LED, knobs, a small display and a footswitch.
class EffectUnit final : public juce::Component
{
public:
    EffectUnit (SubterraLookAndFeel&, const juce::String& title, const juce::String& subtitle, juce::Colour accent);

    void setOn (bool on);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    std::function<void (juce::Graphics&, juce::Rectangle<float> display, float onAmount)> drawDisplay;
    std::function<bool()> animateDisplay;   // returns true while the display needs redrawing every frame

    Footswitch footswitch;
    juce::Rectangle<float> displayArea { 22.0f, 284.0f, 156.0f, 62.0f };

private:
    SubterraLookAndFeel& lnf;
    juce::String title, subtitle;
    juce::Colour accent;
    Led led;
    Tween onAmount;
    bool initialised = false;
    FrameDriver frames { *this, [this]
    {
        const bool a = led.update();
        const bool b = onAmount.update();
        const bool c = animateDisplay != nullptr && animateDisplay();
        return a || b || c;
    } };
};

class EffectsPage final : public juce::Component
{
public:
    EffectsPage (SubterraLookAndFeel&, juce::AudioProcessorValueTreeState&, std::atomic<float>& hostTempo);

    // Called from the editor's timer: LEDs, the delay's Time/Division knob, displays.
    void refresh();
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    SegmentedControl routing;
    EffectUnit chorusUnit, delayUnit, reverbUnit;
    Knob chorusRate, chorusDepth, chorusMix;
    Knob delayTime, delayDivision, delayFeedback, delayTone, delayMix;
    Knob reverbSize, reverbTone, reverbMix;
    PillToggle delaySync;

private:
    float param (const char* id) const;

    SubterraLookAndFeel& lnf;
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>& tempo;
    double chorusPhase = 0.0, lastFrameMs = 0.0;
    float lastChorus[2] {}, lastDelay[5] {}, lastReverb[2] {};
};
}
