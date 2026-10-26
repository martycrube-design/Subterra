#include "SubterraLookAndFeel.h"
#include "BinaryData.h"

namespace subterra
{
SubterraLookAndFeel::SubterraLookAndFeel()
{
    bebasFace = juce::Typeface::createSystemTypefaceFor (BinaryData::BebasNeueRegular_ttf, BinaryData::BebasNeueRegular_ttfSize);
    interMedium = juce::Typeface::createSystemTypefaceFor (BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
    interSemiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    interBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);

    setDefaultSansSerifTypeface (interSemiBold);

    setColour (juce::PopupMenu::backgroundColourId, Colours::frame.brighter (0.06f));
    setColour (juce::PopupMenu::textColourId, Colours::frameInk);
    setColour (juce::PopupMenu::headerTextColourId, Colours::frameInk.withAlpha (0.6f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::frameInk.withAlpha (0.14f));
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::frameInk);

    setColour (juce::AlertWindow::backgroundColourId, Colours::frame);
    setColour (juce::AlertWindow::textColourId, Colours::frameInk);
    setColour (juce::AlertWindow::outlineColourId, Colours::frameInk.withAlpha (0.6f));
    setColour (juce::TextButton::buttonColourId, Colours::track);
    setColour (juce::TextButton::buttonOnColourId, Colours::frameInk);
    setColour (juce::TextButton::textColourOffId, Colours::frameInk);
    setColour (juce::TextButton::textColourOnId, Colours::frame);
    setColour (juce::TextEditor::backgroundColourId, Colours::track);
    setColour (juce::TextEditor::textColourId, Colours::frameInk);
    setColour (juce::TextEditor::highlightColourId, Colours::frameInk.withAlpha (0.3f));
    setColour (juce::TextEditor::outlineColourId, Colours::frameInk.withAlpha (0.4f));
    setColour (juce::TextEditor::focusedOutlineColourId, Colours::frameInk);
    setColour (juce::CaretComponent::caretColourId, Colours::frameInk);
    setColour (juce::TooltipWindow::backgroundColourId, Colours::frame.brighter (0.08f));
    setColour (juce::TooltipWindow::textColourId, Colours::frameInk);
    setColour (juce::TooltipWindow::outlineColourId, Colours::frameInk.withAlpha (0.5f));
    setColour (juce::BubbleComponent::backgroundColourId, Colours::frame);
    setColour (juce::BubbleComponent::outlineColourId, Colours::frameInk);
}

juce::Font SubterraLookAndFeel::bebas (float height, float letterSpacingPx) const
{
    return juce::Font (juce::FontOptions (bebasFace).withPointHeight (height)
                           .withKerningFactor (letterSpacingPx / height));
}

juce::Font SubterraLookAndFeel::inter (float height, int weight, float letterSpacingPx) const
{
    auto face = weight >= 700 ? interBold : weight >= 600 ? interSemiBold : interMedium;
    return juce::Font (juce::FontOptions (face).withPointHeight (height)
                           .withKerningFactor (letterSpacingPx / height));
}

void SubterraLookAndFeel::drawSpacedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                                          const juce::Font& font, juce::Justification justification)
{
    g.setFont (font);
    g.drawText (text, area, justification, false);
}

void SubterraLookAndFeel::drawDottedRing (juce::Graphics& g, juce::Point<float> centre, float radius, float dotSize,
                                          juce::Colour colour)
{
    g.setColour (colour);
    const int dots = juce::jmax (12, (int) std::round (juce::MathConstants<float>::twoPi * radius / (dotSize * 2.0f)));
    for (int i = 0; i < dots; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) dots;
        const auto p = centre.getPointOnCircumference (radius, a);
        g.fillEllipse (p.x - dotSize * 0.5f, p.y - dotSize * 0.5f, dotSize, dotSize);
    }
}

void SubterraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                            float startAngle, float endAngle, juce::Slider& slider)
{
    const auto style = (KnobStyle) (int) slider.getProperties().getWithDefault ("knob", (int) KnobStyle::frameSmall);
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    if (style == KnobStyle::pedalLarge || style == KnobStyle::pedalSmall)
        drawPedalKnob (g, bounds, angle, style == KnobStyle::pedalSmall);
    else
        drawFrameKnob (g, bounds, angle, style);

    if (slider.hasKeyboardFocus (true) && slider.isShowing())
    {
        g.setColour (Colours::frameInk.withAlpha (0.5f));
        g.drawEllipse (bounds.reduced (1.0f), 1.0f);
    }
}

