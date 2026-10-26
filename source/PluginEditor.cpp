#include "PluginEditor.h"

namespace subterra
{
namespace
{
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

juce::String quoted (const juce::String& s)
{
    return juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + s + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d"));
}

juce::String trashName()
{
   #if JUCE_WINDOWS
    return "Recycle Bin";
   #else
    return "Trash";
   #endif
}
}

//==============================================================================
PageStack::PageStack (juce::Component& first, juce::Component& second) : pages { &first, &second }
{
    for (auto* p : pages)
    {
        addAndMakeVisible (p);
        // Each page is cached as an image so sliding it is a cheap blit, not a full repaint.
        p->setBufferedToImage (true);
    }
    pages[1]->setVisible (false);
}

void PageStack::show (int newIndex, bool animate)
{
    newIndex = juce::jlimit (0, 1, newIndex);
    index = newIndex;
    for (auto* p : pages)
        p->setVisible (true);
    slide.animateTo ((float) newIndex, animate && isShowing() ? 420.0 : 0.0, Easing::inOutCubic);
    layout();
}

void PageStack::layout()
{
    const float t = slide.get();
    const float w = (float) getWidth();
    for (auto* p : pages)
        p->setBounds (getLocalBounds());
    pages[0]->setTransform (juce::AffineTransform::translation (-w * t, 0.0f));
    pages[1]->setTransform (juce::AffineTransform::translation (w * (1.0f - t), 0.0f));

    if (! slide.isRunning())
    {
        pages[0]->setVisible (index == 0);
        pages[1]->setVisible (index == 1);
    }
}

//==============================================================================
class SubterraEditor::Content final : public juce::Component
{
public:
    Content (SubterraEditor& e, SubterraProcessor& p, SubterraLookAndFeel& l)
        : editor (e), processor (p), lnf (l), header (l), pedalPage (l), effectsPage (l, p.getState(), p.hostTempo),
          pages (pedalPage, effectsPage), nav (l, "Page", { "PEDAL", "EFFECTS" }), overlay (l), toast (l)
    {
        setSize (designWidth, designHeight);

        addAndMakeVisible (header);
        addAndMakeVisible (pages);
        addAndMakeVisible (nav);
        addChildComponent (toast);
        addChildComponent (overlay);

        header.setBounds (0, 0, designWidth, 190);
        pages.setBounds (0, 190, designWidth, designHeight - 190);
        nav.setBounds (20, 830, 186, 34);
        toast.setBounds (200, 838, 340, 48);
        overlay.setBounds (getLocalBounds());

        auto& pedal = pedalPage.pedal;
        for (auto* k : { &header.input, &header.output, &header.inHp, &header.inLp, &header.outHp, &header.outLp,
                         &pedalPage.tracking, &pedalPage.glide, &pedal.wave, &pedal.octave, &pedal.filter, &pedal.reso,
                         &pedal.env, &pedal.drive, &pedal.mix, &effectsPage.chorusRate, &effectsPage.chorusDepth,
                         &effectsPage.chorusMix, &effectsPage.delayTime, &effectsPage.delayDivision, &effectsPage.delayFeedback,
                         &effectsPage.delayTone, &effectsPage.delayMix, &effectsPage.reverbSize, &effectsPage.reverbTone,
                         &effectsPage.reverbMix })
            k->setPopupDisplayEnabled (true, true, this, 900);

        attach (header.input, ParamIDs::inputGain);
        attach (header.output, ParamIDs::outputGain);
        attach (header.inHp, ParamIDs::inHighPass);
        attach (header.inLp, ParamIDs::inLowPass);
        attach (header.outHp, ParamIDs::outHighPass);
        attach (header.outLp, ParamIDs::outLowPass);
        attach (pedalPage.tracking, ParamIDs::tracking);
        attach (pedalPage.glide, ParamIDs::glide);
        attach (pedal.wave, ParamIDs::wave);
        attach (pedal.octave, ParamIDs::octave);
        attach (pedal.filter, ParamIDs::filter);
        attach (pedal.reso, ParamIDs::reso);
        attach (pedal.env, ParamIDs::env);
        attach (pedal.drive, ParamIDs::drive);
        attach (pedal.mix, ParamIDs::mix);
        attach (effectsPage.chorusRate, ParamIDs::chorusRate);
        attach (effectsPage.chorusDepth, ParamIDs::chorusDepth);
        attach (effectsPage.chorusMix, ParamIDs::chorusMix);
        attach (effectsPage.delayTime, ParamIDs::delayTime);
        attach (effectsPage.delayDivision, ParamIDs::delayDivision);
        attach (effectsPage.delayFeedback, ParamIDs::delayFeedback);
        attach (effectsPage.delayTone, ParamIDs::delayTone);
        attach (effectsPage.delayMix, ParamIDs::delayMix);
        attach (effectsPage.reverbSize, ParamIDs::reverbSize);
        attach (effectsPage.reverbTone, ParamIDs::reverbTone);
        attach (effectsPage.reverbMix, ParamIDs::reverbMix);

        attach (header.inFilters, ParamIDs::inFilterOn);
        attach (header.outFilters, ParamIDs::outFilterOn);
        attach (pedalPage.tunerButton, ParamIDs::tuner);
        attach (pedal.bypassSwitch, ParamIDs::bypass);
        attach (pedal.holdSwitch, ParamIDs::hold);
        attach (effectsPage.chorusUnit.footswitch, ParamIDs::chorusOn);
        attach (effectsPage.delayUnit.footswitch, ParamIDs::delayOn);
        attach (effectsPage.reverbUnit.footswitch, ParamIDs::reverbOn);
        attach (effectsPage.delaySync, ParamIDs::delaySync);
        effectsPage.routing.attachTo (*processor.getState().getParameter (ParamIDs::fxRouting));

        header.abButton.drawContent = [this] (juce::Graphics& g, juce::Rectangle<float> area)
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

        // Dots on the Effects tab show which effects are on.
        nav.setTooltip ("Switch between the pedal and the effects");
        nav.drawExtra = [this] (juce::Graphics& g, juce::Rectangle<float> r, int index, bool selected)
        {
            if (index != 1)
                return;
            const juce::Colour colours[] = { Accent::chorus, Accent::delay, Accent::reverb };
            const char* ids[] = { ParamIDs::chorusOn, ParamIDs::delayOn, ParamIDs::reverbOn };
            for (int i = 0; i < 3; ++i)
            {
                const bool on = processor.getState().getRawParameterValue (ids[i])->load() > 0.5f;
                const auto dot = juce::Rectangle<float> (3.5f, 3.5f).withCentre ({ r.getRight() - 15.0f + 4.5f * (float) i, r.getY() + 6.0f });
                g.setColour (on ? colours[i] : (selected ? Colours::frame : Colours::frameInk).withAlpha (0.25f));
                g.fillEllipse (dot);
            }
        };

        header.prevButton.onClick = [this] { processor.getPresets().loadPrevious(); editor.refreshPresetName(); };
        header.nextButton.onClick = [this] { processor.getPresets().loadNext(); editor.refreshPresetName(); };
        header.presetName.onClick = [this] { editor.showPresetBrowser(); };
        header.browseButton.onClick = [this] { editor.showPresetBrowser(); };
        header.saveButton.onClick = [this] { editor.showSaveDialog(); };
        header.settingsButton.onClick = [this] { editor.showSettingsMenu(); };
        header.abButton.onClick = [this]
        {
            if (juce::ModifierKeys::getCurrentModifiers().isPopupMenu())
            {
                editor.showAbMenu();
                return;
            }
            processor.switchSlot();
            header.abButton.repaint();
            editor.refreshPresetName();
        };
        nav.onChange = [this] (int page) { editor.showPage (page); };
    }

