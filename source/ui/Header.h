#pragma once

#include "Controls.h"

namespace subterra
{
// Top of the window, shared by both pages: meters, Input/Output, the preset bar and the In/Out filters.
class Header final : public juce::Component
{
public:
    explicit Header (SubterraLookAndFeel&);
    void paint (juce::Graphics&) override;

    StereoMeter inMeter, outMeter;
    Knob input { KnobStyle::frameLarge, "Input", "gain into the pedal" };
    Knob output { KnobStyle::frameLarge, "Output", "final output level" };
    Knob inHp { KnobStyle::frameSmall, "In HP", "input high-pass cutoff" };
    Knob inLp { KnobStyle::frameSmall, "In LP", "input low-pass cutoff" };
    Knob outHp { KnobStyle::frameSmall, "Out HP", "output high-pass cutoff" };
    Knob outLp { KnobStyle::frameSmall, "Out LP", "output low-pass cutoff" };
    RoundButton settingsButton, saveButton, browseButton, abButton;
    FlatTextButton prevButton, nextButton, presetName;
    PillToggle inFilters, outFilters;

private:
    SubterraLookAndFeel& lnf;
};
}
