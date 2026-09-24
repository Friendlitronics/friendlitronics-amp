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
#include "BiquadHelpers.h"
#include "AmpVoicing.h"

namespace amp
{

/**
    Passive guitar tone stack, realised as a voiced cascade of shelving/peaking
    biquads keyed to the amp topology. Bass/Mid/Treble knobs (0..10, noon ≈ 5)
    map onto the characteristic centres and depths of each circuit:

      - Fender FMV  : bright with a scooped midrange (~400 Hz)
      - Marshall FMV: brighter treble, shallower scoop, upper-mid presence
      - Vox TopBoost: treble/bass boosts + a post "Cut" low-pass (the AC30 Cut knob)
      - Champ Tilt  : simple treble/bass tilt (the Champ has only a tone control)
      - Acoustic    : AER-style hi-fi EQ (60-80 Hz bass, broad 700 Hz mid, 8 kHz air)
      - Bass SVT    : Ampeg-style 40 Hz bass, 800 Hz mid, 4 kHz treble
      - Bass Vintage: flip-top Baxandall — 40 Hz / mild 500 Hz / 5 kHz treble
      - Bass Variamp: Acoustic 360 — scoop-depth stack + asymmetric LC mid
      - Bass HiFi   : modern/solid-state — deep 50 Hz, broad 700 Hz, extended top
      - Line Flat   : gentle Baxandall-ish mix EQ for the no-amp colour path

    Runs inside the oversampled region (linear, so harmless there).
*/
class ToneStack
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        low.prepare (sampleRate);
        mid.prepare (sampleRate);
        high.prepare (sampleRate);
        cut.prepare (sampleRate);
        update();
    }

    void reset()
    {
        low.reset(); mid.reset(); high.reset(); cut.reset();
    }

    void setType (ToneStackType t)
    {
        if (t != type) { type = t; update(); }
    }

    /** All knobs on the 0..10 scale. */
    void setControls (float bass, float midC, float treble, float cutC)
    {
        bass10 = bass; mid10 = midC; treble10 = treble; cut10 = cutC;
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
                float x = low.processSample (ch, d[i]);
                x = mid.processSample (ch, x);
                x = high.processSample (ch, x);
                if (cutActive)
                    x = cut.processSample (ch, x);
                d[i] = x;
            }
        }
    }

private:
    static float bipolar (float knob10) { return (knob10 - 5.0f) / 5.0f; } // -1..+1

    void update()
    {
        const float b = bipolar (bass10);
        const float m = bipolar (mid10);
        const float t = bipolar (treble10);

        cutActive = false;

        switch (type)
        {
            case ToneStackType::FenderFMV:
                low.setLowShelf  (100.0,  0.7, b * 12.0);
                mid.setPeak      (400.0,  0.8, m * 12.0 - 3.0); // baked-in scoop
                high.setHighShelf(3000.0, 0.7, t * 12.0);
                break;

            case ToneStackType::MarshallFMV:
                low.setLowShelf  (110.0,  0.7, b * 10.0);
                mid.setPeak      (620.0,  0.9, m * 10.0 - 1.0);
                high.setHighShelf(4500.0, 0.7, t * 12.0);
                break;

            case ToneStackType::VoxTopBoost:
                low.setLowShelf  (120.0,  0.7, b * 9.0);
                mid.setPeak      (1500.0, 0.8, m * 6.0);        // AC30 has present mids
                high.setHighShelf(2000.0, 0.7, t * 10.0);
                // "Cut": progressively roll off the top (10 = most cut).
                cut.setLowpass (juce::jmap (cut10, 0.0f, 10.0f, 9000.0f, 2200.0f), 0.7);
                cutActive = true;
                break;

            case ToneStackType::AcousticHiFi:
                // AER-style: work the extremes, gentle broad mids.
                low.setLowShelf  (80.0,   0.7, b * 10.0);
                mid.setPeak      (700.0,  0.6, m * 6.0);
                high.setHighShelf(8000.0, 0.7, t * 10.0);
                break;

            case ToneStackType::BassSVT:
                // Ampeg's published figures: Bass +/-12 dB @ 40 Hz, Treble
                // +/-12 @ 4 kHz, and a Midrange that is deliberately lopsided
                // (+10 boost, -20 cut) around its selected centre — the cut is
                // the useful direction on an SVT.
                low.setLowShelf  (40.0,   0.7, b * 12.0);
                mid.setPeak      (800.0,  0.8, m >= 0.0f ? m * 10.0 : m * 20.0);
                high.setHighShelf(4000.0, 0.7, t * 12.0);
                break;

            case ToneStackType::BassVintage:
                // B-15N Baxandall: +/-10 dB @ 40 Hz and +/-18 dB @ 5 kHz. The
                // amp has no mid control; the gentle 500 Hz band is ours.
                low.setLowShelf  (40.0,   0.7, b * 10.0);
                mid.setPeak      (500.0,  0.7, m * 5.0);
                high.setHighShelf(5000.0, 0.7, t * 18.0);
                break;

            case ToneStackType::BassVariamp:
                // Acoustic 360. Its passive Bass/Treble pair is a scoop-depth
                // control (flat at minimum, ~+12 dB @ 50 Hz and +19 dB @ 5 kHz
                // wide open), and the Variamp is a series-LC band whose cut is
                // a near-total null while its boost is modest — so the mid band
                // is strongly asymmetric. Centres run 65/100/136/400/800 Hz;
                // 400 Hz is the most musically useful of them for a Mid knob.
                low.setLowShelf  (65.0,   0.7, b * 12.0);
                mid.setPeak      (400.0,  1.0, m >= 0.0f ? m * 6.0 : m * 20.0);
                high.setHighShelf(5000.0, 0.7, t * 14.0);
                break;

            case ToneStackType::BassHiFi:
                low.setLowShelf  (50.0,   0.7, b * 12.0);
                mid.setPeak      (700.0,  0.7, m * 8.0);
                high.setHighShelf(5000.0, 0.7, t * 12.0);
                break;

            case ToneStackType::LineFlat:
                low.setLowShelf  (100.0,   0.7, b * 8.0);
                mid.setPeak      (1000.0,  0.6, m * 6.0);
                high.setHighShelf(10000.0, 0.7, t * 8.0);
                break;

            case ToneStackType::ChampTilt:
            default:
                low.setLowShelf  (120.0,  0.7, b * 8.0);
                mid.setPeak      (800.0,  0.7, m * 4.0);
                high.setHighShelf(3500.0, 0.7, t * 8.0);
                break;
        }
    }

    double sampleRate = 176400.0;
    ToneStackType type = ToneStackType::FenderFMV;
    float bass10 = 5.0f, mid10 = 5.0f, treble10 = 6.0f, cut10 = 3.0f;
    bool cutActive = false;

    Biquad low, mid, high, cut;
};

} // namespace amp
