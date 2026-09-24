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

namespace amp
{

/**
    "Recorded to tape" colour for the end of the chain — the other half of why
    old bass tones sound the way they do: a Motown or Stax bass was not just an
    amp, it was an amp committed to tape.

    Adapted from the sibling `tape_emulator` project, keeping the principles
    that make a tape model sound like tape rather than a static waveshaper:

      1. **Derivative-normalised saturation.** y = tanh(k(x + bias))/k, so the
         curve is unity at low level and *compresses* as it is pushed, instead
         of adding a hidden makeup gain the way tanh(kx)/tanh(k) does.
      2. **Pre/de-emphasis around the shaper.** Highs are lifted going in and
         cut by the exact algebraic inverse coming out, so high frequencies
         saturate harder than lows (as on real tape) while the linear response
         stays flat.
      3. **Programme-dependent drive.** A slow envelope applies a few dB of
         breathing compression, so the effect grows with how hard you play.
      4. **Head bump as a resonance, not a shelf** — a bell at 70 Hz, which is
         exactly the lift that flatters a bass.
      5. **Gap-loss HF rolloff** — a gentle shelf from 4 kHz plus a low-pass,
         rather than a brickwall.

    TAPE drives 1-5 together. FLUTTER is transport instability: summed
    incommensurate wow sines plus low-passed noise and a ~8 Hz scrape, read from
    a fractional delay line. The modulation is shared by both channels (one
    transport, so the wobble is coherent and mono-safe).

    NOTE: with FLUTTER above zero the delay line adds ~1.5 ms that is not
    reported to the host, so use Mix at 100% when fluttering.

    Host sample rate, last in the chain.
*/
class TapeColour
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        headBump.prepare (sampleRate);
        lossShelf.prepare (sampleRate);
        lossLp.prepare (sampleRate);

        // Pre-emphasis one-pole corner ~4 kHz, and the exact inverse pair.
        const float corner = 4000.0f;
        emphC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * corner / (float) sampleRate);
        oneMinusC = 1.0f - emphC;
        invA = kEmphasisK + emphC - kEmphasisK * emphC;
        invB = -kEmphasisK * oneMinusC;

        envAtk = (float) std::exp (-1.0 / (0.005 * sampleRate));   // 5 ms
        envRel = (float) std::exp (-1.0 / (0.120 * sampleRate));   // 120 ms

        centreSamples = (float) (kCentreDelayMs * 0.001 * sampleRate);
        juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };
        for (auto& dl : delays)
        {
            dl.setMaximumDelayInSamples ((int) std::ceil ((kCentreDelayMs + kMaxDepthMs) * 0.001 * sampleRate) + 4);
            dl.prepare (monoSpec);
        }

        const float twoPiOverSr = juce::MathConstants<float>::twoPi / (float) sampleRate;
        wowInc1  = twoPiOverSr * 0.6f;
        wowInc2  = twoPiOverSr * 1.1f;
        driftInc = twoPiOverSr * 0.13f;
        scrapeInc = twoPiOverSr * 8.0f;
        flutterLpCoeff = std::exp (-juce::MathConstants<float>::twoPi * 18.0f / (float) sampleRate);

        reset();
        update();
    }

    void reset()
    {
        headBump.reset(); lossShelf.reset(); lossLp.reset();
        for (auto& dl : delays) dl.reset();
        for (auto& c : chans) c = ChannelState {};
        wowPhase1 = wowPhase2 = driftPhase = scrapePhase = 0.0f;
        flutterLpState = 0.0f;
    }

    /** Both controls normalised 0..1. */
    void setControls (float tape01, float flutter01)
    {
        tape01    = juce::jlimit (0.0f, 1.0f, tape01);
        flutter01 = juce::jlimit (0.0f, 1.0f, flutter01);
        if (! moved (tape01, tape) && ! moved (flutter01, flutter))
            return;
        tape = tape01; flutter = flutter01;
        update();
    }

    bool isActive() const { return tape > 0.001f || flutter > 0.001f; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        if (! isActive())
            return;

        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const bool saturate = tape > 0.001f;
        const bool wobble   = flutter > 0.001f;
        const auto twoPi = juce::MathConstants<float>::twoPi;

        for (int i = 0; i < numSamples; ++i)
        {
            // --- transport instability (one modulator for both channels) ---
            float delaySamples = centreSamples;
            if (wobble)
            {
                const float wowMix = (0.6f * std::sin (wowPhase1)
                                    + 0.4f * std::sin (wowPhase2)
                                    + 0.25f * std::sin (driftPhase)) / 1.25f;
                wowPhase1  += wowInc1;   if (wowPhase1  >= twoPi) wowPhase1  -= twoPi;
                wowPhase2  += wowInc2;   if (wowPhase2  >= twoPi) wowPhase2  -= twoPi;
                driftPhase += driftInc;  if (driftPhase >= twoPi) driftPhase -= twoPi;

                const float white = random.nextFloat() * 2.0f - 1.0f;
                flutterLpState = flutterLpCoeff * flutterLpState + (1.0f - flutterLpCoeff) * white;
                const float scrape = 0.3f * std::sin (scrapePhase);
                scrapePhase += scrapeInc; if (scrapePhase >= twoPi) scrapePhase -= twoPi;

                delaySamples += wowMix * wowDepthSamples
                              + (flutterLpState + scrape) * flutterDepthSamples;
                delaySamples = juce::jlimit (1.0f, centreSamples + maxDepthSamples, delaySamples);
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto& s = chans[(size_t) ch];
                float x = block.getSample (ch, i);

                if (wobble)
                {
                    delays[(size_t) ch].pushSample (0, x);
                    x = delays[(size_t) ch].popSample (0, delaySamples);
                }

                if (saturate)
                {
                    // pre-emphasis (lift highs into the shaper)
                    s.emphLp += emphC * (x - s.emphLp);
                    const float pre = kEmphasisK * x + (1.0f - kEmphasisK) * s.emphLp;

                    // programme-dependent drive
                    const float rect = std::abs (pre);
                    s.env = (rect > s.env ? envAtk : envRel) * (s.env - rect) + rect;
                    const float compGain = 1.0f / (1.0f + maxComp * s.env);

                    // derivative-normalised, slightly asymmetric shaper
                    const float shaped = std::tanh (curvature * (pre * compGain + bias)) / curvature - dcOffset;

                    // de-emphasis (exact inverse of the pre-emphasis)
                    const float de = (shaped - oneMinusC * s.dePrev) / invA - (invB / invA) * s.deOut;
                    s.dePrev = shaped;
                    s.deOut  = de;
                    x = de;
                }

                x = headBump.processSample (ch, x);
                x = lossShelf.processSample (ch, x);
                x = lossLp.processSample (ch, x);
                block.setSample (ch, i, x);
            }
        }
    }