    void paint (juce::Graphics& g) override { g.fillAll (Colours::frame); }

    void attach (Knob& knob, const char* id)
    {
        auto& apvts = processor.getState();
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (apvts, id, knob));
        auto* param = apvts.getParameter (id);
        knob.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    void attach (juce::Button& button, const char* id)
    {
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (processor.getState(), id, button));
    }

    SubterraEditor& editor;
    SubterraProcessor& processor;
    SubterraLookAndFeel& lnf;

    Header header;
    PedalPage pedalPage;
    EffectsPage effectsPage;
    PageStack pages;
    SegmentedControl nav;
    Overlay overlay;
    Toast toast;
    int fxMask = -1, lastSlot = -1;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
};

//==============================================================================
SubterraEditor::SubterraEditor (SubterraProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);

    content = std::make_unique<Content> (*this, processor, lnf);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) designHeight);

    const float scale = juce::jlimit (0.5f, 2.0f, processor.editorScale.load());
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));

    showPage (processor.editorPage.load(), false);
    refreshPresetName();
    timerCallback();
    startTimerHz (30);
}

SubterraEditor::~SubterraEditor()
{
    stopTimer();
    content->overlay.dismissImmediately();
    content.reset();
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void SubterraEditor::paint (juce::Graphics& g) { g.fillAll (Colours::frame); }

void SubterraEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    content->setTransform (juce::AffineTransform::scale (scale));
    processor.editorScale = scale;
}

Overlay& SubterraEditor::getOverlay() { return content->overlay; }
Toast& SubterraEditor::getToast() { return content->toast; }
int SubterraEditor::getPage() const { return content->pages.getIndex(); }

