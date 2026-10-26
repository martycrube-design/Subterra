#include "EffectsPage.h"
#include "PedalPage.h"

namespace subterra
{
namespace
{
constexpr float unitWidth = 200.0f, unitHeight = 476.0f, unitTop = 70.0f, jackY = 120.0f;
constexpr float unitX[] = { 40.0f, 270.0f, 500.0f };

void drawUnitLabel (juce::Graphics& g, SubterraLookAndFeel& lnf, const juce::String& text, juce::Rectangle<int> knobBounds)
{
    SubterraLookAndFeel::drawSpacedText (g, text, knobBounds.toFloat().withY ((float) knobBounds.getBottom() + 4.0f).withHeight (20.0f)
                                                       .withSizeKeepingCentre (100.0f, 20.0f),
                                         lnf.bebas (18.0f, 1.0f));
}

void drawGlass (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colour (0xff0b0c0e));
    g.fillRoundedRectangle (r, 6.0f);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(),
                                juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r.reduced (1.0f), 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (r.getX() + 6.0f, r.getBottom() + 0.5f, r.getRight() - 6.0f, r.getBottom() + 0.5f, 1.0f);
}
}

// A patch cable between two jacks, hanging in a curve, with chrome plugs at each end.
void paintPatchCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float sag)
{
    juce::Path cable;
    cable.startNewSubPath (from);
    const float dx = (to.x - from.x) * 0.35f;
    cable.cubicTo (from.translated (dx, sag), to.translated (-dx, sag), to);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (cable, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0.0f, 6.0f));
    g.setColour (juce::Colour (0xff0e0e10));
    g.strokePath (cable, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff3a3a3e));
    g.strokePath (cable, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (0.0f, -1.5f));

    for (auto [p, dir] : { std::pair { from, 1.0f }, std::pair { to, -1.0f } })
    {
        const auto plug = juce::Rectangle<float> (dir > 0 ? p.x - 2.0f : p.x - 16.0f, p.y - 7.0f, 18.0f, 14.0f);
        juce::ColourGradient chrome (juce::Colour (0xfff4f4f4), 0.0f, plug.getY(), juce::Colour (0xff8e8e8e), 0.0f, plug.getBottom(), false);
        chrome.addColour (0.45, juce::Colour (0xffcfcfcf));
        g.setGradientFill (chrome);
        g.fillRoundedRectangle (plug, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        for (float x = plug.getX() + 4.0f; x < plug.getRight() - 3.0f; x += 3.0f)
            g.fillRect (x, plug.getY() + 1.0f, 1.0f, plug.getHeight() - 2.0f);
    }
}

//==============================================================================
EffectUnit::EffectUnit (SubterraLookAndFeel& l, const juce::String& t, const juce::String& sub, juce::Colour a)
    : footswitch (t.substring (0, 1) + t.substring (1).toLowerCase() + " on or off"),
      lnf (l), title (t), subtitle (sub), accent (a), led (a)
{
    const auto name = t.substring (0, 1) + t.substring (1).toLowerCase();
    setTitle (name);
    footswitch.setTooltip (name + ": switch the effect on or off");
    addAndMakeVisible (footswitch);
    footswitch.setBounds (64, 372, 72, 72);
}

void EffectUnit::setOn (bool on)
{
    led.set (on, initialised);
    onAmount.animateTo (on ? 1.0f : 0.0f, initialised ? 200.0 : 0.0);
    initialised = true;
}

