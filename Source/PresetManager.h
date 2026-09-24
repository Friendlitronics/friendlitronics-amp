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

/**
    User presets, stored as one XML file per preset under

        ~/Library/Application Support/Amp Sim/Presets/

    A preset always contains the whole APVTS state, but carries a **scope** that
    says how much of it to apply when loaded:

      Full    - everything (amp, cab, mic, pedals, chain order, colour)
      Amp     - the amp side only, so it can be dropped onto a board you like
      Pedals  - the pedalboard and chain order only, leaving the amp alone

    That separation is the point: rigs are usually "this board through that amp",
    and the two halves get swapped independently.

    The scope is also encoded in the filename prefix, so two presets can share a
    name across scopes without colliding.
*/
namespace PresetManager
{
    enum class Scope { Full = 0, Amp, Pedals };

    struct Entry
    {
        juce::String name;
        Scope scope = Scope::Full;
    };

    struct LoadResult
    {
        bool ok = false;
        Scope scope = Scope::Full;
        juce::String chainOrder;      // non-empty when the preset carried one
    };

    inline juce::File directory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Amp Sim")
                   .getChildFile ("Presets");
    }

    inline juce::String prefixFor (Scope s)
    {
        switch (s)
        {
            case Scope::Amp:    return "amp_";
            case Scope::Pedals: return "board_";
            default:            return "full_";
        }
    }

    inline juce::String scopeName (Scope s)
    {
        switch (s)
        {
            case Scope::Amp:    return "Amp only";
            case Scope::Pedals: return "Pedalboard only";
            default:            return "Everything";
        }
    }

    inline juce::String sanitise (const juce::String& name)
    {
        return name.trim().removeCharacters ("\\/:*?\"<>|").substring (0, 60);
    }

    inline juce::File fileFor (const juce::String& name, Scope scope)
    {
        return directory().getChildFile (prefixFor (scope) + sanitise (name) + ".ampsim");
    }

    /** Every saved preset, sorted by name within each scope. */
    inline std::vector<Entry> listAll()
    {
        std::vector<Entry> entries;
        auto dir = directory();
        if (! dir.isDirectory())
            return entries;

        for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.ampsim"))
        {
            const auto file = f.getFileNameWithoutExtension();
            Entry e;
            if (file.startsWith ("amp_"))        { e.scope = Scope::Amp;    e.name = file.substring (4); }
            else if (file.startsWith ("board_")) { e.scope = Scope::Pedals; e.name = file.substring (6); }
            else if (file.startsWith ("full_"))  { e.scope = Scope::Full;   e.name = file.substring (5); }
            else                                 { e.scope = Scope::Full;   e.name = file; }
            entries.push_back (e);
        }

        std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
        {
            if (a.scope != b.scope) return (int) a.scope < (int) b.scope;
            return a.name.compareNatural (b.name) < 0;
        });
        return entries;
    }

    inline std::vector<Entry> listOf (Scope scope)
    {
        std::vector<Entry> out;
        for (const auto& e : listAll())
            if (e.scope == scope)
                out.push_back (e);
        return out;
    }

    /** True for the parameters that belong to the pedalboard. */
    inline bool isPedalParam (const juce::String& id)
    {
        for (int i = 0; i < amp::pedal::kNumPedals; ++i)
        {
            const juce::String tag (amp::pedal::pedalTag ((amp::pedal::Id) i));
            if (id.startsWith (tag))
                return true;
        }
        return false;
    }

    inline bool inScope (const juce::String& id, Scope scope)
    {
        switch (scope)
        {
            case Scope::Amp:    return ! isPedalParam (id);
            case Scope::Pedals: return isPedalParam (id);
            default:            return true;
        }
    }

    inline bool save (const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
                      Scope scope, const juce::String& chainOrder)
    {
        const auto clean = sanitise (name);
        if (clean.isEmpty())
            return false;

        auto dir = directory();
        if (! dir.isDirectory() && ! dir.createDirectory().wasOk())
            return false;

        auto state = apvts.copyState();
        state.setProperty ("chainOrder", chainOrder, nullptr);

        if (auto xml = state.createXml())
        {
            xml->setAttribute ("ampSimScope", (int) scope);
            return xml->writeTo (fileFor (clean, scope));
        }
        return false;
    }

    /** Applies a preset. For a scoped preset only the matching parameters are
        touched, so loading a board leaves the amp exactly as it was. */
    inline LoadResult load (const juce::String& name, Scope scope,
                            juce::AudioProcessorValueTreeState& apvts)
    {
        LoadResult result;
        auto file = fileFor (name, scope);
        if (! file.existsAsFile())
            return result;

        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr)
            return result;

        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid())
            return result;

        result.scope = scope;
        result.chainOrder = tree.getProperty ("chainOrder").toString();

        if (scope == Scope::Full)
        {
            apvts.replaceState (tree);
        }
        else
        {
            // Walk the saved parameters and apply only the ones in scope.
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                auto child = tree.getChild (i);
                if (! child.hasProperty ("id") || ! child.hasProperty ("value"))
                    continue;

                const auto id = child.getProperty ("id").toString();
                if (! inScope (id, scope))
                    continue;

                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
            }

            if (scope == Scope::Amp)
                result.chainOrder.clear();     // an amp preset must not move pedals
        }

        result.ok = true;
        return result;
    }

    inline bool remove (const juce::String& name, Scope scope)
    {
        return fileFor (name, scope).deleteFile();
    }
}
