#include "PedalPage.h"

namespace subterra
{
namespace
{
juce::String noteName (int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((midiNote % 12) + 12) % 12]) + juce::String (midiNote / 12 - 1);
}

// The page sits 190 px down the editor; its art is drawn in editor coordinates.
constexpr float pageTop = 190.0f;
}

//==============================================================================
NoteDisplay::NoteDisplay (SubterraLookAndFeel& l) : lnf (l)
{
    setTitle ("Note readout");
    setAccessible (true);
}

void NoteDisplay::update (float newFrequency, bool newTuner, float newReference)
{
    if (newTuner != tunerOn)
        modeFade.animateTo (newTuner ? 1.0f : 0.0f, 200.0);

    if (std::abs (newFrequency - frequency) > 0.05f || newTuner != tunerOn || std::abs (newReference - reference) > 0.01f)
    {
        frequency = newFrequency;
        tunerOn = newTuner;
        reference = newReference;

        if (frequency > 0.0f)
        {
            const float midi = 69.0f + 12.0f * std::log2 (frequency / reference);
            const float cents = (midi - std::round (midi)) * 100.0f;
            needle.animateTo (juce::jlimit (-50.0f, 50.0f, cents), 90.0);
        }
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
    const float x = scale.getCentreX() + juce::jlimit (-1.0f, 1.0f, needle.get() / 50.0f) * scale.getWidth() * 0.5f;
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
    activeLed.set (active, ledsInitialised);
    holdLed.set (hold, ledsInitialised);
    ledsInitialised = true;
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
        fillPowderCoat (g, body, 0.07f);

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

    activeLed.paint (g, { 49.0f, 446.0f });
    holdLed.paint (g, { 235.0f, 446.0f });

    g.setColour (Colours::ink);
    const auto switchLabel = lnf.inter (11.0f, 700, 2.0f);
    SubterraLookAndFeel::drawSpacedText (g, "BYPASS", { 12.0f, 568.0f, 100.0f, 14.0f }, switchLabel);
    SubterraLookAndFeel::drawSpacedText (g, "HOLD", { 198.0f, 568.0f, 100.0f, 14.0f }, switchLabel);
}


//==============================================================================
PedalPage::PedalPage (SubterraLookAndFeel& l)
    : pedal (l), noteDisplay (l), tunerButton (l, "Tuner on or off", "TUNER"), lnf (l)
{
    for (auto* c : std::initializer_list<juce::Component*> { &pedal, &tracking, &glide, &noteDisplay, &tunerButton })
        addAndMakeVisible (c);

    tunerButton.setTooltip ("Tuner: shows tuning in the readout and mutes the output");

    pedal.setBounds (215, 262 - (int) pageTop, 310, 619);
    tracking.setBounds (106, 543 - (int) pageTop, 40, 40);
    glide.setBounds (106, 628 - (int) pageTop, 40, 40);
    noteDisplay.setBounds (556, 520 - (int) pageTop, 140, 88);
    tunerButton.setBounds (560, 830 - (int) pageTop, 124, 34);
}

void PedalPage::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::translation (0.0f, -pageTop));
    g.setColour (Colours::frameInk);

    const auto sideLabel = lnf.inter (12.0f, 700, 0.5f);
    SubterraLookAndFeel::drawSpacedText (g, "TRACKING", { 66.0f, 520.0f, 120.0f, 15.0f }, sideLabel);
    SubterraLookAndFeel::drawSpacedText (g, "GLIDE", { 66.0f, 605.0f, 120.0f, 15.0f }, sideLabel);

    paintCables (g);

    juce::Path pedalShape;
    pedalShape.addRoundedRectangle (juce::Rectangle<float> (215.0f, 262.0f, 310.0f, 619.0f), 28.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.38f), 50, { 0, 34 }).drawForPath (g, pedalShape);
    juce::DropShadow (juce::Colours::black.withAlpha (0.25f), 18, { 0, 12 }).drawForPath (g, pedalShape);
}

void PedalPage::paintCables (juce::Graphics& g)
{
    // Patch cables plugged into the top-mounted jacks.
    for (bool right : { false, true })
    {
        auto x = [right] (float left, float width) { return right ? 740.0f - left - width : left; };

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

}
