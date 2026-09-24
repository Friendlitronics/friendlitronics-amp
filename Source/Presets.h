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
#include <vector>
#include <utility>

/**
    Factory presets: each is a name plus a list of (paramID, raw value) pairs.
    Choice params (amp/cab/mic) store their index as a float. Applying a preset
    notifies the host so automation/GUI follow.
*/
namespace Presets
{
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    inline const std::vector<Preset>& all()
    {
        //                     amp cab mic gain bass mid trb pres res cut mast rev  dist axis ang room out  mix
        static const std::vector<Preset> presets {
            { "Twin Clean", {
                {"ampModel",0},{"cabModel",0},{"micModel",0},
                {"gain",3.0f},{"bass",5.0f},{"mid",5.0f},{"treble",6.5f},
                {"presence",5.0f},{"resonance",3.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",5.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",20.0f},{"micDistance",15.0f},{"micAxis",30.0f},{"micAngle",0.0f},
                {"room",10.0f},{"output",0.0f},{"mix",100.0f} } },

            { "Ack Thirty Chime", {
                {"ampModel",1},{"cabModel",1},{"micModel",2},
                {"gain",5.5f},{"bass",4.0f},{"mid",6.0f},{"treble",7.0f},
                {"presence",5.0f},{"resonance",4.0f},{"sweetness",7.0f},{"cut",4.0f},{"master",6.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",25.0f},{"micAxis",35.0f},{"micAngle",10.0f},
                {"room",15.0f},{"output",0.0f},{"mix",100.0f} } },

            { "Stacked Crunch", {
                {"ampModel",2},{"cabModel",2},{"micModel",0},
                {"gain",7.0f},{"bass",5.0f},{"mid",6.0f},{"treble",6.0f},
                {"presence",6.5f},{"resonance",6.0f},{"sweetness",6.0f},{"cut",3.0f},{"master",6.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",10.0f},{"micAxis",25.0f},{"micAngle",20.0f},
                {"room",0.0f},{"output",-1.0f},{"mix",100.0f} } },

            { "Stacked Lead (off-axis)", {
                {"ampModel",2},{"cabModel",2},{"micModel",0},
                {"gain",9.0f},{"bass",5.0f},{"mid",7.0f},{"treble",6.0f},
                {"presence",7.0f},{"resonance",6.0f},{"sweetness",6.0f},{"cut",3.0f},{"master",7.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",12.0f},{"micAxis",30.0f},{"micAngle",55.0f},
                {"room",5.0f},{"output",-2.0f},{"mix",100.0f} } },

            { "Champion Breakup", {
                {"ampModel",3},{"cabModel",3},{"micModel",1},
                {"gain",6.5f},{"bass",5.0f},{"mid",6.0f},{"treble",6.0f},
                {"presence",3.0f},{"resonance",2.0f},{"sweetness",6.0f},{"cut",3.0f},{"master",6.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",15.0f},{"micDistance",18.0f},{"micAxis",20.0f},{"micAngle",0.0f},
                {"room",10.0f},{"output",0.0f},{"mix",100.0f} } },

            { "Twin Jazz (room mic)", {
                {"ampModel",0},{"cabModel",0},{"micModel",2},
                {"gain",2.5f},{"bass",6.0f},{"mid",5.5f},{"treble",5.5f},
                {"presence",4.0f},{"resonance",3.0f},{"sweetness",4.0f},{"cut",3.0f},{"master",5.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",25.0f},{"micDistance",45.0f},{"micAxis",40.0f},{"micAngle",15.0f},
                {"room",35.0f},{"output",0.0f},{"mix",100.0f} } },

            // Acoustic presets: piezo DI -> imager -> clean full-range amp.
            //                     body  deQuack  comp  colour
            { "Acoustic DI (Studio)", {
                {"ampModel",4},{"cabModel",4},{"micModel",3},
                {"gain",4.0f},{"bass",5.0f},{"mid",5.0f},{"treble",5.5f},
                {"presence",4.0f},{"resonance",3.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",4.0f},
                {"body",6.0f},{"deQuack",6.0f},{"comp",4.0f},{"colour",2.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",12.0f},{"micDistance",30.0f},{"micAxis",20.0f},{"micAngle",10.0f},
                {"room",12.0f},{"output",0.0f},{"mix",100.0f} } },

            { "Acoustic Strum (Bright)", {
                {"ampModel",4},{"cabModel",4},{"micModel",3},
                {"gain",4.5f},{"bass",4.5f},{"mid",5.0f},{"treble",6.5f},
                {"presence",5.0f},{"resonance",2.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",4.0f},
                {"body",4.5f},{"deQuack",5.0f},{"comp",6.5f},{"colour",5.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",10.0f},{"micDistance",25.0f},{"micAxis",15.0f},{"micAngle",5.0f},
                {"room",10.0f},{"output",0.0f},{"mix",100.0f} } },

            { "Acoustic Fingerstyle (Warm)", {
                {"ampModel",4},{"cabModel",4},{"micModel",2},
                {"gain",4.0f},{"bass",5.5f},{"mid",5.0f},{"treble",5.0f},
                {"presence",3.5f},{"resonance",3.5f},{"sweetness",6.0f},{"cut",3.0f},{"master",4.0f},
                {"body",7.5f},{"deQuack",7.0f},{"comp",3.0f},{"colour",1.5f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",18.0f},{"micDistance",35.0f},{"micAxis",25.0f},{"micAngle",10.0f},
                {"room",18.0f},{"output",0.0f},{"mix",100.0f} } },

            //==================================================================
            // Bass. Amp 5-8 / cab 5-8. BLEND 0 means the drive stage is out of
            // circuit; the lows never get distorted, only the band above SPLIT.
            //==================================================================

            { "Bass: Freezer Rock", {
                {"ampModel",5},{"cabModel",5},{"micModel",1},
                {"gain",5.0f},{"bass",6.0f},{"mid",4.5f},{"treble",5.5f},
                {"presence",5.0f},{"resonance",6.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",5.5f},
                {"blend",2.0f},{"split",4.0f},{"grit",3.0f},{"sub",0.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",12.0f},{"micAxis",25.0f},{"micAngle",10.0f},
                {"room",5.0f},{"output",-6.0f},{"mix",100.0f} } },

            { "Bass: Sixties Soul", {
                {"ampModel",6},{"cabModel",6},{"micModel",1},
                {"gain",5.5f},{"bass",6.0f},{"mid",5.5f},{"treble",3.5f},
                {"presence",2.0f},{"resonance",5.0f},{"sweetness",6.0f},{"cut",3.0f},{"master",6.0f},
                {"blend",0.0f},{"split",5.0f},{"grit",3.0f},{"sub",0.0f},
                {"tape",6.0f},{"flutter",1.5f},
                {"reverb",0.0f},{"micDistance",20.0f},{"micAxis",30.0f},{"micAngle",5.0f},
                {"room",8.0f},{"output",-4.0f},{"mix",100.0f} } },

            { "Bass: Funk Fingerstyle", {
                {"ampModel",7},{"cabModel",7},{"micModel",1},
                {"gain",4.0f},{"bass",6.0f},{"mid",6.5f},{"treble",5.0f},
                {"presence",4.0f},{"resonance",5.5f},{"sweetness",5.0f},{"cut",3.0f},{"master",4.5f},
                {"blend",0.0f},{"split",5.0f},{"grit",2.0f},{"sub",0.0f},
                {"tape",2.5f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",15.0f},{"micAxis",20.0f},{"micAngle",0.0f},
                {"room",6.0f},{"output",4.0f},{"mix",100.0f} } },

            { "Bass: Slap Funk (4x10)", {
                {"ampModel",8},{"cabModel",8},{"micModel",3},
                {"gain",4.0f},{"bass",6.5f},{"mid",3.5f},{"treble",7.0f},
                {"presence",6.0f},{"resonance",5.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",4.0f},
                {"blend",0.0f},{"split",5.0f},{"grit",2.0f},{"sub",0.0f},
                {"tape",0.0f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",18.0f},{"micAxis",15.0f},{"micAngle",5.0f},
                {"room",8.0f},{"output",2.0f},{"mix",100.0f} } },

            { "Bass: Fuzz Blend", {
                {"ampModel",5},{"cabModel",5},{"micModel",0},
                {"gain",5.5f},{"bass",5.5f},{"mid",6.0f},{"treble",5.5f},
                {"presence",5.5f},{"resonance",6.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",6.0f},
                {"blend",6.0f},{"split",4.0f},{"grit",8.0f},{"sub",0.0f},
                {"tape",1.5f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",10.0f},{"micAxis",25.0f},{"micAngle",15.0f},
                {"room",0.0f},{"output",-7.0f},{"mix",100.0f} } },

            { "Bass: Indie Pick (warm)", {
                {"ampModel",8},{"cabModel",8},{"micModel",1},
                {"gain",4.5f},{"bass",5.5f},{"mid",5.5f},{"treble",6.0f},
                {"presence",5.0f},{"resonance",4.5f},{"sweetness",5.5f},{"cut",3.0f},{"master",4.5f},
                {"blend",2.5f},{"split",5.0f},{"grit",4.0f},{"sub",0.0f},
                {"tape",3.5f},{"flutter",1.0f},
                {"reverb",0.0f},{"micDistance",16.0f},{"micAxis",25.0f},{"micAngle",10.0f},
                {"room",10.0f},{"output",4.5f},{"mix",100.0f} } },

            { "Bass: Sub Dub", {
                {"ampModel",7},{"cabModel",7},{"micModel",1},
                {"gain",4.0f},{"bass",7.0f},{"mid",4.0f},{"treble",3.0f},
                {"presence",1.5f},{"resonance",7.0f},{"sweetness",5.0f},{"cut",3.0f},{"master",4.5f},
                {"blend",0.0f},{"split",2.0f},{"grit",2.0f},{"sub",7.0f},
                {"tape",4.0f},{"flutter",0.5f},
                {"reverb",0.0f},{"micDistance",25.0f},{"micAxis",30.0f},{"micAngle",0.0f},
                {"room",12.0f},{"output",3.0f},{"mix",100.0f} } },

            // --- deliberately strange ---
            { "Bass: Octave Fuzz Monster", {
                {"ampModel",8},{"cabModel",8},{"micModel",0},
                {"gain",6.0f},{"bass",5.0f},{"mid",7.0f},{"treble",6.5f},
                {"presence",6.5f},{"resonance",5.0f},{"sweetness",6.0f},{"cut",3.0f},{"master",6.0f},
                {"blend",9.0f},{"split",8.0f},{"grit",10.0f},{"sub",6.0f},
                {"tape",2.0f},{"flutter",0.0f},
                {"reverb",0.0f},{"micDistance",10.0f},{"micAxis",20.0f},{"micAngle",20.0f},
                {"room",0.0f},{"output",-2.0f},{"mix",100.0f} } },

            { "Bass: Broken Tape Machine", {
                {"ampModel",6},{"cabModel",6},{"micModel",1},
                {"gain",6.5f},{"bass",6.0f},{"mid",5.0f},{"treble",2.5f},
                {"presence",1.0f},{"resonance",5.0f},{"sweetness",7.0f},{"cut",3.0f},{"master",7.0f},
                {"blend",4.0f},{"split",6.0f},{"grit",5.0f},{"sub",2.0f},
                {"tape",9.0f},{"flutter",8.0f},
                {"reverb",0.0f},{"micDistance",30.0f},{"micAxis",35.0f},{"micAngle",15.0f},
                {"room",20.0f},{"output",-1.0f},{"mix",100.0f} } },

            // Tape works with any amp — here it is on the Twin.
            { "Twin Tape (vintage)", {
                {"ampModel",0},{"cabModel",0},{"micModel",3},
                {"gain",3.5f},{"bass",5.0f},{"mid",5.5f},{"treble",6.0f},
                {"presence",4.5f},{"resonance",3.0f},{"sweetness",6.0f},{"cut",3.0f},{"master",5.0f},
                {"tape",5.0f},{"flutter",2.0f},
                {"reverb",18.0f},{"micDistance",20.0f},{"micAxis",30.0f},{"micAngle",10.0f},
                {"room",15.0f},{"output",0.0f},{"mix",100.0f} } },
            //==================================================================
            // Colour / lofi: no amp (9), no cab (9), no mic (4). Bass/Mid/Treble
            // are a gentle 100 Hz / 1 kHz / 10 kHz mix EQ here; everything else
            // is the LoFi + Tape colour. Good on a bus or a master.
            //==================================================================

            { "Master: Tape Glue", {
                {"ampModel",9},{"cabModel",9},{"micModel",4},
                {"bass",5.0f},{"mid",5.0f},{"treble",5.0f},
                {"tape",3.5f},{"flutter",0.5f},
                {"crush",0.0f},{"srate",0.0f},{"noise",0.0f},{"narrow",0.0f},
                {"room",0.0f},{"output",1.5f},{"mix",100.0f} } },

            { "Master: Cassette Lofi", {
                {"ampModel",9},{"cabModel",9},{"micModel",4},
                {"bass",5.5f},{"mid",5.0f},{"treble",4.0f},
                {"tape",6.0f},{"flutter",3.0f},
                {"crush",2.5f},{"srate",3.0f},{"noise",2.5f},{"narrow",3.0f},
                {"room",0.0f},{"output",2.5f},{"mix",100.0f} } },

            { "Master: Vinyl Wear", {
                {"ampModel",9},{"cabModel",9},{"micModel",4},
                {"bass",5.5f},{"mid",5.0f},{"treble",4.5f},
                {"tape",4.5f},{"flutter",1.5f},
                {"crush",1.0f},{"srate",0.0f},{"noise",4.0f},{"narrow",2.0f},
                {"room",0.0f},{"output",2.0f},{"mix",100.0f} } },

            { "Master: Broken Radio", {
                {"ampModel",9},{"cabModel",9},{"micModel",4},
                {"bass",3.5f},{"mid",7.0f},{"treble",2.5f},
                {"tape",7.0f},{"flutter",5.0f},
                {"crush",5.0f},{"srate",6.0f},{"noise",5.0f},{"narrow",8.0f},
                {"room",0.0f},{"output",2.0f},{"mix",100.0f} } },
        };
        return presets;
    }

    inline void apply (juce::AudioProcessorValueTreeState& apvts, int index)
    {
        if (index < 0 || index >= (int) all().size())
            return;

        // Presets list only what they care about, so first put every parameter
        // back to its default. Without this, loading a preset would inherit
        // whatever the previous one left in the controls it doesn't mention
        // (Tape, Crush, Sub, ...) — presets must be self-contained.
        for (auto* p : apvts.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (ranged->getParameterID() != "bypass")
                    ranged->setValueNotifyingHost (ranged->getDefaultValue());

        for (const auto& [id, raw] : all()[(size_t) index].values)
        {
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (raw));
        }
    }
}
