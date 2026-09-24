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
#include "../BiquadHelpers.h"

namespace amp::pedal
{

/** Where a pedal sits relative to the amp. */
enum class Slot { Pre = 0, Post };

/** One pedal's common controls: three knobs (0..10), on, and pre/post. */
struct Controls
{
    bool  on   = false;
    Slot  slot = Slot::Pre;
    float a = 5.0f, b = 5.0f, c = 5.0f, d = 5.0f;   // meaning is per pedal
};

/** Sine LFO with a quadrature output, for stereo modulation. */
struct Lfo
{
    void prepare (double sr) { sampleRate = sr; phase = 0.0f; }
    void setRate (float hz)  { inc = (float) (juce::MathConstants<double>::twoPi * hz / sampleRate); }

    /** Advances and returns the modulator in [-1, 1]; `quad` lags by 90 degrees. */
    float next (float& quad)
    {
        const float s = std::sin (phase);
        quad = std::sin (phase + juce::MathConstants<float>::halfPi);
        phase += inc;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        return s;
    }

    double sampleRate = 44100.0;
    float phase = 0.0f, inc = 0.0f;
};

/** First-order allpass — the phaser's building block. Flat magnitude, and the
    phase rotates through 90 degrees at its corner, so summing it with the dry
    signal makes a notch that moves as the corner sweeps. */
struct AllpassStage
{
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { for (auto& s : state) s = State {}; }

    void setFrequency (float hz)
    {
        const float t = std::tan (juce::MathConstants<float>::pi
                                  * juce::jlimit (20.0f, (float) (sampleRate * 0.45), hz)
                                  / (float) sampleRate);
        coeff = (t - 1.0f) / (t + 1.0f);
    }

    inline float processSample (int ch, float x) noexcept
    {
        auto& s = state[(size_t) ch];
        const float y = coeff * x + s.x1 - coeff * s.y1;
        s.x1 = x; s.y1 = y;
        return y;
    }

    struct State { float x1 = 0.0f, y1 = 0.0f; };
    std::array<State, Biquad::kMaxChannels> state {};
    double sampleRate = 44100.0;
    float coeff = 0.0f;
};

/** Rectified envelope follower with separate attack/release. */
struct EnvFollower
{
    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { value.fill (0.0f); }

    void setTimes (double attackMs, double releaseMs)
    {
        atk = (float) std::exp (-1.0 / (0.001 * attackMs  * sampleRate));
        rel = (float) std::exp (-1.0 / (0.001 * releaseMs * sampleRate));
    }

    inline float process (int ch, float x) noexcept
    {
        const float r = std::abs (x);
        float& v = value[(size_t) ch];
        v = (r > v ? atk : rel) * (v - r) + r;
        return v;
    }

    /** Highest envelope across channels, for linked behaviour. */
    float linked() const
    {
        float m = 0.0f;
        for (auto v : value) m = juce::jmax (m, v);
        return m;
    }

    std::array<float, Biquad::kMaxChannels> value {};
    double sampleRate = 44100.0;
    float atk = 0.0f, rel = 0.0f;
};

/**
    Monophonic period tracker, by autocorrelation.

    Zero-crossing detection is the obvious approach and it does not work on a
    guitar: a plucked low E has enough harmonic energy that the signal crosses
    zero several times per cycle, so the estimate locks onto a harmonic (an
    octave error) or refuses to settle. Autocorrelation on a decimated copy is
    robust to that, and the classic "first peak above a fraction of the best
    peak" rule is what stops it choosing a harmonic over the fundamental.

    Two stages, because each is bad at the other's job: a coarse search on the
    decimated signal finds the right *octave* cheaply, then a narrow search at
    full rate around that answer gets the precision. Grain alignment needs the
    period to well under a percent — a coarse estimate alone leaves the grains
    visibly misaligned and the splices audible.
*/
struct PeriodTracker
{
    void prepare (double sr)
    {
        sampleRate = sr;
        decimation = juce::jmax (1, (int) std::round (sr / 12000.0));
        decRate = sr / decimation;
        minLag = juce::jmax (4, (int) (decRate / 1200.0));      // up to ~1200 Hz
        maxLag = juce::jmin (kBufferSize / 2 - 1, (int) (decRate / 55.0));
        reset();
    }

