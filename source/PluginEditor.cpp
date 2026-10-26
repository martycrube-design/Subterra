#include "PluginEditor.h"

namespace subterra
{
namespace
{
constexpr int designWidth = 740;
constexpr int designHeight = 900;

juce::Path svgPath (const char* data) { return juce::Drawable::parseSVGPath (data); }

void strokeIcon (juce::Graphics& g, const juce::Path& path, juce::Point<float> topLeft, float thickness,
                 juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved,
                 juce::PathStrokeType::EndCapStyle cap = juce::PathStrokeType::rounded)
{
    g.strokePath (path, juce::PathStrokeType (thickness, joint, cap), juce::AffineTransform::translation (topLeft));
}

juce::String noteName (int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
}
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

//==============================================================================
PillToggle::PillToggle (SubterraLookAndFeel& l, const juce::String& title, const juce::String& outer)
    : juce::Button (title), lnf (l), outerLabel (outer)
{
    setClickingTogglesState (true);
    setTitle (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PillToggle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced (0.75f);
    const bool on = getToggleState();
    const auto ink = Colours::frameInk.withAlpha (down ? 0.7f : 1.0f);

    if (highlighted)
    {
        g.setColour (Colours::frameInk.withAlpha (0.08f));
        g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
    }

    g.setColour (ink);

    juce::Rectangle<float> pill = area;
    if (outerLabel.isNotEmpty())
    {
        // Outer pill: "TUNER" then the inner switch, as in the design.
        g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.5f);
        SubterraLookAndFeel::drawSpacedText (g, outerLabel, area.withTrimmedLeft (13.0f).withWidth (50.0f),
                                             lnf.inter (12.0f, 700, 0.5f), juce::Justification::centredLeft);
        pill = juce::Rectangle<float> (area.getRight() - 5.0f - 48.0f, area.getCentreY() - 11.0f, 48.0f, 22.0f);
    }

    const float dot = outerLabel.isNotEmpty() ? 12.0f : 14.0f;
    g.drawRoundedRectangle (pill.reduced (outerLabel.isNotEmpty() ? 0.75f : 0.0f), pill.getHeight() * 0.5f, 1.5f);

    const auto dotArea = juce::Rectangle<float> (dot, dot).withPosition (pill.getX() + 4.0f, pill.getCentreY() - dot * 0.5f);
    if (on)
        g.fillEllipse (dotArea);
    else
        g.drawEllipse (dotArea.reduced (0.75f), 1.5f);

    SubterraLookAndFeel::drawSpacedText (g, on ? "ON" : "OFF", pill.withTrimmedLeft (dot + 8.0f).withTrimmedRight (6.0f),
                                         lnf.inter (10.0f, 700), juce::Justification::centredLeft);
}

//==============================================================================
RoundButton::RoundButton (SubterraLookAndFeel& l, const juce::String& title, juce::Path p, bool iconFilled)
    : juce::Button (title), lnf (l), icon (std::move (p)), filled (iconFilled)
{
    setTitle (title);
    setTooltip (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void RoundButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto area = getLocalBounds().toFloat().reduced (0.75f);
    if (highlighted || down)
    {
        g.setColour (Colours::frameInk.withAlpha (down ? 0.2f : 0.1f));
        g.fillEllipse (area);
    }

    g.setColour (Colours::frameInk);
    g.drawEllipse (area, 1.5f);

    if (drawContent)
    {
        drawContent (g, area);
        return;
    }

    const auto offset = area.getCentre() - juce::Point<float> (8.0f, 8.0f);
    if (filled)
        g.fillPath (icon, juce::AffineTransform::translation (offset));
    else
        strokeIcon (g, icon, offset, 1.5f);
    juce::ignoreUnused (lnf);
}

//==============================================================================
FlatTextButton::FlatTextButton (SubterraLookAndFeel& l, const juce::String& title, float size, int w)
    : juce::Button (title), lnf (l), fontSize (size), weight (w)
{
    setTitle (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FlatTextButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (highlighted || down)
    {
        g.setColour (Colours::frameInk.withAlpha (down ? 0.18f : 0.09f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), getHeight() * 0.5f);
    }
    g.setColour (Colours::frameInk);
    g.setFont (lnf.inter (fontSize, weight));
    g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, true);
}

//==============================================================================
Footswitch::Footswitch (const juce::String& title) : juce::Button (title)
{
    setClickingTogglesState (true);
    setTitle (title);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto c = bounds.getCentre();

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

    // 36 px dome, pushed in slightly while pressed
    const float pressed = down ? 1.5f : 0.0f;
    const auto dome = juce::Rectangle<float> (36.0f, 36.0f).withCentre (c.translated (0.0f, pressed));
    juce::Path domePath;
    domePath.addEllipse (dome);
    juce::DropShadow (juce::Colours::black.withAlpha (down ? 0.3f : 0.45f), down ? 3 : 5, { 0, down ? 1 : 3 }).drawForPath (g, domePath);
    juce::ColourGradient domeFill (juce::Colours::white, dome.getX() + dome.getWidth() * 0.4f, dome.getY() + dome.getHeight() * 0.32f,
                                   juce::Colour (0xff858585), dome.getRight(), dome.getBottom(), true);
    domeFill.addColour (0.5, juce::Colour (0xffc4c4c4));
    g.setGradientFill (domeFill);
    g.fillPath (domePath);
    g.setColour (juce::Colour (0xff6c6c6c));
    g.drawEllipse (dome.reduced (0.5f), 1.0f);

    if (highlighted && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (dome);
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
NoteDisplay::NoteDisplay (SubterraLookAndFeel& l) : lnf (l)
{
    setTitle ("Note readout");
    setAccessible (true);
}

void NoteDisplay::update (float newFrequency, bool newTuner, float newReference)
{
    if (std::abs (newFrequency - frequency) > 0.05f || newTuner != tunerOn || std::abs (newReference - reference) > 0.01f)
    {
        frequency = newFrequency;
        tunerOn = newTuner;
        reference = newReference;
        repaint();
    }
}

void NoteDisplay::paint (juce::Graphics& g)
{
    const auto box = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (Colours::frameInk);
    g.drawRoundedRectangle (box, 12.0f, 1.5f);

    auto content = box.reduced (14.0f, 10.0f);
    SubterraLookAndFeel::drawSpacedText (g, tunerOn ? "TUNER" : "PLAYING", content.removeFromTop (12.0f),
                                         lnf.inter (10.0f, 700, 1.5f), juce::Justification::centredLeft);
    content.removeFromTop (2.0f);
    auto noteArea = content.removeFromTop (34.0f);
    content.removeFromTop (2.0f);
    auto detailArea = content.removeFromTop (15.0f);

    const bool hasNote = frequency > 0.0f;
    const float midi = hasNote ? 69.0f + 12.0f * std::log2 (frequency / reference) : 0.0f;
    const int nearest = (int) std::lround (midi);
    const float cents = (midi - (float) nearest) * 100.0f;

    g.setFont (lnf.bebas (34.0f));
    g.drawText (hasNote ? noteName (nearest) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")),
                noteArea, juce::Justification::centredLeft, false);

    g.setFont (lnf.inter (12.0f, 500));
    if (! tunerOn)
    {
        g.drawText (hasNote ? juce::String (frequency, 1) + " Hz" : juce::String ("-- Hz"), detailArea,
                    juce::Justification::centredLeft, false);
        return;
    }

    if (! hasNote)
    {
        g.drawText ("A4 = " + juce::String (reference, 0) + " Hz", detailArea, juce::Justification::centredLeft, false);
        return;
    }

    const int roundedCents = (int) std::lround (cents);
    const bool inTune = std::abs (cents) < 3.0f;
    g.drawText (inTune ? juce::String ("In tune")
                       : (roundedCents > 0 ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")))
                             + juce::String (std::abs (roundedCents)) + " cents",
                detailArea, juce::Justification::centredLeft, false);

    // Needle scale to the right of the note: -50 to +50 cents.
    const auto scale = noteArea.withTrimmedLeft (44.0f).withSizeKeepingCentre (noteArea.getWidth() - 44.0f, 20.0f);
    g.setColour (Colours::frameInk.withAlpha (0.5f));
    g.fillRect (scale.withSizeKeepingCentre (scale.getWidth(), 1.5f));
    g.fillRect (scale.withSizeKeepingCentre (1.5f, scale.getHeight()));
    const float x = scale.getCentreX() + juce::jlimit (-1.0f, 1.0f, cents / 50.0f) * scale.getWidth() * 0.5f;
    g.setColour (inTune ? Colours::frameInk : Colours::led);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x, scale.getCentreY() }));
}

//==============================================================================
PedalBody::PedalBody (SubterraLookAndFeel& l)
    : wave (KnobStyle::pedalLarge, "Wave", "oscillator shape, from sine through saw and square to triangle"),
      octave (KnobStyle::pedalLarge, "Octave", "synth pitch relative to your note, -2 to +2 octaves"),
      filter (KnobStyle::pedalLarge, "Filter", "synth low-pass cutoff"),
      reso (KnobStyle::pedalLarge, "Reso", "filter resonance"),
      env (KnobStyle::pedalLarge, "Env", "how far your picking dynamics sweep the filter, down or up"),
      drive (KnobStyle::pedalLarge, "Drive", "saturation on the synth voice"),
      mix (KnobStyle::pedalSmall, "Mix", "dry bass to synth balance"),
      lnf (l)
{
    for (auto* k : { &wave, &octave, &filter, &reso, &env, &drive, &mix })
        addAndMakeVisible (k);

    bypassSwitch.setTooltip ("Bypass: true bypass on and off");
    holdSwitch.setTooltip ("Hold: freeze the synth note so it keeps sounding");
    addAndMakeVisible (bypassSwitch);
    addAndMakeVisible (holdSwitch);

    wave.setBounds (30, 34, 64, 64);
    octave.setBounds (123, 34, 64, 64);
    filter.setBounds (216, 34, 64, 64);
    reso.setBounds (30, 140, 64, 64);
    env.setBounds (123, 140, 64, 64);
    drive.setBounds (216, 140, 64, 64);
    mix.setBounds (132, 252, 46, 46);
    bypassSwitch.setBounds (26, 482, 72, 72);
    holdSwitch.setBounds (212, 482, 72, 72);
}

void PedalBody::setLeds (bool active, bool hold)
{
    if (active != activeLed || hold != holdLed)
    {
        activeLed = active;
        holdLed = hold;
        repaint (juce::Rectangle<int> (20, 420, 270, 70));
    }
}

void PedalBody::paintLed (juce::Graphics& g, juce::Point<float> topLeft, bool lit)
{
    const auto bezel = juce::Rectangle<float> (topLeft.x, topLeft.y, 26.0f, 26.0f);
    juce::Path bezelPath;
    bezelPath.addEllipse (bezel);
    juce::DropShadow (juce::Colours::black.withAlpha (0.4f), 3, { 0, 2 }).drawForPath (g, bezelPath);
    juce::ColourGradient chrome (juce::Colours::white, bezel.getX() + 9.0f, bezel.getY() + 8.0f,
                                 juce::Colour (0xff5a5a5a), bezel.getRight(), bezel.getBottom(), true);
    chrome.addColour (0.55, juce::Colour (0xff9c9c9c));
    g.setGradientFill (chrome);
    g.fillPath (bezelPath);

    const auto lens = juce::Rectangle<float> (14.0f, 14.0f).withCentre (bezel.getCentre());
    if (lit)
    {
        juce::ColourGradient glow (Colours::led.withAlpha (0.75f), lens.getCentreX(), lens.getCentreY(),
                                   Colours::led.withAlpha (0.0f), lens.getCentreX() + 20.0f, lens.getCentreY(), true);
        g.setGradientFill (glow);
        g.fillEllipse (lens.expanded (13.0f));
        juce::ColourGradient fill (juce::Colours::white, lens.getX() + 5.6f, lens.getY() + 4.9f,
                                   Colours::led, lens.getX() + 5.6f + 7.0f, lens.getY() + 4.9f + 7.0f, true);
        g.setGradientFill (fill);
    }
    else
    {
        juce::ColourGradient fill (juce::Colour (0xff6a3030), lens.getX() + 5.6f, lens.getY() + 4.9f,
                                   juce::Colour (0xff2a0e0e), lens.getX() + 5.6f + 7.0f, lens.getY() + 4.9f + 7.0f, true);
        g.setGradientFill (fill);
    }
    g.fillEllipse (lens);
}

void PedalBody::paint (juce::Graphics& g)
{
    const auto body = getLocalBounds().toFloat().withTrimmedBottom (7.0f).reduced (0.0f);
    const float radius = 28.0f;

    // Enclosure: solid lip, base colour, light from the top left.
    g.setColour (Colours::pedalDark);
    g.fillRoundedRectangle (body.translated (0.0f, 7.0f), radius);
    g.setColour (Colours::pedal);
    g.fillRoundedRectangle (body, radius);

    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.30f), body.getX(), body.getY(),
                                juce::Colours::black.withAlpha (0.22f), body.getRight(), body.getBottom(), false);
    sheen.addColour (0.32, juce::Colours::white.withAlpha (0.0f));
    sheen.addColour (0.62, juce::Colours::black.withAlpha (0.0f));
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (body, radius);

