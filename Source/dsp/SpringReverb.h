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

namespace amp
{

/**
    Onboard amp reverb (the Twin's tank, the Champ's verb). A spring tank is a
    dispersive, slightly metallic medium; a full physical model is out of scope
    for v1, so this is a brighter, longer reverb than the room sim, mixed by the
    Reverb knob. Inserted after the power amp, before the cab (it lives "in the
    amp"). Only enabled for models whose voicing has reverb.

    Host sample rate.
*/
class SpringReverb
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        reverb.prepare (spec);
        updateParams();
    }

    void reset() { reverb.reset(); }

    void setAmount (float a) { amount = juce::jlimit (0.0f, 1.0f, a); updateParams(); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

private:
    void updateParams()
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize   = 0.6f;
        p.damping    = 0.35f;         // brighter, longer than the room sim
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.dryLevel   = 1.0f;
        p.wetLevel   = amount * 0.4f;
        reverb.setParameters (p);
    }

    juce::dsp::Reverb reverb;
    float amount = 0.0f;
};

} // namespace amp
