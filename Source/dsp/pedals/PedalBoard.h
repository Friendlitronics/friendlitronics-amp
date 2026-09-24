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
#include "DrivePedals.h"
#include "FilterPedals.h"
#include "PitchPedal.h"
#include "TimePedals.h"

namespace amp::pedal
{

/** Pedal identity. The *default* order is the conventional one — filter, pitch
    and dynamics first so they feed the dirt a controlled signal, dirt next,
    modulation and time last — but the chain is user-orderable, so this enum is
    only an identity, not a sequence. */
enum class Id { Wah = 0, Dive, Sustainer, Screamer, Fuzz, Chorus, Phaser, Delay, Count };

inline constexpr int kNumPedals = (int) Id::Count;

/** The amp's position in the chain is itself a draggable item; this is its
    index in a chain order. Pedals before it run into the amp input, pedals
    after it run after the cab. */
inline constexpr int kAmpToken = kNumPedals;
inline constexpr int kChainLength = kNumPedals + 1;

using ChainOrder = std::array<int, (size_t) kChainLength>;

/** Wah, Dive, Sustainer, Screamer, Fuzz -> AMP -> Chorus, Phaser, Delay. */
inline ChainOrder defaultChainOrder()
{
    return { (int) Id::Wah, (int) Id::Dive, (int) Id::Sustainer, (int) Id::Screamer,
             (int) Id::Fuzz, kAmpToken, (int) Id::Chorus, (int) Id::Phaser, (int) Id::Delay };
}

/** True if every slot appears exactly once — a malformed order (an old or
    hand-edited session) must never be trusted, since it indexes arrays. */
inline bool chainOrderValid (const ChainOrder& order)
{
    std::array<bool, (size_t) kChainLength> seen {};
    for (int v : order)
    {
        if (v < 0 || v >= kChainLength || seen[(size_t) v])
            return false;
        seen[(size_t) v] = true;
    }
    return true;
}

/** Chain orders travel in saved state as text, e.g. "0,1,2,3,4,8,5,6,7". */
inline juce::String chainOrderToString (const ChainOrder& order)
{
    juce::StringArray parts;
    for (int v : order)
        parts.add (juce::String (v));
    return parts.joinIntoString (",");
}

inline ChainOrder chainOrderFromString (const juce::String& text)
{
    juce::StringArray parts;
    parts.addTokens (text, ",", {});
    ChainOrder order {};
    if (parts.size() != kChainLength)
        return defaultChainOrder();

    for (int i = 0; i < kChainLength; ++i)
        order[(size_t) i] = parts[i].getIntValue();

    return chainOrderValid (order) ? order : defaultChainOrder();
}

/** Short id used to build parameter names: "wah" -> "wahOn", "wahA", ... */
inline const char* pedalTag (Id id)
{
    switch (id)
    {
        case Id::Wah:       return "wah";
        case Id::Dive:      return "dive";
        case Id::Sustainer: return "sustain";
        case Id::Screamer:  return "screamer";
        case Id::Fuzz:      return "fuzzbox";
        case Id::Chorus:    return "chorus";
        case Id::Phaser:    return "phaser";
        case Id::Delay:     return "delay";
        default:            return "pedal";
    }
}

/** Parameter id for one of a pedal's knobs: "wah" + A/B/C/D.
    NB: build the suffix from a string literal, not juce::String('A') — that
    resolves to the numeric constructor and silently yields "wah65". */
inline juce::String pedalKnobId (Id id, int knob)
{
    static const char* const suffix[] = { "A", "B", "C", "D" };
    return juce::String (pedalTag (id)) + suffix[juce::jlimit (0, 3, knob)];
}

inline const char* pedalName (Id id)
{
    switch (id)
    {
        case Id::Wah:       return "Wah";
        case Id::Dive:      return "Dive";
        case Id::Sustainer: return "Sustainer";
        case Id::Screamer:  return "Screamer";
        case Id::Fuzz:      return "Fuzz";
        case Id::Chorus:    return "Chorus";
        case Id::Phaser:    return "Phaser";
        case Id::Delay:     return "Delay";
        default:            return "Pedal";
    }
}

/** Abbreviated name, for the chain strip's small tiles. */
inline const char* pedalShortName (Id id)
{
    switch (id)
    {
        case Id::Wah:       return "WAH";
        case Id::Dive:      return "DIVE";
        case Id::Sustainer: return "SUST";
        case Id::Screamer:  return "SCRM";
        case Id::Fuzz:      return "FUZZ";
        case Id::Chorus:    return "CHOR";
        case Id::Phaser:    return "PHAS";
        case Id::Delay:     return "DLY";
        default:            return "PDL";
    }
}

/** How many knobs each pedal shows (3 for all but Delay). */
inline int pedalKnobCount (Id id) { return id == Id::Delay ? 4 : 3; }

/** Knob labels, left to right. */
inline juce::StringArray pedalKnobNames (Id id)
{
    switch (id)
    {
        case Id::Wah:       return { "PEDAL", "Q", "AUTO" };
        case Id::Dive:      return { "PITCH", "MIX", "DOUBLE" };
        case Id::Sustainer: return { "SUSTAIN", "ATTACK", "LEVEL" };
        case Id::Screamer:  return { "DRIVE", "TONE", "LEVEL" };
        case Id::Fuzz:      return { "SUSTAIN", "TONE", "LEVEL" };
        case Id::Chorus:    return { "RATE", "DEPTH", "MIX" };
        case Id::Phaser:    return { "RATE", "DEPTH", "FDBK" };
        case Id::Delay:     return { "TIME", "FDBK", "MIX", "TONE" };
        default:            return { "A", "B", "C" };
    }
}

/** Sensible defaults per knob, on the 0..10 scale. */
inline std::array<float, 4> pedalKnobDefaults (Id id)
{
    switch (id)
    {
        case Id::Wah:       return { 5.0f, 5.0f, 0.0f, 0.0f };
        case Id::Dive:      return { 5.0f, 5.0f, 0.0f, 0.0f };
        case Id::Sustainer: return { 5.0f, 3.0f, 5.0f, 0.0f };
        case Id::Screamer:  return { 4.0f, 5.0f, 5.0f, 0.0f };
        case Id::Fuzz:      return { 6.0f, 5.0f, 5.0f, 0.0f };
        case Id::Chorus:    return { 3.0f, 5.0f, 4.0f, 0.0f };
        case Id::Phaser:    return { 3.0f, 6.0f, 2.0f, 0.0f };
        case Id::Delay:     return { 3.0f, 3.0f, 3.0f, 5.0f };
        default:            return { 5.0f, 5.0f, 5.0f, 5.0f };
    }
}

//==============================================================================
/**
    The pedalboard: eight pedals, each independently switchable and placeable
    either in front of the amp or after the cab.

    "Pre" runs inside the oversampled region, so the drive pedals' harmonics are
    band-limited before they fold back down — dirt belongs there. "Post" runs at
    host rate after the cab and mic, which is right for modulation and delay
    (it is effectively an effects loop) but will alias if you put a fuzz there.
*/
class PedalBoard
{
public:
    struct State
    {
        /** Indexed by Id. `slot` is derived from where the pedal sits relative
            to the amp token, so it is set by the caller, not the user. */
        std::array<Controls, (size_t) kNumPedals> pedals {};
        ChainOrder order = defaultChainOrder();
    };

