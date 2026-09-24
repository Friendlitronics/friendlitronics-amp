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

/**
    Vintage amp-faceplate look: dark tolex panel, brushed-silver chassis accents,
    cream chicken-head knobs with a pointer, and styled combos / toggles. Fully
    programmatic (no image assets).
*/
class AmpLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static const juce::Colour panelDark;   // tolex / chassis back
    static const juce::Colour panelMid;    // faceplate
    static const juce::Colour cream;        // knob / numbers
    static const juce::Colour accent;       // pointer / value arc (warm gold)
    static const juce::Colour accentDim;
    static const juce::Colour textLight;
    static const juce::Colour metalHi;
    static const juce::Colour metalLo;

    AmpLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, panelDark);

        setColour (juce::Slider::textBoxTextColourId, textLight);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, panelDark.brighter (0.06f));

        setColour (juce::Label::textColourId, textLight);

        setColour (juce::ComboBox::backgroundColourId, panelDark.brighter (0.10f));
        setColour (juce::ComboBox::textColourId, textLight);
        setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.5f));
        setColour (juce::ComboBox::arrowColourId, accent);

        setColour (juce::PopupMenu::backgroundColourId, panelDark.brighter (0.06f));
        setColour (juce::PopupMenu::textColourId, textLight);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, panelDark);

        setColour (juce::ToggleButton::textColourId, textLight);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::ToggleButton::tickDisabledColourId, textLight.withAlpha (0.4f));
    }

    //==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const float r = dia * 0.5f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Track arc.
        const float trackR = r - 2.0f;
        juce::Path track;
        track.addCentredArc (cx, cy, trackR, trackR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (panelDark.darker (0.5f));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // Value arc.
        juce::Path valueArc;
        valueArc.addCentredArc (cx, cy, trackR, trackR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.isEnabled() ? accent : accentDim);
        g.strokePath (valueArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        // Knob body — radial metal gradient.
        const float knobR = r - 7.0f;
        juce::Rectangle<float> knob (cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
        juce::ColourGradient grad (metalHi, cx - knobR * 0.4f, cy - knobR * 0.6f,
                                   metalLo, cx + knobR * 0.5f, cy + knobR * 0.7f, true);
        g.setGradientFill (grad);
        g.fillEllipse (knob);

        g.setColour (panelDark.darker (0.7f));
        g.drawEllipse (knob, 1.4f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (knob.reduced (1.6f), 1.0f);

        // Pointer.
        juce::Path pointer;
        const float pw = juce::jmax (2.0f, knobR * 0.16f);
        pointer.addRoundedRectangle (-pw * 0.5f, -knobR + 3.0f, pw, knobR * 0.6f, pw * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
        g.setColour (slider.isEnabled() ? accent.brighter (0.2f) : accentDim);
        g.fillPath (pointer);

        const float capR = knobR * 0.16f;
        g.setColour (panelDark.darker (0.3f));
        g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    }

    //==============================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        const float arrowH = 4.0f;
        const float ax = (float) width - 16.0f;
        const float ay = (float) height * 0.5f;
        juce::Path arrow;
        arrow.startNewSubPath (ax, ay - arrowH * 0.5f);
        arrow.lineTo (ax + 7.0f, ay - arrowH * 0.5f);
        arrow.lineTo (ax + 3.5f, ay + arrowH * 0.9f);
        arrow.closeSubPath();
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    //==============================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        const float boxSize = juce::jmin (18.0f, bounds.getHeight());
        juce::Rectangle<float> box (bounds.getX() + 1.0f,
                                    bounds.getCentreY() - boxSize * 0.5f,
                                    boxSize, boxSize);

        const bool on = button.getToggleState();
        g.setColour (panelDark.brighter (shouldDrawButtonAsHighlighted ? 0.18f : 0.10f));
        g.fillRoundedRectangle (box, 3.0f);
        g.setColour (on ? accent : textLight.withAlpha (0.35f));
        g.drawRoundedRectangle (box, 3.0f, 1.2f);

        if (on)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (box.reduced (4.0f), 2.0f);
            g.setColour (accent.withAlpha (0.25f));
            g.fillRoundedRectangle (box.reduced (1.5f), 3.0f);
        }

        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.setFont (juce::Font (13.0f));
        g.drawText (button.getButtonText(),
                    bounds.withTrimmedLeft (boxSize + 8.0f),
                    juce::Justification::centredLeft, false);
    }

    juce::Font getLabelFont (juce::Label& label) override { return label.getFont(); }
};
