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
#include "../dsp/pedals/PedalBoard.h"

/** Colours and lettering that give each pedal its own identity. */
struct PedalSkin
{
    juce::Colour body, bodyEdge, accent, text, knobFace;
    juce::String subtitle;
};

inline PedalSkin getPedalSkin (amp::pedal::Id id)
{
    using juce::Colour;
    switch (id)
    {
        case amp::pedal::Id::Wah:        // black and chrome treadle
            return { Colour (0xff1b1b1e), Colour (0xff09090b), Colour (0xffd8d8dc),
                     Colour (0xfff2f2f4), Colour (0xffb9b9bf), "VOCAL FILTER" };
        case amp::pedal::Id::Dive:     // loud red, white screen print
            return { Colour (0xffb3121c), Colour (0xff6d0a11), Colour (0xffffe08a),
                     Colour (0xfffff4f4), Colour (0xffe8e2d8), "PITCH SHIFT" };
        case amp::pedal::Id::Sustainer:  // Dyna-red with cream knobs
            return { Colour (0xffc8363b), Colour (0xff7d2023), Colour (0xfff5e3bd),
                     Colour (0xfffdf3e0), Colour (0xfff0e2c4), "COMPRESSOR" };
        case amp::pedal::Id::Screamer:   // the green one
            return { Colour (0xff5f8f2f), Colour (0xff35571a), Colour (0xfff6e7a8),
                     Colour (0xfff7f3df), Colour (0xffe9dfb8), "OVERDRIVE" };
        case amp::pedal::Id::Fuzz:       // muff black / white lettering
            return { Colour (0xff141416), Colour (0xff000000), Colour (0xffe8e8ea),
                     Colour (0xffffffff), Colour (0xffcfcfd4), "SUSTAIN FUZZ" };
        case amp::pedal::Id::Chorus:     // pale BBD blue
            return { Colour (0xff2f6ea8), Colour (0xff1b4368), Colour (0xffcfe6ff),
                     Colour (0xfff1f7ff), Colour (0xffdfe9f2), "ANALOG CHORUS" };
        case amp::pedal::Id::Phaser:     // that orange
            return { Colour (0xffd9721f), Colour (0xff8f4410), Colour (0xff2b1a08),
                     Colour (0xff2b1a08), Colour (0xfff2ddc4), "FOUR STAGE" };
        case amp::pedal::Id::Delay:      // dark green echo box
            return { Colour (0xff1f5c4a), Colour (0xff10352a), Colour (0xffbfe8d6),
                     Colour (0xffeafaf3), Colour (0xffd6e6dd), "ANALOG ECHO" };
        default:
            return { Colour (0xff333338), Colour (0xff1a1a1d), Colour (0xffd0d0d4),
                     Colour (0xffffffff), Colour (0xffcccccc), "" };
    }
}

