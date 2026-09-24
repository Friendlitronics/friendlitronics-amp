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
#include <array>
#include "BiquadHelpers.h"

namespace amp
{

/**
    Continuous mic-placement model. Three controls, each mapped to musically
    sensible filtering so dragging them sounds like physically moving the mic:

      - DISTANCE (0 close … 1 far):
          proximity low-shelf boost that fades out as you pull back, plus
          progressive HF "air loss" low-pass and a slight level drop (1/r).
      - AXIS (0 cone-centre … 1 cone-edge):
          a high tilt (centre = bright/aggressive, edge = dark/smooth) plus a
          subtle short comb to mimic moving across the cone.
      - ANGLE (0 on-axis … 1 off-axis):
          a high-shelf cut that tames the harsh top (the classic "angle the 57").

    Linear-ish (the comb is a fixed short FIR), host sample rate.
*/
class MicPosition
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        prox.prepare (sampleRate);
        air.prepare (sampleRate);
        axisTilt.prepare (sampleRate);
        angleCut.prepare (sampleRate);
        combSamples = juce::jlimit (1, kCombMax - 1,
                                    (int) std::round (0.00018 * sampleRate)); // ~0.18 ms
        reset();
        update();
    }

    void reset()
    {
        prox.reset(); air.reset(); axisTilt.reset(); angleCut.reset();
        for (auto& line : comb) line.fill (0.0f);
        writePos = 0;
    }

    void setDistance (float d) { distance = juce::jlimit (0.0f, 1.0f, d); update(); }
    void setAxis     (float a) { axis     = juce::jlimit (0.0f, 1.0f, a); update(); }
    void setAngle    (float a) { angle    = juce::jlimit (0.0f, 1.0f, a); update(); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            auto& line = comb[(size_t) ch];
            int wp = writePos;

            for (int i = 0; i < numSamples; ++i)
            {
                float x = prox.processSample (ch, d[i]);
                x = air.processSample (ch, x);
                x = axisTilt.processSample (ch, x);

                // Short comb for off-centre placement.
                line[(size_t) wp] = x;
                const int rp = (wp - combSamples + kCombMax) % kCombMax;
                x = x + combMix * line[(size_t) rp];
                wp = (wp + 1) % kCombMax;

                x = angleCut.processSample (ch, x);
                d[i] = x * distanceGain;
            }

            if (ch == numCh - 1)
                writePos = wp;
        }
    }

private:
    void update()
    {
        // Distance: proximity bass that fades with distance + air-loss LPF + level.
        prox.setLowShelf (150.0, 0.707, (1.0f - distance) * 6.0f);
        air.setLowpass (juce::jmap (distance, 0.0f, 1.0f, 20000.0f, 6000.0f), 0.707);
        distanceGain = juce::jmap (distance, 0.0f, 1.0f, 1.0f, 0.6f);

        // Axis: bright centre -> dark edge, plus growing comb depth.
        axisTilt.setHighShelf (3000.0, 0.707, juce::jmap (axis, 0.0f, 1.0f, 3.0f, -6.0f));
        combMix = axis * 0.25f;

        // Angle: tame the top as you tilt off-axis.
        angleCut.setHighShelf (4000.0, 0.707, juce::jmap (angle, 0.0f, 1.0f, 0.0f, -5.0f));
    }

    static constexpr int kCombMax = 64;

    double sampleRate = 44100.0;
    float distance = 0.15f, axis = 0.3f, angle = 0.0f;
    float distanceGain = 1.0f, combMix = 0.0f;
    int combSamples = 8, writePos = 0;

    Biquad prox, air, axisTilt, angleCut;
    std::array<std::array<float, kCombMax>, Biquad::kMaxChannels> comb {};
};

} // namespace amp
