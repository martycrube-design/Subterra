#include "PluginProcessor.h"

#include <map>

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
const juce::Identifier editorPageProp ("editorPage");

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
               get (glide), get (tuner),
               get (fxRouting),
               get (chorusOn), get (chorusRate), get (chorusDepth), get (chorusMix),
               get (delayOn), get (delaySync), get (delayTime), get (delayDivision),
               get (delayFeedback), get (delayTone), get (delayMix),
               get (reverbOn), get (reverbSize), get (reverbTone), get (reverbMix) };

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

void SubterraProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxChunk, 2 };
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

    resetSmooth (routingSmooth, 0.03, params.fxRouting->load());
    resetSmooth (chorusAmount, 0.02, params.chorusOn->load() * params.chorusMix->load());
    resetSmooth (chorusRateSmooth, 0.05, params.chorusRate->load());
    resetSmooth (chorusDepthSmooth, 0.05, params.chorusDepth->load());
    resetSmooth (echoSend, 0.01, params.delayOn->load());
    resetSmooth (echoAmount, 0.02, params.delayMix->load());
    resetSmooth (echoFeedbackSmooth, 0.03, params.delayFeedback->load());
    resetSmooth (echoToneSmooth, 0.03, params.delayTone->load());
    resetSmooth (plateSend, 0.01, params.reverbOn->load());
    resetSmooth (plateAmount, 0.02, params.reverbMix->load());

    tracker.prepare (sampleRate);
    follower.prepare (sampleRate);
    synthFilter.prepare (sampleRate);
    chorus.prepare (sampleRate);
    echo.prepare (sampleRate);
    plate.prepare (sampleRate);

    scratch.setSize (numScratch, maxChunk, false, true, false);
    resetDsp();
}

void SubterraProcessor::resetDsp()
{
    tracker.reset();
    follower.reset();
    synthFilter.reset();
    oscillator.reset();
    for (auto* f : { &inHighPassFilter, &outHighPassFilter, &inLowPassFilter, &outLowPassFilter })
        f->reset();
    chorus.reset();
    echo.reset();
    plate.reset();
    chorusActive = echoActive = plateActive = false;
    echoSilentSamples = plateSilentSamples = 0;

    envelopeSmoothed = 0.0f;
    gateOpen = false;
    gateGain = 0.0f;
    haveFrequency = false;
    waitingForFirstPitch = true;
    notePeak = 0.0f;
    holdActive = false;
    heldLevel = 0.0f;
    holdMix = 0.0f;
    needsReset = false;
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

    if (needsReset)
        resetDsp();

    if (auto* host = getPlayHead())
        if (auto position = host->getPosition())
            if (auto bpm = position->getBpm(); bpm.hasValue() && *bpm > 10.0 && *bpm < 1000.0)
                hostTempo = (float) *bpm;

    // Input pointers are read before outputs are written, chunk by chunk, so in-place buffers are safe.
    const float* rawL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* rawR = numIn > 1 ? buffer.getReadPointer (1) : rawL;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - start);
        processChunk (rawL != nullptr ? rawL + start : nullptr, rawR != nullptr ? rawR + start : nullptr,
                      outL + start, outR != nullptr ? outR + start : nullptr, n, numIn > 0);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