void EffectUnit::paint (juce::Graphics& g)
{
    const auto body = getLocalBounds().toFloat().withTrimmedBottom (6.0f);
    constexpr float radius = 20.0f;

    // Enclosure: dark anodised metal with a lip, lit from the top left.
    g.setColour (juce::Colour (0xff101113));
    g.fillRoundedRectangle (body.translated (0.0f, 6.0f), radius);
    juce::ColourGradient metal (juce::Colour (0xff2c2d32), body.getX(), body.getY(),
                                juce::Colour (0xff1c1d21), body.getX(), body.getBottom(), false);
    g.setGradientFill (metal);
    g.fillRoundedRectangle (body, radius);

    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.16f), body.getX(), body.getY(),
                                juce::Colours::black.withAlpha (0.18f), body.getRight(), body.getBottom(), false);
    sheen.addColour (0.3, juce::Colours::white.withAlpha (0.0f));
    sheen.addColour (0.65, juce::Colours::black.withAlpha (0.0f));
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (body, radius);

    {
        g.saveState();
        juce::Path clip;
        clip.addRoundedRectangle (body, radius);
        g.reduceClipRegion (clip);
        fillPowderCoat (g, body, 0.08f);
        g.restoreState();
    }

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (radius, 1.0f, body.getRight() - radius, 1.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawRoundedRectangle (body.reduced (0.5f), radius, 1.0f);

    // Title block
    g.setColour (Colours::ink);
    SubterraLookAndFeel::drawSpacedText (g, title, { 22.0f, 18.0f, 140.0f, 34.0f }, lnf.bebas (30.0f, 4.0f),
                                         juce::Justification::centredLeft);
    g.setColour (Colours::ink.withAlpha (0.55f));
    SubterraLookAndFeel::drawSpacedText (g, subtitle, { 22.0f, 52.0f, 156.0f, 12.0f }, lnf.inter (8.0f, 700, 2.5f),
                                         juce::Justification::centredLeft);
    g.setColour (accent.withAlpha (0.25f + 0.55f * onAmount.get()));
    g.fillRoundedRectangle (22.0f, 70.0f, 156.0f, 2.0f, 1.0f);

    led.paint (g, { 158.0f, 24.0f }, 20.0f);

    // Knob shadows (the knobs draw themselves on top).
    for (auto* child : getChildren())
    {
        if (auto* knob = dynamic_cast<Knob*> (child); knob != nullptr && knob->isVisible())
        {
            juce::Path p;
            p.addEllipse (knob->getBounds().toFloat());
            const bool small = knob->getWidth() < 60;
            juce::DropShadow (juce::Colours::black.withAlpha (0.45f), small ? 10 : 14, { 0, small ? 7 : 10 }).drawForPath (g, p);
            juce::DropShadow (juce::Colours::black.withAlpha (0.5f), small ? 2 : 3, { 0, 2 }).drawForPath (g, p);
        }
    }

    // Knob legends
    g.setColour (Colours::ink);
    for (auto* child : getChildren())
        if (auto* knob = dynamic_cast<Knob*> (child); knob != nullptr && knob->isVisible())
            drawUnitLabel (g, lnf, knob->getProperties().getWithDefault ("legend", knob->getTitle()).toString(), knob->getBounds());

    // Display
    drawGlass (g, displayArea);
    if (drawDisplay)
    {
        g.saveState();
        g.reduceClipRegion (displayArea.reduced (2.0f).toNearestInt());
        drawDisplay (g, displayArea.reduced (8.0f, 6.0f), onAmount.get());
        g.restoreState();
    }

    // Side jacks
    for (float x : { -3.0f, body.getRight() - 11.0f })
    {
        const auto jack = juce::Rectangle<float> (x, jackY - 10.0f, 14.0f, 20.0f);
        juce::ColourGradient nut (juce::Colour (0xffe6e6e6), 0.0f, jack.getY(), juce::Colour (0xff7b7b7b), 0.0f, jack.getBottom(), false);
        g.setGradientFill (nut);
        g.fillRoundedRectangle (jack, 3.0f);
    }

    g.setColour (Colours::ink.withAlpha (0.8f));
    SubterraLookAndFeel::drawSpacedText (g, "ON / OFF", { 0.0f, 450.0f, unitWidth, 14.0f }, lnf.inter (9.0f, 700, 2.0f));
}

void EffectUnit::paintOverChildren (juce::Graphics& g)
{
    // A bypassed unit is dimmed slightly so the active chain reads at a glance.
    const float off = 1.0f - onAmount.get();
    if (off > 0.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f * off));
        g.fillRoundedRectangle (getLocalBounds().toFloat().withTrimmedBottom (6.0f).withTrimmedTop (80.0f).withTrimmedBottom (100.0f), 8.0f);
    }
}

