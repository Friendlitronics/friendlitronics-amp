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

#include "PedalCommon.h"

namespace amp::pedal
{

/**
    Wah — Cry Baby style.

    A wah is an inductor-based resonant band-pass with a big boost at its peak,
    swept from roughly 400 Hz to 2.2 kHz by the treadle. There is no treadle
    here, so PEDAL is the position (automate it in the host for the real thing)
    and AUTO hands the sweep to an envelope follower, which turns the same
    circuit into the envelope filter people use for funk.

      PEDAL / Q / AUTO
*/
class Wah
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        peak.prepare (sampleRate);
        top.prepare (sampleRate);
        env.prepare (sampleRate);
        env.setTimes (8.0, 140.0);
        // Retuning a high-Q filter in jumps is audible, so the centre frequency
        // is smoothed towards its target rather than stepped.
        smoothCoeff = (float) std::exp (-1.0 / (0.004 * sampleRate / 32.0));
        reset();
        smoothedHz = 700.0f;
        update (0.0f);
    }

    void reset() { peak.reset(); top.reset(); env.reset(); counter = 0; smoothedHz = 700.0f; }

    void setControls (float pedal01, float q01, float auto01)
    {
        pedal = pedal01; q = q01; autoAmt = auto01;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            // Track the envelope every sample but retune at control rate —
            // recomputing a biquad per sample would cost more than it's worth.
            float e = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                e = juce::jmax (e, env.process (ch, block.getSample (ch, i)));

            if (counter-- <= 0)
            {
                counter = 31;
                update (e);
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                float y = peak.processSample (ch, block.getSample (ch, i));
                y = top.processSample (ch, y);   // a wah is not a tweeter
                block.setSample (ch, i, y);
            }
        }
    }

private:
    void update (float envValue)
    {
        // Envelope pushes the treadle forward; 6 is a comfortable full-sweep
        // scaling for guitar level.
        const float pos = juce::jlimit (0.0f, 1.0f, pedal + autoAmt * juce::jmin (1.0f, envValue * 6.0f));
        const float targetHz = 400.0f * std::pow (5.5f, pos);        // 400 Hz .. 2.2 kHz
        smoothedHz = smoothCoeff * (smoothedHz - targetHz) + targetHz;
        peak.setPeak ((double) smoothedHz, juce::jmap ((double) q, 2.0, 5.5), 12.0);
        top.setLowpass (6000.0, 0.707);
    }

    double sampleRate = 44100.0;
    Biquad peak, top;
    EnvFollower env;
    float pedal = 0.5f, q = 0.5f, autoAmt = 0.0f;
    float smoothedHz = 700.0f, smoothCoeff = 0.0f;
    int counter = 0;
};

//==============================================================================
/**
    Sustainer — Dyna Comp style squash.

    A studio compressor tries to be transparent; this is the opposite. Fixed
    ratio, soft knee, and a threshold that drops as SUSTAIN comes up, so quiet
    tails get hauled forward and every note ends at the same volume. ATTACK
    decides whether the pick transient survives: fast squashes it flat, slow
    lets the click through before the clamp lands.

      SUSTAIN / ATTACK / LEVEL
*/
class Sustainer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        env.prepare (sampleRate);
        reset();
        update();
    }

    void reset() { env.reset(); grDb = 0.0f; }

    void setControls (float sustain01, float attack01, float level01)
    {
        if (! moved (sustain01, sustain) && ! moved (attack01, attack) && ! moved (level01, level))
            return;
        sustain = sustain01; attack = attack01; level = level01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                peak = juce::jmax (peak, env.process (ch, block.getSample (ch, i)));

            const float levelDb = 20.0f * std::log10 (peak + 1.0e-9f);
            const float over = levelDb - thresholdDb;
            float target = 0.0f;
            if (2.0f * over >= -kKnee)
            {
                const float slope = 1.0f / kRatio - 1.0f;
                target = (2.0f * std::abs (over) <= kKnee)
                             ? slope * (over + 0.5f * kKnee) * (over + 0.5f * kKnee) / (2.0f * kKnee)
                             : slope * over;
            }
            grDb = (target < grDb ? grAtk : grRel) * (grDb - target) + target;
            const float g = dbToGain (grDb + makeupDb);

            for (int ch = 0; ch < numCh; ++ch)
                block.setSample (ch, i, block.getSample (ch, i) * g);
        }
    }

private:
    void update()
    {
        thresholdDb = juce::jmap (sustain, -6.0f, -34.0f);
        const double atkMs = juce::jmap ((double) attack, 0.8, 40.0);
        env.setTimes (atkMs, 200.0);
        grAtk = (float) std::exp (-1.0 / (0.001 * atkMs * sampleRate));
        grRel = (float) std::exp (-1.0 / (0.200 * sampleRate));
        // Auto makeup keeps the pedal from being a volume drop, then LEVEL trims.
        makeupDb = 0.5f * -thresholdDb * (1.0f - 1.0f / kRatio)
                 + juce::jmap (level, -10.0f, 10.0f);
    }

    static constexpr float kRatio = 5.0f;
    static constexpr float kKnee  = 8.0f;

    double sampleRate = 44100.0;
    EnvFollower env;
    float sustain = 0.5f, attack = 0.3f, level = 0.5f;
    float thresholdDb = -20.0f, makeupDb = 0.0f, grDb = 0.0f, grAtk = 0.0f, grRel = 0.0f;
};

} // namespace amp::pedal
