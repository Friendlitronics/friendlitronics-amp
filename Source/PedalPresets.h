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
#include "dsp/pedals/PedalBoard.h"
#include <vector>
#include <utility>

/**
    Factory pedalboards: pedal settings plus a chain order, and nothing else.
    Loading one rearranges the board and leaves the amp, cab and mic alone, so
    any board can be tried through any amp.

    Chain orders are written out in full (all nine slots, amp included) because
    where the amp sits is the whole point — a fuzz in front of the amp and a
    delay after the cab is a different rig from both of them in front.
*/
namespace PedalPresets
{
    using Id = amp::pedal::Id;

    struct Board
    {
        juce::String name;
        juce::String blurb;
        std::vector<std::pair<juce::String, float>> values;
        amp::pedal::ChainOrder order;
    };

    /** Chain order helper: list what comes before the amp, then after it, and
        every unused pedal is parked at the end (off, so its place is moot). */
    inline amp::pedal::ChainOrder chain (std::vector<Id> before, std::vector<Id> after)
    {
        amp::pedal::ChainOrder order {};
        std::array<bool, (size_t) amp::pedal::kNumPedals> used {};
        int at = 0;

        for (auto id : before) { order[(size_t) at++] = (int) id; used[(size_t) id] = true; }
        order[(size_t) at++] = amp::pedal::kAmpToken;
        for (auto id : after)  { order[(size_t) at++] = (int) id; used[(size_t) id] = true; }
        for (int i = 0; i < amp::pedal::kNumPedals; ++i)
            if (! used[(size_t) i])
                order[(size_t) at++] = i;

        return order;
    }

