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

#include "PluginEditor.h"
#include "Presets.h"
#include "PresetManager.h"
#include "PedalPresets.h"
#include "dsp/AmpVoicing.h"

AmpSimAudioProcessorEditor::AmpSimAudioProcessorEditor (AmpSimAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    // Title.
    titleLabel.setText ("FRIENDLITRONICS AMP", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, AmpLookAndFeel::cream);
    addAndMakeVisible (titleLabel);

    // Preset menu.
    presetLabel.setText ("PRESET", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    presetLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetLabel);

    presetBox.onChange = [this] { presetSelected(); };
    addAndMakeVisible (presetBox);

    savePresetButton.onClick = [this] { savePresetDialog(); };
    deletePresetButton.onClick = [this] { deleteSelectedPreset(); };
    for (auto* b : { &savePresetButton, &deletePresetButton })
    {
        b->setColour (juce::TextButton::buttonColourId, AmpLookAndFeel::panelMid);
        b->setColour (juce::TextButton::textColourOffId, AmpLookAndFeel::textLight);
        addAndMakeVisible (*b);
    }
    refreshPresetMenu();

    // Amp/Cab/Mic selectors.
    setupSelector (ampSelector, "ampModel", "AMP", amp::ampNames());
    setupSelector (cabSelector, "cabModel", "CAB", amp::cabNames());
    setupSelector (micSelector, "micModel", "MIC", amp::micNames());

    // Amp knobs.
    gainKnob   = &addKnob (ampKnobs, "gain",      "GAIN");
    addKnob (ampKnobs, "bass",      "BASS");
    addKnob (ampKnobs, "mid",       "MID");
    addKnob (ampKnobs, "treble",    "TREBLE");
    presenceKnob  = &addKnob (ampKnobs, "presence",  "PRESENCE");
    resonanceKnob = &addKnob (ampKnobs, "resonance", "RESON");
    sweetKnob     = &addKnob (ampKnobs, "sweetness", "SWEET");
    cutKnob    = &addKnob (ampKnobs, "cut",    "CUT");
    masterKnob = &addKnob (ampKnobs, "master",    "MASTER");
    reverbKnob = &addKnob (ampKnobs, "reverb", "REVERB");

    // Acoustic piezo imager.
    addKnob (acousticKnobs, "body",    "BODY");
    addKnob (acousticKnobs, "deQuack", "DE-QUACK");
    addKnob (acousticKnobs, "comp",    "COMP");
    addKnob (acousticKnobs, "colour",  "COLOUR");

    // Bass drive front end.
    addKnob (bassKnobs, "blend", "BLEND");
    addKnob (bassKnobs, "split", "SPLIT");
    addKnob (bassKnobs, "grit",  "GRIT");
    addKnob (bassKnobs, "sub",   "SUB");

    // LoFi degradation.
    addKnob (lofiKnobs, "crush",  "CRUSH");
    addKnob (lofiKnobs, "srate",  "SR");
    addKnob (lofiKnobs, "noise",  "NOISE");
    addKnob (lofiKnobs, "narrow", "NARROW");

    // Tape colour (works with every amp).
    addKnob (tapeKnobs, "tape",    "TAPE");
    addKnob (tapeKnobs, "flutter", "FLUTTER");

    // Mic placement. The first three are greyed when there is no mic in the
    // chain; Room stays live, since it is just an ambience send.
    addKnob (micKnobs, "micDistance", "DISTANCE");
    addKnob (micKnobs, "micAxis",     "AXIS");
    addKnob (micKnobs, "micAngle",    "ANGLE");
    addKnob (micKnobs, "room",        "ROOM");

    // Output section.
    addKnob (outKnobs, "output", "OUTPUT");
    addKnob (outKnobs, "mix",    "MIX");

    bypassAttachment = std::make_unique<ButtonAttachment> (
        processorRef.getValueTreeState(), "bypass", bypassButton);
    addAndMakeVisible (bypassButton);

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    // Everything built so far belongs to the amp page; collect it before the
    // pedalboard is added so the two pages can be swapped wholesale.
    for (auto* c : getChildren())
        if (c != &titleLabel && c != &presetLabel && c != &presetBox)
            ampViewComponents.push_back (c);

    pedalsView = std::make_unique<PedalBoardView> (processorRef.getValueTreeState());
    pedalsView->chainStrip().getOrder = [this] { return processorRef.getChainOrder(); };
    pedalsView->chainStrip().setOrder = [this] (const amp::pedal::ChainOrder& o)
    {
        processorRef.setChainOrder (o);
    };
    addChildComponent (*pedalsView);

    auto setupTab = [this] (juce::TextButton& b, int tab)
    {
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, AmpLookAndFeel::panelMid);
        b.setColour (juce::TextButton::buttonOnColourId, AmpLookAndFeel::accent.withAlpha (0.85f));
        b.setColour (juce::TextButton::textColourOffId, AmpLookAndFeel::textLight);
        b.setColour (juce::TextButton::textColourOnId, AmpLookAndFeel::panelDark);
        b.onClick = [this, tab] { setTab (tab); };
        addAndMakeVisible (b);
    };
    setupTab (ampTabButton, 0);
    setupTab (pedalsTabButton, 1);

    setSize (940, 610);
    setTab (0);
    startTimerHz (30);
}

