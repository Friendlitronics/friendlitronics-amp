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
    Whammy / Octave — pitch, by the classic two-tap delay-line method.

    A delay line read back at a rate other than the one it is written at comes
    out transposed, but the read head eventually runs out of buffer and has to
    jump. The old rack harmonisers hid that jump with a splice: one moving tap,
    and a short equal-power crossfade to the next grain only across the wrap.

    Note the trap avoided here. The textbook version runs two taps half a window
    apart *continuously*, which sounds fine until the input is close to a steady
    tone: the two grains are then a fixed fraction of a cycle apart and can land
    exactly antiphase, silencing the note. (A 50 ms half-window offset does
    precisely that to 220 Hz — 5.5 cycles.) Overlapping only during the splice
    confines that comb to a few ms per wrap instead of the whole note.

    The splice is also **pitch-synchronous**: the grain length is snapped to a
    whole number of periods of the incoming note, measured by a zero-crossing
    tracker. That way the two ends of the splice sit at the same point in the
    waveform's cycle and the join is phase-continuous — which is the difference
    between a smooth shift and the periodic clicking that an arbitrary grain
    length produces at 10-20 splices per second. When the input is too noisy or
    polyphonic to have a period (chords, palm mutes), the tracker gives up and
    a fixed grain length is used, which is the classic warble.

    Still not perfect — chords warble and transients smear — but honest, cheap
    and characterful.

      PITCH  : 5 = unison, 0 = one octave down, 10 = one octave up. Automate it
               and you have the treadle sweep.
      MIX    : dry/shifted balance.
      DOUBLE : adds a second voice a few cents off and ~18 ms late — the ADT
               doubling trick, which needs no pitch tracking at all.
*/
class Whammy
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        targetWindow = (float) (0.070 * sampleRate);       // aim for ~70 ms grains
        windowSamples = targetWindow;
        windowPrev = targetWindow;
        line.prepare (sampleRate, 0.4);                    // room for two grains
        doubleLine.prepare (sampleRate, 0.3);
        tracker.prepare (sampleRate);
        reset();
        update();
    }

    void reset()
    {
        line.reset();
        doubleLine.reset();
        tracker.reset();
        phase = 0.0f;
        doublePhase = 0.0f;
        windowSamples = windowPrev = targetWindow;
        smoothedRatio = ratio;
    }

    void setControls (float pitch01, float mix01, float double01)
    {
        if (! moved (pitch01, pitch) && ! moved (mix01, mix) && ! moved (double01, dbl))
            return;
        pitch = pitch01; mix = mix01; dbl = double01;
        update();
    }

    /** Test/diagnostic: has the period tracker locked, and to what period? */
    bool  trackerLocked() const { return locked(); }
    float trackedPeriod() const { return tracker.periodSamples(); }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        const int numCh = (int) juce::jmin ((size_t) Biquad::kMaxChannels, block.getNumChannels());
        const int numSamples = (int) block.getNumSamples();
        const bool doubling = dbl > 0.001f;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                line.write (ch, block.getSample (ch, i));
                if (doubling)
                    doubleLine.write (ch, block.getSample (ch, i));
            }

            tracker.push (block.getSample (0, i));

            // Pitch is smoothed per sample: stepping the ratio once per block
            // makes a swept PITCH knob sound like it is climbing stairs.
            smoothedRatio += (ratio - smoothedRatio) * ratioSmooth;
            phaseInc = (1.0f - smoothedRatio) / windowSamples;

            // One moving tap; the previous grain is only mixed in across the
            // splice, so the two are never summed for long enough to comb.
            // Crossfade law depends on whether the two grains are correlated.
            // Phase-locked, they are the same waveform, so the gains must sum
            // to one (equal GAIN) or the splice bumps +3 dB every time —
            // which is most of what "choppy" was. Unlocked, they are unrelated
            // and equal POWER is the right law.
            const float gNew = spliceGain (phase, phaseInc);
            const float gOld = locked() ? (1.0f - gNew)
                                        : std::sqrt (juce::jmax (0.0f, 1.0f - gNew * gNew));
            const float dNew = 1.0f + phase * windowSamples;
            const float dOld = dNew + windowPrev;   // the grain we are leaving

            float dgNew = 0.0f, dgOld = 0.0f, ddNew = 0.0f, ddOld = 0.0f;
            if (doubling)
            {
                dgNew = spliceGain (doublePhase, doublePhaseInc);
                dgOld = locked() ? (1.0f - dgNew)
                                 : std::sqrt (juce::jmax (0.0f, 1.0f - dgNew * dgNew));
                ddNew = doubleOffset + doublePhase * windowSamples;
                ddOld = ddNew + windowSamples;
            }

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float dry = block.getSample (ch, i);
                float wet = gNew * line.read (ch, dNew) + gOld * line.read (ch, dOld);

                if (doubling)
                {
                    const float dv = dgNew * doubleLine.read (ch, ddNew)
                                   + dgOld * doubleLine.read (ch, ddOld);
                    // The detuned voice sits alongside the dry, not instead of it.
                    wet += dblLevel * dv;
                }

                block.setSample (ch, i, dry * (1.0f - mixAmt) + wet * mixAmt);
            }

            line.advance();
            if (doubling)
                doubleLine.advance();

            phase += phaseInc;
            if (phase >= 1.0f || phase < 0.0f)
            {
                phase -= std::floor (phase);
                // A grain just ended: this is the only safe moment to resize,
                // and the new length is snapped to whole periods of the note.
                windowPrev = windowSamples;
                windowSamples = chooseWindow();
                phaseInc = (1.0f - ratio) / windowSamples;
            }

            doublePhase += doublePhaseInc;
            if (doublePhase >= 1.0f) doublePhase -= 1.0f;
            if (doublePhase < 0.0f)  doublePhase += 1.0f;
        }
    }