void SubterraProcessor::processChunk (const float* rawL, const float* rawR, float* outL, float* outR,
                                      int numSamples, bool haveInput)
{
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

    routingSmooth.setTargetValue (params.fxRouting->load());

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

    auto* dryL = scratch.getWritePointer (dryLeft);
    auto* dryR = scratch.getWritePointer (dryRight);
    auto* mono = scratch.getWritePointer (monoIn);
    auto* synth = scratch.getWritePointer (synthOut);
    auto* fxL = scratch.getWritePointer (fxLeft);
    auto* fxR = scratch.getWritePointer (fxRight);
    auto* dryGains = scratch.getWritePointer (dryGainCh);

    // Pass 1: input gain, input filters, meters.
    float inPeakL = 0.0f, inPeakR = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = inputGainSmooth.getNextValue();
        float l = (haveInput ? rawL[i] : 0.0f) * g;
        float r = (haveInput ? rawR[i] : 0.0f) * g;
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
    inputMeters[1].push (rawR != rawL ? inPeakR : inPeakL);

    // Pass 2: tracking and the synth voice. The tracker analyses on its own hop schedule and is
    // read every sample, so the sound is the same whatever block size the host uses.
    float publishedFrequency = 0.0f;

    {
        for (int i = 0; i < numSamples; ++i)
        {
            tracker.process (mono + i, 1);
            const float trackedHz = tracker.getFrequency();
            if (trackedHz > 0.0f && (gateOpen || tunerOn))
                publishedFrequency = trackedHz;

            envelopeSmoothed = follower.process (mono[i]);
            const float levelDb = juce::Decibels::gainToDecibels (envelopeSmoothed, -120.0f);

            if (! gateOpen && levelDb > openDb)
            {
                gateOpen = true;
                waitingForFirstPitch = true;
                notePeak = 0.0f;
            }
            else if (gateOpen && (levelDb < closeDb || envelopeSmoothed < notePeak * 0.01f))
            {
                // Closes at the Tracking threshold, or once the note has died 40 dB below its peak,
                // so hum and noise under a decayed note don't keep the synth droning.
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

            // The effects bus carries the synth, plus the dry bass when routed to "Synth + Bass".
            const float mixValue = mixSmooth.getNextValue();
            const float dryGain = std::cos (mixValue * juce::MathConstants<float>::halfPi);
            const float wetGain = std::sin (mixValue * juce::MathConstants<float>::halfPi);
            const float routeAll = routingSmooth.getNextValue();

            synth[i] = wet * wetGain;
            dryGains[i] = dryGain * (1.0f - routeAll);
            fxL[i] = synth[i] + dryL[i] * dryGain * routeAll;
            fxR[i] = synth[i] + dryR[i] * dryGain * routeAll;
        }
    }
    detectedFrequency = publishedFrequency;

    // Pass 3: effects, in signal-chain order.

    chorusAmount.setTargetValue (params.chorusOn->load() > 0.5f ? params.chorusMix->load() : 0.0f);
    chorusRateSmooth.setTargetValue (params.chorusRate->load());
    chorusDepthSmooth.setTargetValue (params.chorusDepth->load());
    if (chorusAmount.getTargetValue() > 0.0f || chorusAmount.isSmoothing())
    {
        chorusActive = true;
        for (int i = 0; i < numSamples; ++i)
        {
            float wl, wr;
            chorus.process (fxL[i], fxR[i], chorusRateSmooth.getNextValue(), chorusDepthSmooth.getNextValue(), wl, wr);
            const float m = chorusAmount.getNextValue();
            // Full mix is the classic even blend of dry and modulated signal.
            fxL[i] += (wl - fxL[i]) * m * 0.5f;
            fxR[i] += (wr - fxR[i]) * m * 0.5f;
        }
    }
    else
    {
        chorusActive = false;
        for (int i = 0; i < numSamples; ++i)
            chorus.feed (fxL[i], fxR[i]);
        chorusRateSmooth.skip (numSamples);
        chorusDepthSmooth.skip (numSamples);
    }

    const float beats = delayDivisionBeats[juce::jlimit (0, 7, (int) std::lround (params.delayDivision->load()))];
    const float delaySeconds = params.delaySync->load() > 0.5f ? beats * 60.0f / hostTempo.load()
                                                               : params.delayTime->load() * 0.001f;
    const bool delayOn = params.delayOn->load() > 0.5f;
    echoSend.setTargetValue (delayOn ? 1.0f : 0.0f);
    echoAmount.setTargetValue (params.delayMix->load());
    echoFeedbackSmooth.setTargetValue (params.delayFeedback->load());
    echoToneSmooth.setTargetValue (params.delayTone->load());
    if (delayOn || echoSend.isSmoothing() || echoActive)
    {
        if (! echoActive)
            echo.reset();
        echoActive = true;
        float tail = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float send = echoSend.getNextValue();
            float el, er;
            echo.process (fxL[i] * send, fxR[i] * send, delaySeconds, echoFeedbackSmooth.getNextValue(),
                          echoToneSmooth.getNextValue(), el, er);
            const float m = echoAmount.getNextValue();
            fxL[i] += el * m;
            fxR[i] += er * m;
            tail = juce::jmax (tail, std::abs (el), std::abs (er));
        }
        // Once switched off and the repeats have died away (silent for longer than one repeat), stop running it.
        echoSilentSamples = tail < 1.0e-5f ? echoSilentSamples + numSamples : 0;
        if (! delayOn && ! echoSend.isSmoothing() && echoSilentSamples > (int) ((delaySeconds + 0.1f) * (float) sampleRate))
            echoActive = false;
    }
    else
    {
        echoAmount.skip (numSamples);
        echoFeedbackSmooth.skip (numSamples);
        echoToneSmooth.skip (numSamples);
    }

    const bool reverbOn = params.reverbOn->load() > 0.5f;
    plateSend.setTargetValue (reverbOn ? 1.0f : 0.0f);
    plateAmount.setTargetValue (params.reverbMix->load());
    if (reverbOn || plateSend.isSmoothing() || plateActive)
    {
        if (! plateActive)
            plate.reset();
        plateActive = true;
        plate.setParameters (params.reverbSize->load(), params.reverbTone->load());
        auto* vl = scratch.getWritePointer (verbLeft);
        auto* vr = scratch.getWritePointer (verbRight);
        for (int i = 0; i < numSamples; ++i)
        {
            const float send = plateSend.getNextValue();
            vl[i] = fxL[i] * send;
            vr[i] = fxR[i] * send;
        }
        plate.process (vl, vr, numSamples);
        float tail = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = plateAmount.getNextValue();
            fxL[i] += vl[i] * m;
            fxR[i] += vr[i] * m;
            tail = juce::jmax (tail, std::abs (vl[i]), std::abs (vr[i]));
        }
        plateSilentSamples = tail < 1.0e-5f ? plateSilentSamples + numSamples : 0;
        if (! reverbOn && ! plateSend.isSmoothing() && plateSilentSamples > (int) (0.1 * sampleRate))
            plateActive = false;
    }
    else
    {
        plateAmount.skip (numSamples);
    }

    // Pass 4: sum, output filters, tuner mute, output gain, bypass, meters.
    float outPeakL = 0.0f, outPeakR = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        float l = fxL[i] + dryL[i] * dryGains[i];
        float r = fxR[i] + dryR[i] * dryGains[i];

        const float fm = outFilterMix.getNextValue();
        const float fl = outLowPassFilter.processSample (0, outHighPassFilter.processSample (0, l));
        const float fr = outLowPassFilter.processSample (1, outHighPassFilter.processSample (1, r));
        l += (fl - l) * fm;
        r += (fr - r) * fm;

        const float post = tunerMuteSmooth.getNextValue() * outputGainSmooth.getNextValue();
        l *= post;
        r *= post;

        // Never pass on a broken value: mute this sample and rebuild the DSP state next block.
        if (! std::isfinite (l) || ! std::isfinite (r))
        {
            l = r = 0.0f;
            needsReset = true;
        }

        // True bypass passes the untouched input.
        const float b = bypassSmooth.getNextValue();
        const float inL = haveInput ? rawL[i] : 0.0f;
        const float inR = haveInput ? rawR[i] : 0.0f;
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

    outputMeters[0].push (outPeakL);
    outputMeters[1].push (outPeakR);
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