AmpSimAudioProcessorEditor::~AmpSimAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

AmpSimAudioProcessorEditor::Knob&
AmpSimAudioProcessorEditor::addKnob (std::vector<std::unique_ptr<Knob>>& target,
                                     const juce::String& paramID, const juce::String& text)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 15);
    addAndMakeVisible (knob->slider);

    knob->label.setText (text, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (knob->label);

    knob->attachment = std::make_unique<SliderAttachment> (
        processorRef.getValueTreeState(), paramID, knob->slider);

    target.push_back (std::move (knob));
    return *target.back();
}

void AmpSimAudioProcessorEditor::setupSelector (Selector& sel, const juce::String& paramID,
                                                const juce::String& text,
                                                const juce::StringArray& items)
{
    sel.label.setText (text, juce::dontSendNotification);
    sel.label.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (sel.label);

    for (int i = 0; i < items.size(); ++i)
        sel.box.addItem (items[i], i + 1);

    addAndMakeVisible (sel.box);
    sel.attachment = std::make_unique<ComboBoxAttachment> (
        processorRef.getValueTreeState(), paramID, sel.box);
}

void AmpSimAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (AmpLookAndFeel::panelDark);

    // Faceplate panels.
    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        if (r.isEmpty()) return;
        auto rf = r.toFloat();
        g.setColour (AmpLookAndFeel::panelMid);
        g.fillRoundedRectangle (rf, 8.0f);
        g.setColour (AmpLookAndFeel::accent.withAlpha (0.35f));
        g.drawRoundedRectangle (rf, 8.0f, 1.0f);
        g.setColour (AmpLookAndFeel::accent.withAlpha (0.85f));
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (title, r.removeFromTop (18).reduced (10, 2),
                    juce::Justification::topLeft);
    };

    // Tab strip underline.
    {
        auto strip = tabStrip.toFloat();
        g.setColour (AmpLookAndFeel::accent.withAlpha (0.25f));
        g.fillRect (strip.removeFromBottom (1.0f));
    }

    if (currentTab != 0)
        return;   // the pedalboard page paints itself

    drawPanel (ampPanel, "AMPLIFIER");
    drawPanel (instrumentPanel, acousticActive ? "PIEZO IMAGING  /  TAPE"
                                               : (bassActive ? "BASS DRIVE  /  TAPE"
                                                             : "LOFI  /  TAPE"));
    drawPanel (micPanel, "MIC PLACEMENT  /  OUTPUT");

    g.setColour (AmpLookAndFeel::textLight.withAlpha (0.7f));
    g.setFont (juce::Font (11.0f));
    g.drawFittedText (acousticActive
                          ? "Piezo DI: BODY adds the resonance a mic hears, DE-QUACK tames "
                            "the 2-3 kHz attack, COLOUR scoops mids."
                          : (bassActive
                                 ? "Lows stay clean; only the band above SPLIT is driven. "
                                   "BLEND sets how dirty, SUB adds the octave below."
                                 : (lineActive
                                        ? "No amp, no cab: tone stack plus colour only. "
                                          "Everything here at 0 is a straight wire."
                                        : "CRUSH bits, SR holds samples, NOISE adds hiss and "
                                          "crackle, NARROW collapses to mono.")),
                      instrumentInfo, juce::Justification::topLeft, 6);
}

void AmpSimAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Header.
    auto header = area.removeFromTop (34);
    titleLabel.setBounds (header.removeFromLeft (260));
    deletePresetButton.setBounds (header.removeFromRight (44).withSizeKeepingCentre (42, 24));
    header.removeFromRight (4);
    savePresetButton.setBounds (header.removeFromRight (52).withSizeKeepingCentre (50, 24));
    header.removeFromRight (8);
    presetBox.setBounds (header.removeFromRight (200).withSizeKeepingCentre (200, 26));
    presetLabel.setBounds (header.removeFromRight (56).withSizeKeepingCentre (56, 26));

    // Tab strip.
    tabStrip = area.removeFromTop (26);
    {
        auto t = tabStrip;
        ampTabButton.setBounds (t.removeFromLeft (110).reduced (1, 2));
        t.removeFromLeft (6);
        pedalsTabButton.setBounds (t.removeFromLeft (110).reduced (1, 2));
    }

    area.removeFromTop (8);
    if (pedalsView != nullptr)
        pedalsView->setBounds (area);

    // Selectors row.
    auto selRow = area.removeFromTop (46);
    auto layoutSelector = [] (Selector& s, juce::Rectangle<int> r)
    {
        s.label.setBounds (r.removeFromTop (16));
        s.box.setBounds (r.withSizeKeepingCentre (r.getWidth(), 26));
    };
    const int selW = (selRow.getWidth() - 20) / 3;
    layoutSelector (ampSelector, selRow.removeFromLeft (selW));
    selRow.removeFromLeft (10);
    layoutSelector (cabSelector, selRow.removeFromLeft (selW));
    selRow.removeFromLeft (10);
    layoutSelector (micSelector, selRow);

    area.removeFromTop (10);

    // Amp panel (row of knobs).
    ampPanel = area.removeFromTop (160);
    auto ampInner = ampPanel.reduced (10);
    ampInner.removeFromTop (16); // panel title
    const int nAmp = (int) ampKnobs.size();
    const int kw = ampInner.getWidth() / juce::jmax (1, nAmp);
    for (int i = 0; i < nAmp; ++i)
    {
        auto cell = ampInner.removeFromLeft (kw);
        ampKnobs[(size_t) i]->label.setBounds (cell.removeFromTop (16));
        ampKnobs[(size_t) i]->slider.setBounds (cell);
    }

    area.removeFromTop (10);

    // Instrument panel: four context knobs (acoustic OR bass, swapped by the
    // selected amp and occupying the same cells), then the tape pair, then text.
    instrumentPanel = area.removeFromTop (130);
    {
        auto inner = instrumentPanel.reduced (10);
        inner.removeFromTop (16);
        const int kwv = 108;
        auto place = [] (Knob& k, juce::Rectangle<int> cell)
        {
            k.label.setBounds (cell.removeFromTop (16));
            k.slider.setBounds (cell);
        };
        for (size_t i = 0; i < 4; ++i)
        {
            auto cell = inner.removeFromLeft (kwv);
            if (i < acousticKnobs.size()) place (*acousticKnobs[i], cell);
            if (i < bassKnobs.size())     place (*bassKnobs[i], cell);
            if (i < lofiKnobs.size())     place (*lofiKnobs[i], cell);
        }
        inner.removeFromLeft (10);
        for (auto& k : tapeKnobs)
            place (*k, inner.removeFromLeft (kwv));
        instrumentInfo = inner.reduced (12, 2);
    }

    area.removeFromTop (10);

    // Mic placement + output panel.
    micPanel = area;
    auto micInner = micPanel.reduced (10);
    micInner.removeFromTop (16);

    // Right portion: meters + bypass + output/mix.
    auto right = micInner.removeFromRight (250);

    auto meters = right.removeFromRight (90);
    bypassButton.setBounds (right.removeFromBottom (28).reduced (4, 0));
    {
        const int mw = (meters.getWidth() - 8) / 2;
        inputMeter.setBounds (meters.removeFromLeft (mw));
        meters.removeFromLeft (8);
        outputMeter.setBounds (meters);
    }
    {
        const int ow = right.getWidth() / juce::jmax (1, (int) outKnobs.size());
        for (auto& k : outKnobs)
        {
            auto cell = right.removeFromLeft (ow);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell);
        }
    }

    micInner.removeFromRight (16);

    // Left portion: the four placement knobs.
    const int mn = (int) micKnobs.size();
    const int mkw = micInner.getWidth() / juce::jmax (1, mn);
    for (int i = 0; i < mn; ++i)
    {
        auto cell = micInner.removeFromLeft (mkw);
        micKnobs[(size_t) i]->label.setBounds (cell.removeFromTop (16));
        micKnobs[(size_t) i]->slider.setBounds (cell);
    }
}

