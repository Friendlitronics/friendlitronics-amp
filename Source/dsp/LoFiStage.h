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
#include <cmath>
#include "BiquadHelpers.h"

namespace amp
{

/**
    Digital degradation, for using the plugin as a colour/lofi box rather than
    an amp. Deliberately crude in the places where crudeness is the sound:

      CRUSH  : bit-depth quantisation, 16 bits down to 4. No dither — the
               quantisation noise correlating with the signal is the effect.
      SR     : sample-rate reduction by zero-order hold, host rate down to
               ~3 kHz. The aliasing is the point; the counter is shared by both
               channels so they stay time-aligned.
      NOISE  : tape hiss (low-passed white) plus sparse vinyl crackle (random
               impulses through a resonant band-pass). Constant, not gated, so
               it sits under the programme like the real thing.
      NARROW : 0 leaves the stereo image alone, 10 collapses it to mono, via
               mid/side. Old and cheap media were narrow.

    Placed before TapeColour, so the tape's HF loss smooths the crush aliasing
    and shapes the hiss — the same order as a real lofi chain (bad converter,
    then bad tape). Every control at 0 is a bit-exact bypass.

    Host sample rate.
*/
class LoFiStage
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        hissLp.prepare (sampleRate);
        crackleBp.prepare (sampleRate);
        reset();
        update();
    }

    void reset()
    {
        hissLp.reset();
        crackleBp.reset();
        held.fill (0.0f);
        holdPhase = 0.0f;
    }

    /** All controls normalised 0..1. */
    void setControls (float crush01, float srate01, float noise01, float narrow01)
    {
        crush01  = juce::jlimit (0.0f, 1.0f, crush01);
        srate01  = juce::jlimit (0.0f, 1.0f, srate01);
        noise01  = juce::jlimit (0.0f, 1.0f, noise01);
        narrow01 = juce::jlimit (0.0f, 1.0f, narrow01);

        if (! moved (crush01, crush) && ! moved (srate01, srate)
            && ! moved (noise01, noise) && ! moved (narrow01, narrow))
            return;

        crush = crush01; srate = srate01; noise = noise01; narrow = narrow01;
        update();
    }

    bool isActive() const
    {
        return crush > 0.001f || srate > 0.001f || noise > 0.001f || narrow > 0.001f;
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! isActive())
            return;

        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const bool crushOn  = crush  > 0.001f;
        const bool holdOn   = srate  > 0.001f;
        const bool noiseOn  = noise  > 0.001f;
        const bool narrowOn = narrow > 0.001f && numCh >= 2;

        for (int i = 0; i < numSamples; ++i)
        {
            // --- sample-rate reduction (one counter for all channels) ---
            bool take = true;
            if (holdOn)
            {
                holdPhase += holdInc;
                if (holdPhase >= 1.0f) holdPhase -= 1.0f;
                else                   take = false;
            }

            std::array<float, Biquad::kMaxChannels> s {};
            for (int ch = 0; ch < numCh; ++ch)
            {
                float x = block.getSample (ch, i);
                if (holdOn)
                {
                    if (take) held[(size_t) ch] = x;
                    x = held[(size_t) ch];
                }
                if (crushOn)
                    x = std::round (x / step) * step;
                s[(size_t) ch] = x;
            }

            if (narrowOn)
            {
                const float mid  = 0.5f * (s[0] + s[1]);
                const float side = 0.5f * (s[0] - s[1]) * (1.0f - narrow);
                s[0] = mid + side;
                s[1] = mid - side;
            }

            if (noiseOn)
            {
                const float white = random.nextFloat() * 2.0f - 1.0f;
                float n = hissLevel * hissLp.processSample (0, white);

                // Sparse clicks: an impulse into a resonant band-pass rings
                // briefly, which is what a scratch sounds like.
                const float imp = (random.nextFloat() < crackleProb)
                                      ? (random.nextFloat() * 2.0f - 1.0f) * crackleLevel
                                      : 0.0f;
                n += crackleBp.processSample (0, imp);

                for (int ch = 0; ch < numCh; ++ch)
                    s[(size_t) ch] += n;
            }

            for (int ch = 0; ch < numCh; ++ch)
                block.setSample (ch, i, s[(size_t) ch]);
        }
    }

private:
    void update()
    {
        // 16 bits (transparent) down to 4 (obvious). Signal is nominally -1..1.
        const float bits = juce::jmap (crush, 16.0f, 4.0f);
        step = 2.0f / (std::pow (2.0f, bits) - 1.0f);

        // Host rate down to ~3 kHz.
        const double holdHz = juce::jmap ((double) srate, sampleRate, 3000.0);
        holdInc = (float) juce::jlimit (0.0, 1.0, holdHz / sampleRate);

        hissLp.setLowpass (8000.0, 0.707);
        hissLevel = 0.004f * noise;                     // ~-48 dBFS at full
        crackleBp.setBandpass (1800.0, 1.2);
        crackleProb = 0.0004f * noise;                  // clicks per sample
        crackleLevel = 0.5f * noise;
    }

    double sampleRate = 44100.0;
    float crush = 0.0f, srate = 0.0f, noise = 0.0f, narrow = 0.0f;

    float step = 1.0f / 32768.0f;
    float holdInc = 1.0f, holdPhase = 0.0f;
    std::array<float, Biquad::kMaxChannels> held {};

    Biquad hissLp, crackleBp;
    float hissLevel = 0.0f, crackleProb = 0.0f, crackleLevel = 0.0f;
    juce::Random random { 0x6D2F81AB };
};

} // namespace amp