//==============================================================================
EffectsPage::EffectsPage (SubterraLookAndFeel& l, juce::AudioProcessorValueTreeState& state, std::atomic<float>& hostTempo)
    : routing (l, "Effects routing", { "SYNTH", "SYNTH + BASS" }),
      chorusUnit (l, "CHORUS", "STEREO MODULATION", Accent::chorus),
      delayUnit (l, "DELAY", "TAPE-STYLE ECHO", Accent::delay),
      reverbUnit (l, "REVERB", "PLATE", Accent::reverb),
      chorusRate (KnobStyle::pedalLarge, "Chorus Rate", "speed of the chorus sweep"),
      chorusDepth (KnobStyle::pedalLarge, "Chorus Depth", "how far the chorus sweeps"),
      chorusMix (KnobStyle::pedalSmall, "Chorus Mix", "amount of chorus"),
      delayTime (KnobStyle::pedalLarge, "Delay Time", "time between repeats"),
      delayDivision (KnobStyle::pedalLarge, "Delay Division", "time between repeats, in beats of the host tempo"),
      delayFeedback (KnobStyle::pedalLarge, "Delay Feedback", "how many repeats"),
      delayTone (KnobStyle::pedalLarge, "Delay Tone", "repeats get darker as you turn it down"),
      delayMix (KnobStyle::pedalLarge, "Delay Mix", "level of the repeats"),
      reverbSize (KnobStyle::pedalLarge, "Reverb Size", "length of the reverb"),
      reverbTone (KnobStyle::pedalLarge, "Reverb Tone", "dark to bright"),
      reverbMix (KnobStyle::pedalSmall, "Reverb Mix", "amount of reverb"),
      delaySync (l, "Delay sync to host tempo", {}, "SYNC", "FREE"),
      lnf (l), apvts (state), tempo (hostTempo)
{
    routing.setTooltip ("Which signal goes through the effects: the synth only, or the synth and your dry bass");
    addAndMakeVisible (routing);
    routing.setBounds (300, 18, 240, 28);

    EffectUnit* units[] = { &chorusUnit, &delayUnit, &reverbUnit };
    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible (units[i]);
        units[i]->setBounds ((int) unitX[i], (int) unitTop, (int) unitWidth, (int) unitHeight);
    }

    auto place = [] (EffectUnit& unit, Knob& knob, const char* legend, int x, int y, int size)
    {
        knob.getProperties().set ("legend", legend);
        unit.addAndMakeVisible (knob);
        knob.setBounds (x, y, size, size);
    };

    place (chorusUnit, chorusRate, "RATE", 26, 88, 64);
    place (chorusUnit, chorusDepth, "DEPTH", 110, 88, 64);
    place (chorusUnit, chorusMix, "MIX", 77, 192, 46);

    place (delayUnit, delayTime, "TIME", 26, 88, 64);
    place (delayUnit, delayDivision, "TIME", 26, 88, 64);
    place (delayUnit, delayFeedback, "FEEDBACK", 110, 88, 64);
    place (delayUnit, delayTone, "TONE", 26, 184, 64);
    place (delayUnit, delayMix, "MIX", 110, 184, 64);
    delayUnit.addAndMakeVisible (delaySync);
    delaySync.setBounds (64, 351, 72, 24);
    delaySync.setTooltip ("Sync: delay time follows the host tempo in note values");
    delayUnit.displayArea = { 22.0f, 280.0f, 156.0f, 62.0f };
    delayUnit.footswitch.setBounds (64, 378, 72, 72);

    place (reverbUnit, reverbSize, "SIZE", 26, 88, 64);
    place (reverbUnit, reverbTone, "TONE", 110, 88, 64);
    place (reverbUnit, reverbMix, "MIX", 77, 192, 46);

    // Displays
    chorusUnit.animateDisplay = [this]
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const double dt = lastFrameMs > 0.0 ? juce::jmin (100.0, now - lastFrameMs) : 0.0;
        lastFrameMs = now;
        if (param (ParamIDs::chorusOn) < 0.5f)
            return false;
        chorusPhase = std::fmod (chorusPhase + dt * 0.001 * param (ParamIDs::chorusRate), 1.0);
        return true;
    };
    chorusUnit.drawDisplay = [this] (juce::Graphics& g, juce::Rectangle<float> r, float on)
    {
        const float depth = param (ParamIDs::chorusDepth);
        juce::Path wave;
        const auto plot = r.withTrimmedBottom (14.0f);
        for (int i = 0; i <= 64; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / 64.0f;
            const double ph = chorusPhase + 2.0 * i / 64.0;
            const float y = plot.getCentreY() - (0.15f + 0.85f * depth) * plot.getHeight() * 0.45f
                                                    * (float) std::sin (juce::MathConstants<double>::twoPi * ph);
            if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
        }
        const auto colour = Accent::chorus.withAlpha (0.35f + 0.65f * on);
        g.setColour (colour.withAlpha (0.25f * on));
        g.strokePath (wave, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont (lnf.inter (9.0f, 600));
        const auto labels = r.withTop (r.getBottom() - 12.0f);
        g.drawText (apvts.getParameter (ParamIDs::chorusRate)->getCurrentValueAsText(), labels,
                    juce::Justification::centredLeft, false);
        g.drawText ("DEPTH " + apvts.getParameter (ParamIDs::chorusDepth)->getCurrentValueAsText(),
                    labels, juce::Justification::centredRight, false);
    };

    delayUnit.drawDisplay = [this] (juce::Graphics& g, juce::Rectangle<float> r, float on)
    {
        const bool sync = param (ParamIDs::delaySync) > 0.5f;
        const int division = juce::jlimit (0, 7, juce::roundToInt (param (ParamIDs::delayDivision)));
        const float bpm = tempo.load();
        const float ms = sync ? delayDivisionBeats[division] * 60000.0f / bpm : param (ParamIDs::delayTime);

        g.setColour (Accent::delay.withAlpha (0.35f + 0.65f * on));
        g.setFont (lnf.bebas (28.0f, 1.0f));
        const auto main = sync ? apvts.getParameter (ParamIDs::delayDivision)->getCurrentValueAsText()
                               : apvts.getParameter (ParamIDs::delayTime)->getCurrentValueAsText();
        g.drawText (main.toUpperCase(), r.removeFromTop (30.0f), juce::Justification::centredLeft, false);
        g.setFont (lnf.inter (9.0f, 600));
        g.drawText (sync ? juce::String (juce::roundToInt (ms)) + " ms  @ " + juce::String (bpm, 1) + " BPM" : juce::String ("FREE TIME"),
                    r.removeFromTop (14.0f), juce::Justification::centredLeft, false);

        // Repeat markers fading by the feedback amount.
        const float fb = param (ParamIDs::delayFeedback);
        auto row = r.withTrimmedTop (2.0f);
        float level = 1.0f;
        for (int i = 0; i < 9 && level > 0.05f; ++i)
        {
            g.setColour (Accent::delay.withAlpha ((0.2f + 0.8f * on) * level));
            g.fillRoundedRectangle (row.getX() + (float) i * 16.0f, row.getY(), 10.0f, 4.0f, 2.0f);
            level *= juce::jmax (fb, 0.0001f);
        }
    };

    reverbUnit.drawDisplay = [this] (juce::Graphics& g, juce::Rectangle<float> r, float on)
    {
        const float size = param (ParamIDs::reverbSize);
        const float tone = param (ParamIDs::reverbTone);
        const auto plot = r.withTrimmedBottom (14.0f);
        const float length = 0.25f + 0.75f * size;

        juce::Path tail;
        tail.startNewSubPath (plot.getBottomLeft());
        for (int i = 0; i <= 48; ++i)
        {
            const float t = (float) i / 48.0f;
            const float level = t < 0.04f ? t / 0.04f : std::exp (-(t - 0.04f) * 5.0f / length);
            tail.lineTo (plot.getX() + plot.getWidth() * t, plot.getBottom() - plot.getHeight() * level);
        }
        tail.lineTo (plot.getBottomRight());
        tail.closeSubPath();

        const auto colour = Accent::reverb.withAlpha (0.35f + 0.65f * on);
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f * (0.4f + 0.6f * tone)), plot.getX(), plot.getY(),
                                                 colour.withAlpha (0.05f), plot.getX(), plot.getBottom(), false));
        g.fillPath (tail);
        g.setColour (colour);
        g.strokePath (tail, juce::PathStrokeType (1.4f));

        g.setFont (lnf.inter (9.0f, 600));
        g.drawText ("SIZE " + apvts.getParameter (ParamIDs::reverbSize)->getCurrentValueAsText(), r.withTop (r.getBottom() - 12.0f),
                    juce::Justification::centredLeft, false);
        g.drawText ("TONE " + apvts.getParameter (ParamIDs::reverbTone)->getCurrentValueAsText(), r.withTop (r.getBottom() - 12.0f),
                    juce::Justification::centredRight, false);
    };

    refresh();
}

