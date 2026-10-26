#include "PluginProcessor.h"

#if ! SUBTERRA_HEADLESS
 #include "PluginEditor.h"
#endif

namespace subterra
{
namespace
{
const juce::Identifier settingsType ("Settings");
const juce::Identifier slotsType ("Slots");
const juce::Identifier slotType ("Slot");
const juce::Identifier activeProp ("active");
const juce::Identifier referenceProp ("tunerReference");
const juce::Identifier tunerMutesProp ("tunerMutesOutput");
const juce::Identifier editorScaleProp ("editorScale");

float onePole (double sampleRate, float ms)
{
    return 1.0f - (float) std::exp (-1.0 / (sampleRate * juce::jmax (0.01f, ms) * 0.001));
}
}

SubterraProcessor::SubterraProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      presets (apvts)
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    using namespace ParamIDs;
    params = { get (inputGain), get (outputGain),
               get (inFilterOn), get (inHighPass), get (inLowPass),
               get (outFilterOn), get (outHighPass), get (outLowPass),
               get (wave), get (octave), get (filter),
               get (reso), get (env), get (drive), get (mix),
               get (bypass), get (hold), get (tracking),
               get (glide), get (tuner) };

    presets.loadPreset (0);
}

SubterraProcessor::~SubterraProcessor() = default;

bool SubterraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in.size() <= out.size();
}

void SubterraProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock), 2 };
    for (auto* f : { &inHighPassFilter, &outHighPassFilter })
    {
        f->prepare (spec);
        f->setType (juce::dsp::StateVariableTPTFilterType::highpass);
    }
    for (auto* f : { &inLowPassFilter, &outLowPassFilter })
    {
        f->prepare (spec);
        f->setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    }
    updateFilters();

    auto resetSmooth = [this] (juce::SmoothedValue<float>& s, double seconds, float value)
    {
        s.reset (sampleRate, seconds);
        s.setCurrentAndTargetValue (value);
    };

    resetSmooth (inputGainSmooth, 0.02, juce::Decibels::decibelsToGain (params.inputGain->load()));
    resetSmooth (outputGainSmooth, 0.02, juce::Decibels::decibelsToGain (params.outputGain->load()));
    resetSmooth (inFilterMix, 0.01, params.inFilterOn->load());
    resetSmooth (outFilterMix, 0.01, params.outFilterOn->load());
    resetSmooth (waveSmooth, 0.03, params.wave->load());
    resetSmooth (cutoffSmooth, 0.02, std::log2 (params.filter->load()));
    resetSmooth (resoSmooth, 0.02, params.reso->load());
    resetSmooth (envSmooth, 0.02, params.env->load());
    resetSmooth (driveSmooth, 0.02, params.drive->load());
    resetSmooth (mixSmooth, 0.02, params.mix->load());
    resetSmooth (tunerMuteSmooth, 0.01, 1.0f);
    resetSmooth (bypassSmooth, 0.01, params.bypass->load());

    tracker.prepare (sampleRate);
    follower.prepare (sampleRate);
    synthFilter.prepare (sampleRate);
    oscillator.reset();

    envelopeSmoothed = 0.0f;
    gateOpen = false;
    gateGain = 0.0f;
    haveFrequency = false;
    waitingForFirstPitch = true;
    notePeak = 0.0f;
    holdActive = false;
    heldLevel = 0.0f;
    holdMix = 0.0f;

    scratch.setSize (3, juce::jmax (1, samplesPerBlock), false, false, true);
}

void SubterraProcessor::updateFilters()
{
    inHighPassFilter.setCutoffFrequency (params.inHighPass->load());
    inLowPassFilter.setCutoffFrequency (juce::jmin (params.inLowPass->load(), (float) sampleRate * 0.45f));
    outHighPassFilter.setCutoffFrequency (params.outHighPass->load());
    outLowPassFilter.setCutoffFrequency (juce::jmin (params.outLowPass->load(), (float) sampleRate * 0.45f));
}

void SubterraProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = juce::jmin (getTotalNumInputChannels(), 2);
    const int numOut = juce::jmin (getTotalNumOutputChannels(), 2);

    if (numSamples == 0 || numOut == 0)
        return;

    if (scratch.getNumSamples() < numSamples)
        scratch.setSize (3, numSamples, false, false, true);

    updateFilters();
    inputGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (params.inputGain->load()));
    outputGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (params.outputGain->load()));
    inFilterMix.setTargetValue (params.inFilterOn->load());
    outFilterMix.setTargetValue (params.outFilterOn->load());
    waveSmooth.setTargetValue (params.wave->load());
    cutoffSmooth.setTargetValue (std::log2 (params.filter->load()));
    resoSmooth.setTargetValue (params.reso->load());
    envSmooth.setTargetValue (params.env->load());
    driveSmooth.setTargetValue (params.drive->load());
    mixSmooth.setTargetValue (params.mix->load());

    const bool tunerOn = params.tuner->load() > 0.5f;
    tunerMuteSmooth.setTargetValue (tunerOn && tunerMutesOutput.load() ? 0.0f : 1.0f);
    bypassSmooth.setTargetValue (params.bypass->load());

    const int octaveIndex = juce::jlimit (0, 4, (int) std::lround (params.octave->load()));
    const float octaveShift = (float) octaveSteps[octaveIndex];

    const float openDb = -20.0f - 50.0f * params.tracking->load();
    const float closeDb = openDb - 6.0f;
    const float glideCoef = onePole (sampleRate, juce::jmax (2.0f, params.glide->load()));
    const float gateAttack = onePole (sampleRate, 4.0f);
    const float gateRelease = onePole (sampleRate, 40.0f);
    const float holdCoef = onePole (sampleRate, 20.0f);
    const float voiceCoef = onePole (sampleRate, 1.5f);   // de-clicks the switch into and out of Hold

    // Hold latches the current note and its level.
    const bool holdOn = params.hold->load() > 0.5f;
    if (holdOn && ! holdActive)
    {
        holdActive = true;
        heldLogFreq = voiceLogFreq;
        const float live = envelopeSmoothed * gateGain;
        heldLevel = haveFrequency ? juce::jmax (live, notePeak * 0.6f) : 0.0f;
    }
    else if (! holdOn)
    {
        holdActive = false;
    }
    holdEngaged = holdActive && heldLevel > 0.0f;

    // Pass 1: input gain, input filters, meters. Keeps the raw input in the buffer for bypass.
    auto* dryL = scratch.getWritePointer (0);
    auto* dryR = scratch.getWritePointer (1);
    auto* mono = scratch.getWritePointer (2);
    const float* rawL = buffer.getReadPointer (0);
    const float* rawR = numIn > 1 ? buffer.getReadPointer (1) : rawL;

    float inPeakL = 0.0f, inPeakR = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = inputGainSmooth.getNextValue();
        float l = (numIn > 0 ? rawL[i] : 0.0f) * g;
        float r = (numIn > 0 ? rawR[i] : 0.0f) * g;
        inPeakL = juce::jmax (inPeakL, std::abs (l));
        inPeakR = juce::jmax (inPeakR, std::abs (r));

        const float fm = inFilterMix.getNextValue();
        const float fl = inLowPassFilter.processSample (0, inHighPassFilter.processSample (0, l));
        const float fr = inLowPassFilter.processSample (1, inHighPassFilter.processSample (1, r));
        l += (fl - l) * fm;
        r += (fr - r) * fm;

        dryL[i] = l;
        dryR[i] = r;
        mono[i] = 0.5f * (l + r);
    }
    inputMeters[0].push (inPeakL);
    inputMeters[1].push (numIn > 1 ? inPeakR : inPeakL);

    // Pass 2: tracking and synth, in short chunks so pitch updates land quickly.
    constexpr int chunkSize = 32;
    float outPeakL = 0.0f, outPeakR = 0.0f;
    float publishedFrequency = 0.0f;

    auto* outL = buffer.getWritePointer (0);
    auto* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int end = juce::jmin (numSamples, start + chunkSize);
        tracker.process (mono + start, end - start);
        const float trackedHz = tracker.getFrequency();

        for (int i = start; i < end; ++i)
        {
            envelopeSmoothed = follower.process (mono[i]);
            const float levelDb = juce::Decibels::gainToDecibels (envelopeSmoothed, -120.0f);

            if (! gateOpen && levelDb > openDb)
            {
                gateOpen = true;
                waitingForFirstPitch = true;
                notePeak = 0.0f;
            }
            else if (gateOpen && levelDb < closeDb)
            {
                gateOpen = false;
            }

            if (gateOpen)
            {
                notePeak = juce::jmax (notePeak, envelopeSmoothed);

                if (trackedHz > 0.0f)
                {
                    const float target = std::log2 (trackedHz);
                    // A note that starts from silence jumps straight to pitch; legato notes glide.
                    if (waitingForFirstPitch && gateGain < 0.05f)
                    {
                        currentLogFreq = target;
                        if (! holdActive)
                            voiceLogFreq = target;
                    }
                    targetLogFreq = target;
                    waitingForFirstPitch = false;
                    haveFrequency = true;
                }
            }

            currentLogFreq += (targetLogFreq - currentLogFreq) * glideCoef;

            const bool voiceOn = gateOpen && haveFrequency && ! waitingForFirstPitch;
            gateGain += ((voiceOn ? 1.0f : 0.0f) - gateGain) * (voiceOn ? gateAttack : gateRelease);

            holdMix += ((holdActive ? 1.0f : 0.0f) - holdMix) * holdCoef;
            const float liveAmp = envelopeSmoothed * gateGain;
            const float amp = liveAmp + (heldLevel - liveAmp) * holdMix;

            const float voiceTarget = holdActive ? heldLogFreq : currentLogFreq;
            if (amp < 1.0e-5f)
                voiceLogFreq = voiceTarget;
            else
                voiceLogFreq += (voiceTarget - voiceLogFreq) * voiceCoef;

            // Synth voice: oscillator -> drive -> resonant filter -> level from the bass.
            const float wave = waveSmooth.getNextValue();
            const float drive = driveSmooth.getNextValue();
            const float reso = resoSmooth.getNextValue();
            const float envAmount = envSmooth.getNextValue();
            const float cutoffLog = cutoffSmooth.getNextValue();

            float wet = 0.0f;
            if (haveFrequency)
            {
                const double frequency = std::exp2 ((double) voiceLogFreq + octaveShift);
                float s = oscillator.process (frequency / sampleRate, wave);
                s = driveShape (s, drive);

                const float envNorm = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (amp, -120.0f) + 48.0f) / 48.0f);
                const float cutoff = std::exp2 (cutoffLog + envAmount * envNorm * 5.0f);
                s = synthFilter.process (s, cutoff, reso);

                wet = s * amp * (1.0f - 0.35f * reso);
            }

            const float mixValue = mixSmooth.getNextValue();
            const float dryGain = std::cos (mixValue * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin (mixValue * juce::MathConstants<float>::halfPi);

            float l = dryL[i] * dryGain + wet * wetGain;
            float r = dryR[i] * dryGain + wet * wetGain;

            const float fm = outFilterMix.getNextValue();
            const float fl = outLowPassFilter.processSample (0, outHighPassFilter.processSample (0, l));
            const float fr = outLowPassFilter.processSample (1, outHighPassFilter.processSample (1, r));
            l += (fl - l) * fm;
            r += (fr - r) * fm;

            const float post = tunerMuteSmooth.getNextValue() * outputGainSmooth.getNextValue();
            l *= post;
            r *= post;

            // True bypass passes the untouched input.
            const float b = bypassSmooth.getNextValue();
            const float inL = numIn > 0 ? rawL[i] : 0.0f;
            const float inR = numIn > 0 ? rawR[i] : 0.0f;
            l += (inL - l) * b;
            r += (inR - r) * b;

            if (outR != nullptr)
            {
                outL[i] = l;
                outR[i] = r;
            }
            else
            {
                outL[i] = 0.5f * (l + r);
            }

            outPeakL = juce::jmax (outPeakL, std::abs (outL[i]));
            outPeakR = juce::jmax (outPeakR, std::abs (outR != nullptr ? outR[i] : outL[i]));
        }

        if (trackedHz > 0.0f && (gateOpen || tunerOn))
            publishedFrequency = trackedHz;
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    outputMeters[0].push (outPeakL);
    outputMeters[1].push (outPeakR);
    detectedFrequency = publishedFrequency;
}