void SubterraEditor::showPage (int page, bool animate)
{
    page = juce::jlimit (0, 1, page);
    content->pages.show (page, animate);
    content->nav.setSelected (page, animate && isShowing());
    processor.editorPage = page;
}

void SubterraEditor::refreshPresetName()
{
    auto& presets = processor.getPresets();
    presets.displayChanged = false;
    content->header.presetName.setButtonText (presets.getCurrentName() + (presets.isModified() ? " *" : ""));
    content->header.presetName.repaint();
    content->header.abButton.repaint();
}

void SubterraEditor::timerCallback()
{
    auto& c = *content;

    // Meters: instant attack, about 20 dB/s fall.
    const float fall = juce::Decibels::decibelsToGain (-20.0f / 30.0f);
    PeakMeter* meters[] = { &processor.inputMeters[0], &processor.inputMeters[1], &processor.outputMeters[0], &processor.outputMeters[1] };
    for (int i = 0; i < 4; ++i)
        meterDisplay[i] = juce::jmax (meters[i]->take(), meterDisplay[i] * fall);
    c.header.inMeter.setLevels (meterDisplay[0], meterDisplay[1]);
    c.header.outMeter.setLevels (meterDisplay[2], meterDisplay[3]);

    auto& apvts = processor.getState();
    auto value = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    c.pedalPage.noteDisplay.update (processor.detectedFrequency.load(), value (ParamIDs::tuner) > 0.5f, processor.tunerReference.load());
    c.pedalPage.pedal.setLeds (value (ParamIDs::bypass) < 0.5f, value (ParamIDs::hold) > 0.5f);
    c.effectsPage.refresh();

    const int mask = (value (ParamIDs::chorusOn) > 0.5f ? 1 : 0) | (value (ParamIDs::delayOn) > 0.5f ? 2 : 0)
                     | (value (ParamIDs::reverbOn) > 0.5f ? 4 : 0);
    if (mask != c.fxMask)
    {
        c.fxMask = mask;
        c.nav.repaint();
    }

    if (processor.getActiveSlot() != c.lastSlot)
    {
        c.lastSlot = processor.getActiveSlot();
        c.header.abButton.repaint();
    }

    if (processor.getPresets().displayChanged.load())
        refreshPresetName();
}

//==============================================================================
void SubterraEditor::showPresetBrowser()
{
    auto& presets = processor.getPresets();
    presets.rescanUserPresets();
    const int current = presets.getCurrentIndex();

    auto loader = [this] (int index, juce::String name)
    {
        return [this, index, name]
        {
            auto& pm = processor.getPresets();
            if (index < pm.getNumPresets() && pm.getPreset (index).name == name)
                pm.loadPreset (index);
            refreshPresetName();
        };
    };

    std::vector<MenuItem> items;
    items.push_back (MenuItem::header ("Factory"));
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        const auto name = presets.getPreset (i).name;
        items.push_back (MenuItem::item (name, loader (i, name), i == current));
    }

    items.push_back (MenuItem::header ("Your presets"));
    if (presets.getNumPresets() == presets.getNumFactoryPresets())
    {
        auto none = MenuItem::item ("No saved presets yet", {});
        none.enabled = false;
        items.push_back (none);
    }
    for (int i = presets.getNumFactoryPresets(); i < presets.getNumPresets(); ++i)
    {
        const auto name = presets.getPreset (i).name;
        auto item = MenuItem::item (name, loader (i, name), i == current);
        item.onDelete = [this, i] { confirmDeletePreset (i); };
        items.push_back (item);
    }

    items.push_back (MenuItem::separator());
    items.push_back (MenuItem::item ("Save current sound...", [this] { showSaveDialog(); }));
    items.push_back (MenuItem::item ("Show presets folder", [this]
    {
        const auto folder = processor.getPresets().getUserFolder();
        if (folder.createDirectory())
            folder.startAsProcess();
        else
            getToast().show ("Couldn't open the presets folder", true);
    }));

    getOverlay().showMenu ({ 200, 68, 340, 34 }, "Presets", std::move (items), 280);
}

void SubterraEditor::showSaveDialog()
{
    auto& presets = processor.getPresets();
    const auto current = presets.getCurrentName();

    auto& dialog = getOverlay().showDialog ("Save preset", "Name this sound. It will be listed under Your presets.", "Save");
    dialog.addTextField (current == "Init" ? juce::String() : current, [this] (const juce::String& text)
    {
        auto& pm = processor.getPresets();
        const auto name = text.trim();
        if (name.isEmpty())
            return juce::String();
        if (pm.userPresetExists (name))
            return juce::String ("Replaces your saved preset with this name.");
        for (int i = 0; i < pm.getNumFactoryPresets(); ++i)
            if (pm.getPreset (i).name.equalsIgnoreCase (name))
                return juce::String ("Saves your own copy. The factory one stays as it is.");
        return juce::String();
    });

    dialog.onConfirm = [this, &dialog]
    {
        const auto name = dialog.getText().trim();
        const bool ok = processor.getPresets().saveUserPreset (name);
        refreshPresetName();
        if (ok)
            getToast().show ("Saved " + quoted (juce::File::createLegalFileName (name)));
        else
            getToast().show ("Couldn't save. Check that the presets folder can be written to.", true);
        return true;
    };
}

