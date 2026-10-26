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

    int getNumPresets() const { return (int) presets.size(); }
    int getNumFactoryPresets() const { return numFactory; }
    const Preset& getPreset (int index) const { return presets[(size_t) index]; }

    void loadPreset (int index);
    void loadNext();
    void loadPrevious();
    bool saveUserPreset (const juce::String& name);

    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const { return currentName; }
    bool isModified() const { return modified.load(); }

    // Captures or applies the sound parameters (not Bypass, Hold or Tuner).
    juce::ValueTree captureSound() const;
    void applySound (const juce::ValueTree& sound);

    // Session state: which preset is loaded and whether it was edited.
    juce::ValueTree getState() const;
    void setState (const juce::ValueTree& state);

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
};
}