    // Fine powder-coat texture.
    {
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (body, radius);
        g.reduceClipRegion (clip);
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        for (float y = 1.5f; y < body.getBottom(); y += 3.0f)
            for (float x = 1.5f; x < body.getRight(); x += 3.0f)
                g.fillRect (x, y, 1.0f, 1.0f);

        juce::ColourGradient left (juce::Colours::white.withAlpha (0.10f), 0.0f, 0.0f,
                                   juce::Colours::transparentWhite, 14.0f, 0.0f, false);
        g.setGradientFill (left);
        g.fillRect (body.withWidth (14.0f));
        juce::ColourGradient right (juce::Colours::transparentBlack, body.getRight() - 16.0f, 0.0f,
                                    juce::Colours::black.withAlpha (0.20f), body.getRight(), 0.0f, false);
        g.setGradientFill (right);
        g.fillRect (body.withLeft (body.getRight() - 16.0f));
        juce::ColourGradient bottom (juce::Colours::transparentBlack, 0.0f, body.getBottom() - 18.0f,
                                     juce::Colours::black.withAlpha (0.18f), 0.0f, body.getBottom(), false);
        g.setGradientFill (bottom);
        g.fillRect (body.withTop (body.getBottom() - 18.0f));
        g.restoreState();
    }

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawLine (radius, 1.0f, body.getRight() - radius, 1.0f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawRoundedRectangle (body.reduced (0.5f), radius, 1.0f);

    // Knob shadows (the knobs draw themselves on top).
    for (auto* k : { &wave, &octave, &filter, &reso, &env, &drive, &mix })
    {
        juce::Path p;
        p.addEllipse (k->getBounds().toFloat());
        const bool small = k == &mix;
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), small ? 10 : 14, { 0, small ? 7 : 10 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), small ? 2 : 3, { 0, 2 }).drawForPath (g, p);
    }