private:
    /** True when the period tracker has a stable reading, i.e. the grains are
        phase-aligned and therefore correlated. */
    bool locked() const { return tracker.locked(); }

    /** Weight for the incoming grain: 1 for most of the window, ramping up only
        just after a wrap (either direction of travel). Raised-cosine, so the
        ramp starts and ends smoothly instead of cornering. */
    static float spliceGain (float ph, float inc)
    {
        const float sinceWrap = (inc >= 0.0f) ? ph : 1.0f - ph;
        if (sinceWrap >= kSplice)
            return 1.0f;
        const float u = sinceWrap / kSplice;
        return 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * u));
    }

    /** Grain length: a whole number of periods near the target, so the splice
        joins matching points of the waveform. */
    float chooseWindow() const
    {
        if (! tracker.locked())
            return targetWindow;

        const float p = tracker.periodSamples();
        if (p <= 0.0f)
            return targetWindow;

        const float periods = juce::jmax (2.0f, std::round (targetWindow / p));
        return juce::jlimit (0.02f * (float) sampleRate, 0.18f * (float) sampleRate,
                             periods * p);
    }

    void update()
    {
        // 0 -> half speed (octave down), 0.5 -> unison, 1 -> double (octave up).
        ratio = std::pow (2.0f, (pitch - 0.5f) * 2.0f);
        if (smoothedRatio <= 0.0f)
            smoothedRatio = ratio;
        ratioSmooth = 1.0f - std::exp (-1.0f / (float) (0.02 * sampleRate));   // ~20 ms
        phaseInc = (1.0f - smoothedRatio) / windowSamples;

        // A few cents sharp, ~18 ms behind: classic double-tracking.
        const float doubleRatio = std::pow (2.0f, (0.10f * dbl) / 12.0f);   // up to ~10 cents
        doublePhaseInc = (1.0f - doubleRatio) / windowSamples;
        doubleOffset = (float) (0.018 * sampleRate);
        dblLevel = dbl;

        mixAmt = mix;
    }

    static constexpr float kSplice = 0.12f;   // fraction of the window spent splicing

    double sampleRate = 44100.0;
    DelayBuffer line, doubleLine;
    PeriodTracker tracker;
    float targetWindow = 2048.0f, windowPrev = 2048.0f;
    float smoothedRatio = 0.0f, ratioSmooth = 0.001f;
    float pitch = 0.5f, mix = 0.5f, dbl = 0.0f;
    float ratio = 1.0f, windowSamples = 2048.0f;
    float phase = 0.0f, phaseInc = 0.0f;
    float doublePhase = 0.0f, doublePhaseInc = 0.0f, doubleOffset = 0.0f;
    float dblLevel = 0.0f, mixAmt = 0.5f;
};

} // namespace amp::pedal