    void reset()
    {
        buffer.fill (0.0f);
        full.fill (0.0f);
        writePos = 0; fullPos = 0; decCount = 0; decAcc = 0.0f; sinceUpdate = 0;
        period = 0.0f; confidence = 0.0f;
    }

    /** Feed one sample; the analysis itself runs a few times per second. */
    inline void push (float x) noexcept
    {
        full[(size_t) fullPos] = x;
        fullPos = (fullPos + 1) % kFullSize;

        decAcc += x;
        if (++decCount < decimation)
            return;

        buffer[(size_t) writePos] = decAcc / (float) decimation;
        writePos = (writePos + 1) % kBufferSize;
        decCount = 0; decAcc = 0.0f;

        if (++sinceUpdate >= kUpdateEvery)
        {
            sinceUpdate = 0;
            analyse();
        }
    }

    /** Period in samples at the original rate, 0 if not tracking. */
    float periodSamples() const { return period * (float) decimation; }
    bool  locked() const        { return confidence > 0.55f && period > 0.0f; }

private:
    void analyse()
    {
        // Copy the most recent window out of the ring, oldest first.
        std::array<float, kWindow> w {};
        for (int i = 0; i < kWindow; ++i)
            w[(size_t) i] = buffer[(size_t) ((writePos - kWindow + i + kBufferSize * 2) % kBufferSize)];

        double energy = 0.0;
        for (float v : w) energy += (double) v * v;
        if (energy < 1.0e-7)               // silence: keep the last estimate but drop confidence
        {
            confidence *= 0.5f;
            return;
        }

        const int lagMax = juce::jmin (maxLag, kWindow / 2 - 1);
        double best = 0.0; int bestLag = 0;
        std::array<double, 512> corr {};
        for (int lag = minLag; lag <= lagMax; ++lag)
        {
            double num = 0.0, den0 = 0.0, den1 = 0.0;
            for (int i = 0; i + lag < kWindow; ++i)
            {
                const double a = w[(size_t) i], b = w[(size_t) (i + lag)];
                num += a * b; den0 += a * a; den1 += b * b;
            }
            const double c = num / (std::sqrt (den0 * den1) + 1.0e-12);
            corr[(size_t) lag] = c;
            if (c > best) { best = c; bestLag = lag; }
        }

        if (bestLag <= 0 || best < 0.4)
        {
            confidence *= 0.7f;
            return;
        }

        // Prefer the FIRST lag that is nearly as good as the best one: that is
        // the fundamental, where the later peak would be an octave down.
        int chosen = bestLag;
        for (int lag = minLag; lag < bestLag; ++lag)
            if (corr[(size_t) lag] > 0.85 * best)
            { chosen = lag; break; }

        // Parabolic interpolation around the chosen peak for sub-sample accuracy.
        float refined = (float) chosen;
        if (chosen > minLag && chosen < lagMax)
        {
            const double y0 = corr[(size_t) chosen - 1], y1 = corr[(size_t) chosen], y2 = corr[(size_t) chosen + 1];
            const double denom = (y0 - 2.0 * y1 + y2);
            if (std::abs (denom) > 1.0e-9)
                refined += (float) (0.5 * (y0 - y2) / denom);
        }

        const float coarse = refined * (float) decimation;     // back to full rate
        const float precise = refineAtFullRate (coarse);

        period = (period > 0.0f) ? 0.5f * period + 0.5f * (precise / (float) decimation)
                                 : precise / (float) decimation;
        confidence = (float) corr[(size_t) chosen];
    }