juce::AudioProcessorEditor* SubterraProcessor::createEditor()
{
   #if SUBTERRA_HEADLESS
    return new juce::GenericAudioProcessorEditor (*this);
   #else
    return new SubterraEditor (*this);
   #endif
}

bool SubterraProcessor::hasEditor() const { return true; }

int SubterraProcessor::getNumPrograms() { return presets.getNumFactoryPresets(); }

int SubterraProcessor::getCurrentProgram()
{
    const int index = presets.getCurrentIndex();
    return index < presets.getNumFactoryPresets() ? index : 0;
}

void SubterraProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, presets.getNumFactoryPresets()))
        presets.loadPreset (index);
}

const juce::String SubterraProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, presets.getNumPresets()) ? presets.getPreset (index).name : juce::String();
}

juce::AudioProcessorParameter* SubterraProcessor::getBypassParameter() const
{
    return apvts.getParameter (ParamIDs::bypass);
}

void SubterraProcessor::switchSlot()
{
    auto capture = [this]
    {
        juce::ValueTree slot (slotType);
        slot.appendChild (presets.captureSound(), nullptr);
        slot.appendChild (presets.getState(), nullptr);
        return slot;
    };

    slots[activeSlot] = capture();
    activeSlot = 1 - activeSlot;

    if (! slots[activeSlot].isValid())
        slots[activeSlot] = slots[1 - activeSlot].createCopy();

    presets.applySound (slots[activeSlot].getChild (0));
    presets.setState (slots[activeSlot].getChild (1));
}

