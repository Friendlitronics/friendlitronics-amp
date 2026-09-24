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
#include <vector>
#include <cmath>
#include "BiquadHelpers.h"

namespace amp
{

/**
    Piezo "imaging" front end for the Acoustic amp — the job a Fishman Aura or
    LR Baggs Voiceprint pedal does, done parametrically. An under-saddle piezo
    hears the strings through the saddle rather than the air and wood around
    them, so a DI'd acoustic comes out spiky, nasal and thin ("quack"). This
    stage undoes that before the signal reaches the clean, full-range amp:

      1. DE-QUACK : rumble HP, boxy 400 Hz and nasal 1.2 kHz dips, a 2.7 kHz
                    cut that deepens on pick attacks (a dynamic EQ keyed to
                    transients, so strums lose the spike but keep sparkle),
                    and a brittle 6.5 kHz shelf.
      2. COMP     : stereo-linked soft-knee compressor that evens out the
                    piezo's exaggerated attacks.
      3. BODY     : what the pickup misses — a bank of body resonators (air /
                    Helmholtz ~100 Hz, top and back plates ~200-240 Hz, one
                    higher plate mode) that ring after each note, a warmth bell,
                    and a short cluster of darkened early reflections standing
                    in for a mic hearing the whole soundboard.
      4. COLOUR   : AER-style contour — bass and treble lift with a mid dip.

    All controls 0..1. Host sample rate, before the oversampled core.
*/
class AcousticImager
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        for (auto* f : allFilters())
            f->prepare (sampleRate);

        // Early-reflection line; the right channel's taps are ~7% longer for a
        // little mic-pair width (mono sum stays clean — it's just a denser comb).
        erLen = (int) std::ceil (0.0085 * sampleRate) + 2;
        for (auto& line : erLine)
            line.assign ((size_t) erLen, 0.0f);
        for (int ch = 0; ch < Biquad::kMaxChannels; ++ch)
            for (size_t t = 0; t < kNumTaps; ++t)
            {
                const double spread = (ch == 0) ? 1.0 : 1.07;
                erTap[(size_t) ch][t] = juce::jlimit (1, erLen - 1,
                    (int) std::round (kTapMs[t] * 0.001 * spread * sampleRate));
            }

        // Onset detector: a short mean-|x| (not a peak follower, which would
        // read a noisy band's crest factor as a transient) against a slow
        // programme-level follower.
        quackAvg = envCoeff (3.0);
        slowAtk  = envCoeff (50.0);   slowRel  = envCoeff (150.0);
        // Leveling compressor on a 10 ms RMS detector: it evens out loud vs
        // soft playing without pumping on the piezo's ~1 ms spikes (those
        // are De-Quack's job).
        compRms  = envCoeff (10.0);
        compAtk  = envCoeff (5.0);    compRel  = envCoeff (150.0);

        reset();
        update();
    }

    void reset()
    {
        for (auto* f : allFilters())
            f->reset();
        for (auto& line : erLine)
            std::fill (line.begin(), line.end(), 0.0f);
        erWrite = 0;
        fastEnv.fill (0.0f);
        slowEnv.fill (0.0f);
        meanSquare = 0.0f;
        grDb = 0.0f;
    }

    /** All controls normalised 0..1. */
    void setControls (float body01, float quack01, float comp01, float colour01)
    {
        body01   = juce::jlimit (0.0f, 1.0f, body01);
        quack01  = juce::jlimit (0.0f, 1.0f, quack01);
        comp01   = juce::jlimit (0.0f, 1.0f, comp01);
        colour01 = juce::jlimit (0.0f, 1.0f, colour01);

        if (! moved (body01, body) && ! moved (quack01, quack)
            && ! moved (comp01, comp) && ! moved (colour01, colour))
            return;

        body = body01; quack = quack01; comp = comp01; colour = colour01;
        update();
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const bool compOn = comp > 0.001f;
        std::array<float, Biquad::kMaxChannels> x {};

        for (int i = 0; i < numSamples; ++i)
        {
            // --- 1. de-quack (per channel) ---
            float power = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                float s = block.getSample (ch, i);
                s = rumbleHp.processSample (ch, s);
                s = boxCut.processSample (ch, s);
                s = nasalCut.processSample (ch, s);

                // Dynamic 2.7 kHz cut: transient = short-term band level
                // running ahead of the programme level. It's a ratio, so it
                // tracks pick attacks whatever the DI level is.
                const float band = quackBand.processSample (ch, s);
                const float a = std::abs (band);
                float& f = fastEnv[(size_t) ch];
                float& sl = slowEnv[(size_t) ch];
                f  = quackAvg * (f - a) + a;
                sl = (f > sl ? slowAtk : slowRel) * (sl - f) + f;
                const float trans = juce::jlimit (0.0f, 1.0f,
                    (f + 1.0e-6f) / (sl + 1.0e-5f) - 1.4f);
                s -= juce::jmin (0.8f, quackStaticK + quackDynK * trans) * band;

                s = brittle.processSample (ch, s);
                x[(size_t) ch] = s;
                power = juce::jmax (power, s * s);
            }

            // --- 2. linked compressor ---
            float g = 1.0f;
            if (compOn)
            {
                meanSquare = compRms * (meanSquare - power) + power;
                const float levelDb = 10.0f * std::log10 (meanSquare + 1.0e-12f);
                const float target = gainComputerDb (levelDb);
                grDb = (target < grDb ? compAtk : compRel) * (grDb - target) + target;
                g = dbToGain (grDb + makeupDb);
            }

            // --- 3. body + 4. colour ---
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float s = x[(size_t) ch] * g;

                float res = 0.0f;
                for (size_t m = 0; m < kNumModes; ++m)
                    res += kModes[m].gain * modes[m].processSample (ch, s);

                auto& line = erLine[(size_t) ch];
                line[(size_t) erWrite] = erLowpass.processSample (ch, s);
                float er = 0.0f;
                for (size_t t = 0; t < kNumTaps; ++t)
                {
                    int rp = erWrite - erTap[(size_t) ch][t];
                    if (rp < 0) rp += erLen;
                    er += kTapGain[t] * line[(size_t) rp];
                }

                float y = s + body * (1.2f * res + 0.7f * er);
                y = warmth.processSample (ch, y);
                y = colLow.processSample (ch, y);
                y = colMid.processSample (ch, y);
                y = colHigh.processSample (ch, y);
                block.setSample (ch, i, y);
            }

            if (++erWrite >= erLen)
                erWrite = 0;
        }
    }

