#include "Presets.h"
#include "Parameters.h"

namespace subterra
{
namespace
{
const juce::Identifier presetType ("Preset");
const juce::Identifier paramType ("Param");
const juce::Identifier idProp ("id");
const juce::Identifier valueProp ("value");
const juce::Identifier nameProp ("name");
const juce::Identifier presetStateType ("PresetState");
const juce::Identifier indexProp ("index");
const juce::Identifier modifiedProp ("modified");
const juce::String fileExtension (".subterrapreset");

struct FactoryValue { const char* id; float value; };

struct FactoryPreset
{
    const char* name;
    std::initializer_list<FactoryValue> values;   // real units; anything missing uses its default
};

// Wave: 0 sine, 0.333 saw, 0.667 square, 1 triangle. Octave is the knob index (2 = unison).
// Delay division index: 0 1/16, 1 1/8T, 2 1/8, 3 1/8D, 4 1/4T, 5 1/4, 6 1/4D, 7 1/2. Routing: 0 synth, 1 synth + bass.
using namespace ParamIDs;
const FactoryPreset factoryPresets[] = {
    { "Bedrock",      { { wave, 0.333f }, { octave, 2 }, { filter, 650.0f }, { reso, 0.35f }, { env, 0.55f }, { drive, 0.25f },
                        { mix, 0.7f }, { glide, 20.0f } } },
    { "Sub Floor",    { { wave, 0.0f }, { octave, 1 }, { filter, 320.0f }, { reso, 0.1f }, { env, 0.1f }, { drive, 0.1f },
                        { mix, 0.5f }, { glide, 10.0f } } },
    { "Magma",        { { wave, 0.667f }, { octave, 2 }, { filter, 250.0f }, { reso, 0.7f }, { env, 0.8f }, { drive, 0.6f },
                        { mix, 0.85f }, { glide, 15.0f },
                        { reverbOn, 1 }, { reverbSize, 0.35f }, { reverbTone, 0.4f }, { reverbMix, 0.15f } } },
    { "Fault Line",   { { wave, 0.5f }, { octave, 1 }, { filter, 180.0f }, { reso, 0.85f }, { env, 1.0f }, { drive, 0.35f },
                        { mix, 1.0f }, { glide, 5.0f },
                        { delayOn, 1 }, { delaySync, 1 }, { delayDivision, 3 }, { delayFeedback, 0.3f }, { delayTone, 3000.0f },
                        { delayMix, 0.2f } } },
    { "Deep Core",    { { wave, 1.0f }, { octave, 0 }, { filter, 900.0f }, { reso, 0.2f }, { env, -0.4f }, { drive, 0.0f },
                        { mix, 0.6f }, { glide, 80.0f },
                        { chorusOn, 1 }, { chorusRate, 0.4f }, { chorusDepth, 0.6f }, { chorusMix, 0.45f },
                        { reverbOn, 1 }, { reverbSize, 0.6f }, { reverbTone, 0.45f }, { reverbMix, 0.25f } } },
    { "Tremor",       { { wave, 0.333f }, { octave, 3 }, { filter, 1200.0f }, { reso, 0.5f }, { env, -0.7f }, { drive, 0.45f },
                        { mix, 0.6f }, { glide, 1.0f },
                        { chorusOn, 1 }, { chorusRate, 1.2f }, { chorusDepth, 0.4f }, { chorusMix, 0.4f } } },
    { "Cavern",       { { wave, 0.0f }, { octave, 1 }, { filter, 500.0f }, { reso, 0.2f }, { env, 0.2f }, { drive, 0.1f },
                        { mix, 0.6f }, { glide, 40.0f }, { fxRouting, 1 },
                        { delayOn, 1 }, { delaySync, 1 }, { delayDivision, 5 }, { delayFeedback, 0.35f }, { delayTone, 2500.0f },
                        { delayMix, 0.2f },
                        { reverbOn, 1 }, { reverbSize, 0.85f }, { reverbTone, 0.35f }, { reverbMix, 0.4f } } },
    { "Bloom",        { { wave, 0.9f }, { octave, 2 }, { filter, 2000.0f }, { reso, 0.15f }, { env, 0.3f }, { drive, 0.0f },
                        { mix, 0.55f }, { glide, 60.0f },
                        { chorusOn, 1 }, { chorusRate, 0.35f }, { chorusDepth, 0.7f }, { chorusMix, 0.55f },
                        { reverbOn, 1 }, { reverbSize, 0.7f }, { reverbTone, 0.55f }, { reverbMix, 0.3f } } },
    { "Echo Chamber", { { wave, 0.333f }, { octave, 2 }, { filter, 900.0f }, { reso, 0.4f }, { env, 0.6f }, { drive, 0.3f },
                        { mix, 0.8f }, { glide, 20.0f },
                        { delayOn, 1 }, { delaySync, 1 }, { delayDivision, 3 }, { delayFeedback, 0.45f }, { delayTone, 3500.0f },
                        { delayMix, 0.35f },
                        { reverbOn, 1 }, { reverbSize, 0.5f }, { reverbTone, 0.5f }, { reverbMix, 0.2f } } },
    { "Pulse Grid",   { { wave, 0.667f }, { octave, 3 }, { filter, 1500.0f }, { reso, 0.55f }, { env, 0.7f }, { drive, 0.4f },
                        { mix, 0.65f }, { glide, 1.0f },
                        { delayOn, 1 }, { delaySync, 1 }, { delayDivision, 0 }, { delayFeedback, 0.25f }, { delayTone, 6000.0f },
                        { delayMix, 0.25f } } },
    { "Abyss",        { { wave, 0.0f }, { octave, 0 }, { filter, 250.0f }, { reso, 0.1f }, { env, 0.0f }, { drive, 0.25f },
                        { mix, 0.5f }, { glide, 30.0f },
                        { reverbOn, 1 }, { reverbSize, 0.9f }, { reverbTone, 0.25f }, { reverbMix, 0.3f } } },
    { "Wide Saw",     { { wave, 0.333f }, { octave, 2 }, { filter, 1800.0f }, { reso, 0.3f }, { env, 0.4f }, { drive, 0.25f },
                        { mix, 0.7f }, { glide, 20.0f }, { fxRouting, 1 },
                        { chorusOn, 1 }, { chorusRate, 0.8f }, { chorusDepth, 0.5f }, { chorusMix, 0.5f } } },
    { "Tape Drift",   { { wave, 0.45f }, { octave, 2 }, { filter, 1100.0f }, { reso, 0.45f }, { env, 0.5f }, { drive, 0.5f },
                        { mix, 0.75f }, { glide, 25.0f },
                        { chorusOn, 1 }, { chorusRate, 0.2f }, { chorusDepth, 0.3f }, { chorusMix, 0.25f },
                        { delayOn, 1 }, { delaySync, 0 }, { delayTime, 420.0f }, { delayFeedback, 0.55f }, { delayTone, 1800.0f },
                        { delayMix, 0.3f } } },
    { "Init",         {} },
};

}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isPerformanceParameter (ranged->getParameterID()))
                apvts.addParameterListener (ranged->getParameterID(), this);

    addFactoryPresets();
    userFolder = getDefaultUserFolder();
    rescanUserPresets();
    currentIndex = 0;
    currentName = presets.front().name;
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (ranged->getParameterID(), this);
}

