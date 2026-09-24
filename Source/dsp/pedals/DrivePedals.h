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
#include "../Waveshapers.h"

namespace amp::pedal
{

/**
    Screamer — the TS-808 overdrive.

    The thing that makes a Tube Screamer sound like one is not its clipping
    diodes, it is *what reaches them*. The op-amp's gain leg has a high-pass at
    ~720 Hz in it, so bass passes at unity and never gets distorted while
    everything above gets the full drive. That asymmetry is the mid-hump people
    describe, and it's why a TS tightens a rhythm sound instead of blurring it.

    Here: split at 720 Hz (subtractively, so the two halves sum back exactly),
    drive only the upper half into a soft symmetric clip, recombine, then the
    tone control's low-pass and a level trim. The clip runs anti-aliased, which
    matters most when the pedal is placed AFTER CAB, since that slot runs at
    host rate with no oversampling around it.

      DRIVE / TONE / LEVEL
*/
class Screamer
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        for (auto* f : { &gainLegA, &gainLegB, &toneLp, &postHp, &preLp })
            f->prepare (spec.sampleRate);
        reset();
        update();
    }

    void reset()
    {
        for (auto* f : { &gainLegA, &gainLegB, &toneLp, &postHp, &preLp }) f->reset();
        adaa.reset();
    }

    void setControls (float drive01, float tone01, float level01)
    {
        if (! moved (drive01, drive) && ! moved (tone01, tone) && ! moved (level01, level))
            return;
        drive = drive01; tone = tone01; level = level01;
        update();
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
                // The real circuit is not fed full bandwidth either; band-limit
                // before the clip so the diodes are not asked to make harmonics
                // out of 15 kHz content.
                const float x = preLp.processSample (ch, d[i]);
                const float low = gainLegB.processSample (ch, gainLegA.processSample (ch, x));
                const float high = x - low;

                // Lows arrive at unity, highs arrive hot: the clip stage only
                // ever sees the upper band pushed.
                float y = adaa.process (ch, low + high * driveGain,
                                        shape::softClip<double>, shape::softClipAD<double>) * outTrim;
                y = toneLp.processSample (ch, y);
                y = postHp.processSample (ch, y);
                d[i] = y * levelGain;
            }
        }
    }

private:
    void update()
    {
        // 720 Hz gain-leg corner, as a 4th-order pair so the split is decisive.
        gainLegA.setLowpass (720.0, 0.54119610);
        gainLegB.setLowpass (720.0, 1.30656296);

        driveGain = 1.0f + 60.0f * drive * drive;          // unity .. ~x61
        outTrim   = 1.0f / (1.0f + 0.35f * std::log10 (1.0f + driveGain));
        preLp.setLowpass (5500.0, 0.707);
        // A TS is a midrange pedal: even wide open its top is gone by ~4 kHz.
        toneLp.setLowpass (juce::jmap ((double) tone, 900.0, 4000.0), 0.707);
        postHp.setHighpass (120.0, 0.707);                 // TS output is never boomy
        levelGain = dbToGain (juce::jmap (level, -12.0f, 12.0f));
    }

    Biquad gainLegA, gainLegB, toneLp, postHp, preLp;
    shape::Adaa1 adaa;
    float drive = 0.4f, tone = 0.5f, level = 0.5f;
    float driveGain = 1.0f, outTrim = 1.0f, levelGain = 1.0f;
};

//==============================================================================
/**
    Fuzz — Big Muff lineage.

    A Muff is three cascaded soft-clipping stages inside what is effectively a
    90 Hz - 1.2 kHz band-pass, which is exactly why it sounds huge on its own
    and disappears in a band mix: the guitar's low end is gone before the first
    diode and the midrange is scooped on the way out.

    Modelled honestly, including the scoop — TONE sweeps between the bass-heavy
    and treble-heavy sides of the same 1 kHz-centred network.

      SUSTAIN / TONE / LEVEL
*/
class Fuzz
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        for (auto* f : { &inHp, &stage1Lp, &stage2Lp, &stage3Lp, &scoop, &toneLp, &toneHp })
            f->prepare (spec.sampleRate);
        reset();
        update();
    }

    void reset()
    {
        for (auto* f : { &inHp, &stage1Lp, &stage2Lp, &stage3Lp, &scoop, &toneLp, &toneHp })
            f->reset();
        for (auto& a : adaa) a.reset();
    }

    void setControls (float sustain01, float tone01, float level01)
    {
        if (! moved (sustain01, sustain) && ! moved (tone01, tone) && ! moved (level01, level))
            return;
        sustain = sustain01; tone = tone01; level = level01;
        update();
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
                float x = inHp.processSample (ch, d[i]) * sustainGain;

                // Three soft clips, each band-limited on the way out — the
                // cascade is what gives a Muff its endless sustain.
                x = stage1Lp.processSample (ch, adaa[0].process (ch, x * 3.0f,
                        shape::softClip<double>, shape::softClipAD<double>));
                x = stage2Lp.processSample (ch, adaa[1].process (ch, x * 3.0f,
                        shape::softClip<double>, shape::softClipAD<double>));
                x = stage3Lp.processSample (ch, adaa[2].process (ch, x * 2.0f,
                        shape::softClip<double>, shape::softClipAD<double>));

                x = scoop.processSample (ch, x);           // the 1 kHz dip
                x = toneLp.processSample (ch, x);
                x = toneHp.processSample (ch, x);
                d[i] = x * levelGain;
            }
        }
    }

private:
    void update()
    {
        inHp.setHighpass (90.0, 0.707);                    // lows never reach the diodes
        sustainGain = 1.0f + 40.0f * sustain * sustain;

        stage1Lp.setLowpass (1800.0, 0.707);
        stage2Lp.setLowpass (1200.0, 0.707);
        stage3Lp.setLowpass (1200.0, 0.707);

        scoop.setPeak (1000.0, 0.8, -8.0);                 // the Muff mid scoop
        // TONE crossfades the network from dark to bright.
        // A Muff's tone control brightens by cutting bass, not by extending the
        // top — it never gets airy, and pretending otherwise is an ice pick.
        toneLp.setLowpass (juce::jmap ((double) tone, 800.0, 5000.0), 0.707);
        toneHp.setHighpass (juce::jmap ((double) tone, 60.0, 500.0), 0.707);
        levelGain = 0.5f * dbToGain (juce::jmap (level, -12.0f, 12.0f));
    }

    Biquad inHp, stage1Lp, stage2Lp, stage3Lp, scoop, toneLp, toneHp;
    std::array<shape::Adaa1, 3> adaa;
    float sustain = 0.5f, tone = 0.5f, level = 0.5f;
    float sustainGain = 1.0f, levelGain = 0.5f;
};

} // namespace amp::pedal