    inline const std::vector<Board>& all()
    {
        static const std::vector<Board> boards {
            //==================================================================
            // Shoegaze
            //==================================================================
            { "Shoegaze: Glide (MBV)",
              "Fuzz into a slow, deep chorus, with the pitch pedal detuning underneath — "
              "the seasick, bent-into-tune sound.",
              { {"fuzzboxOn",1}, {"fuzzboxA",7.5f}, {"fuzzboxB",4.0f}, {"fuzzboxC",5.0f},
                {"whammyOn",1},  {"whammyA",5.0f},  {"whammyB",3.0f},  {"whammyC",7.5f},
                {"chorusOn",1},  {"chorusA",1.2f},  {"chorusB",8.5f},  {"chorusC",6.0f},
                {"delayOn",1},   {"delayA",2.0f},   {"delayB",2.5f},   {"delayC",2.0f}, {"delayD",4.0f} },
              chain ({ Id::Whammy, Id::Fuzz }, { Id::Chorus, Id::Delay }) },

            { "Shoegaze: Shimmer (Cocteau)",
              "No dirt at all — compression, chorus and a long phased delay. The guitar "
              "stops sounding like a guitar and starts sounding like weather.",
              { {"sustainOn",1},  {"sustainA",6.0f},  {"sustainB",4.0f},  {"sustainC",5.0f},
                {"chorusOn",1},   {"chorusA",2.5f},   {"chorusB",7.0f},   {"chorusC",6.5f},
                {"phaserOn",1},   {"phaserA",1.5f},   {"phaserB",7.0f},   {"phaserC",3.0f},
                {"delayOn",1},    {"delayA",5.5f},    {"delayB",6.0f},    {"delayC",5.0f}, {"delayD",6.0f} },
              chain ({ Id::Sustainer }, { Id::Chorus, Id::Phaser, Id::Delay }) },

            { "Shoegaze: Wash (Slowdive)",
              "Edge-of-breakup drive, then an enormous delay after the cab. Chords blur "
              "into the next one before they finish.",
              { {"screamerOn",1}, {"screamerA",3.0f}, {"screamerB",5.0f}, {"screamerC",5.0f},
                {"chorusOn",1},   {"chorusA",1.8f},   {"chorusB",6.0f},   {"chorusC",5.0f},
                {"delayOn",1},    {"delayA",7.0f},    {"delayB",7.0f},    {"delayC",6.0f}, {"delayD",5.0f} },
              chain ({ Id::Screamer }, { Id::Chorus, Id::Delay }) },

            //==================================================================
            // Indie
            //==================================================================
            { "Indie: Wobble (Mac)",
              "Very slow, very deep chorus on a clean amp — the drunk-tape warble. "
              "Compression first so the wobble sits still.",
              { {"sustainOn",1}, {"sustainA",5.0f}, {"sustainB",5.0f}, {"sustainC",5.0f},
                {"chorusOn",1},  {"chorusA",1.0f},  {"chorusB",9.0f},  {"chorusC",7.0f} },
              chain ({ Id::Sustainer }, { Id::Chorus }) },

            { "Indie: Jangle Doubler",
              "Light compression, a hint of chorus and a short slap — thickens single-coil "
              "arpeggios without anyone noticing an effect is on.",
              { {"sustainOn",1}, {"sustainA",4.0f}, {"sustainB",3.0f}, {"sustainC",5.0f},
                {"whammyOn",1},  {"whammyA",5.0f},  {"whammyB",2.0f},  {"whammyC",5.0f},
                {"chorusOn",1},  {"chorusA",4.0f},  {"chorusB",3.0f},  {"chorusC",3.0f},
                {"delayOn",1},   {"delayA",1.2f},   {"delayB",1.5f},   {"delayC",2.5f}, {"delayD",6.0f} },
              chain ({ Id::Sustainer, Id::Whammy }, { Id::Chorus, Id::Delay }) },

            //==================================================================
            // Everyday
            //==================================================================
            { "Funk: Auto-Wah Clav",
              "Envelope filter with the treadle knob parked low, squashed hard so every "
              "note opens the filter the same amount.",
              { {"wahOn",1},     {"wahA",2.0f},     {"wahB",6.0f},     {"wahC",8.0f},
                {"sustainOn",1}, {"sustainA",7.0f}, {"sustainB",2.0f}, {"sustainC",5.0f} },
              chain ({ Id::Wah, Id::Sustainer }, {}) },

            { "Blues: Screamer + Slap",
              "The standard: a mid-humped overdrive pushing the amp, and one repeat to "
              "put it in a room.",
              { {"screamerOn",1}, {"screamerA",5.0f}, {"screamerB",6.0f}, {"screamerC",5.5f},
                {"delayOn",1},    {"delayA",2.2f},    {"delayB",2.0f},    {"delayC",2.0f}, {"delayD",5.0f} },
              chain ({ Id::Screamer }, { Id::Delay }) },

            { "Lead: Octave Up",
              "An octave above, blended under the dry note and then overdriven — synthetic "
              "and vocal at the top of the neck.",
              { {"whammyOn",1},   {"whammyA",10.0f},  {"whammyB",4.0f},   {"whammyC",0.0f},
                {"screamerOn",1}, {"screamerA",6.0f}, {"screamerB",6.0f}, {"screamerC",5.0f},
                {"delayOn",1},    {"delayA",3.5f},    {"delayB",4.0f},    {"delayC",3.0f}, {"delayD",5.0f} },
              chain ({ Id::Whammy, Id::Screamer }, { Id::Delay }) },

            { "Ambient: Infinite Wash",
              "Sustainer holding notes up, delay just short of self-oscillation, phaser "
              "stirring the repeats. Play two notes and leave.",
              { {"sustainOn",1}, {"sustainA",8.0f}, {"sustainB",6.0f}, {"sustainC",5.0f},
                {"chorusOn",1},  {"chorusA",1.5f},  {"chorusB",7.0f},  {"chorusC",6.0f},
                {"phaserOn",1},  {"phaserA",0.8f},  {"phaserB",8.0f},  {"phaserC",4.0f},
                {"delayOn",1},   {"delayA",6.0f},   {"delayB",8.5f},   {"delayC",6.0f}, {"delayD",4.0f} },
              chain ({ Id::Sustainer }, { Id::Chorus, Id::Phaser, Id::Delay }) },

            { "Grunge: Cocked Wah Fuzz",
              "Wah parked (not swept) in front of a fuzz — the nasal, mid-forward honk of "
              "a wah used as a fixed filter.",
              { {"wahOn",1},     {"wahA",4.5f},     {"wahB",7.0f},     {"wahC",0.0f},
                {"fuzzboxOn",1}, {"fuzzboxA",8.0f}, {"fuzzboxB",6.0f}, {"fuzzboxC",5.0f} },
              chain ({ Id::Wah, Id::Fuzz }, {}) },
        };
        return boards;
    }

    /** Applies a board: every pedal back to its default and off, then this
        board's settings, then its chain order. Boards never touch the amp. */
    inline void apply (juce::AudioProcessorValueTreeState& apvts, int index,
                       const std::function<void (const amp::pedal::ChainOrder&)>& setOrder)
    {
        if (index < 0 || index >= (int) all().size())
            return;

        for (int i = 0; i < amp::pedal::kNumPedals; ++i)
        {
            const auto id = (amp::pedal::Id) i;
            const juce::String tag (amp::pedal::pedalTag (id));
            const auto defaults = amp::pedal::pedalKnobDefaults (id);

            if (auto* p = apvts.getParameter (tag + "On"))
                p->setValueNotifyingHost (0.0f);

            for (int k = 0; k < amp::pedal::pedalKnobCount (id); ++k)
                if (auto* p = apvts.getParameter (amp::pedal::pedalKnobId (id, k)))
                    p->setValueNotifyingHost (p->convertTo0to1 (defaults[(size_t) k]));
        }

        for (const auto& [id, raw] : all()[(size_t) index].values)
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (raw));

        if (setOrder)
            setOrder (all()[(size_t) index].order);
    }
}