juce::File PresetManager::getDefaultUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Subterra").getChildFile ("Presets");
}

void PresetManager::setUserFolder (const juce::File& folder)
{
    userFolder = folder;
    rescanUserPresets();
}

void PresetManager::addFactoryPresets()
{
    for (const auto& fp : factoryPresets)
    {
        juce::ValueTree tree (presetType);
        tree.setProperty (nameProp, fp.name, nullptr);

        for (const auto& v : fp.values)
            if (apvts.getParameter (v.id) == nullptr || isPerformanceParameter (v.id))
                unknownFactoryIds.addIfNotAlreadyThere (juce::String (fp.name) + ": " + v.id);

        for (auto* p : apvts.processor.getParameters())
        {
            auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
            if (ranged == nullptr || isPerformanceParameter (ranged->getParameterID()))
                continue;

            float value = ranged->convertFrom0to1 (ranged->getDefaultValue());
            for (const auto& v : fp.values)
                if (ranged->getParameterID() == v.id)
                    value = v.value;

            juce::ValueTree param (paramType);
            param.setProperty (idProp, ranged->getParameterID(), nullptr);
            param.setProperty (valueProp, value, nullptr);
            tree.appendChild (param, nullptr);
        }

        presets.push_back ({ fp.name, tree, true, {} });
    }

    numFactory = (int) presets.size();
}