private:
    struct ChannelState { float emphLp = 0, dePrev = 0, deOut = 0, env = 0; };

    void update()
    {
        // Saturation: curvature and breathing both scale with the knob, so at 0
        // the stage is a straight wire.
        curvature = 1.0f + 2.2f * tape;
        bias      = 0.12f * tape;                                  // a little asymmetry
        dcOffset  = std::tanh (curvature * bias) / curvature;
        maxComp   = 0.6f * tape;

        headBump.setPeak  (70.0, 1.1, 4.0 * tape);                 // resonance, not a shelf
        lossShelf.setHighShelf (4000.0, 0.5, -3.5 * tape);
        lossLp.setLowpass (juce::jmap ((double) tape, 20000.0, 9000.0), 0.707);

        const float msToSamp = (float) (0.001 * sampleRate);
        wowDepthSamples     = flutter * 1.2f * msToSamp;
        flutterDepthSamples = flutter * 0.35f * msToSamp;
        maxDepthSamples     = kMaxDepthMs * msToSamp;
    }

    static constexpr float kEmphasisK     = 2.0f;
    static constexpr float kCentreDelayMs = 1.5f;
    static constexpr float kMaxDepthMs    = 2.0f;

    double sampleRate = 44100.0;
    float tape = 0.0f, flutter = 0.0f;

    // saturation
    float emphC = 0.0f, oneMinusC = 0.0f, invA = 1.0f, invB = 0.0f;
    float curvature = 1.0f, bias = 0.0f, dcOffset = 0.0f, maxComp = 0.0f;
    float envAtk = 0.0f, envRel = 0.0f;
    std::array<ChannelState, Biquad::kMaxChannels> chans {};

    // EQ
    Biquad headBump, lossShelf, lossLp;

    // transport instability
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>,
               Biquad::kMaxChannels> delays;
    float centreSamples = 0.0f, maxDepthSamples = 0.0f;
    float wowDepthSamples = 0.0f, flutterDepthSamples = 0.0f;
    float wowInc1 = 0, wowInc2 = 0, driftInc = 0, scrapeInc = 0;
    float wowPhase1 = 0, wowPhase2 = 0, driftPhase = 0, scrapePhase = 0;
    float flutterLpCoeff = 0.0f, flutterLpState = 0.0f;
    juce::Random random { 0x3A91C7D5 };
};

} // namespace amp