void SubterraEditor::confirmDeletePreset (int index)
{
    auto& presets = processor.getPresets();
    if (index < presets.getNumFactoryPresets() || index >= presets.getNumPresets())
        return;

    const auto name = presets.getPreset (index).name;
    auto& dialog = getOverlay().showDialog ("Delete preset", quoted (name) + " will be moved to the " + trashName() + ".", "Delete", true);
    dialog.onConfirm = [this, index, name]
    {
        auto& pm = processor.getPresets();
        const bool ok = index < pm.getNumPresets() && pm.getPreset (index).name == name && pm.deleteUserPreset (index);
        refreshPresetName();
        getToast().show (ok ? "Deleted " + quoted (name) : juce::String ("Couldn't delete the preset"), ! ok);
        return true;
    };
}

void SubterraEditor::showSettingsMenu()
{
    auto safe = juce::Component::SafePointer<SubterraEditor> (this);
    const float reference = processor.tunerReference.load();
    const int percent = juce::roundToInt ((float) getWidth() / (float) designWidth * 100.0f);

    std::vector<MenuItem> items;

    auto tuner = MenuItem::item ("Tuner reference", {});
    tuner.detail = "A4 = " + juce::String (juce::roundToInt (reference)) + " Hz";
    tuner.submenu = [this]
    {
        std::vector<MenuItem> sub;
        const float now = processor.tunerReference.load();
        for (int hz : { 432, 435, 438, 440, 441, 442, 443, 444, 446 })
            sub.push_back (MenuItem::item ("A4 = " + juce::String (hz) + " Hz", [this, hz] { processor.tunerReference = (float) hz; },
                                           std::abs (now - (float) hz) < 0.01f));
        return sub;
    };
    items.push_back (tuner);

    auto mute = MenuItem::item ("Mute output while tuning", [this] { processor.tunerMutesOutput = ! processor.tunerMutesOutput.load(); },
                                processor.tunerMutesOutput.load());
    mute.keepOpen = true;
    items.push_back (mute);

    auto size = MenuItem::item ("Window size", {});
    size.detail = juce::String (percent) + "%";
    size.submenu = [safe, percent]
    {
        std::vector<MenuItem> sub;
        for (int p : { 50, 75, 100, 125, 150, 200 })
            sub.push_back (MenuItem::item (juce::String (p) + "%", [safe, p]
            {
                if (safe != nullptr)
                    safe->setSize (designWidth * p / 100, designHeight * p / 100);
            }, std::abs (percent - p) <= 1));
        return sub;
    };
    items.push_back (size);

    items.push_back (MenuItem::separator());
    const bool onA = processor.getActiveSlot() == 0;
    items.push_back (MenuItem::item (onA ? "Copy A to B" : "Copy B to A", [this, onA]
    {
        processor.copyActiveSlotToOther();
        getToast().show (onA ? "Copied A to B" : "Copied B to A");
    }));
    items.push_back (MenuItem::item ("Reset to Init", [this]
    {
        auto& presets = processor.getPresets();
        for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
            if (presets.getPreset (i).name == "Init")
                presets.loadPreset (i);
        refreshPresetName();
        getToast().show ("Reset to Init");
    }));

    items.push_back (MenuItem::separator());
    auto about = MenuItem::item ("Subterra " + juce::String (JucePlugin_VersionString), {});
    about.detail = "Subterra Audio";
    about.enabled = false;
    items.push_back (about);

    getOverlay().showMenu ({ 291, 24, 32, 32 }, "Settings", std::move (items), 270);
}

void SubterraEditor::showAbMenu()
{
    const bool onA = processor.getActiveSlot() == 0;
    std::vector<MenuItem> items;
    items.push_back (MenuItem::item (onA ? "Switch to B" : "Switch to A", [this]
    {
        processor.switchSlot();
        refreshPresetName();
    }));
    items.push_back (MenuItem::item (onA ? "Copy A to B" : "Copy B to A", [this, onA]
    {
        processor.copyActiveSlotToOther();
        getToast().show (onA ? "Copied A to B" : "Copied B to A");
    }));
    getOverlay().showMenu ({ 417, 24, 32, 32 }, "Compare", std::move (items), 200);
}
}
