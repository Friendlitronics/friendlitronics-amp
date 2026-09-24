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

namespace amp
{

/**
    Simulated room/ambient mic, added in parallel to the close mic. The direct
    signal stays at full level (dry = 1) and a darkened small-room reverb is
    mixed in by the Room knob — like opening up a distant room mic.

    Host sample rate, after the cab/mic chain.
*/
class RoomSim
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
        p.roomSize   = 0.32f;
        p.damping    = 0.7f;          // darker, like a distant mic
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.dryLevel   = 1.0f;          // keep the close mic intact
        p.wetLevel   = amount * 0.5f; // blend the room in
        reverb.setParameters (p);
    }

    juce::dsp::Reverb reverb;
    float amount = 0.0f;
};

} // namespace amp
