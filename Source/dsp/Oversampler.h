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
#include <memory>

/**
    Thin wrapper around juce::dsp::Oversampling.

    Provides 4x oversampling (factor 2 = 2^2) using a half-band polyphase IIR
    filter for low CPU. The saturation stage is processed inside the up/down
    bracket so that the harmonics it generates are band-limited before being
    folded back down to the host sample rate.
*/
class Oversampler
{
public:
    Oversampler() = default;

    /** Construct / reset the engine for the given channel count and block size.
        Safe to call from prepareToPlay (allocates); never from the audio thread. */
    void prepare (int numChannels, int maxBlockSize)
    {
        engine = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) juce::jmax (1, numChannels),
            factorOrder,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            /* isMaximumQuality */ true,
            /* useIntegerLatency */ false);

        engine->initProcessing ((size_t) juce::jmax (1, maxBlockSize));
        engine->reset();
    }

    void reset()
    {
        if (engine != nullptr)
            engine->reset();
    }

    /** Upsample the input block, returning an AudioBlock at the oversampled rate. */
    juce::dsp::AudioBlock<float> processSamplesUp (juce::dsp::AudioBlock<float>& block)
    {
        jassert (engine != nullptr);
        return engine->processSamplesUp (block);
    }

    /** Downsample back into the supplied (host-rate) block. */
    void processSamplesDown (juce::dsp::AudioBlock<float>& block)
    {
        jassert (engine != nullptr);
        engine->processSamplesDown (block);
    }

    /** Latency introduced by the oversampling filters, in host-rate samples. */
    int getLatencySamples() const
    {
        return engine != nullptr ? juce::roundToInt (engine->getLatencyInSamples()) : 0;
    }

    int getOversamplingFactor() const
    {
        return engine != nullptr ? (int) engine->getOversamplingFactor() : 1;
    }

private:
    // factor order 2 => 2^2 = 4x oversampling
    static constexpr size_t factorOrder = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> engine;
};