juce::ValueTree SubterraProcessor::captureSlot() const
{
    juce::ValueTree slot (slotType);
    slot.appendChild (presets.captureSound(), nullptr);
    slot.appendChild (presets.getState(), nullptr);
    return slot;
}

void SubterraProcessor::switchSlot()
{
    juce::ValueTree target;
    {
        const juce::ScopedLock sl (slotLock);
        const int from = activeSlot.load(), to = 1 - from;
        slots[from] = captureSlot();
        if (! slots[to].isValid())
            slots[to] = slots[from].createCopy();
        target = slots[to].createCopy();
        activeSlot = to;
    }

    presets.applySound (target.getChild (0));
    presets.setState (target.getChild (1));
}

void SubterraProcessor::copyActiveSlotToOther()
{
    const juce::ScopedLock sl (slotLock);
    slots[1 - activeSlot.load()] = captureSlot();
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
    settings.setProperty (editorPageProp, editorPage.load(), nullptr);
    state.appendChild (settings, nullptr);

    juce::ValueTree slotTree (slotsType);
    {
        const juce::ScopedLock sl (slotLock);
        slotTree.setProperty (activeProp, activeSlot.load(), nullptr);
        for (const auto& slot : slots)
            slotTree.appendChild (slot.isValid() ? slot.createCopy() : juce::ValueTree (slotType), nullptr);
    }
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

    // Note the stored values before replaceState, which adds entries for missing parameters.
    std::map<juce::String, float> stored;
    for (const auto& child : state)
        if (child.hasProperty ("id") && child.hasProperty ("value"))
            stored[child.getProperty ("id").toString()] = (float) child.getProperty ("value");

    apvts.replaceState (state);

    // replaceState skips parameters whose stored value already matches the tree, which leaves
    // e.g. a switch at an in-between normalised value. Push every stored value explicitly.
    // Parameters missing from the state (saved by an older version) go to their defaults.
    for (auto* p : getParameters())
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (param == nullptr)
            continue;
        const auto it = stored.find (param->getParameterID());
        const float normalised = it != stored.end() ? param->convertTo0to1 (it->second) : param->getDefaultValue();
        if (std::abs (param->getValue() - normalised) > 1.0e-6f)
            param->setValueNotifyingHost (normalised);
    }
    presets.setState (presetState);

    if (settings.isValid())
    {
        tunerReference = juce::jlimit (400.0f, 480.0f, (float) settings.getProperty (referenceProp, 440.0f));
        tunerMutesOutput = (bool) settings.getProperty (tunerMutesProp, true);
        editorScale = juce::jlimit (0.5f, 2.0f, (float) settings.getProperty (editorScaleProp, 1.0f));
        editorPage = juce::jlimit (0, 1, (int) settings.getProperty (editorPageProp, 0));
    }

    if (slotTree.isValid())
    {
        const juce::ScopedLock sl (slotLock);
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
