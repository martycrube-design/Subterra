#include "Controls.h"

namespace subterra
{
juce::Path svgPath (const char* data) { return juce::Drawable::parseSVGPath (data); }

void fillPowderCoat (juce::Graphics& g, juce::Rectangle<float> area, float alpha)
{
    // One 3 x 3 px cell with a 1 px dot, stored at double resolution so it stays crisp when scaled up.
    juce::Image tile (juce::Image::ARGB, 6, 6, true, juce::SoftwareImageType());
    tile.clear (tile.getBounds().withPosition (3, 3).withSize (2, 2), juce::Colours::black);

    g.setFillType (juce::FillType (tile, juce::AffineTransform::scale (0.5f)));
    g.setOpacity (alpha);
    g.fillRect (area);
    g.setOpacity (1.0f);
}

void strokeIcon (juce::Graphics& g, const juce::Path& path, juce::Point<float> topLeft, float thickness,
                 juce::PathStrokeType::JointStyle joint, juce::PathStrokeType::EndCapStyle cap)
{
    g.strokePath (path, juce::PathStrokeType (thickness, joint, cap), juce::AffineTransform::translation (topLeft));
}

//==============================================================================
Knob::Knob (KnobStyle style, const juce::String& title, const juce::String& tooltip)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (220);
    setScrollWheelEnabled (true);
    setTitle (title);
    setName (title);
    setTooltip (title + ": " + tooltip + "  (double-click to reset)");
    getProperties().set ("knob", (int) style);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void Knob::valueChanged()
{
    const auto proportion = (float) valueToProportionOfLength (getValue());
    if (! isShowing() || isMouseOverOrDragging() || isMouseButtonDown())
        shown.setImmediately (proportion);
    else
        shown.animateTo (proportion, 280.0);
}

void Knob::paint (juce::Graphics& g)
{
    if (! shown.isRunning())
        shown.setImmediately ((float) valueToProportionOfLength (getValue()));

    const auto rotary = getRotaryParameters();
    getLookAndFeel().drawRotarySlider (g, 0, 0, getWidth(), getHeight(), juce::jlimit (0.0f, 1.0f, shown.get()),
                                       rotary.startAngleRadians, rotary.endAngleRadians, *this);
}

//==============================================================================
PillToggle::PillToggle (SubterraLookAndFeel& l, const juce::String& title, const juce::String& outer,
                        const juce::String& on, const juce::String& off)
    : juce::Button (title), lnf (l), outerLabel (outer), onText (on), offText (off)
{
    setClickingTogglesState (true);
    setTitle (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PillToggle::buttonStateChanged()
{
    hover.animateTo (isOver() || isDown() ? 1.0f : 0.0f, 120.0);
}

void PillToggle::paintButton (juce::Graphics& g, bool, bool down)
{
    const float target = getToggleState() ? 1.0f : 0.0f;
    if (! juce::exactlyEqual (fill.getTarget(), target))
        fill.animateTo (target, isShowing() ? 160.0 : 0.0);

    auto area = getLocalBounds().toFloat().reduced (0.75f);
    const float on = fill.get();
    const auto ink = Colours::frameInk.withAlpha (down ? 0.7f : 1.0f);

    if (hover.get() > 0.0f)
    {
        g.setColour (Colours::frameInk.withAlpha (0.08f * hover.get()));
        g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    }

    g.setColour (ink);

    juce::Rectangle<float> pill = area;
    if (outerLabel.isNotEmpty())
    {
        g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.5f);
        SubterraLookAndFeel::drawSpacedText (g, outerLabel, area.withTrimmedLeft (13.0f).withWidth (60.0f),
                                             lnf.inter (12.0f, 700, 0.5f), juce::Justification::centredLeft);
        pill = juce::Rectangle<float> (area.getRight() - 5.0f - 48.0f, area.getCentreY() - 11.0f, 48.0f, 22.0f);
    }

    const float dot = outerLabel.isNotEmpty() ? 12.0f : 14.0f;
    g.drawRoundedRectangle (pill.reduced (outerLabel.isNotEmpty() ? 0.75f : 0.0f), pill.getHeight() * 0.5f, 1.5f);

    // The dot fills from the centre as the switch turns on.
    const auto dotArea = juce::Rectangle<float> (dot, dot).withPosition (pill.getX() + 4.0f, pill.getCentreY() - dot * 0.5f);
    g.drawEllipse (dotArea.reduced (0.75f), 1.5f);
    if (on > 0.0f)
        g.fillEllipse (dotArea.withSizeKeepingCentre (dot * on, dot * on));

    const auto textArea = pill.withTrimmedLeft (dot + 8.0f).withTrimmedRight (6.0f);
    g.setFont (lnf.inter (10.0f, 700));
    g.setColour (ink.withMultipliedAlpha (1.0f - on));
    g.drawText (offText, textArea, juce::Justification::centredLeft, false);
    g.setColour (ink.withMultipliedAlpha (on));
    g.drawText (onText, textArea, juce::Justification::centredLeft, false);
}

//==============================================================================
RoundButton::RoundButton (const juce::String& title, juce::Path p)
    : juce::Button (title), icon (std::move (p))
{
    setTitle (title);
    setTooltip (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void RoundButton::buttonStateChanged()
{
    hover.animateTo (isDown() ? 2.0f : isOver() ? 1.0f : 0.0f, 120.0);
}

void RoundButton::paintButton (juce::Graphics& g, bool, bool)
{
    const auto area = getLocalBounds().toFloat().reduced (0.75f);
    if (hover.get() > 0.0f)
    {
        g.setColour (Colours::frameInk.withAlpha (0.1f * hover.get()));
        g.fillEllipse (area);
    }

    g.setColour (Colours::frameInk);
    g.drawEllipse (area, 1.5f);

    if (drawContent)
    {
        drawContent (g, area);
        return;
    }

    strokeIcon (g, icon, area.getCentre() - juce::Point<float> (8.0f, 8.0f), 1.5f);
}

//==============================================================================
FlatTextButton::FlatTextButton (SubterraLookAndFeel& l, const juce::String& title, float size, int w)
    : juce::Button (title), lnf (l), fontSize (size), weight (w)
{
    setTitle (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FlatTextButton::buttonStateChanged()
{
    hover.animateTo (isDown() ? 2.0f : isOver() ? 1.0f : 0.0f, 120.0);
}

void FlatTextButton::paintButton (juce::Graphics& g, bool, bool)
{
    if (hover.get() > 0.0f)
    {
        g.setColour (Colours::frameInk.withAlpha (0.09f * hover.get()));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), getHeight() * 0.5f);
    }
    g.setColour (Colours::frameInk);
    g.setFont (lnf.inter (fontSize, weight));
    g.drawText (getButtonText(), getLocalBounds().reduced (6, 0), juce::Justification::centred, true);
}

//==============================================================================
Footswitch::Footswitch (const juce::String& title) : juce::Button (title)
{
    setClickingTogglesState (true);
    setTitle (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Footswitch::buttonStateChanged()
{
    if (isDown())
        press.animateTo (1.0f, 50.0);
    else
        press.animateTo (0.0f, 260.0, Easing::outBack);
}

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto c = bounds.getCentre();
    const float p = press.get();

    // 72 px bezel
    juce::Path bezel;
    bezel.addEllipse (bounds.reduced (1.0f));
    juce::ColourGradient bezelFill (juce::Colour (0xfff2f2f2), bounds.getX() + bounds.getWidth() * 0.4f, bounds.getY() + bounds.getHeight() * 0.35f,
                                    juce::Colour (0xff707070), bounds.getRight(), bounds.getBottom(), true);
    bezelFill.addColour (0.6, juce::Colour (0xffa9a9a9));
    g.setGradientFill (bezelFill);
    g.fillPath (bezel);

    // 58 px hex nut
    const auto nut = juce::Rectangle<float> (58.0f, 58.0f).withCentre (c);
    juce::Path hex;
    hex.startNewSubPath (nut.getX() + nut.getWidth() * 0.25f, nut.getY() + nut.getHeight() * 0.05f);
    hex.lineTo (nut.getX() + nut.getWidth() * 0.75f, nut.getY() + nut.getHeight() * 0.05f);
    hex.lineTo (nut.getRight(), nut.getCentreY());
    hex.lineTo (nut.getX() + nut.getWidth() * 0.75f, nut.getY() + nut.getHeight() * 0.95f);
    hex.lineTo (nut.getX() + nut.getWidth() * 0.25f, nut.getY() + nut.getHeight() * 0.95f);
    hex.lineTo (nut.getX(), nut.getCentreY());
    hex.closeSubPath();
    juce::ColourGradient nutFill (juce::Colour (0xfffafafa), nut.getX() + 10.0f, nut.getY(),
                                  juce::Colour (0xffcfcfcf), nut.getRight() - 10.0f, nut.getBottom(), false);
    nutFill.addColour (0.55, juce::Colour (0xff9a9a9a));
    g.setGradientFill (nutFill);
    g.fillPath (hex);

    // 36 px dome; sinks and shrinks slightly while pressed
    const float size = 36.0f - 1.5f * p;
    const auto dome = juce::Rectangle<float> (size, size).withCentre (c.translated (0.0f, 1.5f * p));
    juce::Path domePath;
    domePath.addEllipse (dome);
    juce::DropShadow (juce::Colours::black.withAlpha (0.45f - 0.15f * p), juce::roundToInt (5.0f - 2.0f * p),
                      { 0, juce::roundToInt (3.0f - 2.0f * p) }).drawForPath (g, domePath);
    juce::ColourGradient domeFill (juce::Colours::white.darker (0.12f * p), dome.getX() + dome.getWidth() * 0.4f, dome.getY() + dome.getHeight() * 0.32f,
                                   juce::Colour (0xff858585), dome.getRight(), dome.getBottom(), true);
    domeFill.addColour (0.5, juce::Colour (0xffc4c4c4).darker (0.1f * p));
    g.setGradientFill (domeFill);
    g.fillPath (domePath);
    g.setColour (juce::Colour (0xff6c6c6c));
    g.drawEllipse (dome.reduced (0.5f), 1.0f);

    if (highlighted && p < 0.5f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (dome);
    }
}

//==============================================================================
void Led::paint (juce::Graphics& g, juce::Point<float> topLeft, float bezelSize) const
{
    const auto bezel = juce::Rectangle<float> (topLeft.x, topLeft.y, bezelSize, bezelSize);
    const float s = bezelSize / 26.0f;
    juce::Path bezelPath;
    bezelPath.addEllipse (bezel);
    juce::DropShadow (juce::Colours::black.withAlpha (0.4f), 3, { 0, 2 }).drawForPath (g, bezelPath);
    juce::ColourGradient chrome (juce::Colours::white, bezel.getX() + 9.0f * s, bezel.getY() + 8.0f * s,
                                 juce::Colour (0xff5a5a5a), bezel.getRight(), bezel.getBottom(), true);
    chrome.addColour (0.55, juce::Colour (0xff9c9c9c));
    g.setGradientFill (chrome);
    g.fillPath (bezelPath);

    const auto lens = juce::Rectangle<float> (14.0f * s, 14.0f * s).withCentre (bezel.getCentre());
    const float on = level.get();

    // Unlit lens, then the lit one faded in on top.
    juce::ColourGradient dark (colour.darker (1.6f).withSaturation (0.5f), lens.getX() + lens.getWidth() * 0.4f, lens.getY() + lens.getHeight() * 0.35f,
                               colour.darker (3.2f), lens.getX() + lens.getWidth() * 0.9f, lens.getBottom(), true);
    g.setGradientFill (dark);
    g.fillEllipse (lens);

    if (on > 0.0f)
    {
        juce::ColourGradient glow (colour.withAlpha (0.75f * on), lens.getCentreX(), lens.getCentreY(),
                                   colour.withAlpha (0.0f), lens.getCentreX() + 20.0f * s, lens.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillEllipse (lens.expanded (13.0f * s));
        juce::ColourGradient fill (juce::Colours::white.withAlpha (on), lens.getX() + lens.getWidth() * 0.4f, lens.getY() + lens.getHeight() * 0.35f,
                                   colour.withAlpha (on), lens.getX() + lens.getWidth() * 0.9f, lens.getBottom(), true);
        g.setGradientFill (fill);
        g.fillEllipse (lens);
    }
}

//==============================================================================
void StereoMeter::setLevels (float left, float right)
{
    if (std::abs (left - levels[0]) > 1.0e-4f || std::abs (right - levels[1]) > 1.0e-4f)
    {
        levels[0] = left;
        levels[1] = right;
        repaint();
    }
}

void StereoMeter::paint (juce::Graphics& g)
{
    // Two 7 x 132 bars, 6 px apart. Scale: -60 dBFS to 0 dBFS.
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto bar = juce::Rectangle<float> ((float) ch * 13.0f, 0.0f, 7.0f, (float) getHeight());
        g.setColour (Colours::track);
        g.fillRoundedRectangle (bar, 3.5f);

        const float db = juce::Decibels::gainToDecibels (levels[ch], -60.0f);
        const float fill = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        if (fill <= 0.0f)
            continue;

        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (bar, 3.5f);
        g.reduceClipRegion (clip);
        g.setColour (Colours::frameInk);
        g.fillRect (bar.withTop (bar.getBottom() - bar.getHeight() * fill));
        if (levels[ch] >= 0.999f)
        {
            g.setColour (Colours::led);
            g.fillRect (bar.withHeight (8.0f));
        }
        g.restoreState();
    }
}

//==============================================================================
SegmentedControl::SegmentedControl (SubterraLookAndFeel& l, const juce::String& title, juce::StringArray opts)
    : lnf (l), options (std::move (opts))
{
    setTitle (title);
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

SegmentedControl::~SegmentedControl() = default;

void SegmentedControl::attachTo (juce::RangedAudioParameter& param)
{
    attachment = std::make_unique<juce::ParameterAttachment> (param, [this] (float value)
    {
        setSelected (juce::roundToInt (value), isShowing());
    });
    attachment->sendInitialUpdate();
}

void SegmentedControl::setSelected (int index, bool animate)
{
    index = juce::jlimit (0, options.size() - 1, index);
    selected = index;
    position.animateTo ((float) index, animate ? 220.0 : 0.0);
    repaint();
}

void SegmentedControl::choose (int index)
{
    if (index == selected)
        return;
    setSelected (index);
    if (attachment != nullptr)
        attachment->setValueAsCompleteGesture ((float) index);
    if (onChange)
        onChange (index);
}

juce::Rectangle<float> SegmentedControl::segment (int index) const
{
    const auto area = getLocalBounds().toFloat().reduced (3.0f);
    const float w = area.getWidth() / (float) options.size();
    return { area.getX() + w * (float) index, area.getY(), w, area.getHeight() };
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (Colours::frameInk);
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.5f);

    // Sliding highlight between segments.
    const float pos = position.get();
    const int lower = juce::jlimit (0, options.size() - 1, (int) std::floor (pos));
    const int upper = juce::jmin (options.size() - 1, lower + 1);
    const auto a = segment (lower), b = segment (upper);
    const float t = pos - (float) lower;
    const auto pill = juce::Rectangle<float> (a.getX() + (b.getX() - a.getX()) * t, a.getY(),
                                              a.getWidth() + (b.getWidth() - a.getWidth()) * t, a.getHeight());
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

    g.setFont (lnf.inter (10.0f, 700, 1.0f));
    for (int i = 0; i < options.size(); ++i)
    {
        const auto r = segment (i);
        const float closeness = juce::jlimit (0.0f, 1.0f, 1.0f - std::abs (pos - (float) i));
        if (i == hovered && i != selected)
        {
            g.setColour (Colours::frameInk.withAlpha (0.1f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        }
        g.setColour (Colours::frameInk.interpolatedWith (Colours::frame, closeness));
        g.drawText (options[i], r, juce::Justification::centred, false);
        if (drawExtra)
            drawExtra (g, r, i, i == selected);
    }

    if (hasKeyboardFocus (false))
    {
        g.setColour (Colours::frameInk.withAlpha (0.5f));
        g.drawRoundedRectangle (bounds.expanded (2.0f), bounds.getHeight() * 0.5f + 2.0f, 1.0f);
    }
}

void SegmentedControl::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;
    for (int i = 0; i < options.size(); ++i)
        if (segment (i).contains (e.position))
            choose (i);
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    int now = -1;
    for (int i = 0; i < options.size(); ++i)
        if (segment (i).contains (e.position))
            now = i;
    if (now != hovered)
    {
        hovered = now;
        repaint();
    }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

bool SegmentedControl::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)  { choose (juce::jmax (0, selected - 1)); return true; }
    if (key == juce::KeyPress::rightKey) { choose (juce::jmin (options.size() - 1, selected + 1)); return true; }
    return false;
}

std::unique_ptr<juce::AccessibilityHandler> SegmentedControl::createAccessibilityHandler()
{
    return std::make_unique<juce::AccessibilityHandler> (
        *this, juce::AccessibilityRole::group,
        juce::AccessibilityActions().addAction (juce::AccessibilityActionType::press, [this] { choose ((selected + 1) % options.size()); }));
}
}
