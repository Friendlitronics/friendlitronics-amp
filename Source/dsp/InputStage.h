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
#include "BiquadHelpers.h"
#include "AmpVoicing.h"

namespace amp
{

/**
    Input conditioning before the preamp: a coupling/DC-block high-pass plus an
    amp-specific "bright cap" high shelf. Runs at the host sample rate (pre
    oversampling). Cheap, linear, per-channel.
*/
class InputStage
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        hp.prepare (sampleRate);
        bright.prepare (sampleRate);
        applyVoicing (voicing);
    }

    void reset()
    {
        hp.reset();
        bright.reset();
    }

    void setVoicing (const AmpVoicing& v)
    {
        voicing = v;
        applyVoicing (v);
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = bright.processSample (ch, hp.processSample (ch, d[i]));
        }
    }

private:
    void applyVoicing (const AmpVoicing& v)
    {
        hp.setHighpass (v.inputHpHz, 0.707);
        bright.setHighShelf (v.brightHz, 0.707, v.brightDb);
    }

    double sampleRate = 44100.0;
    AmpVoicing voicing;
    Biquad hp, bright;
};

} // namespace amp