//==============================================================================
/** Draws the pedal-sized controls: small tinted knobs, a stomp switch with an
    LED, and a PRE/POST pill. One instance per pedal so each can carry its own
    colours. */
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit PedalLookAndFeel (const PedalSkin& s) : skin (s)
    {
        setColour (juce::Slider::textBoxTextColourId, s.text.withAlpha (0.85f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, s.bodyEdge.withAlpha (0.55f));
        setColour (juce::Label::textColourId, s.text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const float r = dia * 0.5f;
        const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (cx, cy, r - 1.0f, r - 1.0f, 0.0f, startAngle, endAngle, true);
        g.setColour (skin.bodyEdge.darker (0.4f));
        g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (cx, cy, r - 1.0f, r - 1.0f, 0.0f, startAngle, angle, true);
        g.setColour (skin.accent);
        g.strokePath (value, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        const float kr = r - 5.0f;
        juce::Rectangle<float> knob (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
        juce::ColourGradient grad (skin.knobFace.brighter (0.25f), cx - kr * 0.4f, cy - kr * 0.6f,
                                   skin.knobFace.darker (0.45f), cx + kr * 0.5f, cy + kr * 0.7f, true);
        g.setGradientFill (grad);
        g.fillEllipse (knob);
        g.setColour (skin.bodyEdge.darker (0.6f));
        g.drawEllipse (knob, 1.0f);

        juce::Path pointer;
        const float pw = juce::jmax (1.8f, kr * 0.2f);
        pointer.addRoundedRectangle (-pw * 0.5f, -kr + 1.5f, pw, kr * 0.62f, pw * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));
        g.setColour (skin.bodyEdge.darker (0.8f));
        g.fillPath (pointer);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool over, bool) override
    {
        const bool on = button.getToggleState();
        auto b = button.getLocalBounds().toFloat();

        if ((bool) button.getProperties().getWithDefault ("stomp", false))
        {
            // Footswitch: chrome disc with an LED above it.
            const float led = 7.0f;
            juce::Rectangle<float> ledRect (b.getCentreX() - led * 0.5f, b.getY(), led, led);
            g.setColour (on ? skin.accent : skin.bodyEdge.darker (0.3f));
            g.fillEllipse (ledRect);
            if (on)
            {
                g.setColour (skin.accent.withAlpha (0.35f));
                g.fillEllipse (ledRect.expanded (3.0f));
            }
            g.setColour (skin.bodyEdge.darker (0.7f));
            g.drawEllipse (ledRect, 1.0f);

            auto sw = b.withTrimmedTop (led + 4.0f);
            const float d = juce::jmin (sw.getWidth(), sw.getHeight());
            juce::Rectangle<float> disc (sw.getCentreX() - d * 0.5f, sw.getY(), d, d);
            juce::ColourGradient chrome (juce::Colour (0xffe6e6ea), disc.getX(), disc.getY(),
                                         juce::Colour (0xff6e6e76), disc.getRight(), disc.getBottom(), false);
            g.setGradientFill (chrome);
            g.fillEllipse (disc.reduced (over ? 0.0f : 1.0f));
            g.setColour (juce::Colour (0xff2a2a2e));
            g.drawEllipse (disc, 1.2f);
            return;
        }

        // PRE / POST pill.
        g.setColour (on ? skin.accent.withAlpha (0.9f) : skin.bodyEdge.withAlpha (0.8f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (skin.text.withAlpha (0.55f));
        g.drawRoundedRectangle (b.reduced (0.5f), b.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? skin.bodyEdge.darker (0.6f) : skin.text.withAlpha (0.85f));
        g.setFont (juce::Font (9.5f, juce::Font::bold));
        g.drawText (on ? "AFTER CAB" : "INTO AMP", b, juce::Justification::centred);
    }

    PedalSkin skin;
};

//==============================================================================
/** A single pedal: enclosure, knobs and stomp switch, wired to that pedal's
    parameters. Where it sits relative to the amp is set by dragging it in the
    chain strip, not here. */
class PedalComponent : public juce::Component
{
public:
    PedalComponent (juce::AudioProcessorValueTreeState& state, amp::pedal::Id pedalId)
        : apvts (state), id (pedalId), skin (getPedalSkin (pedalId)), lnf (skin)
    {
        setLookAndFeel (&lnf);

        const juce::String tag (amp::pedal::pedalTag (id));
        const auto names = amp::pedal::pedalKnobNames (id);
        const int n = amp::pedal::pedalKnobCount (id);

        for (int k = 0; k < n; ++k)
        {
            auto knob = std::make_unique<Knob>();
            knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 12);
            addAndMakeVisible (knob->slider);

            knob->label.setText (names[k], juce::dontSendNotification);
            knob->label.setJustificationType (juce::Justification::centred);
            knob->label.setFont (juce::Font (8.5f, juce::Font::bold));
            knob->label.setColour (juce::Label::textColourId, skin.text.withAlpha (0.8f));
            addAndMakeVisible (knob->label);

            knob->attachment = std::make_unique<SliderAttachment> (
                apvts, amp::pedal::pedalKnobId (id, k), knob->slider);
            knobs.push_back (std::move (knob));
        }

        stomp.getProperties().set ("stomp", true);
        addAndMakeVisible (stomp);
        onAttachment = std::make_unique<ButtonAttachment> (apvts, tag + "On", stomp);
    }

    ~PedalComponent() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (4.0f);

        // Enclosure.
        juce::ColourGradient body (skin.body.brighter (0.10f), b.getX(), b.getY(),
                                   skin.bodyEdge, b.getRight(), b.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (b, 9.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (b, 9.0f, 1.4f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (b.reduced (1.6f), 8.0f, 1.0f);

        // Corner screws.
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        for (auto pt : { b.getTopLeft(), b.getTopRight(), b.getBottomLeft(), b.getBottomRight() })
        {
            const float sx = juce::jlimit (b.getX() + 7.0f, b.getRight() - 7.0f, pt.x);
            const float sy = juce::jlimit (b.getY() + 7.0f, b.getBottom() - 7.0f, pt.y);
            g.fillEllipse (sx - 2.2f, sy - 2.2f, 4.4f, 4.4f);
        }

        // Screen printing.
        auto top = b.reduced (10.0f, 8.0f).removeFromTop (30.0f);
        g.setColour (skin.text);
        g.setFont (juce::Font (15.0f, juce::Font::bold));
        g.drawText (juce::String (amp::pedal::pedalName (id)).toUpperCase(), top.removeFromTop (17.0f),
                    juce::Justification::centredLeft);
        g.setColour (skin.text.withAlpha (0.6f));
        g.setFont (juce::Font (8.5f));
        g.drawText (skin.subtitle, top, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10, 12);
        b.removeFromTop (28);                              // name plate

        auto knobRow = b.removeFromTop (66);
        const int n = (int) knobs.size();
        const int w = knobRow.getWidth() / juce::jmax (1, n);
        for (auto& k : knobs)
        {
            auto cell = knobRow.removeFromLeft (w);
            k->label.setBounds (cell.removeFromTop (11));
            k->slider.setBounds (cell);
        }

        b.removeFromTop (4);
        const int sw = juce::jmin (52, b.getWidth());
        stomp.setBounds (b.withSizeKeepingCentre (sw, juce::jmin (62, b.getHeight()))
                          .withBottomY (b.getBottom() - 2));
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    juce::AudioProcessorValueTreeState& apvts;
    amp::pedal::Id id;
    PedalSkin skin;
    PedalLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    juce::ToggleButton stomp;
    std::unique_ptr<ButtonAttachment> onAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalComponent)
};

//==============================================================================
/**
    The signal chain, as draggable tiles: eight pedals and the amp itself.
    Drag to reorder; anything left of the AMP tile hits the amp's input, anything
    right of it runs after the cab. Click a pedal tile to switch it on or off.
*/
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    explicit ChainStrip (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        order = amp::pedal::defaultChainOrder();
        startTimerHz (8);
    }

    std::function<amp::pedal::ChainOrder()> getOrder;
    std::function<void (const amp::pedal::ChainOrder&)> setOrder;

    void paint (juce::Graphics& g) override
    {
        g.setColour (AmpLookAndFeel::panelDark.brighter (0.05f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

        for (int pos = 0; pos < amp::pedal::kChainLength; ++pos)
            if (pos != dragPos)
                drawTile (g, order[(size_t) pos], tileBounds (pos), false);

        // The tile being dragged is painted last so it floats over the others.
        if (dragPos >= 0)
        {
            auto r = tileBounds (dragPos).withX (dragX - tileWidth() * 0.5f);
            drawTile (g, order[(size_t) dragPos], r, true);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragPos = positionAt (e.position.x);
        dragX = e.position.x;
        moved = false;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragPos < 0)
            return;

        moved = true;
        dragX = e.position.x;

        const int target = positionAt (dragX);
        if (target != dragPos && target >= 0)
        {
            const int item = order[(size_t) dragPos];
            if (target < dragPos)
                std::rotate (order.begin() + target, order.begin() + dragPos, order.begin() + dragPos + 1);
            else
                std::rotate (order.begin() + dragPos, order.begin() + dragPos + 1, order.begin() + target + 1);
            order[(size_t) target] = item;
            dragPos = target;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragPos >= 0)
        {
            if (moved)
            {
                if (setOrder != nullptr)
                    setOrder (order);
            }
            else
            {
                // A click, not a drag: toggle that pedal.
                const int item = order[(size_t) dragPos];
                if (item != amp::pedal::kAmpToken)
                    if (auto* p = apvts.getParameter (juce::String (amp::pedal::pedalTag ((amp::pedal::Id) item)) + "On"))
                        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
            }
        }
        dragPos = -1;
        repaint();
    }

private:
    void timerCallback() override
    {
        if (dragPos >= 0 || getOrder == nullptr)
            return;

        const auto current = getOrder();
        if (current != order)
        {
            order = current;
            repaint();
        }
        repaint();   // keeps the on/off dimming in step with the stomp switches
    }

    float tileWidth() const
    {
        return (float) (getWidth() - 2 * kMargin) / (float) amp::pedal::kChainLength;
    }

    juce::Rectangle<float> tileBounds (int pos) const
    {
        const float w = tileWidth();
        return { kMargin + w * pos, 4.0f, w - 4.0f, (float) getHeight() - 8.0f };
    }

    int positionAt (float x) const
    {
        return juce::jlimit (0, amp::pedal::kChainLength - 1,
                             (int) ((x - kMargin) / juce::jmax (1.0f, tileWidth())));
    }

    bool pedalIsOn (int item) const
    {
        if (auto* v = apvts.getRawParameterValue (
                juce::String (amp::pedal::pedalTag ((amp::pedal::Id) item)) + "On"))
            return v->load() > 0.5f;
        return false;
    }

    void drawTile (juce::Graphics& g, int item, juce::Rectangle<float> r, bool dragging)
    {
        const bool isAmp = (item == amp::pedal::kAmpToken);
        const bool on = isAmp || pedalIsOn (item);

        auto body = isAmp ? AmpLookAndFeel::accent.withAlpha (0.9f)
                          : getPedalSkin ((amp::pedal::Id) item).body;
        if (! on)
            body = body.withSaturation (0.15f).darker (0.45f);

        if (dragging)
        {
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (r.translated (2.0f, 3.0f), 5.0f);
        }

        g.setColour (body);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (isAmp ? AmpLookAndFeel::cream.withAlpha (0.9f)
                           : juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (r, 5.0f, isAmp ? 1.4f : 1.0f);

        g.setColour (isAmp ? AmpLookAndFeel::panelDark
                           : (on ? getPedalSkin ((amp::pedal::Id) item).text
                                 : AmpLookAndFeel::textLight.withAlpha (0.5f)));
        g.setFont (juce::Font (isAmp ? 12.0f : 10.5f, juce::Font::bold));
        g.drawText (isAmp ? "AMP" : amp::pedal::pedalShortName ((amp::pedal::Id) item),
                    r, juce::Justification::centred);
    }

    static constexpr float kMargin = 6.0f;

    juce::AudioProcessorValueTreeState& apvts;
    amp::pedal::ChainOrder order;
    int dragPos = -1;
    float dragX = 0.0f;
    bool moved = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainStrip)
};

//==============================================================================
/** The pedalboard page: the chain strip, then eight pedals on a dark board. */
class PedalBoardView : public juce::Component
{
public:
    explicit PedalBoardView (juce::AudioProcessorValueTreeState& apvts)
        : chain (std::make_unique<ChainStrip> (apvts))
    {
        addAndMakeVisible (*chain);

        for (int i = 0; i < amp::pedal::kNumPedals; ++i)
        {
            auto p = std::make_unique<PedalComponent> (apvts, (amp::pedal::Id) i);
            addAndMakeVisible (*p);
            pedals.push_back (std::move (p));
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (AmpLookAndFeel::panelMid.darker (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (AmpLookAndFeel::accent.withAlpha (0.25f));
        g.drawRoundedRectangle (getLocalBounds().toFloat(), 8.0f, 1.0f);

        g.setColour (AmpLookAndFeel::textLight.withAlpha (0.45f));
        g.setFont (juce::Font (10.5f));
        g.drawText ("Drag tiles to reorder. Left of AMP = into the amp input (put dirt there). "
                    "Right of AMP = effects loop. Click a tile to switch it on.",
                    getLocalBounds().removeFromBottom (18).reduced (14, 2),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        area.removeFromBottom (18);                        // footer line
        chain->setBounds (area.removeFromTop (40));
        area.removeFromTop (6);

        const int cols = 4;
        const int rows = 2;
        const int cw = area.getWidth() / cols;
        const int ch = area.getHeight() / rows;

        for (int i = 0; i < (int) pedals.size(); ++i)
            pedals[(size_t) i]->setBounds (juce::Rectangle<int> (area.getX() + (i % cols) * cw,
                                                                 area.getY() + (i / cols) * ch,
                                                                 cw, ch).reduced (3, 12));
    }

    ChainStrip& chainStrip() { return *chain; }

private:
    std::unique_ptr<ChainStrip> chain;
    std::vector<std::unique_ptr<PedalComponent>> pedals;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalBoardView)
};