private:
    struct Mode { float hz, q, gain; };

    // Dreadnought-ish body: air (Helmholtz) resonance, the coupled top/back
    // plate pair ~100 Hz above it, and one higher top mode.
    static constexpr size_t kNumModes = 4;
    static constexpr Mode kModes[kNumModes] {
        {  98.0f, 7.0f, 0.90f },
        { 196.0f, 6.0f, 0.60f },
        { 235.0f, 6.0f, 0.40f },
        { 520.0f, 5.0f, 0.18f },
    };

    // Irregular, sign-alternating taps so the reflections read as "wood and
    // air" rather than a single flanger-like comb.
    static constexpr size_t kNumTaps = 6;
    static constexpr double kTapMs[kNumTaps]   { 0.63, 1.37, 2.11, 3.07, 4.53, 6.29 };
    static constexpr float  kTapGain[kNumTaps] { 0.22f, -0.18f, 0.15f, -0.12f, 0.10f, -0.08f };

    float envCoeff (double ms) const
    {
        return (float) std::exp (-1.0 / (0.001 * ms * sampleRate));
    }

    // Soft-knee downward compression, in dB (<= 0).
    float gainComputerDb (float levelDb) const
    {
        const float over = levelDb - thresholdDb;
        const float slope = 1.0f / ratio - 1.0f;
        if (2.0f * over < -kKneeDb)
            return 0.0f;
        if (2.0f * std::abs (over) <= kKneeDb)
        {
            const float t = over + 0.5f * kKneeDb;
            return slope * t * t / (2.0f * kKneeDb);
        }
        return slope * over;
    }

    void update()
    {
        // 1. De-quack.
        rumbleHp.setHighpass (55.0, 0.707);   // below drop-D's 73 Hz
        boxCut.setPeak       (400.0,  1.5, -3.0 * quack);
        nasalCut.setPeak     (1200.0, 1.4, -2.0 * quack);
        quackBand.setBandpass(2700.0, 1.8);
        brittle.setHighShelf (6500.0, 0.707, -4.0 * quack);
        quackStaticK = 1.0f - dbToGain (-5.0f * quack);   // up to -5 dB always
        quackDynK    = 0.45f * quack;                      // deeper on attacks (<= -14 dB)

        // 2. Compressor: one knob lowers the threshold and raises the ratio.
        thresholdDb = -12.0f - 26.0f * comp;   // RMS dBFS
        ratio       = 1.0f + 3.0f * comp;
        makeupDb    = 0.25f * -thresholdDb * (1.0f - 1.0f / ratio);

        // 3. Body.
        for (size_t m = 0; m < kNumModes; ++m)
            modes[m].setBandpass ((double) kModes[m].hz, (double) kModes[m].q);
        erLowpass.setLowpass (4500.0, 0.707);
        warmth.setPeak (150.0, 1.2, 2.5 * body);

        // 4. Colour.
        colLow.setLowShelf  (120.0,  0.707,  4.5 * colour);
        colMid.setPeak      (800.0,  0.7,   -5.0 * colour);
        colHigh.setHighShelf(9000.0, 0.707,  4.0 * colour);
    }

    std::array<Biquad*, 14> allFilters()
    {
        return { &rumbleHp, &boxCut, &nasalCut, &quackBand, &brittle,
                 &modes[0], &modes[1], &modes[2], &modes[3],
                 &erLowpass, &warmth, &colLow, &colMid, &colHigh };
    }

    static constexpr float kKneeDb = 8.0f;

    double sampleRate = 44100.0;
    float body = 0.0f, quack = 0.0f, comp = 0.0f, colour = 0.0f;

    Biquad rumbleHp, boxCut, nasalCut, quackBand, brittle;
    std::array<Biquad, kNumModes> modes;
    Biquad erLowpass, warmth, colLow, colMid, colHigh;

    float quackStaticK = 0.0f, quackDynK = 0.0f;
    float quackAvg = 0.0f, slowAtk = 0.0f, slowRel = 0.0f;
    std::array<float, Biquad::kMaxChannels> fastEnv {}, slowEnv {};

    float thresholdDb = -8.0f, ratio = 1.0f, makeupDb = 0.0f;
    float compRms = 0.0f, compAtk = 0.0f, compRel = 0.0f;
    float meanSquare = 0.0f, grDb = 0.0f;

    std::array<std::vector<float>, Biquad::kMaxChannels> erLine;
    std::array<std::array<int, kNumTaps>, Biquad::kMaxChannels> erTap {};
    int erLen = 1, erWrite = 0;
};

} // namespace amp