    // Wave shape icons around the Wave knob: sine and saw on the left, square and triangle on the right.
    g.setColour (Colours::ink);
    strokeIcon (g, svgPath ("M1 11L8 1V11L15 1"), { 8.0f, 24.0f }, 1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
    strokeIcon (g, svgPath ("M1 11V1H8V11H15V1"), { 98.0f, 24.0f }, 1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
    strokeIcon (g, svgPath ("M1 6C4 -2 6 -2 8 6S12 14 15 6"), { 8.0f, 96.0f }, 1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);
    strokeIcon (g, svgPath ("M1 11L5 1L9 11L13 1"), { 98.0f, 96.0f }, 1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt);

    // Knob legends
    const auto legend = lnf.bebas (20.0f, 1.0f);
    const auto drawLegend = [&] (const char* text, float x, float y, const juce::Font& f, float h)
    {
        SubterraLookAndFeel::drawSpacedText (g, text, { x, y, 100.0f, h }, f);
    };
    drawLegend ("WAVE", 12.0f, 106.0f, legend, 24.0f);
    drawLegend ("OCTAVE", 105.0f, 106.0f, legend, 24.0f);
    drawLegend ("FILTER", 198.0f, 106.0f, legend, 24.0f);
    drawLegend ("RESO", 12.0f, 212.0f, legend, 24.0f);
    drawLegend ("ENV", 105.0f, 212.0f, legend, 24.0f);
    drawLegend ("DRIVE", 198.0f, 212.0f, legend, 24.0f);

    g.setColour (Colours::ink.withAlpha (0.35f));
    g.fillRect (24.0f, 241.0f, 262.0f, 1.5f);

    g.setColour (Colours::ink);
    drawLegend ("MIX", 105.0f, 304.0f, lnf.bebas (17.0f, 1.0f), 20.0f);

    // Strata logo, name and subtitle
    strokeIcon (g, svgPath ("M6 10H114M6 20H44L54 28H114M6 32H72L82 38H114"), { 95.0f, 338.0f }, 2.5f);
    SubterraLookAndFeel::drawSpacedText (g, "SUBTERRA", { 0.0f, 380.0f, 310.0f, 46.0f }, lnf.bebas (40.0f, 7.0f));
    SubterraLookAndFeel::drawSpacedText (g, "BASS SYNTHESIZER", { 0.0f, 424.0f, 310.0f, 12.0f }, lnf.inter (9.0f, 700, 4.0f));

    paintLed (g, { 49.0f, 446.0f }, activeLed);
    paintLed (g, { 235.0f, 446.0f }, holdLed);

    g.setColour (Colours::ink);
    const auto switchLabel = lnf.inter (11.0f, 700, 2.0f);
    SubterraLookAndFeel::drawSpacedText (g, "BYPASS", { 12.0f, 568.0f, 100.0f, 14.0f }, switchLabel);
    SubterraLookAndFeel::drawSpacedText (g, "HOLD", { 198.0f, 568.0f, 100.0f, 14.0f }, switchLabel);
}

//==============================================================================
class SubterraEditor::Panel final : public juce::Component
{
public:
    Panel (SubterraEditor& e, SubterraProcessor& p, SubterraLookAndFeel& l)
        : editor (e), processor (p), lnf (l), noteDisplay (l), pedal (l),
          settingsButton (l, "Settings", settingsIcon()),
          saveButton (l, "Save preset", svgPath ("M2.5 2.5h8l3 3v8h-11z M5 2.5v3h5v-3M5 13.5v-4h6v4")),
          browseButton (l, "Browse presets", svgPath ("M2 4h4l1.5 1.5H14v7.5H2z")),
          abButton (l, "Compare A and B", {}),
          prevButton (l, "Previous preset", 16.0f, 500),
          nextButton (l, "Next preset", 16.0f, 500),
          presetName (l, "Preset name", 15.0f, 500),
          inFilters (l, "Input filters on or off"),
          outFilters (l, "Output filters on or off"),
          tunerButton (l, "Tuner on or off", "TUNER")
    {
        setSize (designWidth, designHeight);
        auto& apvts = processor.getState();

        for (auto* k : { &input, &output, &inHp, &inLp, &outHp, &outLp, &tracking, &glide,
                         &pedal.wave, &pedal.octave, &pedal.filter, &pedal.reso, &pedal.env, &pedal.drive, &pedal.mix })
            k->setPopupDisplayEnabled (true, true, this, 900);

        for (auto* c : std::initializer_list<juce::Component*> { &inMeter, &outMeter, &input, &output, &settingsButton, &saveButton,
                                                                  &browseButton, &abButton, &prevButton, &nextButton, &presetName,
                                                                  &inFilters, &inHp, &inLp, &outFilters, &outHp, &outLp,
                                                                  &pedal, &tracking, &glide, &noteDisplay, &tunerButton })
            addAndMakeVisible (c);

        attach (input, ParamIDs::inputGain);
        attach (output, ParamIDs::outputGain);
        attach (inHp, ParamIDs::inHighPass);
        attach (inLp, ParamIDs::inLowPass);
        attach (outHp, ParamIDs::outHighPass);
        attach (outLp, ParamIDs::outLowPass);
        attach (tracking, ParamIDs::tracking);
        attach (glide, ParamIDs::glide);
        attach (pedal.wave, ParamIDs::wave);
        attach (pedal.octave, ParamIDs::octave);
        attach (pedal.filter, ParamIDs::filter);
        attach (pedal.reso, ParamIDs::reso);
        attach (pedal.env, ParamIDs::env);
        attach (pedal.drive, ParamIDs::drive);
        attach (pedal.mix, ParamIDs::mix);

        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, ParamIDs::inFilterOn, inFilters));
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, ParamIDs::outFilterOn, outFilters));
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, ParamIDs::tuner, tunerButton));
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, ParamIDs::bypass, pedal.bypassSwitch));
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (apvts, ParamIDs::hold, pedal.holdSwitch));

        inFilters.setTooltip ("In filters: high- and low-pass on the bass before the pedal");
        outFilters.setTooltip ("Out filters: high- and low-pass on the final output");
        tunerButton.setTooltip ("Tuner: shows tuning in the readout and mutes the output");
        prevButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x80\xb9"));
        nextButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x80\xba"));
        prevButton.setTooltip ("Previous preset");
        nextButton.setTooltip ("Next preset");
        presetName.setTooltip ("Choose a preset");
        abButton.setTooltip ("A/B: switch between two settings to compare (right-click to copy)");

        abButton.drawContent = [this] (juce::Graphics& g, juce::Rectangle<float> area)
        {
            const bool onB = processor.getActiveSlot() == 1;
            g.setFont (lnf.inter (10.0f, 700));
            const auto r = area.withSizeKeepingCentre (24.0f, 12.0f);
            g.setColour (Colours::frameInk.withAlpha (onB ? 0.4f : 1.0f));
            g.drawText ("A", r.withWidth (8.0f), juce::Justification::centred, false);
            g.setColour (Colours::frameInk);
            g.drawText ("/", r.withTrimmedLeft (8.0f).withWidth (8.0f), juce::Justification::centred, false);
            g.setColour (Colours::frameInk.withAlpha (onB ? 1.0f : 0.4f));
            g.drawText ("B", r.withTrimmedLeft (16.0f), juce::Justification::centred, false);
        };

        prevButton.onClick = [this] { processor.getPresets().loadPrevious(); editor.refreshPresetName(); };
        nextButton.onClick = [this] { processor.getPresets().loadNext(); editor.refreshPresetName(); };
        presetName.onClick = [this] { editor.showPresetMenu(); };
        browseButton.onClick = [this] { editor.showPresetMenu(); };
        saveButton.onClick = [this] { editor.showSaveDialog(); };
        settingsButton.onClick = [this] { editor.showSettingsMenu(); };
        abButton.onClick = [this]
        {
            if (juce::ModifierKeys::getCurrentModifiers().isPopupMenu())
            {
                editor.showAbMenu();
                return;
            }
            processor.switchSlot();
            abButton.repaint();
            editor.refreshPresetName();
        };

        inMeter.setBounds (36, 34, 20, 132);
        outMeter.setBounds (691, 34, 20, 132);
        input.setBounds (88, 51, 80, 80);
        output.setBounds (572, 51, 80, 80);
        settingsButton.setBounds (291, 24, 32, 32);
        saveButton.setBounds (333, 24, 32, 32);
        browseButton.setBounds (375, 24, 32, 32);
        abButton.setBounds (417, 24, 32, 32);
        prevButton.setBounds (204, 71, 28, 28);
        nextButton.setBounds (508, 71, 28, 28);
        presetName.setBounds (236, 71, 268, 28);

        inFilters.setBounds (184, 146, 53, 24);
        inHp.setBounds (267, 141, 34, 34);
        inLp.setBounds (330, 141, 34, 34);
        outFilters.setBounds (376, 146, 53, 24);
        outHp.setBounds (459, 141, 34, 34);
        outLp.setBounds (522, 141, 34, 34);

        pedal.setBounds (215, 262, 310, 619);
        tracking.setBounds (106, 543, 40, 40);
        glide.setBounds (106, 628, 40, 40);
        noteDisplay.setBounds (556, 520, 140, 88);
        tunerButton.setBounds (560, 830, 124, 34);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::frame);
        g.setColour (Colours::frameInk);

        const auto label = lnf.inter (12.0f, 700, 1.0f);
        SubterraLookAndFeel::drawSpacedText (g, "INPUT", { 82.0f, 28.0f, 92.0f, 15.0f }, label);
        SubterraLookAndFeel::drawSpacedText (g, "OUTPUT", { 566.0f, 28.0f, 92.0f, 15.0f }, label);
        SubterraLookAndFeel::drawSpacedText (g, "IN FILTERS", { 186.0f, 118.0f, 176.0f, 15.0f }, label);
        SubterraLookAndFeel::drawSpacedText (g, "OUT FILTERS", { 378.0f, 118.0f, 176.0f, 15.0f }, label);

        const auto small = lnf.inter (11.0f, 600);
        for (float offset : { 0.0f, 192.0f })
        {
            SubterraLookAndFeel::drawSpacedText (g, "HP", { 243.0f + offset, 150.0f, 18.0f, 16.0f }, small);
            SubterraLookAndFeel::drawSpacedText (g, "LP", { 307.0f + offset, 150.0f, 18.0f, 16.0f }, small);
        }
        g.fillRect (369.0f, 120.0f, 1.5f, 62.0f);

        g.drawRoundedRectangle (juce::Rectangle<float> (200.0f, 68.0f, 340.0f, 34.0f).reduced (0.75f), 17.0f, 1.5f);

        const auto sideLabel = lnf.inter (12.0f, 700, 0.5f);
        SubterraLookAndFeel::drawSpacedText (g, "TRACKING", { 66.0f, 520.0f, 120.0f, 15.0f }, sideLabel);
        SubterraLookAndFeel::drawSpacedText (g, "GLIDE", { 66.0f, 605.0f, 120.0f, 15.0f }, sideLabel);

        paintCables (g);

        juce::Path pedalShape;
        pedalShape.addRoundedRectangle (juce::Rectangle<float> (215.0f, 262.0f, 310.0f, 619.0f), 28.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.38f), 50, { 0, 34 }).drawForPath (g, pedalShape);
        juce::DropShadow (juce::Colours::black.withAlpha (0.25f), 18, { 0, 12 }).drawForPath (g, pedalShape);
    }

    void paintCables (juce::Graphics& g)
    {
        // Patch cables plugged into the top-mounted jacks.
        for (bool right : { false, true })
        {
            auto x = [right] (float left, float width) { return right ? (float) designWidth - left - width : left; };

            juce::ColourGradient cable (juce::Colour (0xff4a4a4a), 0.0f, 208.0f, juce::Colours::black, 0.0f, 228.0f, false);
            cable.addColour (0.55, juce::Colour (0xff151515));
            g.setGradientFill (cable);
            g.fillRect (x (0.0f, 150.0f), 208.0f, 150.0f, 20.0f);

            juce::ColourGradient boot (juce::Colour (0xfff4f4f4), 0.0f, 200.0f, juce::Colour (0xffd9d9d9), 0.0f, 236.0f, false);
            boot.addColour (0.55, juce::Colour (0xff9b9b9b));
            g.setGradientFill (boot);
            g.fillRoundedRectangle (x (140.0f, 34.0f), 200.0f, 34.0f, 36.0f, 6.0f);

            const float gripX = x (172.0f, 56.0f);
            for (float sx = 0.0f; sx < 56.0f; sx += 5.0f)
            {
                g.setColour (juce::Colour (0xff8a8a8a));
                g.fillRect (gripX + sx, 198.0f, 2.0f, 40.0f);
                g.setColour (juce::Colour (0xffe4e4e4));
                g.fillRect (gripX + sx + 2.0f, 198.0f, juce::jmin (3.0f, 56.0f - sx - 2.0f), 40.0f);
            }
            juce::ColourGradient gripShade (juce::Colours::white.withAlpha (0.35f), 0.0f, 198.0f,
                                            juce::Colours::black.withAlpha (0.28f), 0.0f, 238.0f, false);
            g.setGradientFill (gripShade);
            g.fillRect (gripX, 198.0f, 56.0f, 40.0f);

            juce::ColourGradient head (juce::Colour (0xfff6f6f6), 0.0f, 196.0f, juce::Colour (0xffd6d6d6), 0.0f, 242.0f, false);
            head.addColour (0.6, juce::Colour (0xffa2a2a2));
            g.setGradientFill (head);
            g.fillRoundedRectangle (x (226.0f, 58.0f), 196.0f, 58.0f, 46.0f, 8.0f);

            const float nutX = x (240.0f, 30.0f);
            juce::ColourGradient sleeve (juce::Colour (0xff9a9a9a), nutX, 0.0f, juce::Colour (0xff9a9a9a), nutX + 30.0f, 0.0f, false);
            sleeve.addColour (0.5, juce::Colour (0xffefefef));
            g.setGradientFill (sleeve);
            g.fillRect (nutX, 240.0f, 30.0f, 16.0f);

            const float hexX = x (236.0f, 38.0f);
            juce::Path hex;
            hex.startNewSubPath (hexX + 38.0f * 0.12f, 254.0f);
            hex.lineTo (hexX + 38.0f * 0.88f, 254.0f);
            hex.lineTo (hexX + 38.0f, 259.0f);
            hex.lineTo (hexX + 38.0f * 0.88f, 264.0f);
            hex.lineTo (hexX + 38.0f * 0.12f, 264.0f);
            hex.lineTo (hexX, 259.0f);
            hex.closeSubPath();
            juce::ColourGradient hexFill (juce::Colour (0xfff0f0f0), 0.0f, 254.0f, juce::Colour (0xffc8c8c8), 0.0f, 264.0f, false);
            hexFill.addColour (0.6, juce::Colour (0xff8e8e8e));
            g.setGradientFill (hexFill);
            g.fillPath (hex);
        }
    }

    static juce::Path settingsIcon()
    {
        auto p = svgPath ("M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4M11.2 11.2l1.4 1.4M3.4 12.6l1.4-1.4M11.2 4.8l1.4-1.4");
        p.addEllipse (5.5f, 5.5f, 5.0f, 5.0f);
        return p;
    }

    void attach (Knob& knob, const char* id)
    {
        auto& apvts = processor.getState();
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (apvts, id, knob));
        auto* param = apvts.getParameter (id);
        knob.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    SubterraEditor& editor;
    SubterraProcessor& processor;
    SubterraLookAndFeel& lnf;

    StereoMeter inMeter, outMeter;
    Knob input { KnobStyle::frameLarge, "Input", "gain into the pedal" };
    Knob output { KnobStyle::frameLarge, "Output", "final output level" };
    Knob inHp { KnobStyle::frameSmall, "In HP", "input high-pass cutoff" };
    Knob inLp { KnobStyle::frameSmall, "In LP", "input low-pass cutoff" };
    Knob outHp { KnobStyle::frameSmall, "Out HP", "output high-pass cutoff" };
    Knob outLp { KnobStyle::frameSmall, "Out LP", "output low-pass cutoff" };
    Knob tracking { KnobStyle::frameMedium, "Tracking", "sensitivity: how quietly you can play and still trigger the synth" };
    Knob glide { KnobStyle::frameMedium, "Glide", "portamento time between legato notes" };
    NoteDisplay noteDisplay;
    PedalBody pedal;
    RoundButton settingsButton, saveButton, browseButton, abButton;
    FlatTextButton prevButton, nextButton, presetName;
    PillToggle inFilters, outFilters, tunerButton;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
};

