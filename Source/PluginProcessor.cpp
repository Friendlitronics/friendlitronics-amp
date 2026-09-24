/*
    This file is part of Friendlitronics Amp — a guitar, bass and acoustic amp simulator.
    Copyright (C) 2026 David Zevenbergen

    Friendlitronics Amp is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version. It is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See <https://www.gnu.org/licenses/> for the full licence text.

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "dsp/AmpVoicing.h"
#include "dsp/pedals/PedalBoard.h"

namespace ParamID
{
    static constexpr auto ampModel    = "ampModel";
    static constexpr auto cabModel    = "cabModel";
    static constexpr auto micModel    = "micModel";
    static constexpr auto gain        = "gain";
    static constexpr auto bass        = "bass";
    static constexpr auto mid         = "mid";
    static constexpr auto treble      = "treble";
    static constexpr auto presence    = "presence";
    static constexpr auto resonance   = "resonance";
    static constexpr auto cut         = "cut";
    static constexpr auto master      = "master";
    static constexpr auto sweetness   = "sweetness";
    static constexpr auto reverb      = "reverb";
    static constexpr auto body        = "body";
    static constexpr auto deQuack     = "deQuack";
    static constexpr auto comp        = "comp";
    static constexpr auto colour      = "colour";
    static constexpr auto blend       = "blend";
    static constexpr auto split       = "split";
    static constexpr auto grit        = "grit";
    static constexpr auto sub         = "sub";
    static constexpr auto tape        = "tape";
    static constexpr auto flutter     = "flutter";
    static constexpr auto crush       = "crush";
    static constexpr auto srate       = "srate";
    static constexpr auto noise       = "noise";
    static constexpr auto narrow      = "narrow";
    static constexpr auto micDistance = "micDistance";
    static constexpr auto micAxis     = "micAxis";
    static constexpr auto micAngle    = "micAngle";
    static constexpr auto room        = "room";
    static constexpr auto output      = "output";
    static constexpr auto mix         = "mix";
    static constexpr auto bypass      = "bypass";
}

AmpSimAudioProcessor::AmpSimAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    ampModelParam    = apvts.getRawParameterValue (ParamID::ampModel);
    cabModelParam    = apvts.getRawParameterValue (ParamID::cabModel);
    micModelParam    = apvts.getRawParameterValue (ParamID::micModel);
    gainParam        = apvts.getRawParameterValue (ParamID::gain);
    bassParam        = apvts.getRawParameterValue (ParamID::bass);
    midParam         = apvts.getRawParameterValue (ParamID::mid);
    trebleParam      = apvts.getRawParameterValue (ParamID::treble);
    presenceParam    = apvts.getRawParameterValue (ParamID::presence);
    resonanceParam   = apvts.getRawParameterValue (ParamID::resonance);
    cutParam         = apvts.getRawParameterValue (ParamID::cut);
    masterParam      = apvts.getRawParameterValue (ParamID::master);
    sweetnessParam   = apvts.getRawParameterValue (ParamID::sweetness);
    reverbParam      = apvts.getRawParameterValue (ParamID::reverb);
    bodyParam        = apvts.getRawParameterValue (ParamID::body);
    deQuackParam     = apvts.getRawParameterValue (ParamID::deQuack);
    compParam        = apvts.getRawParameterValue (ParamID::comp);
    colourParam      = apvts.getRawParameterValue (ParamID::colour);
    blendParam       = apvts.getRawParameterValue (ParamID::blend);
    splitParam       = apvts.getRawParameterValue (ParamID::split);
    gritParam        = apvts.getRawParameterValue (ParamID::grit);
    subParam         = apvts.getRawParameterValue (ParamID::sub);
    tapeParam        = apvts.getRawParameterValue (ParamID::tape);
    flutterParam     = apvts.getRawParameterValue (ParamID::flutter);
    crushParam       = apvts.getRawParameterValue (ParamID::crush);
    srateParam       = apvts.getRawParameterValue (ParamID::srate);
    noiseParam       = apvts.getRawParameterValue (ParamID::noise);
    narrowParam      = apvts.getRawParameterValue (ParamID::narrow);
    micDistanceParam = apvts.getRawParameterValue (ParamID::micDistance);
    micAxisParam     = apvts.getRawParameterValue (ParamID::micAxis);
    micAngleParam    = apvts.getRawParameterValue (ParamID::micAngle);
    roomParam        = apvts.getRawParameterValue (ParamID::room);
    outputParam      = apvts.getRawParameterValue (ParamID::output);
    mixParam         = apvts.getRawParameterValue (ParamID::mix);
    bypassParam      = apvts.getRawParameterValue (ParamID::bypass);

    publishChainOrder (amp::pedal::defaultChainOrder());

    for (int i = 0; i < amp::pedal::kNumPedals; ++i)
    {
        const juce::String tag (amp::pedal::pedalTag ((amp::pedal::Id) i));
        auto& pp = pedalParams[(size_t) i];
        pp.on   = apvts.getRawParameterValue (tag + "On");
        for (int k = 0; k < amp::pedal::pedalKnobCount ((amp::pedal::Id) i); ++k)
            pp.knob[(size_t) k] = apvts.getRawParameterValue (
                amp::pedal::pedalKnobId ((amp::pedal::Id) i, k));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout
AmpSimAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name,
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
            AudioParameterFloatAttributes().withLabel ("%"));
    };

    auto knob = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name,
            NormalisableRange<float> (0.0f, 10.0f, 0.01f), def);
    };

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::ampModel, 1 }, "Amp", amp::ampNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::cabModel, 1 }, "Cab", amp::cabNames(), 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::micModel, 1 }, "Mic", amp::micNames(), 0));

    layout.add (knob (ParamID::gain,      "Gain",      4.0f));
    layout.add (knob (ParamID::bass,      "Bass",      5.0f));
    layout.add (knob (ParamID::mid,       "Mid",       5.0f));
    layout.add (knob (ParamID::treble,    "Treble",    6.0f));
    layout.add (knob (ParamID::presence,  "Presence",  5.0f));
    layout.add (knob (ParamID::resonance, "Resonance", 4.0f));
    layout.add (knob (ParamID::cut,       "Cut",       3.0f));
    layout.add (knob (ParamID::master,    "Master",    5.0f));
    layout.add (knob (ParamID::sweetness, "Sweetness", 5.0f));

    // Acoustic amp's piezo imager (inactive for the electric amps).
    layout.add (knob (ParamID::body,      "Body",      6.0f));
    layout.add (knob (ParamID::deQuack,   "De-Quack",  6.0f));
    layout.add (knob (ParamID::comp,      "Comp",      4.0f));
    layout.add (knob (ParamID::colour,    "Colour",    2.0f));

    // Bass amps' drive front end (inactive for the other amps).
    layout.add (knob (ParamID::blend,     "Blend",     0.0f));
    layout.add (knob (ParamID::split,     "Split",     5.0f));
    layout.add (knob (ParamID::grit,      "Grit",      3.0f));
    layout.add (knob (ParamID::sub,       "Sub",       0.0f));

    // Tape colour — works with any amp; 0 = stage bypassed entirely.
    layout.add (knob (ParamID::tape,      "Tape",      0.0f));
    layout.add (knob (ParamID::flutter,   "Flutter",   0.0f));

    // LoFi degradation — any amp, and the Line path's main event; 0 = bypassed.
    layout.add (knob (ParamID::crush,     "Crush",     0.0f));
    layout.add (knob (ParamID::srate,     "SR Reduce", 0.0f));
    layout.add (knob (ParamID::noise,     "Noise",     0.0f));
    layout.add (knob (ParamID::narrow,    "Narrow",    0.0f));

    layout.add (pct (ParamID::reverb,      "Reverb",   0.0f));
    layout.add (pct (ParamID::micDistance, "Distance", 15.0f));
    layout.add (pct (ParamID::micAxis,     "Axis",     30.0f));
    layout.add (pct (ParamID::micAngle,    "Angle",    0.0f));
    layout.add (pct (ParamID::room,        "Room",     0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.01f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (pct (ParamID::mix, "Mix", 100.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ParamID::bypass, 1 }, "Bypass", false));

    // --- pedalboard: on / placement / knobs, per pedal ---
    for (int i = 0; i < amp::pedal::kNumPedals; ++i)
    {
        const auto id = (amp::pedal::Id) i;
        const juce::String tag (amp::pedal::pedalTag (id));
        const juce::String name (amp::pedal::pedalName (id));
        const auto knobNames = amp::pedal::pedalKnobNames (id);
        const auto defaults  = amp::pedal::pedalKnobDefaults (id);

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { tag + "On", 1 }, name + " On", false));

        for (int k = 0; k < amp::pedal::pedalKnobCount (id); ++k)
            layout.add (std::make_unique<AudioParameterFloat> (
                ParameterID { amp::pedal::pedalKnobId (id, k), 1 },
                name + " " + knobNames[k].toLowerCase(),
                NormalisableRange<float> (0.0f, 10.0f, 0.01f), defaults[(size_t) k]));
    }

    return layout;
}

static constexpr const char* kChainProperty = "chainOrder";

void AmpSimAudioProcessor::publishChainOrder (const amp::pedal::ChainOrder& order)
{
    juce::uint64 packed = 0;
    for (int i = 0; i < amp::pedal::kChainLength; ++i)
        packed |= (juce::uint64) (order[(size_t) i] & 0xf) << (4 * i);
    packedChain.store (packed);
}

amp::pedal::ChainOrder AmpSimAudioProcessor::getChainOrder() const
{
    const juce::uint64 packed = packedChain.load();
    amp::pedal::ChainOrder order {};
    for (int i = 0; i < amp::pedal::kChainLength; ++i)
        order[(size_t) i] = (int) ((packed >> (4 * i)) & 0xf);

    return amp::pedal::chainOrderValid (order) ? order : amp::pedal::defaultChainOrder();
}

void AmpSimAudioProcessor::setChainOrder (const amp::pedal::ChainOrder& order)
{
    if (! amp::pedal::chainOrderValid (order))
        return;

    publishChainOrder (order);
    apvts.state.setProperty (kChainProperty, amp::pedal::chainOrderToString (order), nullptr);
}

void AmpSimAudioProcessor::readChainOrderFromState()
{
    const auto text = apvts.state.getProperty (kChainProperty).toString();
    publishChainOrder (text.isNotEmpty() ? amp::pedal::chainOrderFromString (text)
                                         : amp::pedal::defaultChainOrder());
}

void AmpSimAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numChannels = juce::jmax (1, getTotalNumInputChannels());

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) numChannels;

    engine.prepare (sampleRate, numChannels, samplesPerBlock);

    outputGain.prepare (spec);
    outputGain.setRampDurationSeconds (0.02);

    const int latency = engine.getLatencySamples();

    dryBuffer.setSize (numChannels, samplesPerBlock);
    dryBuffer.clear();
    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (juce::jmax (1, latency) + 1);
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();

    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);

    inputLevel.store (0.0f);
    outputLevel.store (0.0f);

    setLatencySamples (latency);
}

void AmpSimAudioProcessor::releaseResources()
{
    engine.reset();
}

bool AmpSimAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void AmpSimAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();

    float inPeak = 0.0f;
    for (int ch = 0; ch < totalIn; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));
    inputLevel.store (inPeak);

    if (*bypassParam > 0.5f)
    {
        outputLevel.store (inPeak);
        return;
    }

    const int chans = juce::jmin (totalOut, dryBuffer.getNumChannels());

    for (int ch = 0; ch < chans; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // Gather parameters for the engine.
    amp::AmpEngine::Params p;
    p.ampModel    = (int) std::round (ampModelParam->load());
    p.cabModel    = (int) std::round (cabModelParam->load());
    p.micModel    = (int) std::round (micModelParam->load());
    p.gain        = gainParam->load();
    p.bass        = bassParam->load();
    p.mid         = midParam->load();
    p.treble      = trebleParam->load();
    p.presence    = presenceParam->load();
    p.resonance   = resonanceParam->load();
    p.cut         = cutParam->load();
    p.master      = masterParam->load();
    p.sweetness   = sweetnessParam->load();
    p.reverb      = reverbParam->load() * 0.01f;
    p.body        = bodyParam->load();
    p.deQuack     = deQuackParam->load();
    p.comp        = compParam->load();
    p.colour      = colourParam->load();
    p.blend       = blendParam->load();
    p.split       = splitParam->load();
    p.grit        = gritParam->load();
    p.sub         = subParam->load();
    p.tape        = tapeParam->load();
    p.flutter     = flutterParam->load();
    p.crush       = crushParam->load();
    p.srate       = srateParam->load();
    p.noise       = noiseParam->load();
    p.narrow      = narrowParam->load();

    // Chain order decides both the sequence and which side of the amp each
    // pedal lands on.
    p.pedals.order = getChainOrder();
    bool afterAmp = false;
    std::array<amp::pedal::Slot, (size_t) amp::pedal::kNumPedals> slots {};
    for (int position = 0; position < amp::pedal::kChainLength; ++position)
    {
        const int idx = p.pedals.order[(size_t) position];
        if (idx == amp::pedal::kAmpToken)
            afterAmp = true;
        else
            slots[(size_t) idx] = afterAmp ? amp::pedal::Slot::Post : amp::pedal::Slot::Pre;
    }

    for (int i = 0; i < amp::pedal::kNumPedals; ++i)
    {
        const auto& pp = pedalParams[(size_t) i];
        auto& c = p.pedals.pedals[(size_t) i];
        c.on   = pp.on != nullptr && pp.on->load() > 0.5f;
        c.slot = slots[(size_t) i];
        c.a = pp.knob[0] != nullptr ? pp.knob[0]->load() : 5.0f;
        c.b = pp.knob[1] != nullptr ? pp.knob[1]->load() : 5.0f;
        c.c = pp.knob[2] != nullptr ? pp.knob[2]->load() : 5.0f;
        c.d = pp.knob[3] != nullptr ? pp.knob[3]->load() : 5.0f;
    }
    p.micDistance = micDistanceParam->load() * 0.01f;
    p.micAxis     = micAxisParam->load() * 0.01f;
    p.micAngle    = micAngleParam->load() * 0.01f;
    p.room        = roomParam->load() * 0.01f;
    engine.setParams (p);

    outputGain.setGainDecibels (outputParam->load());

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    engine.process (block);
    outputGain.process (context);

    // --- wet/dry blend (dry delayed to match the wet path latency) ---
    juce::dsp::AudioBlock<float> dryBlock (dryBuffer);
    auto drySub = dryBlock.getSubBlock (0, (size_t) numSamples);
    juce::dsp::ProcessContextReplacing<float> dryCtx (drySub);
    dryDelay.process (dryCtx);

    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = mixSmoothed.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            wet[i] = wet[i] * m + dry[i] * (1.0f - m);
        }
    }

    float outPeak = 0.0f;
    for (int ch = 0; ch < totalOut; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, numSamples));
    outputLevel.store (outPeak);
}

//==============================================================================
int AmpSimAudioProcessor::getNumPrograms()        { return (int) Presets::all().size(); }
int AmpSimAudioProcessor::getCurrentProgram()     { return currentProgram; }

void AmpSimAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    currentProgram = index;
    Presets::apply (apvts, index);
}

const juce::String AmpSimAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return Presets::all()[(size_t) index].name;
}

//==============================================================================
juce::AudioProcessorEditor* AmpSimAudioProcessor::createEditor()
{
    return new AmpSimAudioProcessorEditor (*this);
}

void AmpSimAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void AmpSimAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState (tree);
        readChainOrderFromState();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmpSimAudioProcessor();
}
