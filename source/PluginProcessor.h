#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Presets.h"
#include "dsp/PitchTracker.h"
#include "dsp/SynthVoice.h"

namespace subterra
{
// Peak level written by the audio thread and drained by the UI.
struct PeakMeter
{
    std::atomic<float> peak { 0.0f };

    void push (float level) noexcept
    {
        auto current = peak.load (std::memory_order_relaxed);
        while (level > current && ! peak.compare_exchange_weak (current, level, std::memory_order_relaxed)) {}
    }

    float take() noexcept { return peak.exchange (0.0f, std::memory_order_relaxed); }
};

class SubterraProcessor final : public juce::AudioProcessor
{
public:
    SubterraProcessor();
    ~SubterraProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return "Subterra"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    PresetManager& getPresets() noexcept { return presets; }

    // A/B comparison: two sound slots, one of them live.
    int getActiveSlot() const noexcept { return activeSlot; }
    void switchSlot();
    void copyActiveSlotToOther();

    // Settings menu options.
    std::atomic<float> tunerReference { 440.0f };
    std::atomic<bool> tunerMutesOutput { true };
    std::atomic<float> editorScale { 1.0f };

    // Read by the UI.
    PeakMeter inputMeters[2], outputMeters[2];
    std::atomic<float> detectedFrequency { 0.0f };   // the bass note being played, 0 if none
    std::atomic<bool> holdEngaged { false };

private:
    void updateFilters();

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    juce::ValueTree slots[2];
    int activeSlot = 0;

    struct Params
    {
        std::atomic<float>* inputGain; std::atomic<float>* outputGain;
        std::atomic<float>* inFilterOn; std::atomic<float>* inHighPass; std::atomic<float>* inLowPass;
        std::atomic<float>* outFilterOn; std::atomic<float>* outHighPass; std::atomic<float>* outLowPass;
        std::atomic<float>* wave; std::atomic<float>* octave; std::atomic<float>* filter;
        std::atomic<float>* reso; std::atomic<float>* env; std::atomic<float>* drive; std::atomic<float>* mix;
        std::atomic<float>* bypass; std::atomic<float>* hold; std::atomic<float>* tracking;
        std::atomic<float>* glide; std::atomic<float>* tuner;
    } params {};

    double sampleRate = 44100.0;

    // Input stage
    juce::SmoothedValue<float> inputGainSmooth, outputGainSmooth;
    juce::dsp::StateVariableTPTFilter<float> inHighPassFilter, inLowPassFilter, outHighPassFilter, outLowPassFilter;
    juce::SmoothedValue<float> inFilterMix, outFilterMix;

    // Tracking and synth
    PitchTracker tracker;
    EnvelopeFollower follower;
    MorphOscillator oscillator;
    ResonantLowPass synthFilter;
    juce::SmoothedValue<float> waveSmooth, cutoffSmooth, resoSmooth, envSmooth, driveSmooth, mixSmooth;

    float envelopeSmoothed = 0.0f;
    bool gateOpen = false;
    float gateGain = 0.0f;
    // log2 of the fundamental before the octave shift: tracked, glide target, and what the voice plays
    float currentLogFreq = 0.0f, targetLogFreq = 0.0f, voiceLogFreq = 0.0f;
    bool haveFrequency = false;
    bool waitingForFirstPitch = true;
    float notePeak = 0.0f;

    bool holdActive = false;
    float heldLevel = 0.0f, heldLogFreq = 0.0f, holdMix = 0.0f;

    // Output stage
    juce::SmoothedValue<float> tunerMuteSmooth, bypassSmooth;

    juce::AudioBuffer<float> scratch;   // 0, 1: processed dry signal, 2: mono analysis signal

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubterraProcessor)
};
}