void AmpSimAudioProcessorEditor::refreshPresetMenu (const juce::String& keepName)
{
    presetBox.clear (juce::dontSendNotification);
    userPresets = PresetManager::listAll();

    presetBox.addSectionHeading ("FACTORY RIGS");
    for (int i = 0; i < (int) Presets::all().size(); ++i)
        presetBox.addItem (Presets::all()[(size_t) i].name, i + 1);

    presetBox.addSeparator();
    presetBox.addSectionHeading ("FACTORY PEDALBOARDS");
    for (int i = 0; i < (int) PedalPresets::all().size(); ++i)
        presetBox.addItem (PedalPresets::all()[(size_t) i].name, kFirstBoardId + i);

    auto addUserSection = [this] (PresetManager::Scope scope, const juce::String& heading)
    {
        bool headed = false;
        for (int i = 0; i < (int) userPresets.size(); ++i)
        {
            if (userPresets[(size_t) i].scope != scope)
                continue;
            if (! headed)
            {
                presetBox.addSeparator();
                presetBox.addSectionHeading (heading);
                headed = true;
            }
            presetBox.addItem (userPresets[(size_t) i].name, kFirstUserId + i);
        }
    };
    addUserSection (PresetManager::Scope::Full,   "MY RIGS");
    addUserSection (PresetManager::Scope::Amp,    "MY AMPS");
    addUserSection (PresetManager::Scope::Pedals, "MY PEDALBOARDS");

    if (keepName.isNotEmpty())
    {
        for (int i = 0; i < (int) userPresets.size(); ++i)
            if (userPresets[(size_t) i].name == keepName)
            {
                presetBox.setSelectedId (kFirstUserId + i, juce::dontSendNotification);
                deletePresetButton.setEnabled (true);
                return;
            }
    }

    presetBox.setSelectedId (processorRef.getCurrentProgram() + 1, juce::dontSendNotification);
    deletePresetButton.setEnabled (false);
}

bool AmpSimAudioProcessorEditor::isUserPresetSelected() const
{
    return presetBox.getSelectedId() >= kFirstUserId;
}

void AmpSimAudioProcessorEditor::presetSelected()
{
    const int id = presetBox.getSelectedId();
    if (id <= 0)
        return;

    if (id >= kFirstUserId)
    {
        const auto& entry = userPresets[(size_t) (id - kFirstUserId)];
        const auto result = PresetManager::load (entry.name, entry.scope,
                                                 processorRef.getValueTreeState());
        if (result.ok)
        {
            if (result.scope == PresetManager::Scope::Full)
                processorRef.readChainOrderFromState();
            else if (result.chainOrder.isNotEmpty())
                processorRef.setChainOrder (amp::pedal::chainOrderFromString (result.chainOrder));
        }
    }
    else if (id >= kFirstBoardId)
    {
        // A factory pedalboard: pedals and chain only, amp untouched.
        PedalPresets::apply (processorRef.getValueTreeState(), id - kFirstBoardId,
                             [this] (const amp::pedal::ChainOrder& o) { processorRef.setChainOrder (o); });
    }
    else
    {
        processorRef.setCurrentProgram (id - 1);
        processorRef.setChainOrder (amp::pedal::defaultChainOrder());
    }

    deletePresetButton.setEnabled (isUserPresetSelected());
    applyModelVisibility();
}

void AmpSimAudioProcessorEditor::savePresetDialog()
{
    presetDialog = std::make_unique<juce::AlertWindow> (
        "Save preset", "Save the whole rig, or just one half of it.",
        juce::MessageBoxIconType::NoIcon);

    presetDialog->addTextEditor ("name", presetBox.getText(), "Name:");
    presetDialog->addComboBox ("scope", { PresetManager::scopeName (PresetManager::Scope::Full),
                                          PresetManager::scopeName (PresetManager::Scope::Amp),
                                          PresetManager::scopeName (PresetManager::Scope::Pedals) },
                               "Save:");
    presetDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    presetDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    presetDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this] (int result)
        {
            if (presetDialog == nullptr)
                return;

            const auto name = presetDialog->getTextEditorContents ("name");
            const int scopeIndex = presetDialog->getComboBoxComponent ("scope")->getSelectedItemIndex();
            presetDialog.reset();

            if (result == 1 && name.isNotEmpty())
            {
                const auto scope = (PresetManager::Scope) juce::jlimit (0, 2, scopeIndex);
                const auto order = amp::pedal::chainOrderToString (processorRef.getChainOrder());
                if (PresetManager::save (name, processorRef.getValueTreeState(), scope, order))
                    refreshPresetMenu (PresetManager::sanitise (name));
            }
        }), false);
}

