#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace subterra
{
struct Preset
{
    juce::String name;
    juce::ValueTree state;   // <Preset> with one <Param id= value=> child per sound parameter
    bool isFactory = true;
    juce::File file;         // user presets only
};

// Factory and user presets, plus the "modified since load" flag shown in the preset bar.
class PresetManager : private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);
    ~PresetManager() override;

    static juce::File getDefaultUserFolder();
    void setUserFolder (const juce::File& folder);
    juce::File getUserFolder() const { return userFolder; }
    void rescanUserPresets();

    int getNumPresets() const { const juce::ScopedLock sl (lock); return (int) presets.size(); }
    int getNumFactoryPresets() const { return numFactory; }
    Preset getPreset (int index) const;

    void loadPreset (int index);
    void loadNext();
    void loadPrevious();
    bool saveUserPreset (const juce::String& name);

    int getCurrentIndex() const { const juce::ScopedLock sl (lock); return currentIndex; }
    juce::String getCurrentName() const { const juce::ScopedLock sl (lock); return currentName; }
    bool deleteUserPreset (int index);
    bool userPresetExists (const juce::String& name) const;
    bool isModified() const { return modified.load(); }

    // Captures or applies the sound parameters (not Bypass, Hold or Tuner).
    juce::ValueTree captureSound() const;
    void applySound (const juce::ValueTree& sound);

    // Session state: which preset is loaded and whether it was edited.
    juce::ValueTree getState() const;
    void setState (const juce::ValueTree& state);

    // Factory preset values that name no sound parameter (a typo check for the tests).
    juce::StringArray unknownFactoryIds;

    // Set when the UI should refresh the preset name.
    std::atomic<bool> displayChanged { true };

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void addFactoryPresets();

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> presets;
    int numFactory = 0;
    int currentIndex = 0;
    juce::String currentName;
    std::atomic<bool> modified { false };
    std::atomic<int> applying { 0 };
    juce::File userFolder;
    juce::CriticalSection lock;   // guards the current preset; the host may save state from any thread
};
}
