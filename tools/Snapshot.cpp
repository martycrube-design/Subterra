// Renders the plugin editor to a PNG, for checking the UI against the design.
// Usage: SubterraSnapshot out.png [scale] [param=value ...]
//        SubterraSnapshot --check   (operates every on-screen control and checks the engine responds)

#include "PluginEditor.h"

namespace
{
std::vector<float> snapshotParams (subterra::SubterraProcessor& p)
{
    std::vector<float> values;
    for (auto* param : p.getParameters())
        values.push_back (param->getValue());
    return values;
}

juce::StringArray changedParams (subterra::SubterraProcessor& p, const std::vector<float>& before)
{
    juce::StringArray names;
    auto params = p.getParameters();
    for (int i = 0; i < params.size(); ++i)
        if (std::abs (params[i]->getValue() - before[(size_t) i]) > 1.0e-4f)
            names.add (params[i]->getName (64));
    return names;
}

void collect (juce::Component& c, juce::Array<juce::Component*>& out)
{
    for (auto* child : c.getChildren())
    {
        out.add (child);
        collect (*child, out);
    }
}

int checkControls()
{
    subterra::SubterraProcessor processor;
    processor.prepareToPlay (48000.0, 256);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    juce::Array<juce::Component*> all;
    collect (*editor, all);

    int failures = 0, sliders = 0, buttons = 0;
    juce::StringArray covered;

    for (auto* c : all)
    {
        if (auto* slider = dynamic_cast<juce::Slider*> (c))
        {
            ++sliders;
            const auto before = snapshotParams (processor);
            const double target = slider->getValue() > (slider->getMinimum() + slider->getMaximum()) * 0.5 ? slider->getMinimum() : slider->getMaximum();
            slider->setValue (target, juce::sendNotificationSync);
            const auto changed = changedParams (processor, before);
            std::printf ("  knob %-10s -> %s\n", slider->getTitle().toRawUTF8(), changed.joinIntoString (", ").toRawUTF8());
            if (changed.size() != 1) { ++failures; std::printf ("    FAIL: expected exactly one parameter to change\n"); }
            covered.addArray (changed);

            // Double-click value resets to the parameter default.
            slider->setValue (slider->getDoubleClickReturnValue(), juce::sendNotificationSync);
        }
        else if (auto* button = dynamic_cast<juce::Button*> (c); button != nullptr && button->getClickingTogglesState())
        {
            ++buttons;
            const auto before = snapshotParams (processor);
            button->setToggleState (! button->getToggleState(), juce::sendNotificationSync);
            const auto changed = changedParams (processor, before);
            std::printf ("  switch %-28s -> %s\n", button->getTitle().toRawUTF8(), changed.joinIntoString (", ").toRawUTF8());
            if (changed.size() != 1) { ++failures; std::printf ("    FAIL: expected exactly one parameter to change\n"); }
            covered.addArray (changed);
            button->setToggleState (! button->getToggleState(), juce::sendNotificationSync);
        }
    }

    auto fail = [&] (const char* what) { ++failures; std::printf ("  FAIL: %s\n", what); };

    // Segmented controls: effects routing (a parameter) and the page switch.
    auto& ed = dynamic_cast<subterra::SubterraEditor&> (*editor);
    for (auto* c : all)
    {
        if (auto* seg = dynamic_cast<subterra::SegmentedControl*> (c))
        {
            const auto before = snapshotParams (processor);
            const int pageBefore = ed.getPage();
            seg->keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
            const auto changed = changedParams (processor, before);
            std::printf ("  selector %-26s -> %s\n", seg->getTitle().toRawUTF8(),
                         changed.isEmpty() ? (ed.getPage() != pageBefore ? "page changed" : "nothing") : changed.joinIntoString (", ").toRawUTF8());
            if (seg->getTitle() == "Page" && ed.getPage() != 1) fail ("page switch");
            if (seg->getTitle() != "Page" && changed.size() != 1) fail ("selector should change one parameter");
            covered.addArray (changed);
            seg->keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
        }
    }

    // Every parameter has a control.
    for (auto* param : processor.getParameters())
        if (! covered.contains (param->getName (64)))
        {
            ++failures;
            std::printf ("  FAIL: no control for %s\n", param->getName (64).toRawUTF8());
        }

    // Preset arrows and A/B.
    auto findButton = [&] (const juce::String& title) -> juce::Button*
    {
        for (auto* c : all)
            if (auto* b = dynamic_cast<juce::Button*> (c); b != nullptr && b->getTitle() == title)
                return b;
        return nullptr;
    };

    auto& presets = processor.getPresets();
    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("subterra-ui-check");
    temp.deleteRecursively();
    presets.setUserFolder (temp);

    const int start = presets.getCurrentIndex();
    findButton ("Next preset")->onClick();
    if (presets.getCurrentIndex() != (start + 1) % presets.getNumPresets()) fail ("next preset");
    findButton ("Previous preset")->onClick();
    if (presets.getCurrentIndex() != start) fail ("previous preset");
    const int slot = processor.getActiveSlot();
    findButton ("Compare A and B")->onClick();
    if (processor.getActiveSlot() == slot) fail ("A/B switch");
    findButton ("Compare A and B")->onClick();

    auto& overlay = ed.getOverlay();
    auto menu = [&]() { return dynamic_cast<subterra::MenuCard*> (overlay.getCard()); };
    auto dialog = [&]() { return dynamic_cast<subterra::DialogCard*> (overlay.getCard()); };

    // Buttons open their menus.
    for (auto [title, kind] : { std::pair { "Settings", 0 }, { "Browse presets", 0 }, { "Preset name", 0 }, { "Save preset", 1 } })
    {
        findButton (title)->onClick();
        const bool ok = kind == 0 ? menu() != nullptr : dialog() != nullptr;
        std::printf ("  button %-28s -> %s\n", title, ok ? (kind == 0 ? "menu" : "dialog") : "nothing");
        if (! ok) fail (title);
        overlay.dismiss();
        if (overlay.isActive()) fail ("overlay closes");
    }

    // Preset browser loads a preset.
    ed.showPresetBrowser();
    if (menu() == nullptr || ! menu()->activate ("Magma") || presets.getCurrentName() != "Magma") fail ("preset browser loads Magma");
    if (overlay.isActive()) fail ("menu closes after choosing");

    // Settings: tuner reference submenu, mute toggle, window size, copy, reset.
    ed.showSettingsMenu();
    menu()->openSubmenu (0);
    if (! menu()->activate ("A4 = 442 Hz") || std::abs (processor.tunerReference.load() - 442.0f) > 0.01f) fail ("tuner reference");
    ed.showSettingsMenu();
    const bool muteBefore = processor.tunerMutesOutput.load();
    menu()->activate ("Mute output while tuning");
    if (processor.tunerMutesOutput.load() == muteBefore || ! overlay.isActive()) fail ("mute while tuning toggles and keeps the menu open");
    menu()->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
    if (overlay.isActive()) fail ("Escape closes the menu");
    ed.showSettingsMenu();
    menu()->openSubmenu (2);
    menu()->activate ("125%");
    if (editor->getWidth() != 925) fail ("window size 125%");
    ed.showSettingsMenu();
    menu()->activate ("Reset to Init");
    if (presets.getCurrentName() != "Init") fail ("reset to Init");

    // A/B menu.
    ed.showAbMenu();
    menu()->activate ("Switch to B");
    if (processor.getActiveSlot() != 1) fail ("A/B menu switches");
    ed.showAbMenu();
    if (! menu()->activate ("Copy B to A")) fail ("A/B menu copies");

    // Save dialog: overwrite notice, saving, and deleting with confirmation.
    ed.showSaveDialog();
    auto* field = [&]() -> juce::TextEditor*
    {
        juce::Array<juce::Component*> cards;
        collect (*overlay.getCard(), cards);
        for (auto* c : cards)
            if (auto* te = dynamic_cast<juce::TextEditor*> (c))
                return te;
        return nullptr;
    }();
    if (field == nullptr) fail ("save dialog has a name field");
    field->setText ("UI Check", true);
    dialog()->confirm();
    if (! temp.getChildFile ("UI Check.subterrapreset").existsAsFile() || presets.getCurrentName() != "UI Check") fail ("save preset");
    if (! ed.getToast().isVisible()) fail ("toast after saving");

    int saved = -1;
    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (presets.getPreset (i).name == "UI Check")
            saved = i;
    ed.confirmDeletePreset (saved);
    if (dialog() == nullptr) fail ("delete asks first");
    dialog()->cancel();
    if (! temp.getChildFile ("UI Check.subterrapreset").existsAsFile()) fail ("cancel keeps the preset");
    ed.confirmDeletePreset (saved);
    dialog()->confirm();
    if (temp.getChildFile ("UI Check.subterrapreset").existsAsFile()) fail ("delete preset");
    temp.deleteRecursively();

    std::printf ("%d knobs, %d switches checked, %d failures\n", sliders, buttons, failures);
    editor.reset();
    return failures;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--check")
        return checkControls();

    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "subterra.png"));
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;

    subterra::SubterraProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    for (int i = 3; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (auto* param = processor.getState().getParameter (arg.upToFirstOccurrenceOf ("=", false, false)))
            param->setValueNotifyingHost (param->convertTo0to1 (arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue()));
        else if (arg.startsWith ("note="))
            processor.detectedFrequency = arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    editor->setSize (juce::roundToInt (740 * scale), juce::roundToInt (900 * scale));
    auto& subterraEditor = dynamic_cast<subterra::SubterraEditor&> (*editor);
    for (int i = 3; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        const auto value = arg.fromFirstOccurrenceOf ("=", false, false);
        if (arg.startsWith ("page="))
            subterraEditor.showPage (value.getIntValue(), false);
        else if (arg == "menu=presets")  subterraEditor.showPresetBrowser();
        else if (arg == "menu=settings") subterraEditor.showSettingsMenu();
        else if (arg == "menu=save")     subterraEditor.showSaveDialog();
        else if (arg == "menu=ab")       subterraEditor.showAbMenu();
        else if (arg.startsWith ("submenu="))
        {
            if (auto* menu = dynamic_cast<subterra::MenuCard*> (subterraEditor.getOverlay().getCard()))
                for (int item = 0; item < (int) menu->getCurrentItems().size(); ++item)
                    if (menu->getCurrentItems()[(size_t) item].text == value)
                        menu->openSubmenu (item);
        }
        else if (arg.startsWith ("toast="))
            subterraEditor.getToast().show (value);
    }

    // Let the editor's timer pick up meters and the note readout.
    for (int i = 0; i < 3; ++i)
    {
        processor.inputMeters[0].push (0.5f); processor.inputMeters[1].push (0.4f);
        processor.outputMeters[0].push (0.3f); processor.outputMeters[1].push (0.35f);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale >= 1.0f ? 1.0f : 1.0f);
    juce::FileOutputStream stream (out);
    stream.setPosition (0);
    stream.truncate();
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    editor.reset();
    return 0;
}
