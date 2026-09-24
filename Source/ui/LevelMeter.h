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
#include "AmpLookAndFeel.h"

/**
    Vertical peak level meter with quick-rise/slow-fall ballistics and a clip LED.
    The editor feeds it block peaks on a timer; it owns no timer itself.
*/
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String labelText) : label (std::move (labelText)) {}

    void setLevel (float linearPeak)
    {
        const float targetDb = juce::Decibels::gainToDecibels (linearPeak, kMinDb);
        const float coeff = (targetDb > currentDb) ? kRise : kFall;
        currentDb = coeff * currentDb + (1.0f - coeff) * targetDb;

        if (linearPeak >= 0.999f) clipHold = kClipHoldTicks;
        else if (clipHold > 0)    --clipHold;

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (2.0f);

        auto labelArea = bounds.removeFromBottom (14.0f);
        g.setColour (AmpLookAndFeel::textLight.withAlpha (0.8f));
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (label, labelArea.toNearestInt(), juce::Justification::centred);

        auto led = bounds.removeFromTop (8.0f).withSizeKeepingCentre (8.0f, 8.0f);
        g.setColour (clipHold > 0 ? juce::Colour (0xffff3b30)
                                  : juce::Colours::darkred.withAlpha (0.4f));
        g.fillEllipse (led);

        bounds.removeFromTop (3.0f);

        g.setColour (AmpLookAndFeel::panelDark.darker (0.4f));
        g.fillRoundedRectangle (bounds, 3.0f);

        const float norm = juce::jlimit (0.0f, 1.0f, (currentDb - kMinDb) / (kMaxDb - kMinDb));
        auto fill = bounds.reduced (2.0f);
        const float h = fill.getHeight() * norm;
        auto bar = fill.removeFromBottom (h);

        juce::ColourGradient grad (juce::Colour (0xff4caf50), 0.0f, bounds.getBottom(),
                                   juce::Colour (0xffff3b30), 0.0f, bounds.getY(), false);
        grad.addColour (0.75, juce::Colour (0xffd9a441));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bar, 2.0f);
    }

private:
    static constexpr float kMinDb = -48.0f;
    static constexpr float kMaxDb =   6.0f;
    static constexpr float kRise  = 0.30f;
    static constexpr float kFall  = 0.88f;
    static constexpr int   kClipHoldTicks = 25;

    juce::String label;
    float currentDb = kMinDb;
    int   clipHold  = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};
