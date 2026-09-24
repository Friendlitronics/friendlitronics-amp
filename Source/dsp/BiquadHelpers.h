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

namespace amp
{

/**
    A tiny, allocation-free RBJ biquad supporting up to two channels of state.

    All `set*` methods compute coefficients from the standard Audio-EQ-Cookbook
    formulas (Robert Bristow-Johnson) using plain float math — they are safe to
    call from the audio thread. Filtering uses Direct Form I.

    This is deliberately self-contained (rather than juce::dsp::IIR) so that
    re-tuning a filter every block — which the amp/cab/mic models do constantly
    as the user turns knobs — never allocates or touches a reference count.
*/
struct Biquad
{
    static constexpr int kMaxChannels = 2;

    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        for (auto& s : state)
            s = ChannelState {};
    }

    void setBypass()
    {
        b0 = 1.0f; b1 = 0.0f; b2 = 0.0f; a1 = 0.0f; a2 = 0.0f;
    }

    inline float processSample (int ch, float x) noexcept
    {
        auto& s = state[(size_t) ch];
        const float y = b0 * x + b1 * s.x1 + b2 * s.x2 - a1 * s.y1 - a2 * s.y2;
        s.x2 = s.x1; s.x1 = x;
        s.y2 = s.y1; s.y1 = y;
        return y;
    }

    // --- coefficient setters (RBJ cookbook) ---

    void setPeak (double freq, double Q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * clampFreq (freq) / sampleRate;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (1.0e-4, Q));

        const double b0d = 1.0 + alpha * A;
        const double b1d = -2.0 * cw;
        const double b2d = 1.0 - alpha * A;
        const double a0d = 1.0 + alpha / A;
        const double a1d = -2.0 * cw;
        const double a2d = 1.0 - alpha / A;
        normalise (b0d, b1d, b2d, a0d, a1d, a2d);
    }

    void setLowShelf (double freq, double Q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * clampFreq (freq) / sampleRate;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * juce::jmax (1.0e-4, Q));
        const double twoSqrtAalpha = 2.0 * std::sqrt (A) * alpha;

        const double b0d =      A * ((A + 1.0) - (A - 1.0) * cw + twoSqrtAalpha);
        const double b1d =  2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
        const double b2d =      A * ((A + 1.0) - (A - 1.0) * cw - twoSqrtAalpha);
        const double a0d =           (A + 1.0) + (A - 1.0) * cw + twoSqrtAalpha;
        const double a1d = -2.0 *    ((A - 1.0) + (A + 1.0) * cw);
        const double a2d =           (A + 1.0) + (A - 1.0) * cw - twoSqrtAalpha;
        normalise (b0d, b1d, b2d, a0d, a1d, a2d);
    }

    void setHighShelf (double freq, double Q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * clampFreq (freq) / sampleRate;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * juce::jmax (1.0e-4, Q));
        const double twoSqrtAalpha = 2.0 * std::sqrt (A) * alpha;

        const double b0d =      A * ((A + 1.0) + (A - 1.0) * cw + twoSqrtAalpha);
        const double b1d = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
        const double b2d =      A * ((A + 1.0) + (A - 1.0) * cw - twoSqrtAalpha);
        const double a0d =           (A + 1.0) - (A - 1.0) * cw + twoSqrtAalpha;
        const double a1d =  2.0 *    ((A - 1.0) - (A + 1.0) * cw);
        const double a2d =           (A + 1.0) - (A - 1.0) * cw - twoSqrtAalpha;
        normalise (b0d, b1d, b2d, a0d, a1d, a2d);
    }

    void setHighpass (double freq, double Q = 0.70710678)
    {
        const double w0 = juce::MathConstants<double>::twoPi * clampFreq (freq) / sampleRate;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (1.0e-4, Q));

        const double b0d =  (1.0 + cw) * 0.5;
        const double b1d = -(1.0 + cw);
        const double b2d =  (1.0 + cw) * 0.5;
        const double a0d =   1.0 + alpha;
        const double a1d =  -2.0 * cw;
        const double a2d =   1.0 - alpha;
        normalise (b0d, b1d, b2d, a0d, a1d, a2d);
    }

    void setLowpass (double freq, double Q = 0.70710678)
    {
        const double w0 = juce::MathConstants<double>::twoPi * clampFreq (freq) / sampleRate;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (1.0e-4, Q));

        const double b0d =  (1.0 - cw) * 0.5;
        const double b1d =   1.0 - cw;
        const double b2d =  (1.0 - cw) * 0.5;
        const double a0d =   1.0 + alpha;
        const double a1d =  -2.0 * cw;
        const double a2d =   1.0 - alpha;
        normalise (b0d, b1d, b2d, a0d, a1d, a2d);
    }

    /** Band-pass with a constant 0 dB peak at `freq` (zero phase at centre), so
        `x - k * bp(x)` is a clean k-deep bell cut and `sum g_i * bp_i(x)` is a
        resonator bank. */
    void setBandpass (double freq, double Q)
    {
        const double w0 = juce::MathConstants<double>::twoPi * clampFreq (freq) / sampleRate;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (1.0e-4, Q));

        const double b0d =  alpha;
        const double b1d =  0.0;
        const double b2d = -alpha;
        const double a0d =  1.0 + alpha;
        const double a1d = -2.0 * cw;
        const double a2d =  1.0 - alpha;
        normalise (b0d, b1d, b2d, a0d, a1d, a2d);
    }

    double sampleRate = 44100.0;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

private:
    struct ChannelState { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    std::array<ChannelState, kMaxChannels> state {};

    double clampFreq (double f) const
    {
        return juce::jlimit (10.0, sampleRate * 0.49, f);
    }

    void normalise (double b0d, double b1d, double b2d,
                    double a0d, double a1d, double a2d)
    {
        const double inv = 1.0 / a0d;
        b0 = (float) (b0d * inv);
        b1 = (float) (b1d * inv);
        b2 = (float) (b2d * inv);
        a1 = (float) (a1d * inv);
        a2 = (float) (a2d * inv);
    }
};

// 4th-order (24 dB/oct) lowpass/highpass = two cascaded Butterworth biquads with
// the standard Linkwitz-style Q pair, handy for steep speaker rolloffs.
struct CascadedLPF
{
    void prepare (double sr) { a.prepare (sr); b.prepare (sr); }
    void reset()             { a.reset();      b.reset(); }

    void set (double freq)
    {
        a.setLowpass (freq, 0.54119610);
        b.setLowpass (freq, 1.30656296);
    }

    inline float processSample (int ch, float x) noexcept
    {
        return b.processSample (ch, a.processSample (ch, x));
    }

    Biquad a, b;
};

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

/** True if a control has moved enough to be worth recomputing coefficients. */
inline bool moved (float a, float b) { return std::abs (a - b) > 1.0e-6f; }

} // namespace amp
