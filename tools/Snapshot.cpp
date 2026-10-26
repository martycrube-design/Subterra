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
    const int start = presets.getCurrentIndex();
    findButton ("Next preset")->onClick();
    if (presets.getCurrentIndex() != (start + 1) % presets.getNumPresets()) { ++failures; std::printf ("  FAIL: next preset\n"); }
    findButton ("Previous preset")->onClick();
    if (presets.getCurrentIndex() != start) { ++failures; std::printf ("  FAIL: previous preset\n"); }
    const int slot = processor.getActiveSlot();
    findButton ("Compare A and B")->onClick();
    if (processor.getActiveSlot() == slot) { ++failures; std::printf ("  FAIL: A/B switch\n"); }
    for (auto* title : { "Settings", "Save preset", "Browse presets", "Preset name" })
        if (findButton (title) == nullptr || ! findButton (title)->onClick) { ++failures; std::printf ("  FAIL: %s has no action\n", title); }

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
