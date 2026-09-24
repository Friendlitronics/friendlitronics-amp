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
#include <array>
#include <cmath>
#include "BiquadHelpers.h"
#include "Waveshapers.h"

namespace amp
{

/**
    Bass drive front end — the pedalboard in front of a bass amp, which is a
    different problem from a guitar amp: distorting a bass full-range eats the
    fundamental and the note stops holding the bottom of the track. So every
    serious bass drive (Sansamp, Darkglass, a Big Muff with a blend knob) keeps
    the lows clean and dirties the band above them.

      SPLIT : crossover point. The low band stays clean; only what's above it
              is driven. The high band is derived by SUBTRACTION (high = x - low)
              so the two bands sum back to exactly the input — at BLEND 0 the
              stage is unity, with no crossover phase dip.
      GRIT  : drive into the fuzz curve for the high band, with a touch of
              full-wave rectification for the octave-up snarl of an octave fuzz.
              The driven band is then auto-levelled back to the clean band's
              envelope, so GRIT changes character, not loudness, and BLEND is a
              real blend rather than a volume control. It is also high-passed at
              the crossover afterwards: clipping generates intermodulation well
              below its input band, and without this the fuzz muddies the clean
              lows that SPLIT exists to protect.
      BLEND : how much of the high band arrives driven rather than clean.
      SUB   : an analogue octave divider, built the way an OC-2 actually is.
              A flip-flop toggles once per cycle of a low-passed copy, but the
              octave voice is NOT a synthesised square: the divider only flips
              the polarity of the rectified dry signal, so the sub inherits the
              note's own envelope and harmonics and stays phase-coherent with
              it — which is why the hardware tracks dynamics with no envelope
              follower at all. Monophonic by nature: single notes track, chords
              confuse it, and on a low E the octave lands near 20 Hz and is
              mostly filtered away. All true of the pedal too.

    Runs at the START of the oversampled region (it is pre-preamp), so the fuzz
    and the divider's square edges are band-limited before downsampling.
*/
class BassStage
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (auto* f : { &lowA, &lowB, &subTrack, &subToneA, &subToneB, &subHp, &gritTone, &dirtyHp })
            f->prepare (sampleRate);

        envAtk  = envCoeff (5.0);
        envRel  = envCoeff (80.0);
        subAtk  = envCoeff (8.0);
        subRel  = envCoeff (120.0);

        reset();
        update();
    }

    void reset()
    {
        for (auto* f : { &lowA, &lowB, &subTrack, &subToneA, &subToneB, &subHp, &gritTone, &dirtyHp })
            f->reset();
        env.fill (0.0f);
        dirtyEnv.fill (0.0f);
        subEnv.fill (0.0f);
        flip.fill (1.0f);
        wasPositive.fill (false);
    }

    /** All controls normalised 0..1 (split is a 0..1 position across 90-400 Hz). */
    void setControls (float blend01, float split01, float grit01, float sub01)
    {
        blend01 = juce::jlimit (0.0f, 1.0f, blend01);
        split01 = juce::jlimit (0.0f, 1.0f, split01);
        grit01  = juce::jlimit (0.0f, 1.0f, grit01);
        sub01   = juce::jlimit (0.0f, 1.0f, sub01);

        if (! moved (blend01, blend) && ! moved (split01, split)
            && ! moved (grit01, grit) && ! moved (sub01, sub))
            return;

        blend = blend01; split = split01; grit = grit01; sub = sub01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const bool driveOn = (blend > 0.001f);
        const bool subOn   = (sub > 0.001f);

        if (! driveOn && ! subOn)
            return;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            float& e  = env[(size_t) ch];
            float& se = subEnv[(size_t) ch];
            float& fl = flip[(size_t) ch];
            bool&  wp = wasPositive[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
            {
                const float x = d[i];

                // Perfect-reconstruction split: low is a 4th-order LP, high is
                // whatever is left, so low + high == x.
                const float low  = lowB.processSample (ch, lowA.processSample (ch, x));
                const float high = x - low;

                float y = low;

                if (driveOn)
                {
                    const float rect = std::abs (high);
                    e = (rect > e ? envAtk : envRel) * (e - rect) + rect;

                    float dirty = shape::fuzz (high * gritDrive, octaveAmount);
                    dirty = dirtyHp.processSample (ch, dirty);   // keep fuzz out of the clean lows
                    dirty = gritTone.processSample (ch, dirty);

                    // Auto-level the driven band to the clean band it replaces.
                    const float dr = std::abs (dirty);
                    float& de = dirtyEnv[(size_t) ch];
                    de = (dr > de ? envAtk : envRel) * (de - dr) + dr;
                    const float match = (de > 1.0e-5f) ? juce::jlimit (0.0f, 4.0f, e / de) : 0.0f;

                    y += (1.0f - blend) * high + blend * dirty * match;
                }
                else
                {
                    y += high;
                }

                if (subOn)
                {
                    // Octave divider: flip on rising zero crossings of a
                    // low-passed copy, with a hysteresis band so hum and string
                    // noise can't chatter the flip-flop.
                    const float track = subTrack.processSample (ch, x);
                    const float hyst = 0.02f * juce::jmax (0.05f, e);
                    if (! wp && track > hyst)       { wp = true;  fl = -fl; }
                    else if (wp && track < -hyst)   { wp = false; }

                    const float a = std::abs (track);
                    se = (a > se ? subAtk : subRel) * (se - a) + a;
                    const float gate = juce::jlimit (0.0f, 1.0f, (se - 0.002f) * 200.0f);

                    // Slice, invert, scale: the sub is the rectified dry
                    // signal with alternate cycles flipped, not an oscillator.
                    const float sliced = fl * std::abs (track) * gate;
                    float shaped = subToneB.processSample (ch, subToneA.processSample (ch, sliced));
                    shaped = subHp.processSample (ch, shaped);   // no pointless subsonics
                    y += subLevel * shaped;
                }

                d[i] = y;
            }
        }
    }

