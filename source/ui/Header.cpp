#include "Header.h"

namespace subterra
{
namespace
{
juce::Path settingsIcon()
{
    auto p = svgPath ("M8 1.5v2M8 12.5v2M1.5 8h2M12.5 8h2M3.4 3.4l1.4 1.4M11.2 11.2l1.4 1.4M3.4 12.6l1.4-1.4M11.2 4.8l1.4-1.4");
    p.addEllipse (5.5f, 5.5f, 5.0f, 5.0f);
    return p;
}
}

Header::Header (SubterraLookAndFeel& l)
    : settingsButton ("Settings", settingsIcon()),
      saveButton ("Save preset", svgPath ("M2.5 2.5h8l3 3v8h-11z M5 2.5v3h5v-3M5 13.5v-4h6v4")),
      browseButton ("Browse presets", svgPath ("M2 4h4l1.5 1.5H14v7.5H2z")),
      abButton ("Compare A and B", {}),
      prevButton (l, "Previous preset", 16.0f, 500),
      nextButton (l, "Next preset", 16.0f, 500),
      presetName (l, "Preset name", 15.0f, 500),
      inFilters (l, "Input filters on or off"),
      outFilters (l, "Output filters on or off"),
      lnf (l)
{
    for (auto* c : std::initializer_list<juce::Component*> { &inMeter, &outMeter, &input, &output, &settingsButton, &saveButton,
                                                              &browseButton, &abButton, &prevButton, &nextButton, &presetName,
                                                              &inFilters, &inHp, &inLp, &outFilters, &outHp, &outLp })
        addAndMakeVisible (c);

    inFilters.setTooltip ("In filters: high- and low-pass on the bass before the pedal");
    outFilters.setTooltip ("Out filters: high- and low-pass on the final output");
    prevButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x80\xb9"));
    nextButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x80\xba"));
    prevButton.setTooltip ("Previous preset");
    nextButton.setTooltip ("Next preset");
    presetName.setTooltip ("Browse presets");
    abButton.setTooltip ("A/B: click to switch between two settings, right-click to copy one to the other");
    inMeter.setTitle ("Input level");
    outMeter.setTitle ("Output level");

    inMeter.setBounds (36, 34, 20, 132);
    outMeter.setBounds (691, 34, 20, 132);
    input.setBounds (88, 51, 80, 80);
    output.setBounds (572, 51, 80, 80);
    settingsButton.setBounds (291, 24, 32, 32);
    saveButton.setBounds (333, 24, 32, 32);
    browseButton.setBounds (375, 24, 32, 32);
    abButton.setBounds (417, 24, 32, 32);
    prevButton.setBounds (204, 71, 28, 28);
    nextButton.setBounds (508, 71, 28, 28);
    presetName.setBounds (236, 71, 268, 28);

    inFilters.setBounds (184, 146, 53, 24);
    inHp.setBounds (267, 141, 34, 34);
    inLp.setBounds (330, 141, 34, 34);
    outFilters.setBounds (376, 146, 53, 24);
    outHp.setBounds (459, 141, 34, 34);
    outLp.setBounds (522, 141, 34, 34);
}

void Header::paint (juce::Graphics& g)
{
    g.setColour (Colours::frameInk);

    const auto label = lnf.inter (12.0f, 700, 1.0f);
    SubterraLookAndFeel::drawSpacedText (g, "INPUT", { 82.0f, 28.0f, 92.0f, 15.0f }, label);
    SubterraLookAndFeel::drawSpacedText (g, "OUTPUT", { 566.0f, 28.0f, 92.0f, 15.0f }, label);
    SubterraLookAndFeel::drawSpacedText (g, "IN FILTERS", { 186.0f, 118.0f, 176.0f, 15.0f }, label);
    SubterraLookAndFeel::drawSpacedText (g, "OUT FILTERS", { 378.0f, 118.0f, 176.0f, 15.0f }, label);

    const auto small = lnf.inter (11.0f, 600);
    for (float offset : { 0.0f, 192.0f })
    {
        SubterraLookAndFeel::drawSpacedText (g, "HP", { 243.0f + offset, 150.0f, 18.0f, 16.0f }, small);
        SubterraLookAndFeel::drawSpacedText (g, "LP", { 307.0f + offset, 150.0f, 18.0f, 16.0f }, small);
    }
    g.fillRect (369.0f, 120.0f, 1.5f, 62.0f);

    g.drawRoundedRectangle (juce::Rectangle<float> (200.0f, 68.0f, 340.0f, 34.0f).reduced (0.75f), 17.0f, 1.5f);
}
}
