#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/EffectsPage.h"
#include "ui/Header.h"
#include "ui/Overlay.h"
#include "ui/PedalPage.h"

namespace subterra
{
// The two pages below the header. Switching slides one out and the other in.
class PageStack final : public juce::Component
{
public:
    PageStack (juce::Component& first, juce::Component& second);
    void show (int index, bool animate);
    int getIndex() const { return index; }
    bool isAnimating() const { return slide.isRunning(); }
    void resized() override { layout(); }

private:
    void layout();
    juce::Component* pages[2];
    int index = 0;
    Tween slide;
    FrameDriver frames { *this, [this] { if (! slide.update()) return false; layout(); return false; } };
};

class SubterraEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SubterraEditor (SubterraProcessor&);
    ~SubterraEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int designWidth = 740, designHeight = 900;

    // Page and menu actions, also driven by the UI checks in tools/Snapshot.cpp.
    void showPage (int page, bool animate = true);
    int getPage() const;
    void showPresetBrowser();
    void showSettingsMenu();
    void showSaveDialog();
    void showAbMenu();
    void confirmDeletePreset (int index);
    Overlay& getOverlay();
    Toast& getToast();

private:
    class Content;

    void timerCallback() override;
    void refreshPresetName();

    SubterraProcessor& processor;
    SubterraLookAndFeel lnf;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltips { this, 700 };
    float meterDisplay[4] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubterraEditor)
};
}