void AmpSimAudioProcessorEditor::deleteSelectedPreset()
{
    if (! isUserPresetSelected())
        return;

    const auto entry = userPresets[(size_t) (presetBox.getSelectedId() - kFirstUserId)];
    juce::AlertWindow::showOkCancelBox (
        juce::MessageBoxIconType::QuestionIcon, "Delete preset",
        "Delete \"" + entry.name + "\" (" + PresetManager::scopeName (entry.scope).toLowerCase()
            + ")? This cannot be undone.", "Delete", "Cancel", this,
        juce::ModalCallbackFunction::create ([this, entry] (int result)
        {
            if (result == 1)
            {
                PresetManager::remove (entry.name, entry.scope);
                refreshPresetMenu();
            }
        }));
}

void AmpSimAudioProcessorEditor::setTab (int tab)
{
    currentTab = tab;
    for (auto* c : ampViewComponents)
        c->setVisible (tab == 0);
    if (pedalsView != nullptr)
        pedalsView->setVisible (tab == 1);

    ampTabButton.setToggleState (tab == 0, juce::dontSendNotification);
    pedalsTabButton.setToggleState (tab == 1, juce::dontSendNotification);

    if (tab == 0)
        applyModelVisibility();   // the amp page's rows depend on the model

    repaint();
}

void AmpSimAudioProcessorEditor::applyModelVisibility()
{
    const int ampModel = (int) std::round (
        processorRef.getValueTreeState().getRawParameterValue ("ampModel")->load());
    const int micModel = (int) std::round (
        processorRef.getValueTreeState().getRawParameterValue ("micModel")->load());
    lastAmpModel = ampModel;
    lastMicModel = micModel;

    {
        const auto v = amp::getAmpVoicing ((amp::AmpModel) ampModel);
        const bool isVox = (v.toneType == amp::ToneStackType::VoxTopBoost);

        if (reverbKnob) { reverbKnob->slider.setEnabled (v.hasReverb); reverbKnob->label.setEnabled (v.hasReverb); }
        if (cutKnob)    { cutKnob->slider.setEnabled (isVox);          cutKnob->label.setEnabled (isVox); }

        acousticActive = v.acousticImager;
        bassActive     = v.bassStage;
        lineActive     = v.lineMode;
        auto showGroup = [] (std::vector<std::unique_ptr<Knob>>& group, bool show)
        {
            for (auto& k : group)
            {
                k->slider.setVisible (show);
                k->label.setVisible (show);
            }
        };
        // One group occupies the row at a time: the piezo imager for the
        // acoustic amp, the drive stage for the bass amps, LoFi for everything
        // else (it works with any amp, and it's the point of the Line path).
        showGroup (acousticKnobs, acousticActive);
        showGroup (bassKnobs, bassActive);
        showGroup (lofiKnobs, ! acousticActive && ! bassActive);

        // Line mode has no preamp or power stage, so these do nothing.
        for (auto* k : { gainKnob, presenceKnob, resonanceKnob, sweetKnob, masterKnob })
            if (k != nullptr)
            {
                k->slider.setEnabled (! lineActive);
                k->label.setEnabled (! lineActive);
            }
        repaint (instrumentPanel);
    }

    {
        const bool placed = ! amp::micIsLine ((amp::MicModel) micModel);
        for (size_t i = 0; i < micKnobs.size() && i < 3; ++i)
        {
            micKnobs[i]->slider.setEnabled (placed);
            micKnobs[i]->label.setEnabled (placed);
        }
    }
}

void AmpSimAudioProcessorEditor::timerCallback()
{
    inputMeter.setLevel (processorRef.getInputLevel());
    outputMeter.setLevel (processorRef.getOutputLevel());

    if (currentTab != 0)
        return;

    const int ampModel = (int) std::round (
        processorRef.getValueTreeState().getRawParameterValue ("ampModel")->load());
    const int micModel = (int) std::round (
        processorRef.getValueTreeState().getRawParameterValue ("micModel")->load());
    if (ampModel != lastAmpModel || micModel != lastMicModel)
        applyModelVisibility();
}
