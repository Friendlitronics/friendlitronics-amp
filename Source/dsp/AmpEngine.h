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
#include <cmath>

#include "Oversampler.h"
#include "AmpVoicing.h"
#include "InputStage.h"
#include "AcousticImager.h"
#include "BassStage.h"
#include "PreampStage.h"
#include "ToneStack.h"
#include "PowerAmp.h"
#include "SpringReverb.h"
#include "Cabinet.h"
#include "Mic.h"
#include "MicPosition.h"
#include "RoomSim.h"
#include "LoFiStage.h"
#include "TapeColour.h"
#include "pedals/PedalBoard.h"

namespace amp
{

/**
    Top-level amp signal chain. Owns and wires every stage, manages the single
    oversampling region (preamp -> tone stack -> power amp), and reloads voicing
    coefficients when the amp/cab/mic selection changes.

    Signal flow:
      input -> InputStage
            -> AcousticImager (Acoustic amp only)
            -> [up] Pedals (pre) -> BassStage (bass amps only)
                    -> Preamp -> ToneStack -> PowerAmp [down]
            -> SpringReverb (if model has reverb)
            -> Cabinet -> Mic -> MicPosition -> RoomSim -> Pedals (post)
            -> LoFiStage -> TapeColour

    The Line amp skips the preamp and power stage, and cab / mic set to "None"
    drop out of the chain, which turns the whole thing into a colour box.
*/
class AmpEngine
{
public:
    struct Params
    {
        int   ampModel = 0, cabModel = 0, micModel = 0;
        float gain = 4, bass = 5, mid = 5, treble = 6, presence = 5, resonance = 4, cut = 3;
        float master = 5, reverb = 0, sweetness = 5;
        float body = 6, deQuack = 6, comp = 4, colour = 2;   // Acoustic amp only
        float blend = 0, split = 5, grit = 3, sub = 0;       // bass amps only
        float tape = 0, flutter = 0;                         // any amp
        float crush = 0, srate = 0, noise = 0, narrow = 0;   // any amp (lofi)
        pedal::PedalBoard::State pedals {};
        float micDistance = 0.15f, micAxis = 0.3f, micAngle = 0.0f, room = 0.0f;
    };

    void prepare (double sampleRate, int numChannels, int blockSize)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = sampleRate;
        spec.maximumBlockSize = (juce::uint32) blockSize;
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

        oversampler.prepare (numChannels, blockSize);
        osFactor = juce::jmax (1, oversampler.getOversamplingFactor());

        juce::dsp::ProcessSpec osSpec = spec;
        osSpec.sampleRate       = sampleRate * osFactor;
        osSpec.maximumBlockSize = (juce::uint32) (blockSize * osFactor);

        inputStage.prepare (spec);
        imager.prepare (spec);
        bass.prepare (osSpec);
        pedals.prepare (osSpec, spec);   // pre runs oversampled, post at host rate
        preamp.prepare (osSpec);
        toneStack.prepare (osSpec);
        powerAmp.prepare (osSpec);
        spring.prepare (spec);
        cabinet.prepare (spec);
        mic.prepare (spec);
        micPos.prepare (spec);
        room.prepare (spec);
        lofi.prepare (spec);
        tape.prepare (spec);

        // Force a voicing load on first process.
        loadedAmp = loadedCab = loadedMic = -1;
        applySelections (params, true);
    }

    void reset()
    {
        oversampler.reset();
        inputStage.reset(); imager.reset(); bass.reset();
        preamp.reset(); toneStack.reset(); powerAmp.reset();
        spring.reset(); cabinet.reset(); mic.reset(); micPos.reset(); room.reset();
        lofi.reset(); tape.reset(); pedals.reset();
    }

    int getLatencySamples() const { return oversampler.getLatencySamples(); }

    bool currentModelHasReverb() const { return getAmpVoicing ((AmpModel) params.ampModel).hasReverb; }

    void setParams (const Params& p) { params = p; }

