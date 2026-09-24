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
    Parametric guitar speaker cabinet (no impulse responses). A serial chain of
    biquads recreates the elements that make a cab sound like a cab:

      low-cut HP -> low resonance bump -> two cone-breakup peaks -> steep HF roll-off

    The steep 24 dB/oct low-pass around 4.5–5.5 kHz is the single most important
    part — it removes the buzzy fizz that an un-cabbed waveshaper produces.

    Linear, runs at host sample rate.
*/
class Cabinet
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        lowCut.prepare (sampleRate);
        lowBump.prepare (sampleRate);
        peak1.prepare (sampleRate);
        peak2.prepare (sampleRate);
        hiCut.prepare (sampleRate);
        applyVoicing (voicing);
    }

    void reset()
    {
        lowCut.reset(); lowBump.reset(); peak1.reset(); peak2.reset(); hiCut.reset();
    }

    void setVoicing (const CabVoicing& v)
    {
        voicing = v;
        applyVoicing (v);
    }

    /** Sweetness macro: lifts (or dips) the cone-breakup "bell" peaks. */
    void setExtraPeakDb (float db)
    {
        if (std::abs (db - extraPeakDb) > 1.0e-4f)
        {
            extraPeakDb = db;
            applyVoicing (voicing);
        }
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = lowCut.processSample (ch, d[i]);
                x = lowBump.processSample (ch, x);
                x = peak1.processSample (ch, x);
                x = peak2.processSample (ch, x);
                x = hiCut.processSample (ch, x);
                d[i] = x;
            }
        }
    }

private:
    void applyVoicing (const CabVoicing& v)
    {
        lowCut.setHighpass ((double) v.lowCutHz, 0.707);
        lowBump.setPeak    ((double) v.lowBumpHz, (double) v.lowBumpQ, (double) v.lowBumpDb);
        peak1.setPeak      ((double) v.peak1Hz, (double) v.peak1Q, (double) (v.peak1Db + extraPeakDb));
        peak2.setPeak      ((double) v.peak2Hz, (double) v.peak2Q, (double) (v.peak2Db + extraPeakDb));
        hiCut.set          ((double) v.hiCutHz);
    }

    double sampleRate = 44100.0;
    CabVoicing voicing;
    float extraPeakDb = 0.0f;
    Biquad lowCut, lowBump, peak1, peak2;
    CascadedLPF hiCut;
};

} // namespace amp