//==============================================================================
SubterraEditor::SubterraEditor (SubterraProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);

    panel = std::make_unique<Panel> (*this, processor, lnf);
    addAndMakeVisible (*panel);

    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    const float scale = juce::jlimit (0.5f, 2.0f, processor.editorScale.load());
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));

    refreshPresetName();
    startTimerHz (30);
}

SubterraEditor::~SubterraEditor()
{
    stopTimer();
    saveDialog.reset();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SubterraEditor::paint (juce::Graphics& g) { g.fillAll (Colours::frame); }

void SubterraEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    panel->setTransform (juce::AffineTransform::scale (scale));
    processor.editorScale = scale;
}

void SubterraEditor::refreshPresetName()
{
    auto& presets = processor.getPresets();
    presets.displayChanged = false;
    panel->presetName.setButtonText (presets.getCurrentName() + (presets.isModified() ? " *" : ""));
    panel->presetName.repaint();
    panel->abButton.repaint();
}

void SubterraEditor::timerCallback()
{
    // Meters: instant attack, about 20 dB/s fall.
    const float fall = juce::Decibels::decibelsToGain (-20.0f / 30.0f);
    PeakMeter* meters[] = { &processor.inputMeters[0], &processor.inputMeters[1], &processor.outputMeters[0], &processor.outputMeters[1] };
    for (int i = 0; i < 4; ++i)
        meterDisplay[i] = juce::jmax (meters[i]->take(), meterDisplay[i] * fall);

    panel->inMeter.setLevels (meterDisplay[0], meterDisplay[1]);
    panel->outMeter.setLevels (meterDisplay[2], meterDisplay[3]);

    auto& apvts = processor.getState();
    const bool tunerOn = apvts.getRawParameterValue (ParamIDs::tuner)->load() > 0.5f;
    panel->noteDisplay.update (processor.detectedFrequency.load(), tunerOn, processor.tunerReference.load());

    const bool active = apvts.getRawParameterValue (ParamIDs::bypass)->load() < 0.5f;
    const bool hold = apvts.getRawParameterValue (ParamIDs::hold)->load() > 0.5f;
    panel->pedal.setLeds (active, hold);

    if (processor.getPresets().displayChanged.load())
        refreshPresetName();
}