    void process (juce::dsp::AudioBlock<float>& block)
    {
        applySelections (params, false);

        // Continuous controls (cheap biquad recomputes; harmless per block).
        preamp.setGain (params.gain * 0.1f);
        toneStack.setControls (params.bass, params.mid, params.treble, params.cut);
        powerAmp.setMaster (params.master * 0.1f);
        powerAmp.setPresence (params.presence * 0.1f);
        powerAmp.setResonance (params.resonance * 0.1f);

        // Sweetness macro (5 = neutral): morphs even-harmonic asymmetry, the cab
        // bell-peak lift, and the class-A bloom together.
        const float sweetDelta = (params.sweetness - 5.0f) / 5.0f; // -1..+1
        preamp.setAsymScale (1.0f + 0.8f * sweetDelta);            // 0.2..1.8
        cabinet.setExtraPeakDb (3.0f * sweetDelta);                // -3..+3 dB
        powerAmp.setSagScale (1.0f + 0.4f * sweetDelta);           // 0.6..1.4
        micPos.setDistance (params.micDistance);
        micPos.setAxis (params.micAxis);
        micPos.setAngle (params.micAngle);
        room.setAmount (params.room);

        const bool hasReverb = currentModelHasReverb();
        if (hasReverb)
            spring.setAmount (params.reverb);

        const auto voicing = getAmpVoicing ((AmpModel) params.ampModel);
        const bool acoustic = voicing.acousticImager;
        if (acoustic)
            imager.setControls (params.body * 0.1f, params.deQuack * 0.1f,
                                params.comp * 0.1f, params.colour * 0.1f);

        const bool isBass = voicing.bassStage;
        if (isBass)
            bass.setControls (params.blend * 0.1f, params.split * 0.1f,
                              params.grit * 0.1f, params.sub * 0.1f);

        lofi.setControls (params.crush * 0.1f, params.srate * 0.1f,
                          params.noise * 0.1f, params.narrow * 0.1f);
        tape.setControls (params.tape * 0.1f, params.flutter * 0.1f);
        pedals.setState (params.pedals);

        // --- pre-oversampling ---
        inputStage.process (block);
        if (acoustic)
            imager.process (block);

        // --- oversampled nonlinear core ---
        // The bracket runs even in line mode (where only the tone stack is
        // inside it) so the reported latency never changes with the amp choice.
        const bool line = voicing.lineMode;
        auto osBlock = oversampler.processSamplesUp (block);
        if (pedals.anyIn (pedal::Slot::Pre))
            pedals.process (osBlock, pedal::Slot::Pre);
        if (isBass)
            bass.process (osBlock);
        if (! line)
            preamp.process (osBlock);
        toneStack.process (osBlock);
        if (! line)
            powerAmp.process (osBlock);
        oversampler.processSamplesDown (block);

        // JUCE's 2-stage polyphase up/down round trip comes back 2x hot. Every
        // amp voicing was tuned with that baked in, but line mode has no gain
        // stage to absorb it, so undo it here — before LoFi and Tape, whose
        // saturation and quantisation are both level-dependent.
        if (line)
            block.multiplyBy (0.5f);

        // --- base-rate post chain ---
        if (hasReverb)
            spring.process (block);
        if (! cabIsLine ((CabModel) params.cabModel))
            cabinet.process (block);
        if (! micIsLine ((MicModel) params.micModel))
        {
            mic.process (block);
            micPos.process (block);   // placement is meaningless with no mic
        }
        room.process (block);
        if (pedals.anyIn (pedal::Slot::Post))
            pedals.process (block, pedal::Slot::Post);
        lofi.process (block);
        tape.process (block);

        // Per-model makeup so switching amps doesn't jump in level.
        const float trim = dbToGain (getAmpVoicing ((AmpModel) params.ampModel).outputTrimDb);
        if (std::abs (trim - 1.0f) > 1.0e-4f)
            block.multiplyBy (trim);
    }

private:
    void applySelections (const Params& p, bool force)
    {
        if (force || p.ampModel != loadedAmp)
        {
            const auto v = getAmpVoicing ((AmpModel) p.ampModel);
            inputStage.setVoicing (v);
            if (v.acousticImager)
                imager.reset();   // don't replay stale body/reflection tails
            if (v.bassStage)
                bass.reset();
            preamp.setVoicing (v);
            toneStack.setType (v.toneType);
            powerAmp.setVoicing (v);
            loadedAmp = p.ampModel;
        }
        if (force || p.cabModel != loadedCab)
        {
            cabinet.setVoicing (getCabVoicing ((CabModel) p.cabModel));
            loadedCab = p.cabModel;
        }
        if (force || p.micModel != loadedMic)
        {
            mic.setVoicing (getMicVoicing ((MicModel) p.micModel));
            loadedMic = p.micModel;
        }
    }

    Params params;
    int loadedAmp = -1, loadedCab = -1, loadedMic = -1;
    int osFactor = 1;

    Oversampler oversampler;
    InputStage  inputStage;
    AcousticImager imager;
    BassStage   bass;
    PreampStage preamp;
    ToneStack   toneStack;
    PowerAmp    powerAmp;
    SpringReverb spring;
    Cabinet     cabinet;
    Mic         mic;
    MicPosition micPos;
    RoomSim     room;
    pedal::PedalBoard pedals;
    LoFiStage   lofi;
    TapeColour  tape;
};

} // namespace amp
