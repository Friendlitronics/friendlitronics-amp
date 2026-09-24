/*
    This file is part of Amp Sim — a guitar, bass and acoustic amp simulator.
    Copyright (C) 2026 David Zevenbergen

    Amp Sim is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version. It is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See <https://www.gnu.org/licenses/> for the full licence text.

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>

#include "dsp/AmpEngine.h"

/**
    Amp Sim processor.

    A guitar amp emulator with freely swappable amp / cab / mic and a continuous
    mic-placement model. The DSP lives in amp::AmpEngine; this class owns the
    parameter tree, feeds the engine each block, and handles wet/dry mix +
    metering, mirroring the tape_emulator project's structure.
*/
class AmpSimAudioProcessor : public juce::AudioProcessor
{
public:
    AmpSimAudioProcessor();
    ~AmpSimAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    /** The pedal/amp chain order. Lives in the APVTS state tree (so it travels
        with sessions and user presets) and is mirrored into an atomic, because
        the audio thread must not read a ValueTree property. */
    amp::pedal::ChainOrder getChainOrder() const;
    void setChainOrder (const amp::pedal::ChainOrder& order);
    /** Re-reads the order from the state tree — call after replacing the state
        wholesale, e.g. when a user preset is loaded. */
    void readChainOrderFromState();

    float getInputLevel()  const { return inputLevel.load();  }
    float getOutputLevel() const { return outputLevel.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Cached parameter pointers (lock-free reads on the audio thread).
    std::atomic<float>* ampModelParam   = nullptr;
    std::atomic<float>* cabModelParam    = nullptr;
    std::atomic<float>* micModelParam    = nullptr;
    std::atomic<float>* gainParam        = nullptr;
    std::atomic<float>* bassParam        = nullptr;
    std::atomic<float>* midParam         = nullptr;
    std::atomic<float>* trebleParam      = nullptr;
    std::atomic<float>* presenceParam    = nullptr;
    std::atomic<float>* resonanceParam   = nullptr;
    std::atomic<float>* cutParam         = nullptr;
    std::atomic<float>* masterParam      = nullptr;
    std::atomic<float>* sweetnessParam   = nullptr;
    std::atomic<float>* reverbParam      = nullptr;
    std::atomic<float>* bodyParam        = nullptr;
    std::atomic<float>* deQuackParam     = nullptr;
    std::atomic<float>* compParam        = nullptr;
    std::atomic<float>* colourParam      = nullptr;
    std::atomic<float>* blendParam       = nullptr;
    std::atomic<float>* splitParam       = nullptr;
    std::atomic<float>* gritParam        = nullptr;
    std::atomic<float>* subParam         = nullptr;
    std::atomic<float>* tapeParam        = nullptr;
    std::atomic<float>* flutterParam     = nullptr;
    std::atomic<float>* crushParam       = nullptr;
    std::atomic<float>* srateParam       = nullptr;
    std::atomic<float>* noiseParam       = nullptr;
    std::atomic<float>* narrowParam      = nullptr;
    std::atomic<float>* micDistanceParam = nullptr;
    std::atomic<float>* micAxisParam     = nullptr;
    std::atomic<float>* micAngleParam    = nullptr;
    std::atomic<float>* roomParam        = nullptr;
    std::atomic<float>* outputParam      = nullptr;
    std::atomic<float>* mixParam         = nullptr;
    std::atomic<float>* bypassParam      = nullptr;

    // One entry per pedal: on, pre/post, and up to four knobs.
    struct PedalParams
    {
        std::atomic<float>* on = nullptr;
        std::array<std::atomic<float>*, 4> knob { { nullptr, nullptr, nullptr, nullptr } };
    };
    std::array<PedalParams, (size_t) amp::pedal::kNumPedals> pedalParams;

    /** Chain order packed 4 bits per slot, so the audio thread can read it in
        one atomic load. */
    std::atomic<juce::uint64> packedChain { 0 };
    void publishChainOrder (const amp::pedal::ChainOrder&);

    amp::AmpEngine engine;
    juce::dsp::Gain<float> outputGain;

    // Wet/dry: clean signal delayed to match the engine latency, blended in.
    juce::AudioBuffer<float> dryBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 16 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> outputLevel { 0.0f };

    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpSimAudioProcessor)
};
