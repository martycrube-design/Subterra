#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace subterra
{
// Colours from the "Subterra · dark" design.
namespace Colours
{
    inline const juce::Colour frame     { 0xff17181b };
    inline const juce::Colour frameInk  { 0xffece6da };
    inline const juce::Colour track     { 0xff2e2f34 };
    inline const juce::Colour pedal     { 0xff2a2b30 };
    inline const juce::Colour pedalDark { 0xff141518 };
    inline const juce::Colour ink       { 0xffe8e2d4 };
    inline const juce::Colour knob      { 0xffa8a8a8 };
    inline const juce::Colour knobEdge  { 0xff6e6e6e };
    inline const juce::Colour cap       { 0xffd9d9d9 };
    inline const juce::Colour pointer   { 0xff111111 };
    inline const juce::Colour led       { 0xffff5a4e };
}

// How a rotary slider is drawn; set with Slider::getProperties().set ("knob", ...).
enum class KnobStyle
{
    pedalLarge,   // 64 px knurled knob on the pedal
    pedalSmall,   // 46 px Mix knob
    frameLarge,   // 80 px Input / Output
    frameMedium,  // 40 px Tracking / Glide
    frameSmall    // 34 px filter knobs
};

class SubterraLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SubterraLookAndFeel();

    // Fonts from the design: Bebas Neue for pedal lettering, Inter for the frame.
    juce::Font bebas (float height, float letterSpacingPx = 0.0f) const;
    juce::Font inter (float height, int weight = 600, float letterSpacingPx = 0.0f) const;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip,
                     const juce::Rectangle<float>& body) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    // Shared drawing helpers.
    static void drawDottedRing (juce::Graphics&, juce::Point<float> centre, float radius, float dotSize, juce::Colour);
    static void drawSpacedText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, const juce::Font&,
                                juce::Justification = juce::Justification::centred);

private:
    void drawPedalKnob (juce::Graphics&, juce::Rectangle<float> bounds, float angle, bool small) const;
    void drawFrameKnob (juce::Graphics&, juce::Rectangle<float> bounds, float angle, KnobStyle) const;

    juce::Typeface::Ptr bebasFace, interMedium, interSemiBold, interBold;
};
}