void SubterraEditor::showPresetMenu()
{
    auto& presets = processor.getPresets();
    presets.rescanUserPresets();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSectionHeader ("Factory");
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        menu.addItem (i + 1, presets.getPreset (i).name, true, i == presets.getCurrentIndex());

    if (presets.getNumPresets() > presets.getNumFactoryPresets())
    {
        menu.addSectionHeader ("Your presets");
        for (int i = presets.getNumFactoryPresets(); i < presets.getNumPresets(); ++i)
            menu.addItem (i + 1, presets.getPreset (i).name, true, i == presets.getCurrentIndex());
    }

    constexpr int openFolderId = 10000;
    menu.addSeparator();
    menu.addItem (openFolderId, "Show presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&panel->presetName),
                        [safe = juce::Component::SafePointer<SubterraEditor> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            auto& pm = safe->processor.getPresets();
                            if (result == openFolderId)
                            {
                                pm.getUserFolder().createDirectory();
                                pm.getUserFolder().startAsProcess();
                                return;
                            }
                            pm.loadPreset (result - 1);
                            safe->refreshPresetName();
                        });
}

void SubterraEditor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name this sound:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lnf);
    const auto current = processor.getPresets().getCurrentName();
    saveDialog->addTextEditor ("name", current == "Init" ? juce::String() : current, {});
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create (
                                           [safe = juce::Component::SafePointer<SubterraEditor> (this)] (int result)
                                           {
                                               if (safe == nullptr || safe->saveDialog == nullptr)
                                                   return;
                                               const auto name = safe->saveDialog->getTextEditorContents ("name");
                                               if (result == 1 && name.trim().isNotEmpty())
                                               {
                                                   if (! safe->processor.getPresets().saveUserPreset (name))
                                                       juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Subterra",
                                                                                               "The preset could not be saved.");
                                                   safe->refreshPresetName();
                                               }
                                               safe->saveDialog.reset();
                                           }),
                                false);
}