void SubterraLookAndFeel::drawPedalKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle, bool small) const
{
    // Sizes from the design: 64 px skirt / 48 px cap, or 46 / 34 for Mix.
    const float skirt = small ? 46.0f : 64.0f;
    const float capSize = small ? 34.0f : 48.0f;
    const float wedgeDegrees = small ? 6.0f : 5.0f;
    const auto centre = bounds.getCentre();
    const auto skirtArea = juce::Rectangle<float> (skirt, skirt).withCentre (centre);

    juce::Path skirtPath;
    skirtPath.addEllipse (skirtArea);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f), small ? 10 : 14, { 0, small ? 7 : 10 }).drawForPath (g, skirtPath);
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), small ? 2 : 3, { 0, 2 }).drawForPath (g, skirtPath);

    // Knurled skirt: alternating wedges.
    g.setColour (Colours::knobEdge);
    g.fillPath (skirtPath);
    g.setColour (Colours::knob);
    const int wedges = (int) (360.0f / (wedgeDegrees * 2.0f));
    for (int i = 0; i < wedges; ++i)
    {
        juce::Path wedge;
        const float a0 = juce::degreesToRadians ((float) i * wedgeDegrees * 2.0f);
        wedge.addPieSegment (skirtArea, a0, a0 + juce::degreesToRadians (wedgeDegrees), 0.0f);
        g.fillPath (wedge);
    }

    // Domed cap.
    const auto capArea = juce::Rectangle<float> (capSize, capSize).withCentre (centre);
    juce::ColourGradient dome (juce::Colours::white, capArea.getX() + capSize * 0.38f, capArea.getY() + capSize * 0.30f,
                               Colours::cap, capArea.getX() + capSize * 0.38f + capSize * 0.7f * 0.75f,
                               capArea.getY() + capSize * 0.30f + capSize * 0.7f * 0.75f, true);
    g.setGradientFill (dome);
    g.fillEllipse (capArea);

    juce::ColourGradient shade (juce::Colours::transparentBlack, centre.x, capArea.getY() + capSize * 0.55f,
                                juce::Colours::black.withAlpha (0.28f), centre.x, capArea.getBottom(), false);
    g.setGradientFill (shade);
    g.fillEllipse (capArea);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawEllipse (capArea.reduced (0.5f), 0.8f);

    // Pointer: 3 px wide, from 3 px (2 px small) inside the cap edge.
    const float pointerLength = small ? 11.0f : 16.0f;
    const float inset = small ? 2.0f : 3.0f;
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -capSize * 0.5f + inset, 3.0f, pointerLength, 1.5f);
    g.setColour (Colours::pointer);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void SubterraLookAndFeel::drawFrameKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float angle, KnobStyle style) const
{
    const auto centre = bounds.getCentre();

    if (style == KnobStyle::frameLarge)
    {
        // 80 px dotted ring, 58 px solid knob with a 2 px outline ring 4 px out.
        drawDottedRing (g, centre, 39.0f, 2.0f, Colours::frameInk);
        g.setColour (Colours::frameInk);
        g.drawEllipse (juce::Rectangle<float> (68.0f, 68.0f).withCentre (centre), 2.0f);
        g.fillEllipse (juce::Rectangle<float> (58.0f, 58.0f).withCentre (centre));

        juce::Path pointer;
        pointer.addRoundedRectangle (-2.0f, -29.0f + 6.0f, 4.0f, 16.0f, 2.0f);
        g.setColour (Colours::frame);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
        return;
    }

    // Hollow knob: 34 / 22 px or 40 / 26 px.
    const bool medium = style == KnobStyle::frameMedium;
    const float ring = medium ? 40.0f : 34.0f;
    const float inner = medium ? 26.0f : 22.0f;
    drawDottedRing (g, centre, ring * 0.5f - 1.0f, 2.0f, Colours::frameInk);

    g.setColour (Colours::frame);
    g.fillEllipse (juce::Rectangle<float> (inner, inner).withCentre (centre));
    g.setColour (Colours::frameInk);
    g.drawEllipse (juce::Rectangle<float> (inner + 2.0f, inner + 2.0f).withCentre (centre), 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.0f, -inner * 0.5f + 2.0f, 2.0f, medium ? 10.0f : 8.0f, 1.0f);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

juce::Font SubterraLookAndFeel::getSliderPopupFont (juce::Slider&) { return inter (12.0f, 600); }

int SubterraLookAndFeel::getSliderPopupPlacement (juce::Slider&) { return juce::BubbleComponent::above; }

void SubterraLookAndFeel::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&,
                                      const juce::Rectangle<float>& body)
{
    g.setColour (Colours::frame.withAlpha (0.95f));
    g.fillRoundedRectangle (body, 6.0f);
    g.setColour (Colours::frameInk);
    g.drawRoundedRectangle (body.reduced (0.75f), 6.0f, 1.5f);
}

juce::Font SubterraLookAndFeel::getPopupMenuFont() { return inter (14.0f, 500); }

void SubterraLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (Colours::frameInk.withAlpha (0.35f));
    g.drawRect (0, 0, width, height, 1);
}

juce::Font SubterraLookAndFeel::getAlertWindowTitleFont() { return inter (16.0f, 700); }
juce::Font SubterraLookAndFeel::getAlertWindowMessageFont() { return inter (14.0f, 500); }
juce::Font SubterraLookAndFeel::getAlertWindowFont() { return inter (13.0f, 500); }
juce::Font SubterraLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return inter (13.0f, 700); }

void SubterraLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto area = juce::Rectangle<int> (width, height).toFloat();
    g.setColour (findColour (juce::TooltipWindow::backgroundColourId));
    g.fillRoundedRectangle (area, 5.0f);
    g.setColour (findColour (juce::TooltipWindow::outlineColourId));
    g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (findColour (juce::TooltipWindow::textColourId));
    g.setFont (inter (12.0f, 500));
    g.drawFittedText (text, area.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 3);
}

juce::Rectangle<int> SubterraLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                            juce::Rectangle<int> parentArea)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (inter (12.0f, 500), tipText, 0.0f, 0.0f);
    const int w = juce::jmin (320, (int) std::ceil (glyphs.getBoundingBox (0, -1, true).getWidth()) + 18);
    const int h = 24;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

void SubterraLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (juce::Colour (0xff121315));
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height), 8.0f);
}

void SubterraLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    const bool focused = editor.hasKeyboardFocus (true) && ! editor.isReadOnly();
    g.setColour (Colours::frameInk.withAlpha (focused ? 0.6f : 0.2f));
    g.drawRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f), 8.0f, focused ? 1.5f : 1.0f);
}
}
