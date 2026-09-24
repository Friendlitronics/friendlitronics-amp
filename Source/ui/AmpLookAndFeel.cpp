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

#include "AmpLookAndFeel.h"

// Vintage blackface-ish palette: dark chassis, brushed-silver knobs, warm gold accent.
const juce::Colour AmpLookAndFeel::panelDark = juce::Colour (0xff1a1c20);
const juce::Colour AmpLookAndFeel::panelMid  = juce::Colour (0xff262a30);
const juce::Colour AmpLookAndFeel::cream      = juce::Colour (0xffece3c8);
const juce::Colour AmpLookAndFeel::accent     = juce::Colour (0xffd9a441); // warm gold
const juce::Colour AmpLookAndFeel::accentDim  = juce::Colour (0xff7a5e2a);
const juce::Colour AmpLookAndFeel::textLight  = juce::Colour (0xffd7d3c8);
const juce::Colour AmpLookAndFeel::metalHi    = juce::Colour (0xffcfd2d6);
const juce::Colour AmpLookAndFeel::metalLo    = juce::Colour (0xff6b7077);