void SubterraEditor::showSettingsMenu()
{
    juce::PopupMenu reference;
    const float current = processor.tunerReference.load();
    for (int hz : { 432, 435, 438, 440, 441, 442, 443, 444, 446 })
        reference.addItem ("A4 = " + juce::String (hz) + " Hz", true, std::abs (current - (float) hz) < 0.01f,
                           [this, hz] { processor.tunerReference = (float) hz; });

    juce::PopupMenu size;
    const float scale = (float) getWidth() / (float) designWidth;
    for (int percent : { 50, 75, 100, 125, 150, 200 })
        size.addItem (juce::String (percent) + "%", true, std::abs (scale * 100.0f - (float) percent) < 1.0f,
                      [safe = juce::Component::SafePointer<SubterraEditor> (this), percent]
                      {
                          if (safe != nullptr)
                              safe->setSize (designWidth * percent / 100, designHeight * percent / 100);
                      });

    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addSubMenu ("Tuner reference", reference);
    menu.addItem ("Mute output while tuning", true, processor.tunerMutesOutput.load(),
                  [this] { processor.tunerMutesOutput = ! processor.tunerMutesOutput.load(); });
    menu.addSubMenu ("Window size", size);
    menu.addSeparator();
    menu.addItem ("Reset to Init", [this]
                  {
                      auto& presets = processor.getPresets();
                      for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
                          if (presets.getPreset (i).name == "Init")
                              presets.loadPreset (i);
                      refreshPresetName();
                  });
    menu.addSeparator();
    menu.addItem ("Subterra " + juce::String (JucePlugin_VersionString), false, false, nullptr);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&panel->settingsButton));
}

void SubterraEditor::showAbMenu()
{
    const bool onA = processor.getActiveSlot() == 0;
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    menu.addItem (onA ? "Copy A to B" : "Copy B to A", [this] { processor.copyActiveSlotToOther(); });
    menu.addItem (onA ? "Switch to B" : "Switch to A", [this]
                  {
                      processor.switchSlot();
                      refreshPresetName();
                  });
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&panel->abButton));
}
}
