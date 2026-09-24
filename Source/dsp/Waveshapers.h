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

#include <cmath>
#include <array>
#include <JuceHeader.h>

namespace amp::shape
{

/**
    Static nonlinear shaping functions for tube/diode stages, plus the
    antiderivatives that let them run anti-aliased.

    Two rules everything here follows:

    1. **Monotonic.** A transfer curve whose slope goes negative folds the
       waveform back on itself, which sounds like ring modulation and aliases
       ferociously. Every curve below is monotonic for all input.
    2. **Closed-form antiderivative.** With F(x) available, a stage can use
       first-order antiderivative anti-aliasing (ADAA):

           y[n] = (F(x[n]) - F(x[n-1])) / (x[n] - x[n-1])

       which is the curve's average over the segment between successive samples
       instead of a point sample of it. That is a large reduction in foldover
       for a couple of extra operations - much cheaper than buying the same
       improvement with raw oversampling.

    Each curve is normalised so the small-signal slope at the origin is ~1, so
    upstream "drive" controls how hard the curve is pushed with no hidden level
    jump.
*/

/** log(cosh(x)), guarded against overflow (it grows like |x| - log 2).

    Templated on the scalar type for a reason that matters: ADAA divides the
    difference of two nearly-equal antiderivative values by a very small dx, and
    in float that cancellation leaves ~0.003 of absolute error right at every
    waveform peak — audible as popping static. In double it is ~1e-10. */
template <typename T>
inline T logCosh (T x) noexcept
{
    const T a = std::abs (x);
    return a > (T) 10 ? a - (T) 0.6931471805599453 : std::log (std::cosh (a));
}

//==============================================================================
// Symmetric soft clip - the workhorse triode/clean-ish curve.
template <typename T> inline T softClip (T x) noexcept   { return std::tanh (x); }
template <typename T> inline T softClipAD (T x) noexcept { return logCosh (x); }

/**
    Asymmetric tube-like curve. The negative half is squashed harder, which
    generates the even-order harmonics that make a single tube stage sound warm
    rather than fizzy. `asym` in [0, 1): 0 = symmetric tanh.
*/
template <typename T>
inline T tubeAsym (T x, T asym) noexcept
{
    const T kNeg = (T) 1 + (T) 2 * asym;
    return (x >= (T) 0) ? std::tanh (x) : std::tanh (kNeg * x) / kNeg;
}

/** Antiderivative of tubeAsym. Both branches are zero at the origin, so it is
    continuous there - which ADAA requires. */
template <typename T>
inline T tubeAsymAD (T x, T asym) noexcept
{
    const T kNeg = (T) 1 + (T) 2 * asym;
    return (x >= (T) 0) ? logCosh (x) : logCosh (kNeg * x) / (kNeg * kNeg);
}

/**
    Harder asymmetric clipper for a high-gain last stage (the Marshall's diode
    character). Same family as tubeAsym but driven into saturation sooner, so it
    is aggressive without the discontinuous knee of a literal diode model -
    which would put energy everywhere and alias however fast we ran it.
*/
template <typename T>
inline T hardAsym (T x, T asym) noexcept
{
    const T k = (T) 2.2;
    return tubeAsym (k * x, asym) / k;
}

template <typename T>
inline T hardAsymAD (T x, T asym) noexcept
{
    const T k = (T) 2.2;
    return tubeAsymAD (k * x, asym) / (k * k);
}

/**
    Bass fuzz. Hard, asymmetric clipping (the negative half squashes less, for
    even harmonics), blended with a full-wave-rectified copy: rectification
    doubles the frequency, which is where an octave fuzz's snarl comes from.
    `octave` in [0, 1]. Drive is applied upstream; the caller DC-blocks, since
    both the asymmetry and the rectifier introduce an offset.
*/
inline float fuzz (float x, float octave) noexcept
{
    const float y = std::tanh (x >= 0.0f ? x : 0.7f * x);
    const float rect = 2.0f * std::abs (y) - 1.0f;
    return (1.0f - octave) * y + octave * rect;
}

/**
    Push-pull power-stage curve: symmetric, flatter near zero and rounder at the
    rails than a bare tanh - two tubes sharing the swing.

    Built as (1-m)*tanh(x) + m*tanh(x)^3. The cubic term flattens the middle,
    and because it is a function *of* tanh rather than of x, the curve stays
    monotonic however hard it is driven. (The previous tanh(x - 0.15x^3) form
    folded back above x = 1.49, which is audible as harshness at high Master.)
*/
template <typename T>
inline T pushPull (T x) noexcept
{
    const T m = (T) 0.25;
    const T t = std::tanh (x);
    return ((T) 1 - m) * t + m * t * t * t;
}

/** Antiderivative of pushPull, using integral(tanh^3) = log(cosh) - tanh^2/2. */
template <typename T>
inline T pushPullAD (T x) noexcept
{
    const T m = (T) 0.25;
    const T t = std::tanh (x);
    return logCosh (x) - m * (T) 0.5 * t * t;
}

/**
    Single-ended (class-A) power-stage curve: asymmetric, compresses one rail
    earlier - the breathy, even-harmonic small-amp character (Champ).
*/
template <typename T> inline T singleEnded (T x) noexcept   { return tubeAsym (x, (T) 0.45); }
template <typename T> inline T singleEndedAD (T x) noexcept { return tubeAsymAD (x, (T) 0.45); }

//==============================================================================
/**
    First-order antiderivative anti-aliasing for one nonlinear stage.

    Holds the previous input and its antiderivative per channel. When two
    successive samples are nearly equal the difference quotient is numerically
    unstable, so it falls back to evaluating the curve at the midpoint.
*/
struct Adaa1
{
    static constexpr int kMaxChannels = 2;

    void reset()
    {
        x1.fill (0.0);
        f1.fill (0.0);
    }

    /** @param f the curve, @param F its antiderivative — both evaluated in
        double. The state and the difference quotient are double for the same
        reason: in float, the cancellation in (F(x) - F(x1)) leaves enough error
        near waveform peaks (where dx goes to zero) to be audible as crackle. */
    template <typename Curve, typename Antiderivative>
    inline float process (int ch, float xf, Curve f, Antiderivative F) noexcept
    {
        auto& px = x1[(size_t) ch];
        auto& pf = f1[(size_t) ch];

        const double x = (double) xf;
        const double Fx = F (x);
        const double dx = x - px;
        const double y = (std::abs (dx) < 1.0e-6) ? f (0.5 * (x + px))
                                                  : (Fx - pf) / dx;
        px = x;
        pf = Fx;
        return (float) y;
    }

    std::array<double, kMaxChannels> x1 {}, f1 {};
};

} // namespace amp::shape
