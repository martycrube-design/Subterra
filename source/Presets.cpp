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
const FactoryPreset factoryPresets[] = {
    { "Bedrock",    { { ParamIDs::wave, 0.333f }, { ParamIDs::octave, 2 }, { ParamIDs::filter, 600.0f }, { ParamIDs::reso, 0.3f },
                      { ParamIDs::env, 0.5f }, { ParamIDs::drive, 0.2f }, { ParamIDs::mix, 0.7f }, { ParamIDs::glide, 20.0f } } },
    { "Sub Floor",  { { ParamIDs::wave, 0.0f }, { ParamIDs::octave, 1 }, { ParamIDs::filter, 320.0f }, { ParamIDs::reso, 0.1f },
                      { ParamIDs::env, 0.1f }, { ParamIDs::drive, 0.1f }, { ParamIDs::mix, 0.5f }, { ParamIDs::glide, 10.0f } } },
    { "Magma",      { { ParamIDs::wave, 0.667f }, { ParamIDs::octave, 2 }, { ParamIDs::filter, 250.0f }, { ParamIDs::reso, 0.7f },
                      { ParamIDs::env, 0.8f }, { ParamIDs::drive, 0.6f }, { ParamIDs::mix, 0.85f }, { ParamIDs::glide, 15.0f } } },
    { "Fault Line", { { ParamIDs::wave, 0.5f }, { ParamIDs::octave, 1 }, { ParamIDs::filter, 180.0f }, { ParamIDs::reso, 0.85f },
                      { ParamIDs::env, 1.0f }, { ParamIDs::drive, 0.35f }, { ParamIDs::mix, 1.0f }, { ParamIDs::glide, 5.0f } } },
    { "Deep Core",  { { ParamIDs::wave, 1.0f }, { ParamIDs::octave, 0 }, { ParamIDs::filter, 900.0f }, { ParamIDs::reso, 0.2f },
                      { ParamIDs::env, -0.4f }, { ParamIDs::drive, 0.0f }, { ParamIDs::mix, 0.6f }, { ParamIDs::glide, 80.0f } } },
    { "Tremor",     { { ParamIDs::wave, 0.333f }, { ParamIDs::octave, 3 }, { ParamIDs::filter, 1200.0f }, { ParamIDs::reso, 0.5f },
                      { ParamIDs::env, -0.7f }, { ParamIDs::drive, 0.45f }, { ParamIDs::mix, 0.6f }, { ParamIDs::glide, 1.0f } } },
    { "Init",       {} },
};

juce::RangedAudioParameter* findParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    return apvts.getParameter (id);
}
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
    tree.setProperty (nameProp, currentName, nullptr);

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

    for (const auto& param : sound)
    {
        if (! param.hasType (paramType))
            continue;

        const auto id = param[idProp].toString();
        if (isPerformanceParameter (id))
            continue;

        if (auto* ranged = findParam (apvts, id))
        {
            const float value = (float) param[valueProp];
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (value));
            ranged->endChangeGesture();
        }
    }

    --applying;
}

void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentIndex = index;
    currentName = presets[(size_t) index].name;
    applySound (presets[(size_t) index].state);
    modified = false;
    displayChanged = true;
}

void PresetManager::loadNext()
{
    loadPreset ((currentIndex + 1) % getNumPresets());
}

void PresetManager::loadPrevious()
{
    loadPreset ((currentIndex + getNumPresets() - 1) % getNumPresets());
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

    currentName = state[nameProp].toString();
    currentIndex = 0;
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == currentName)
            currentIndex = i;

    modified = (bool) state[modifiedProp];
    displayChanged = true;
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (applying.load() == 0 && ! modified.exchange (true))
        displayChanged = true;
}
}