void SubterraProcessor::copyActiveSlotToOther()
{
    juce::ValueTree slot (slotType);
    slot.appendChild (presets.captureSound(), nullptr);
    slot.appendChild (presets.getState(), nullptr);
    slots[1 - activeSlot] = slot;
}

void SubterraProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    for (auto type : { juce::Identifier ("PresetState"), settingsType, slotsType })
        if (auto child = state.getChildWithName (type); child.isValid())
            state.removeChild (child, nullptr);

    state.appendChild (presets.getState(), nullptr);

    juce::ValueTree settings (settingsType);
    settings.setProperty (referenceProp, tunerReference.load(), nullptr);
    settings.setProperty (tunerMutesProp, tunerMutesOutput.load(), nullptr);
    settings.setProperty (editorScaleProp, editorScale.load(), nullptr);
    state.appendChild (settings, nullptr);

    juce::ValueTree slotTree (slotsType);
    slotTree.setProperty (activeProp, activeSlot, nullptr);
    for (const auto& slot : slots)
        slotTree.appendChild (slot.isValid() ? slot.createCopy() : juce::ValueTree (slotType), nullptr);
    state.appendChild (slotTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SubterraProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    const auto presetState = state.getChildWithName ("PresetState").createCopy();
    const auto settings = state.getChildWithName (settingsType).createCopy();
    const auto slotTree = state.getChildWithName (slotsType).createCopy();

    for (auto type : { juce::Identifier ("PresetState"), settingsType, slotsType })
        if (auto child = state.getChildWithName (type); child.isValid())
            state.removeChild (child, nullptr);

    apvts.replaceState (state);

    // replaceState skips parameters whose stored value already matches the tree, which leaves
    // e.g. a switch at an in-between normalised value. Push every stored value explicitly.
    for (const auto& child : state)
    {
        if (auto* param = apvts.getParameter (child.getProperty ("id").toString()))
        {
            const float normalised = param->convertTo0to1 ((float) child.getProperty ("value"));
            if (std::abs (param->getValue() - normalised) > 1.0e-6f)
                param->setValueNotifyingHost (normalised);
        }
    }
    presets.setState (presetState);

    if (settings.isValid())
    {
        tunerReference = juce::jlimit (400.0f, 480.0f, (float) settings.getProperty (referenceProp, 440.0f));
        tunerMutesOutput = (bool) settings.getProperty (tunerMutesProp, true);
        editorScale = juce::jlimit (0.5f, 2.0f, (float) settings.getProperty (editorScaleProp, 1.0f));
    }

    if (slotTree.isValid())
    {
        activeSlot = juce::jlimit (0, 1, (int) slotTree[activeProp]);
        for (int i = 0; i < 2; ++i)
        {
            const auto slot = slotTree.getChild (i);
            slots[i] = slot.getNumChildren() == 2 ? slot : juce::ValueTree();
        }
    }
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new subterra::SubterraProcessor();
}