private:
    float envCoeff (double ms) const
    {
        return (float) std::exp (-1.0 / (0.001 * ms * sampleRate));
    }

    void update()
    {
        // Crossover: 90 Hz (keep only the deepest fundamentals clean) to 400 Hz
        // (clean lows well up into the note body) — the range bass drives use.
        const double splitHz = juce::jmap ((double) split, 90.0, 400.0);
        lowA.setLowpass (splitHz, 0.54119610);   // 4th-order Butterworth pair
        lowB.setLowpass (splitHz, 1.30656296);

        // Grit: from edge-of-breakup to full fuzz, with octave-up snarl and a
        // level trim so turning it up doesn't just get louder.
        gritDrive    = 1.0f + 45.0f * grit * grit;
        octaveAmount = 0.35f * grit;
        gritTone.setPeak (1800.0, 0.8, 3.0 * grit);   // presence so it cuts through
        dirtyHp.setHighpass (splitHz * 0.8, 0.707);

        // Sub: divider output is a square — round it off so it reads as weight
        // rather than buzz.
        subTrack.setLowpass (180.0, 0.707);
        // The OC-2's octave-down voice leaves the divider through a 3-pole
        // ~325 Hz lowpass at Q~1.58; darkened a little here, since a bass
        // fundamental is already an octave below a guitar's.
        subToneA.setLowpass (250.0, 1.4);
        subToneB.setLowpass (300.0, 0.707);
        subHp.setHighpass (30.0, 0.707);
        subLevel = 1.6f * sub;   // rectified-dry voice is quieter than a square
    }

    double sampleRate = 176400.0;
    float blend = 0.0f, split = 0.5f, grit = 0.0f, sub = 0.0f;

    Biquad lowA, lowB;                    // crossover low band
    Biquad gritTone, dirtyHp;             // driven-band presence + DC block
    Biquad subTrack, subToneA, subToneB, subHp;  // divider tracking + smoothing

    float gritDrive = 1.0f, octaveAmount = 0.0f, subLevel = 0.0f;
    float envAtk = 0.0f, envRel = 0.0f, subAtk = 0.0f, subRel = 0.0f;

    std::array<float, Biquad::kMaxChannels> env {}, dirtyEnv {}, subEnv {}, flip { { 1.0f, 1.0f } };
    std::array<bool,  Biquad::kMaxChannels> wasPositive {};
};

} // namespace amp
