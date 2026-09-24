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
#include "PluginProcessor.h"
#include "ui/AmpLookAndFeel.h"
#include "ui/LevelMeter.h"
#include "ui/PedalComponent.h"
#include "PresetManager.h"
#include "PedalPresets.h"

#include <memory>
#include <vector>

/**
    Editor: amp/cab/mic selectors, the amp control knobs, a continuous
    mic-placement panel, output/mix, a bypass toggle and IN/OUT meters. Every
    control is wired through APVTS attachments. A 30 Hz timer animates the meters
    and enables/disables the model-specific knobs (Reverb, Cut, Acoustic panel).
*/
class AmpSimAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit AmpSimAudioProcessorEditor (AmpSimAudioProcessor&);
    ~AmpSimAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    /** Shows the amp page or the pedalboard page. */
    void setTab (int tab);
    /** Applies the amp/mic-dependent knob greying and row swapping. */
    void applyModelVisibility();

    /** Rebuilds the preset menu from the factory list plus the user's saved
        presets, and reselects `keepName` if it is still there. */
    void refreshPresetMenu (const juce::String& keepName = {});
    void presetSelected();
    void savePresetDialog();
    void deleteSelectedPreset();
    bool isUserPresetSelected() const;

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Selector
    {
        juce::Label    label;
        juce::ComboBox box;
        std::unique_ptr<ComboBoxAttachment> attachment;
    };

    Knob& addKnob (std::vector<std::unique_ptr<Knob>>& target,
                   const juce::String& paramID, const juce::String& text);
    void  setupSelector (Selector& sel, const juce::String& paramID,
                         const juce::String& text, const juce::StringArray& items);

    AmpSimAudioProcessor& processorRef;

    AmpLookAndFeel lookAndFeel;   // declared first so it outlives the components

    juce::Label titleLabel;

    juce::Label    presetLabel;
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "SAVE" }, deletePresetButton { "DEL" };
    std::vector<PresetManager::Entry> userPresets;   // index = menu id - kFirstUserId
    std::unique_ptr<juce::AlertWindow> presetDialog;

    static constexpr int kFirstBoardId = 501;    // factory pedalboards
    static constexpr int kFirstUserId  = 1001;   // everything the user saved

    // Tabs: 0 = amp, 1 = pedalboard.
    juce::TextButton ampTabButton { "AMP" }, pedalsTabButton { "PEDALS" };
    std::unique_ptr<PedalBoardView> pedalsView;
    std::vector<juce::Component*> ampViewComponents;
    int currentTab = 0;

    Selector ampSelector, cabSelector, micSelector;

    // Amp knobs (index-tracked so we can grey out model-specific ones).
    std::vector<std::unique_ptr<Knob>> ampKnobs;
    Knob* reverbKnob = nullptr;
    Knob* cutKnob    = nullptr;
    // Greyed in line mode, where there is no preamp or power stage.
    Knob* gainKnob = nullptr;
    Knob* presenceKnob = nullptr;
    Knob* resonanceKnob = nullptr;
    Knob* sweetKnob = nullptr;
    Knob* masterKnob = nullptr;

    // Instrument-specific rows. The Acoustic imager and the bass drive stage
    // belong to different amps, so they share one panel row and swap.
    std::vector<std::unique_ptr<Knob>> acousticKnobs;   // Acoustic amp
    std::vector<std::unique_ptr<Knob>> bassKnobs;       // bass amps
    std::vector<std::unique_ptr<Knob>> lofiKnobs;       // everything else
    std::vector<std::unique_ptr<Knob>> tapeKnobs;       // any amp
    bool acousticActive = false;
    bool bassActive = false;
    bool lineActive = false;

    // Mic placement + output knobs.
    std::vector<std::unique_ptr<Knob>> micKnobs;
    std::vector<std::unique_ptr<Knob>> outKnobs;

    juce::Rectangle<int> tabStrip, ampPanel, instrumentPanel, instrumentInfo, micPanel;

    juce::ToggleButton bypassButton { "Bypass" };
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    LevelMeter inputMeter  { "IN" };
    LevelMeter outputMeter { "OUT" };

    int lastAmpModel = -1;
    int lastMicModel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpSimAudioProcessorEditor)
};