    void prepare (const juce::dsp::ProcessSpec& preSpec, const juce::dsp::ProcessSpec& postSpec)
    {
        // Every pedal is prepared for both rates, since either slot is legal.
        pre.prepare (preSpec);
        post.prepare (postSpec);
    }

    void reset() { pre.reset(); post.reset(); }

    void setState (const State& s) { state = s; }

    /** True if any pedal is enabled in this slot — lets the engine skip work. */
    bool anyIn (Slot slot) const
    {
        for (const auto& c : state.pedals)
            if (c.on && c.slot == slot)
                return true;
        return false;
    }

    void process (juce::dsp::AudioBlock<float>& block, Slot slot)
    {
        auto& set = (slot == Slot::Pre) ? pre : post;

        // Walk the user's chain order, not the enum order.
        for (int position = 0; position < kChainLength; ++position)
        {
            const int i = state.order[(size_t) position];
            if (i == kAmpToken)
                continue;

            const auto& c = state.pedals[(size_t) i];
            if (! c.on || c.slot != slot)
                continue;

            switch ((Id) i)
            {
                case Id::Wah:
                    set.wah.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.wah.process (block);
                    break;
                case Id::Dive:
                    set.dive.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.dive.process (block);
                    break;
                case Id::Sustainer:
                    set.sustainer.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.sustainer.process (block);
                    break;
                case Id::Screamer:
                    set.screamer.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.screamer.process (block);
                    break;
                case Id::Fuzz:
                    set.fuzz.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.fuzz.process (block);
                    break;
                case Id::Chorus:
                    set.chorus.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.chorus.process (block);
                    break;
                case Id::Phaser:
                    set.phaser.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f);
                    set.phaser.process (block);
                    break;
                case Id::Delay:
                    set.delay.setControls (c.a * 0.1f, c.b * 0.1f, c.c * 0.1f, c.d * 0.1f);
                    set.delay.process (block);
                    break;
                default:
                    break;
            }
        }
    }

private:
    /** One instance of every pedal per slot, so moving a pedal between slots
        never drags its delay-line tail across with it. */
    struct Set
    {
        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            wah.prepare (spec);       dive.prepare (spec);
            sustainer.prepare (spec); screamer.prepare (spec);
            fuzz.prepare (spec);      chorus.prepare (spec);
            phaser.prepare (spec);    delay.prepare (spec);
        }

        void reset()
        {
            wah.reset();       dive.reset();
            sustainer.reset(); screamer.reset();
            fuzz.reset();      chorus.reset();
            phaser.reset();    delay.reset();
        }

        Wah wah;
        Dive dive;
        Sustainer sustainer;
        Screamer screamer;
        Fuzz fuzz;
        Chorus chorus;
        Phaser phaser;
        Delay delay;
    };

    Set pre, post;
    State state {};
};

} // namespace amp::pedal
