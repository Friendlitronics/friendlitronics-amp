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
#include "AmpVoicing.h"

namespace amp
{

/**
    Output stage. Models the things that give an amp its "feel":

      - Drive into a push-pull (class-AB) or single-ended (class-A) nonlinearity.
      - Power-supply SAG: an envelope follower drops the effective drive under
        load, so hard playing compresses and "blooms". Class-A/SE amps are set
        to sag more than stiff class-AB designs (per the voicing).
      - The OUTPUT TRANSFORMER's bandwidth: a real one does not pass 20 kHz.
        Rolling the top off after the clip stage is what stops power-stage
        distortion from reading as fizz.
      - PRESENCE: a negative-feedback-style HF shelf.
      - RESONANCE/DEPTH: a low-frequency bump from output-transformer/speaker
        interaction.

    Runs inside the oversampled region.
*/
class PowerAmp
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        presence.prepare (sampleRate);
        resonance.prepare (sampleRate);
        transformer.prepare (sampleRate);
        applyVoicing (voicing);
    }

    void reset()
    {
        presence.reset();
        resonance.reset();
        transformer.reset();
        adaa.reset();
        for (auto& e : env) e = 0.0f;
    }

    void setVoicing (const AmpVoicing& v)
    {
        voicing = v;
        applyVoicing (v);
    }

    void setMaster    (float m) { master01    = juce::jlimit (0.0f, 1.0f, m); updateDrive(); }
    void setPresence  (float p) { presence01  = juce::jlimit (0.0f, 1.0f, p); updateShelves(); }
    void setResonance (float r) { resonance01 = juce::jlimit (0.0f, 1.0f, r); updateShelves(); }

    /** Sweetness macro: scales the class-A "bloom" (supply sag depth). */
    void setSagScale  (float s) { sagScale    = juce::jmax (0.0f, s); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const bool se = voicing.singleEnded;
        const float sag = voicing.sagAmount * sagScale;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            float e = env[(size_t) ch];

            for (int i = 0; i < numSamples; ++i)
            {
                float x = d[i] * drive;

                // Sag: track the driven signal envelope, fold back the gain.
                const float rect = std::abs (x);
                const float coeff = (rect > e) ? envAtk : envRel;
                e = coeff * e + (1.0f - coeff) * rect;
                const float sagGain = 1.0f / (1.0f + sag * e * 1.5f);
                x *= sagGain;

                x = se ? adaa.process (ch, x, shape::singleEnded<double>, shape::singleEndedAD<double>)
                       : adaa.process (ch, x, shape::pushPull<double>,    shape::pushPullAD<double>);
                x *= outComp;

                // Tone shaping of the output section.
                x = resonance.processSample (ch, x);
                x = presence.processSample (ch, x);
                x = transformer.processSample (ch, x);
                d[i] = x;
            }

            env[(size_t) ch] = e;
        }
    }

private:
    void applyVoicing (const AmpVoicing& v)
    {
        envAtk = std::exp (-1.0f / (float) (0.001 * v.sagAttackMs  * sampleRate));
        envRel = std::exp (-1.0f / (float) (0.001 * v.sagReleaseMs * sampleRate));
        transformer.setLowpass ((double) v.outputLpHz, 0.707);
        updateDrive();
        updateShelves();
    }

    void updateDrive()
    {
        drive   = voicing.powerDrive * juce::jmap (master01, 0.0f, 1.0f, 0.7f, 4.0f);
        outComp = 0.7f / (1.0f + 0.10f * (drive - 1.0f));
    }

    void updateShelves()
    {
        presence.setHighShelf (3000.0, 0.7, presence01  * voicing.presenceMaxDb);
        resonance.setPeak ((double) voicing.resonanceHz, 1.0, resonance01 * voicing.resonanceMaxDb);
    }

    double sampleRate = 176400.0;
    AmpVoicing voicing;

    float master01 = 0.5f, presence01 = 0.5f, resonance01 = 0.4f;
    float sagScale = 1.0f;
    float drive = 1.0f, outComp = 0.7f;
    float envAtk = 0.0f, envRel = 0.0f;
    std::array<float, Biquad::kMaxChannels> env { { 0.0f, 0.0f } };

    Biquad presence, resonance, transformer;
    shape::Adaa1 adaa;
};

} // namespace amp