float EffectsPage::param (const char* id) const
{
    return apvts.getRawParameterValue (id)->load();
}

void EffectsPage::refresh()
{
    chorusUnit.setOn (param (ParamIDs::chorusOn) > 0.5f);
    delayUnit.setOn (param (ParamIDs::delayOn) > 0.5f);
    reverbUnit.setOn (param (ParamIDs::reverbOn) > 0.5f);

    const bool sync = param (ParamIDs::delaySync) > 0.5f;
    if (delayDivision.isVisible() != sync)
    {
        delayDivision.setVisible (sync);
        delayTime.setVisible (! sync);
        delayUnit.repaint();
    }

    // Displays follow knob moves and tempo changes; redraw one only when what it shows has changed.
    const float chorusState[] = { param (ParamIDs::chorusRate), param (ParamIDs::chorusDepth) };
    const float delayState[] = { param (ParamIDs::delaySync), param (ParamIDs::delayDivision), param (ParamIDs::delayTime),
                                 param (ParamIDs::delayFeedback), tempo.load() };
    const float reverbState[] = { param (ParamIDs::reverbSize), param (ParamIDs::reverbTone) };
    auto changed = [] (auto& last, const auto& now)
    {
        bool diff = false;
        for (size_t i = 0; i < std::size (now); ++i)
            if (! juce::exactlyEqual (last[i], now[i]))
            {
                last[i] = now[i];
                diff = true;
            }
        return diff;
    };
    if (changed (lastChorus, chorusState))
        chorusUnit.repaint (chorusUnit.displayArea.toNearestInt().expanded (2));
    if (changed (lastDelay, delayState))
        delayUnit.repaint (delayUnit.displayArea.toNearestInt().expanded (2));
    if (changed (lastReverb, reverbState))
        reverbUnit.repaint (reverbUnit.displayArea.toNearestInt().expanded (2));
}

