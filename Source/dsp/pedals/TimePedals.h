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

#include "PedalCommon.h"

namespace amp::pedal
{

/**
    Chorus — CE-2 style, which means bucket-brigade style.

    The reason an analogue chorus sounds warm is that the delayed path is a
    BBD: band-limited, noisy, and dark. Modulating a clean digital delay gives
    you that glassy "digital chorus" sound instead. So the wet path here is
    band-limited (100 Hz - 6 kHz) before it comes back, and only the wet path
    is modulated.

    The right channel's LFO runs 90 degrees behind the left, which widens it
    without the wet path cancelling when the mix is summed to mono.

      RATE / DEPTH / MIX
*/
class Chorus
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        line.prepare (sampleRate, 0.06);
        lfo.prepare (sampleRate);
        for (auto* f : { &wetLp, &wetHp }) f->prepare (sampleRate);
        baseSamples = (float) (0.007 * sampleRate);        // 7 ms centre
        reset();
        update();
    }

    void reset()
    {
        line.reset();
        for (auto* f : { &wetLp, &wetHp }) f->reset();
        lfo.prepare (sampleRate);
    }

    void setControls (float rate01, float depth01, float mix01)
    {
        if (! moved (rate01, rate) && ! moved (depth01, depth) && ! moved (mix01, mix))
            return;
        rate = rate01; depth = depth01; mix = mix01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float quad = 0.0f;
            const float mod = lfo.next (quad);

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float dry = block.getSample (ch, i);
                line.write (ch, dry);

                const float m = (ch == 0) ? mod : quad;
                const float delaySamples = baseSamples * (1.0f + 0.6f * depth * m);

                float wet = line.read (ch, delaySamples);
                wet = wetHp.processSample (ch, wetLp.processSample (ch, wet));

                block.setSample (ch, i, dry + wet * mixAmt);
            }

            line.advance();
        }
    }

private:
    void update()
    {
        lfo.setRate (juce::jmap (rate, 0.1f, 6.0f));
        wetLp.setLowpass (6000.0, 0.707);                  // BBD bandwidth
        wetHp.setHighpass (100.0, 0.707);
        mixAmt = 0.9f * mix;
    }

    double sampleRate = 44100.0;
    DelayBuffer line;
    Lfo lfo;
    Biquad wetLp, wetHp;
    float rate = 0.3f, depth = 0.5f, mix = 0.5f;
    float baseSamples = 300.0f, mixAmt = 0.45f;
};

//==============================================================================
/**
    Phaser — Phase 90 style.

    Four first-order allpass stages sweeping together, summed with the dry
    signal. Where the allpass chain hits 180 degrees the sum cancels, so the
    notches move with the sweep; FEEDBACK returns the last stage to the first,
    which sharpens the notches into the throatier Phase 100 territory.

      RATE / DEPTH / FEEDBACK
*/
class Phaser
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (auto& s : stages) s.prepare (sampleRate);
        lfo.prepare (sampleRate);
        reset();
        update();
    }

    void reset()
    {
        for (auto& s : stages) s.reset();
        fbState.fill (0.0f);
        lfo.prepare (sampleRate);
        counter = 0;
    }

    void setControls (float rate01, float depth01, float feedback01)
    {
        if (! moved (rate01, rate) && ! moved (depth01, depth) && ! moved (feedback01, feedback))
            return;
        rate = rate01; depth = depth01; feedback = feedback01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            float quad = 0.0f;
            const float mod = lfo.next (quad);

            // Retune the allpass chain at control rate.
            if (counter-- <= 0)
            {
                counter = 15;
                const float centre = 600.0f;
                const float octaves = 2.0f * depthAmt * mod;
                const float hz = centre * std::pow (2.0f, octaves);
                for (auto& s : stages)
                    s.setFrequency (hz);
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float dry = block.getSample (ch, i);
                float x = dry + fbAmt * fbState[(size_t) ch];
                for (auto& s : stages)
                    x = s.processSample (ch, x);
                fbState[(size_t) ch] = x;
                block.setSample (ch, i, 0.5f * (dry + x));
            }
        }
    }

private:
    void update()
    {
        lfo.setRate (juce::jmap (rate, 0.05f, 4.0f));
        depthAmt = depth;
        fbAmt = 0.7f * feedback;
    }

    static constexpr int kStages = 4;

    double sampleRate = 44100.0;
    std::array<AllpassStage, kStages> stages;
    Lfo lfo;
    std::array<float, Biquad::kMaxChannels> fbState {};
    float rate = 0.3f, depth = 0.6f, feedback = 0.2f;
    float depthAmt = 0.6f, fbAmt = 0.14f;
    int counter = 0;
};

//==============================================================================
/**
    Delay — analogue/tape echo rather than a digital repeat.

    Two things separate an old echo from a digital one: every repeat is darker
    than the last, and the feedback path saturates instead of clipping. Both
    fall out of putting a low-pass and a soft clip *inside* the feedback loop,
    which is what happens here. TONE sets how fast the repeats die into mud.

    Delay time is smoothed, so sweeping TIME bends pitch the way a real echo
    does instead of glitching.

      TIME / FEEDBACK / MIX / TONE
*/
class Delay
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        line.prepare (sampleRate, 1.5);
        for (auto* f : { &fbLp, &fbHp } ) f->prepare (sampleRate);
        smoothedDelay.reset (sampleRate, 0.25);            // slow: bends, not jumps
        reset();
        update();
        smoothedDelay.setCurrentAndTargetValue (targetSamples);
    }

    void reset()
    {
        line.reset();
        for (auto* f : { &fbLp, &fbHp }) f->reset();
        fbState.fill (0.0f);
    }

    void setControls (float time01, float feedback01, float mix01, float tone01)
    {
        if (! moved (time01, time) && ! moved (feedback01, feedback)
            && ! moved (mix01, mix) && ! moved (tone01, tone))
            return;
        time = time01; feedback = feedback01; mix = mix01; tone = tone01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            const float delaySamples = smoothedDelay.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float dry = block.getSample (ch, i);
                const float echo = line.read (ch, delaySamples);

                // Feedback path: darken, then saturate — never hard clip, so
                // runaway settings self-limit instead of exploding.
                float fb = fbHp.processSample (ch, fbLp.processSample (ch, echo));
                fb = std::tanh (fb * fbAmt);

                line.write (ch, dry + fb);
                block.setSample (ch, i, dry + echo * mixAmt);
            }

            line.advance();
        }
    }

private:
    void update()
    {
        targetSamples = (float) (juce::jmap ((double) time, 0.02, 1.2) * sampleRate);
        smoothedDelay.setTargetValue (targetSamples);
        fbAmt = juce::jmap (feedback, 0.0f, 1.05f);         // just past self-oscillation
        fbLp.setLowpass (juce::jmap ((double) tone, 900.0, 8000.0), 0.707);
        fbHp.setHighpass (160.0, 0.707);                    // keeps repeats from piling up
        mixAmt = mix;
    }

    double sampleRate = 44100.0;
    DelayBuffer line;
    Biquad fbLp, fbHp;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDelay;
    std::array<float, Biquad::kMaxChannels> fbState {};
    float time = 0.3f, feedback = 0.3f, mix = 0.3f, tone = 0.5f;
    float targetSamples = 1000.0f, fbAmt = 0.3f, mixAmt = 0.3f;
};

} // namespace amp::pedal
