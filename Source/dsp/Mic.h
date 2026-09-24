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

#pragma once

#include <JuceHeader.h>
#include "BiquadHelpers.h"
#include "AmpVoicing.h"

namespace amp
{

/**
    Microphone voicing — the characteristic frequency response of the chosen mic,
    independent of where it is placed (placement is handled by MicPosition).

      low-cut HP -> optional presence dip -> presence peak -> top-end shelf (air)

    SM57 (bright, mid-forward), SM7B (darker/fatter), and two condensers
    (full+airy LDC, flat+extended SDC). Linear, host sample rate.
*/
class Mic
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        lowCut.prepare (sampleRate);
        midDip.prepare (sampleRate);
        presence.prepare (sampleRate);
        topShelf.prepare (sampleRate);
        applyVoicing (voicing);
    }

    void reset()
    {
        lowCut.reset(); midDip.reset(); presence.reset(); topShelf.reset();
    }

    void setVoicing (const MicVoicing& v)
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
            {
                float x = lowCut.processSample (ch, d[i]);
                if (dipActive)
                    x = midDip.processSample (ch, x);
                x = presence.processSample (ch, x);
                x = topShelf.processSample (ch, x);
                d[i] = x;
            }
        }
    }

private:
    void applyVoicing (const MicVoicing& v)
    {
        lowCut.setHighpass ((double) v.lowCutHz, 0.707);

        dipActive = (std::abs (v.midDipDb) > 0.01f);
        if (dipActive)
            midDip.setPeak ((double) v.midDipHz, (double) v.midDipQ, (double) v.midDipDb);

        presence.setPeak     ((double) v.presHz, (double) v.presQ, (double) v.presDb);
        topShelf.setHighShelf((double) v.topShelfHz, 0.707, (double) v.topShelfDb);
    }

    double sampleRate = 44100.0;
    MicVoicing voicing;
    bool dipActive = false;
    Biquad lowCut, midDip, presence, topShelf;
};

} // namespace amp
