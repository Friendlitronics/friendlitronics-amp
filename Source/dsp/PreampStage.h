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
#include "Waveshapers.h"
#include "AmpVoicing.h"

namespace amp
{

/**
    Cascaded tube preamp. N gain cells, each:

        drive -> asymmetric waveshape (anti-aliased) -> inter-stage high-pass
              -> Miller low-pass

    The high-pass blocks the DC the asymmetry introduces and tightens the low
    end so high gain stays defined. The **low-pass matters just as much and is
    easy to forget**: a real triode stage has the grid-to-plate Miller
    capacitance rolling its top off, so each stage in a cascade feeds the next
    one a darker signal. Without it every stage generates harmonics from
    full-bandwidth input, the harmonic order climbs stage over stage, and the
    result is the thin fizzy top that makes a sim sound like an ice pick rather
    than an amp. Later stages roll off progressively further, as they do in the
    real circuit.

    The waveshaper itself runs through first-order ADAA, which cuts the
    foldover that survives oversampling.

    Runs INSIDE the oversampled region, so prepare() receives the oversampled
    ProcessSpec.
*/
class PreampStage
{
public:
    static constexpr int kMaxStages = 4;

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (auto& f : stageHp)
            f.prepare (sampleRate);
        for (auto& f : stageLp)
            f.prepare (sampleRate);
        applyVoicing (voicing);
        setGain (gain01);
    }

    void reset()
    {
        for (auto& f : stageHp)
            f.reset();
        for (auto& f : stageLp)
            f.reset();
        for (auto& a : adaa)
            a.reset();
    }

    void setVoicing (const AmpVoicing& v)
    {
        voicing = v;
        applyVoicing (v);
        setGain (gain01);
    }

    /** Sweetness macro: scales the even-harmonic asymmetry of every stage. */
    void setAsymScale (float s) { asymScale = juce::jmax (0.0f, s); }

    /** @param g normalised gain knob position, 0..1. */
    void setGain (float g)
    {
        gain01 = juce::jlimit (0.0f, 1.0f, g);

        // Total preamp drive in dB, scaled by the model's base drive.
        const float driveDb = juce::jmap (gain01, 0.0f, 1.0f, 0.0f, voicing.gainRangeDb);
        const float totalDrive = voicing.preampDrive * dbToGain (driveDb);

        const int n = juce::jlimit (1, kMaxStages, voicing.preampStages);
        stageDrive = std::pow (totalDrive, 1.0f / (float) n);

        // Keep loudness in check as gain climbs (clipped output approaches the
        // rails), without squashing clean settings.
        outComp = 0.6f / (1.0f + 0.12f * (totalDrive - 1.0f));
    }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const int n = juce::jlimit (1, kMaxStages, voicing.preampStages);
        const float asym = voicing.stageAsym * asymScale;
        const bool diode = voicing.diodeLastStage;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = block.getChannelPointer ((size_t) ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = d[i];
                for (int s = 0; s < n; ++s)
                {
                    x *= stageDrive;

                    if (diode && s == n - 1)
                        x = adaa[(size_t) s].process (ch, x,
                                [asym] (double v) { return shape::hardAsym (v, (double) asym); },
                                [asym] (double v) { return shape::hardAsymAD (v, (double) asym); });
                    else
                        x = adaa[(size_t) s].process (ch, x,
                                [asym] (double v) { return shape::tubeAsym (v, (double) asym); },
                                [asym] (double v) { return shape::tubeAsymAD (v, (double) asym); });

                    x = stageHp[(size_t) s].processSample (ch, x);
                    x = stageLp[(size_t) s].processSample (ch, x);
                }
                d[i] = x * outComp;
            }
        }
    }

private:
    void applyVoicing (const AmpVoicing& v)
    {
        for (auto& f : stageHp)
            f.setHighpass (v.interStageHpHz, 0.707);

        // Each successive stage sees a darker signal, as it would through the
        // real coupling network.
        float hz = v.stageLpHz;
        for (auto& f : stageLp)
        {
            f.setLowpass ((double) juce::jmax (2000.0f, hz), 0.707);
            hz *= 0.78f;
        }
    }

    double sampleRate = 176400.0;
    AmpVoicing voicing;
    std::array<Biquad, kMaxStages> stageHp, stageLp;
    std::array<shape::Adaa1, kMaxStages> adaa;

    float gain01     = 0.4f;
    float stageDrive = 1.0f;
    float outComp    = 0.6f;
    float asymScale  = 1.0f;
};

} // namespace amp