void PresetManager::rescanUserPresets()
{
    const juce::ScopedLock sl (lock);
    const auto current = currentIndex < (int) presets.size() ? presets[(size_t) currentIndex].name : juce::String();
    presets.resize ((size_t) numFactory);

    if (! userFolder.isDirectory())
        return;

    auto files = userFolder.findChildFiles (juce::File::findFiles, false, "*" + fileExtension);
    files.sort();

    for (const auto& file : files)
    {
        if (auto xml = juce::parseXML (file))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            if (tree.hasType (presetType))
                presets.push_back ({ file.getFileNameWithoutExtension(), tree, false, file });
        }
    }

    // Keep pointing at the same preset if it still exists.
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == current)
            currentIndex = i;
    currentIndex = juce::jlimit (0, (int) presets.size() - 1, currentIndex);
}

juce::ValueTree PresetManager::captureSound() const
{
    juce::ValueTree tree (presetType);
    tree.setProperty (nameProp, getCurrentName(), nullptr);

    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || isPerformanceParameter (ranged->getParameterID()))
            continue;

        juce::ValueTree param (paramType);
        param.setProperty (idProp, ranged->getParameterID(), nullptr);
        param.setProperty (valueProp, ranged->convertFrom0to1 (ranged->getValue()), nullptr);
        tree.appendChild (param, nullptr);
    }

    return tree;
}

void PresetManager::applySound (const juce::ValueTree& sound)
{
    ++applying;

    // Every sound parameter is set. Ones the preset doesn't mention (e.g. effects in a preset
    // saved by an older version) go back to their defaults rather than keeping the last sound's value.
    for (auto* p : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr || isPerformanceParameter (ranged->getParameterID()))
            continue;

        float normalised = ranged->getDefaultValue();
        const auto stored = sound.getChildWithProperty (idProp, ranged->getParameterID());
        if (stored.isValid() && stored.hasProperty (valueProp))
            normalised = ranged->convertTo0to1 ((float) stored[valueProp]);

        if (std::abs (ranged->getValue() - normalised) > 1.0e-7f)
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (normalised);
            ranged->endChangeGesture();
        }
    }

    --applying;
}

Preset PresetManager::getPreset (int index) const
{
    const juce::ScopedLock sl (lock);
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index] : Preset {};
}

void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    juce::ValueTree sound;
    {
        const juce::ScopedLock sl (lock);
        currentIndex = index;
        currentName = presets[(size_t) index].name;
        sound = presets[(size_t) index].state;
    }
    applySound (sound);
    modified = false;
    displayChanged = true;
}

void PresetManager::loadNext()
{
    loadPreset ((getCurrentIndex() + 1) % getNumPresets());
}

void PresetManager::loadPrevious()
{
    loadPreset ((getCurrentIndex() + getNumPresets() - 1) % getNumPresets());
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    if (! userFolder.createDirectory())
        return false;

    auto tree = captureSound();
    tree.setProperty (nameProp, name, nullptr);

    const auto file = userFolder.getChildFile (name + fileExtension);
    auto xml = tree.createXml();
    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    const juce::ScopedLock sl (lock);
    currentName = name;
    currentIndex = numFactory;
    rescanUserPresets();
    for (int i = numFactory; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == name)
            currentIndex = i;

    modified = false;
    displayChanged = true;
    return true;
}

juce::ValueTree PresetManager::getState() const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree state (presetStateType);
    state.setProperty (nameProp, currentName, nullptr);
    state.setProperty (indexProp, currentIndex, nullptr);
    state.setProperty (modifiedProp, modified.load(), nullptr);
    return state;
}

void PresetManager::setState (const juce::ValueTree& state)
{
    if (! state.hasType (presetStateType))
        return;

    const juce::ScopedLock sl (lock);
    currentName = state[nameProp].toString();
    currentIndex = 0;
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == currentName)
            currentIndex = i;

    modified = (bool) state[modifiedProp];
    displayChanged = true;
}

bool PresetManager::userPresetExists (const juce::String& rawName) const
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    return name.isNotEmpty() && userFolder.getChildFile (name + fileExtension).existsAsFile();
}

bool PresetManager::deleteUserPreset (int index)
{
    juce::File file;
    {
        const juce::ScopedLock sl (lock);
        if (index < numFactory || index >= (int) presets.size())
            return false;
        file = presets[(size_t) index].file;
    }

    // Move to the Trash / Recycle Bin so a mistaken delete can be undone.
    if (! file.moveToTrash() && ! file.deleteFile())
        return false;

    const bool wasCurrent = index == getCurrentIndex();
    rescanUserPresets();
    if (wasCurrent)
    {
        // The sound stays as it is; it just no longer has a saved preset behind it.
        const juce::ScopedLock sl (lock);
        currentIndex = 0;
        modified = true;
        displayChanged = true;
    }
    return true;
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (applying.load() == 0 && ! modified.exchange (true))
        displayChanged = true;
}
}
