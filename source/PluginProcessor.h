#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "Presets.h"
#include "dsp/Effects.h"
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
    double getTailLengthSeconds() const override { return 8.0; }

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
    int getActiveSlot() const noexcept { return activeSlot.load(); }
    void switchSlot();
    void copyActiveSlotToOther();

    // Settings menu options.
    std::atomic<float> tunerReference { 440.0f };
    std::atomic<bool> tunerMutesOutput { true };
    std::atomic<float> editorScale { 1.0f };
    std::atomic<int> editorPage { 0 };   // 0 pedal, 1 effects

    // Read by the UI.
    PeakMeter inputMeters[2], outputMeters[2];
    std::atomic<float> detectedFrequency { 0.0f };   // the bass note being played, 0 if none
    std::atomic<bool> holdEngaged { false };
    std::atomic<float> hostTempo { 120.0f };

    // Internal processing block; larger host blocks are split into these.
    static constexpr int maxChunk = 256;

private:
    void updateFilters();
    void processChunk (const float* rawL, const float* rawR, float* outL, float* outR, int numSamples, bool haveInput);
    void resetDsp();
    juce::ValueTree captureSlot() const;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

    juce::ValueTree slots[2];
    std::atomic<int> activeSlot { 0 };

    struct Params
    {
        std::atomic<float>* inputGain; std::atomic<float>* outputGain;
        std::atomic<float>* inFilterOn; std::atomic<float>* inHighPass; std::atomic<float>* inLowPass;
        std::atomic<float>* outFilterOn; std::atomic<float>* outHighPass; std::atomic<float>* outLowPass;
        std::atomic<float>* wave; std::atomic<float>* octave; std::atomic<float>* filter;
        std::atomic<float>* reso; std::atomic<float>* env; std::atomic<float>* drive; std::atomic<float>* mix;
        std::atomic<float>* bypass; std::atomic<float>* hold; std::atomic<float>* tracking;
        std::atomic<float>* glide; std::atomic<float>* tuner;
        std::atomic<float>* fxRouting;
        std::atomic<float>* chorusOn; std::atomic<float>* chorusRate; std::atomic<float>* chorusDepth; std::atomic<float>* chorusMix;
        std::atomic<float>* delayOn; std::atomic<float>* delaySync; std::atomic<float>* delayTime; std::atomic<float>* delayDivision;
        std::atomic<float>* delayFeedback; std::atomic<float>* delayTone; std::atomic<float>* delayMix;
        std::atomic<float>* reverbOn; std::atomic<float>* reverbSize; std::atomic<float>* reverbTone; std::atomic<float>* reverbMix;
    } params {};

    juce::CriticalSection slotLock;

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

    // Effects
    Chorus chorus;
    Echo echo;
    Plate plate;
    juce::SmoothedValue<float> routingSmooth;
    juce::SmoothedValue<float> chorusAmount, chorusRateSmooth, chorusDepthSmooth;
    // Delay and reverb have a send (the on/off switch) and a return (Mix). Switching one off closes
    // the send only, so its repeats or tail ring out naturally, like a pedal with trails.
    juce::SmoothedValue<float> echoSend, echoAmount, echoFeedbackSmooth, echoToneSmooth;
    juce::SmoothedValue<float> plateSend, plateAmount;
    bool chorusActive = false, echoActive = false, plateActive = false;
    int echoSilentSamples = 0, plateSilentSamples = 0;

    // Output stage
    juce::SmoothedValue<float> tunerMuteSmooth, bypassSmooth;

    // Scratch channels for one chunk: dry L/R, mono analysis, synth voice, effect bus L/R, reverb L/R, dry gain
    enum Scratch { dryLeft, dryRight, monoIn, synthOut, fxLeft, fxRight, verbLeft, verbRight, dryGainCh, numScratch };
    juce::AudioBuffer<float> scratch;
    bool needsReset = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubterraProcessor)
};
}