    /** Narrow correlation search at the original sample rate, which is where
        the precision the grain splices need actually comes from. */
    float refineAtFullRate (float coarsePeriod) const
    {
        const int centre = (int) std::round (coarsePeriod);
        const int span = juce::jmax (2, (int) std::round (coarsePeriod * 0.03f) + 2);
        const int win = juce::jmin (2048, kFullSize / 2 - centre - span - 2);
        if (centre < 4 || win < 256)
            return coarsePeriod;

        auto at = [this] (int i)
        {
            int k = (fullPos - 1 - i + kFullSize * 2) % kFullSize;
            return (double) full[(size_t) k];
        };

        double best = -2.0; int bestLag = centre;
        std::array<double, 128> local {};
        const int lo = juce::jmax (2, centre - span), hi = centre + span;
        for (int lag = lo; lag <= hi && (lag - lo) < (int) local.size(); ++lag)
        {
            double num = 0, d0 = 0, d1 = 0;
            for (int i = 0; i < win; ++i)
            {
                const double a = at (i), b = at (i + lag);
                num += a * b; d0 += a * a; d1 += b * b;
            }
            const double c = num / (std::sqrt (d0 * d1) + 1.0e-12);
            local[(size_t) (lag - lo)] = c;
            if (c > best) { best = c; bestLag = lag; }
        }

        const int idx = bestLag - lo;
        float out = (float) bestLag;
        if (idx > 0 && idx + 1 < (int) local.size() && bestLag < hi)
        {
            const double y0 = local[(size_t) idx - 1], y1 = local[(size_t) idx], y2 = local[(size_t) idx + 1];
            const double den = y0 - 2.0 * y1 + y2;
            if (std::abs (den) > 1.0e-9)
                out += (float) (0.5 * (y0 - y2) / den);
        }
        return out;
    }

    static constexpr int kFullSize = 16384;    // full-rate ring for refinement
    static constexpr int kBufferSize = 2048;
    static constexpr int kWindow = 1024;
    static constexpr int kUpdateEvery = 256;   // decimated samples between analyses

    std::array<float, kBufferSize> buffer {};
    std::array<float, kFullSize> full {};
    int fullPos = 0;
    double sampleRate = 44100.0, decRate = 12000.0;
    int decimation = 4, writePos = 0, decCount = 0, sinceUpdate = 0;
    int minLag = 10, maxLag = 300;
    float decAcc = 0.0f, period = 0.0f, confidence = 0.0f;
};

/** Fractional delay line with cubic interpolation, sized in seconds. */
struct DelayBuffer
{
    void prepare (double sr, double maxSeconds)
    {
        sampleRate = sr;
        length = (int) std::ceil (maxSeconds * sr) + 4;
        for (auto& b : buf) b.assign ((size_t) length, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
    }

    inline void write (int ch, float x) noexcept { buf[(size_t) ch][(size_t) writePos] = x; }
    inline void advance() noexcept { if (++writePos >= length) writePos = 0; }

    /** Reads `delaySamples` back from the write head (Hermite interpolation). */
    inline float read (int ch, float delaySamples) const noexcept
    {
        delaySamples = juce::jlimit (1.0f, (float) (length - 3), delaySamples);
        const float pos = (float) writePos - delaySamples;
        const int i0 = (int) std::floor (pos);
        const float frac = pos - (float) i0;
        const auto& b = buf[(size_t) ch];
        auto at = [&] (int i) noexcept
        {
            i %= length; if (i < 0) i += length;
            return b[(size_t) i];
        };
        const float xm1 = at (i0 - 1), x0 = at (i0), x1 = at (i0 + 1), x2 = at (i0 + 2);
        const float c0 = x0;
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    std::array<std::vector<float>, Biquad::kMaxChannels> buf;
    double sampleRate = 44100.0;
    int length = 1, writePos = 0;
};

} // namespace amp::pedal
