#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Animation.h"
#include "SubterraLookAndFeel.h"

namespace subterra
{
juce::Path svgPath (const char* data);

// Fine dot texture of a powder-coated enclosure, drawn as one tiled fill.
void fillPowderCoat (juce::Graphics&, juce::Rectangle<float> area, float alpha);
void strokeIcon (juce::Graphics&, const juce::Path&, juce::Point<float> topLeft, float thickness,
                 juce::PathStrokeType::JointStyle = juce::PathStrokeType::curved,
                 juce::PathStrokeType::EndCapStyle = juce::PathStrokeType::rounded);

// Rotary knob. When its value changes without the mouse (preset, A/B, automation),
// the pointer glides to the new position instead of jumping.
class Knob final : public juce::Slider
{
public:
    Knob (KnobStyle, const juce::String& title, const juce::String& tooltip);

    void paint (juce::Graphics&) override;
    void valueChanged() override;

private:
    Tween shown;
    FrameDriver frames { *this, [this] { return shown.update(); } };
};

// Pill switch drawn as "( ) OFF" / "(o) ON", optionally inside a labelled outer pill (the Tuner button).
class PillToggle final : public juce::Button
{
public:
    PillToggle (SubterraLookAndFeel&, const juce::String& title, const juce::String& outerLabel = {},
                const juce::String& onText = "ON", const juce::String& offText = "OFF");
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;

private:
    SubterraLookAndFeel& lnf;
    juce::String outerLabel, onText, offText;
    Tween fill, hover;
    FrameDriver frames { *this, [this] { const bool a = fill.update(); const bool b = hover.update(); return a || b; } };
};

// 32 px round button with an icon or custom content.
class RoundButton final : public juce::Button
{
public:
    RoundButton (const juce::String& title, juce::Path icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> drawContent;

private:
    juce::Path icon;
    Tween hover;
    FrameDriver frames { *this, [this] { return hover.update(); } };
};

// Text-only button for the preset arrows and the preset name.
class FlatTextButton final : public juce::Button
{
public:
    FlatTextButton (SubterraLookAndFeel&, const juce::String& title, float fontSize, int weight);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;

private:
    SubterraLookAndFeel& lnf;
    float fontSize;
    int weight;
    Tween hover;
    FrameDriver frames { *this, [this] { return hover.update(); } };
};

// Chrome footswitch; the dome sinks when pressed and springs back.
class Footswitch final : public juce::Button
{
public:
    explicit Footswitch (const juce::String& title);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    void buttonStateChanged() override;

private:
    Tween press;
    FrameDriver frames { *this, [this] { return press.update(); } };
};

// Status LED with a soft fade on and off.
class Led
{
public:
    explicit Led (juce::Colour c) : colour (c) {}
    void set (bool on, bool animate = true) { level.animateTo (on ? 1.0f : 0.0f, animate ? 140.0 : 0.0); }
    bool update() { return level.update(); }
    void paint (juce::Graphics&, juce::Point<float> topLeft, float bezelSize = 26.0f) const;
    juce::Colour colour;

private:
    Tween level;
};

class StereoMeter final : public juce::Component
{
public:
    void setLevels (float left, float right);
    void paint (juce::Graphics&) override;

private:
    float levels[2] {};
};

// Row of options with a highlight that slides to the selected one.
// Attach to a choice parameter with attachTo(), or use onChange.
class SegmentedControl final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedControl (SubterraLookAndFeel&, const juce::String& title, juce::StringArray options);
    ~SegmentedControl() override;

    void attachTo (juce::RangedAudioParameter&);
    void setSelected (int index, bool animate = true);
    int getSelected() const { return selected; }
    std::function<void (int)> onChange;
    std::function<void (juce::Graphics&, juce::Rectangle<float>, int index, bool selected)> drawExtra;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

private:
    juce::Rectangle<float> segment (int index) const;
    void choose (int index);

    SubterraLookAndFeel& lnf;
    juce::StringArray options;
    int selected = 0, hovered = -1;
    Tween position;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    FrameDriver frames { *this, [this] { return position.update(); } };
};
}