void EffectsPage::paint (juce::Graphics& g)
{
    g.setColour (Colours::frameInk);
    SubterraLookAndFeel::drawSpacedText (g, "EFFECTS ON", { 196.0f, 18.0f, 96.0f, 28.0f }, lnf.inter (12.0f, 700, 1.0f),
                                         juce::Justification::centredRight);

    // Signal chain: in -> chorus -> delay -> reverb -> out.
    const float y = unitTop + jackY;
    for (float x : unitX)
    {
        juce::Path shadow;
        shadow.addRoundedRectangle (juce::Rectangle<float> (x, unitTop, unitWidth, unitHeight), 20.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.4f), 36, { 0, 22 }).drawForPath (g, shadow);
    }

    g.setColour (Colours::frameInk.withAlpha (0.55f));
    g.setFont (lnf.inter (9.0f, 700));
    g.drawText ("IN", juce::Rectangle<float> (6.0f, y - 30.0f, 30.0f, 12.0f), juce::Justification::centredLeft, false);
    g.drawText ("OUT", juce::Rectangle<float> (704.0f, y - 30.0f, 30.0f, 12.0f), juce::Justification::centredRight, false);
}

void EffectsPage::paintOverChildren (juce::Graphics& g)
{
    const float y = unitTop + jackY;
    paintPatchCable (g, { -10.0f, y }, { unitX[0] + 1.0f, y }, 0.0f);
    paintPatchCable (g, { unitX[0] + unitWidth - 1.0f, y }, { unitX[1] + 1.0f, y }, 26.0f);
    paintPatchCable (g, { unitX[1] + unitWidth - 1.0f, y }, { unitX[2] + 1.0f, y }, 26.0f);
    paintPatchCable (g, { unitX[2] + unitWidth - 1.0f, y }, { 750.0f, y }, 0.0f);
}
}
