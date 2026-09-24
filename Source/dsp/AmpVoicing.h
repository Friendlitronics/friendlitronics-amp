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

namespace amp
{

//==============================================================================
// Model enumerations. Order must match the AudioParameterChoice string lists.
//==============================================================================
enum class AmpModel  { Twin = 0, AC30, Marshall, Champ, Acoustic,
                       BassSVT, BassFlipTop, BassFunk, BassModern, Line, NumAmps };
enum class CabModel  { TwoByTwelve = 0, AlnicoBlue, FourByTwelve, Champ, AcousticFullRange,
                       Bass810, Bass115, Bass118, Bass410T, NoCab, NumCabs };
enum class MicModel  { SM57 = 0, SM7B, CondenserLDC, CondenserSDC, NoMic, NumMics };

/** "None" selections take the speaker / microphone out of the chain entirely,
    which is what makes the plugin usable as a plain colour box. */
inline bool cabIsLine (CabModel c) { return c == CabModel::NoCab; }
inline bool micIsLine (MicModel m) { return m == MicModel::NoMic; }

enum class ToneStackType { FenderFMV, MarshallFMV, VoxTopBoost, ChampTilt, AcousticHiFi,
                           BassSVT, BassVintage, BassVariamp, BassHiFi, LineFlat };

inline juce::StringArray ampNames()
{
    return { "Twin Twin", "Ack Thirty", "Marshall's Stacked", "Champion", "Acoustic (piezo DI)",
             "Bass: Soft Freezer", "Bass: Bee 51", "Bass: Funky Circle", "Bass: Modern DI",
             "Line / Colour (no amp)" };
}

inline juce::StringArray cabNames()
{
    return { "2x12 Open-Back", "2x12 Alnico Chime", "4x12 Closed-Back", "1x10 Small Combo",
             "Acoustic Full-Range", "Bass 8x10 Sealed", "Bass 1x15 Vintage",
             "Bass 1x18 Folded Horn", "Bass 4x10 + Tweeter", "None (line out)" };
}

inline juce::StringArray micNames()
{
    return { "Dynamic (bright)", "Dynamic (dark)", "Condenser LDC", "Condenser SDC",
             "None (line out)" };
}

//==============================================================================
// Per-amp voicing: every coefficient the preamp/tone/power stages read.
//==============================================================================
struct AmpVoicing
{
    // Preamp
    int   preampStages    = 2;
    float preampDrive     = 1.0f;   // fixed gain multiplier folded into the gain knob
    float gainRangeDb     = 36.0f;  // drive added by the gain knob from 0 to 10
    float stageAsym       = 0.10f;  // even-harmonic asymmetry per stage
    bool  diodeLastStage  = false;  // hard diode clip on the final stage (Marshall)
    float interStageHpHz  = 80.0f;  // HP between stages to keep lows tight under gain
    float inputHpHz       = 20.0f;  // coupling/DC-block HP at the input
    float brightDb        = 3.0f;   // input bright high-shelf amount
    float brightHz        = 2000.0f;
    float stageLpHz       = 11000.0f; // Miller rolloff of the FIRST gain stage
                                      // (later stages get progressively darker)

    // Tone stack
    ToneStackType toneType = ToneStackType::FenderFMV;

    // Power amp
    bool  singleEnded     = false;  // class-A single-ended (Champ) vs push-pull
    float powerDrive      = 1.0f;   // how hard Master pushes the output stage
    float sagAmount       = 0.25f;  // 0..1 dynamic supply-sag depth
    float sagAttackMs     = 8.0f;
    float sagReleaseMs    = 120.0f;
    float outputLpHz      = 9000.0f; // output-transformer bandwidth
    float presenceMaxDb   = 6.0f;   // presence knob full-scale HF shelf boost
    float resonanceMaxDb  = 5.0f;   // resonance knob full-scale LF bump
    float resonanceHz     = 90.0f;

    // Features
    bool  hasReverb       = false;
    bool  acousticImager  = false;  // piezo body/de-quack front end (Acoustic amp)
    bool  bassStage       = false;  // bi-amp drive / sub front end (bass amps)
    bool  lineMode        = false;  // no preamp / power amp: tone + colour only
    float outputTrimDb    = 0.0f;   // per-model makeup so levels roughly match
};

inline AmpVoicing getAmpVoicing (AmpModel m)
{
    AmpVoicing v;
    switch (m)
    {
        case AmpModel::Twin:
            v.stageLpHz      = 10000.0f;  v.outputLpHz = 9000.0f;
            v.preampStages   = 2;
            v.preampDrive    = 0.9f;
            v.stageAsym      = 0.05f;
            v.diodeLastStage = false;
            v.interStageHpHz = 30.0f;
            v.inputHpHz      = 15.0f;
            v.brightDb       = 1.5f;  v.brightHz = 2200.0f;
            v.toneType       = ToneStackType::FenderFMV;
            v.singleEnded    = false;
            v.powerDrive     = 0.9f;
            v.sagAmount      = 0.15f;
            v.sagAttackMs    = 10.0f; v.sagReleaseMs = 90.0f;
            v.presenceMaxDb  = 6.0f;
            v.resonanceMaxDb = 4.0f;  v.resonanceHz = 80.0f;
            v.hasReverb      = true;
            v.outputTrimDb   = 0.0f;
            break;

        case AmpModel::AC30:
            v.stageLpHz      = 9000.0f;  v.outputLpHz = 8500.0f;
            v.preampStages   = 3;
            v.preampDrive    = 1.4f;
            v.stageAsym      = 0.10f;
            v.diodeLastStage = false;
            v.interStageHpHz = 40.0f;
            v.inputHpHz      = 30.0f;
            v.brightDb       = 3.0f;  v.brightHz = 3000.0f;
            v.toneType       = ToneStackType::VoxTopBoost;
            v.singleEnded    = false;
            v.powerDrive     = 1.3f;
            v.sagAmount      = 0.45f;  // class-A EL84 bloom/compression
            v.sagAttackMs    = 6.0f;  v.sagReleaseMs = 140.0f;
            v.presenceMaxDb  = 4.0f;
            v.resonanceMaxDb = 4.0f;  v.resonanceHz = 95.0f;
            v.hasReverb      = false;
            v.outputTrimDb   = -2.0f;
            break;

        case AmpModel::Marshall:
            v.stageLpHz      = 7000.0f;  v.outputLpHz = 7000.0f;
            v.preampStages   = 4;
            v.preampDrive    = 2.2f;
            v.stageAsym      = 0.15f;
            v.diodeLastStage = true;
            v.interStageHpHz = 60.0f;  // tightest lows for crunch
            v.inputHpHz      = 50.0f;
            v.brightDb       = 3.5f;  v.brightHz = 2500.0f;
            v.toneType       = ToneStackType::MarshallFMV;
            v.singleEnded    = false;
            v.powerDrive     = 1.6f;
            v.sagAmount      = 0.30f;
            v.sagAttackMs    = 8.0f;  v.sagReleaseMs = 120.0f;
            v.presenceMaxDb  = 9.0f;
            v.resonanceMaxDb = 6.0f;  v.resonanceHz = 85.0f;
            v.hasReverb      = false;
            v.outputTrimDb   = -3.0f;
            break;

        case AmpModel::Champ:
            v.stageLpHz      = 8000.0f;  v.outputLpHz = 7500.0f;
        default:
            v.preampStages   = 2;
            v.preampDrive    = 1.6f;
            v.stageAsym      = 0.22f;   // single-ended is inherently asymmetric
            v.diodeLastStage = false;
            v.interStageHpHz = 30.0f;
            v.inputHpHz      = 30.0f;
            v.brightDb       = 2.0f;  v.brightHz = 2000.0f;
            v.toneType       = ToneStackType::ChampTilt;
            v.singleEnded    = true;
            v.powerDrive     = 1.5f;
            v.sagAmount      = 0.6f;    // lots of small-amp breathing
            v.sagAttackMs    = 12.0f; v.sagReleaseMs = 180.0f;
            v.presenceMaxDb  = 2.0f;
            v.resonanceMaxDb = 2.0f;  v.resonanceHz = 110.0f;
            v.hasReverb      = true;
            v.outputTrimDb   = -1.0f;
            break;

        // AER / Loudbox-style acoustic amp: clean solid-state headroom, no sag,
        // flat full-range voicing. The piezo imager in front does the heavy
        // lifting; Gain only adds a touch of warmth when pushed.
        case AmpModel::Acoustic:
            v.stageLpHz      = 18000.0f;  v.outputLpHz = 18000.0f;
            v.preampStages   = 1;
            v.preampDrive    = 0.126f;  // noon ~ -6 dB into the stage: clean
            v.gainRangeDb    = 24.0f;   // 10 ~ +6 dB: gentle warmth, never fuzz
            v.stageAsym      = 0.03f;
            v.diodeLastStage = false;
            v.interStageHpHz = 20.0f;
            v.inputHpHz      = 20.0f;
            v.brightDb       = 0.0f;  v.brightHz = 4000.0f;
            v.toneType       = ToneStackType::AcousticHiFi;
            v.singleEnded    = false;
            v.powerDrive     = 0.8f;
            v.sagAmount      = 0.03f;
            v.sagAttackMs    = 10.0f; v.sagReleaseMs = 100.0f;
            v.presenceMaxDb  = 4.0f;
            v.resonanceMaxDb = 3.0f;  v.resonanceHz = 110.0f;
            v.hasReverb      = true;
            v.acousticImager = true;
            v.outputTrimDb   = -3.0f;   // presets land near unity vs the DI
            break;

        //======================================================================
        // Bass amps. Two things differ from every guitar model above: the
        // inter-stage high-pass sits at ~15 Hz (a low B is 31 Hz — a 60 Hz
        // guitar-style HP would gut it), and they enable the BassStage front
        // end for bi-amp drive / octave.
        //======================================================================

        // Ampeg SVT: 300 W of tube headroom, the reference rock/metal bass amp.
        case AmpModel::BassSVT:
            v.stageLpHz      = 10000.0f;  v.outputLpHz = 8000.0f;
            v.preampStages   = 3;
            v.preampDrive    = 0.5f;
            v.gainRangeDb    = 30.0f;
            v.stageAsym      = 0.08f;
            v.diodeLastStage = false;
            v.interStageHpHz = 15.0f;
            v.inputHpHz      = 15.0f;
            v.brightDb       = 1.5f;  v.brightHz = 2500.0f;
            v.toneType       = ToneStackType::BassSVT;
            v.singleEnded    = false;
            v.powerDrive     = 0.9f;
            v.sagAmount      = 0.12f;   // 6550s stay clean then clip abruptly
            v.sagAttackMs    = 10.0f; v.sagReleaseMs = 110.0f;
            v.presenceMaxDb  = 5.0f;
            v.resonanceMaxDb = 6.0f;  v.resonanceHz = 60.0f;
            v.hasReverb      = false;
            v.bassStage      = true;
            v.outputTrimDb   = -2.0f;
            break;

        // Ampeg B-15N flip-top: 25 W, warm and dark, breaks up early. The
        // Motown / Jamerson sound — pair it with Tape.
        case AmpModel::BassFlipTop:
            v.stageLpHz      = 7000.0f;  v.outputLpHz = 6000.0f;
            v.preampStages   = 2;
            v.preampDrive    = 0.9f;
            v.gainRangeDb    = 34.0f;
            v.stageAsym      = 0.16f;
            v.diodeLastStage = false;
            v.interStageHpHz = 18.0f;
            v.inputHpHz      = 18.0f;
            v.brightDb       = 0.0f;  v.brightHz = 2000.0f;
            v.toneType       = ToneStackType::BassVintage;
            v.singleEnded    = false;
            v.powerDrive     = 1.5f;
            v.sagAmount      = 0.55f;   // small amp, lots of breathing
            v.sagAttackMs    = 14.0f; v.sagReleaseMs = 180.0f;
            v.presenceMaxDb  = 2.0f;
            v.resonanceMaxDb = 4.0f;  v.resonanceHz = 70.0f;
            v.hasReverb      = false;
            v.bassStage      = true;
            v.outputTrimDb   = -2.0f;
            break;

        // Acoustic 360: solid-state, 1x18 folded horn, the funk/Jaco machine.
        // Clean and deep with a forward midrange.
        case AmpModel::BassFunk:
            v.stageLpHz      = 12000.0f;  v.outputLpHz = 10000.0f;
            v.preampStages   = 1;
            v.preampDrive    = 0.2f;
            v.gainRangeDb    = 26.0f;
            v.stageAsym      = 0.04f;
            v.diodeLastStage = false;
            v.interStageHpHz = 15.0f;
            v.inputHpHz      = 15.0f;
            v.brightDb       = 1.0f;  v.brightHz = 3000.0f;
            v.toneType       = ToneStackType::BassVariamp;
            v.singleEnded    = false;
            v.powerDrive     = 0.7f;
            v.sagAmount      = 0.05f;
            v.sagAttackMs    = 10.0f; v.sagReleaseMs = 90.0f;
            v.presenceMaxDb  = 4.0f;
            v.resonanceMaxDb = 7.0f;  v.resonanceHz = 50.0f;
            v.hasReverb      = false;
            v.bassStage      = true;
            v.outputTrimDb   = -1.0f;
            break;

        // Modern DI / hi-fi head (Aguilar / Darkglass school): near-flat, huge
        // headroom, the indie & pop pick-bass starting point.
        case AmpModel::BassModern:
            v.stageLpHz      = 16000.0f;  v.outputLpHz = 14000.0f;
            v.preampStages   = 1;
            v.preampDrive    = 0.15f;
            v.gainRangeDb    = 22.0f;
            v.stageAsym      = 0.03f;
            v.diodeLastStage = false;
            v.interStageHpHz = 15.0f;
            v.inputHpHz      = 15.0f;
            v.brightDb       = 0.0f;  v.brightHz = 4000.0f;
            v.toneType       = ToneStackType::BassHiFi;
            v.singleEnded    = false;
            v.powerDrive     = 0.6f;
            v.sagAmount      = 0.02f;
            v.sagAttackMs    = 10.0f; v.sagReleaseMs = 80.0f;
            v.presenceMaxDb  = 5.0f;
            v.resonanceMaxDb = 5.0f;  v.resonanceHz = 55.0f;
            v.hasReverb      = false;
            v.bassStage      = true;
            v.outputTrimDb   = -1.0f;
            break;

        // Not an amp at all: a colour path for mixes and master busses. The
        // preamp and power stage are skipped, so the only things in circuit are
        // a gentle hi-fi tone stack and whatever LoFi / Tape you dial in. Pair
        // it with cab and mic set to "None (line out)".
        case AmpModel::Line:
            v.stageLpHz      = 20000.0f;  v.outputLpHz = 20000.0f;
            v.preampStages   = 1;
            v.preampDrive    = 1.0f;
            v.gainRangeDb    = 0.0f;
            v.stageAsym      = 0.0f;
            v.diodeLastStage = false;
            v.interStageHpHz = 10.0f;
            v.inputHpHz      = 10.0f;
            v.brightDb       = 0.0f;  v.brightHz = 4000.0f;
            v.toneType       = ToneStackType::LineFlat;
            v.singleEnded    = false;
            v.powerDrive     = 1.0f;
            v.sagAmount      = 0.0f;
            v.presenceMaxDb  = 0.0f;
            v.resonanceMaxDb = 0.0f;  v.resonanceHz = 80.0f;
            v.hasReverb      = false;
            v.lineMode       = true;
            v.outputTrimDb   = 0.0f;
            break;
    }
    return v;
}

//==============================================================================
// Per-cab voicing: synthesized speaker cabinet as a chain of biquads.
//==============================================================================
struct CabVoicing
{
    float lowBumpHz = 95.0f,  lowBumpQ = 1.0f,  lowBumpDb = 4.0f;  // box/Fs resonance
    float lowCutHz  = 75.0f;                                       // HP, no deep bass
    float peak1Hz   = 1800.0f, peak1Q = 1.5f,  peak1Db = 3.0f;     // cone character
    float peak2Hz   = 3200.0f, peak2Q = 2.0f,  peak2Db = 2.0f;     // upper-mid voice
    float hiCutHz   = 5000.0f;                                     // steep HF rolloff
};

inline CabVoicing getCabVoicing (CabModel c)
{
    CabVoicing v;
    switch (c)
    {
        case CabModel::TwoByTwelve: // Jensen-ish open-back 2x12 (Twin) — scooped, hi-fi
            v = { 95.0f, 1.0f, 4.0f, 75.0f, 1800.0f, 1.5f, 3.0f, 3200.0f, 2.0f, 2.0f, 5500.0f };
            break;
        case CabModel::AlnicoBlue:  // Vox 2x12 Alnico Blue — bell-like chime
            v = { 110.0f, 1.2f, 5.0f, 85.0f, 2200.0f, 1.5f, 4.0f, 3500.0f, 2.0f, 3.0f, 5200.0f };
            break;
        case CabModel::FourByTwelve: // Marshall 4x12 greenback — big lows, 4k dip
            v = { 90.0f, 1.3f, 6.0f, 80.0f, 1500.0f, 1.2f, 3.0f, 2600.0f, 2.0f, 4.0f, 4500.0f };
            break;
        case CabModel::Champ:       // small 1x10 — boxy, leaner lows, earlier HF roll
            v = { 130.0f, 1.5f, 3.0f, 110.0f, 1600.0f, 1.0f, 2.0f, 2800.0f, 2.0f, 3.0f, 5000.0f };
            break;
        // Sealed 8x10. Ampeg publish +/-3 dB 58 Hz - 5 kHz and -10 dB at
        // 40 Hz, which is a clean 2nd-order rolloff at 58 Hz. The midrange
        // shape is from the closest measured analogue (a sealed 4x(2x10) paper
        // cab, no tweeter): broad lift ~200 Hz, deep dip ~1.2 kHz, then the
        // paper-cone breakup lobe at 2.2 kHz which IS the audible "top".
        case CabModel::Bass810:
            v = { 200.0f, 0.7f, 1.5f, 58.0f, 1200.0f, 1.3f, -8.0f, 2200.0f, 3.5f, 6.0f, 3000.0f };
            break;
        // 1x15 flip-top. No response has ever been published for a B-15, so
        // this follows Ampeg's tweeterless paper 1x15 (-3 dB to 3.7 kHz) plus
        // the reported "light below 80 Hz, nothing above ~1.6 kHz" character.
        case CabModel::Bass115:
            v = { 150.0f, 0.8f, 2.5f, 60.0f, 500.0f, 0.4f, -4.0f, 1800.0f, 2.0f, 3.0f, 3700.0f };
            break;
        // 1x18 folded horn (Acoustic 361). The horn only loads from ~100 Hz up
        // and its own output effectively stops by 300-400 Hz; modelled that
        // literally it is unplayably dull, so this keeps the horn's 100-400 Hz
        // punch and lets the direct radiation carry a usable top.
        case CabModel::Bass118:
            v = { 130.0f, 1.5f, 3.5f, 40.0f, 400.0f, 0.8f, 1.5f, 1200.0f, 1.5f, -3.0f, 2500.0f };
            break;
        // Modern ported 4x10 + tweeter: Ampeg spec +/-3 dB 48 Hz - 18 kHz with
        // a 4 kHz crossover; mid shape from the measured ported 4x10.
        case CabModel::Bass410T:
            v = { 160.0f, 0.7f, 1.5f, 50.0f, 1200.0f, 1.0f, -4.0f, 800.0f, 1.0f, 1.5f, 16000.0f };
            break;
        case CabModel::NoCab:     // bypassed by the engine; values unused
        case CabModel::AcousticFullRange: // 8" twin-cone (AER-style) — near-flat to 15 kHz
        default:
            v = { 105.0f, 0.9f, 2.0f, 55.0f, 1200.0f, 1.0f, -1.0f, 3500.0f, 1.5f, 1.5f, 15000.0f };
            break;
    }
    return v;
}

//==============================================================================
// Per-mic voicing: characteristic mic frequency response as biquads.
//==============================================================================
struct MicVoicing
{
    float lowCutHz   = 100.0f;                              // built-in bass rolloff
    float midDipHz   = 0.0f, midDipQ = 1.0f, midDipDb = 0.0f;  // optional presence dip
    float presHz     = 5500.0f, presQ = 1.0f, presDb = 4.0f;   // presence peak
    float topShelfHz = 12000.0f, topShelfDb = 0.0f;            // air / top tilt
};

inline MicVoicing getMicVoicing (MicModel m)
{
    MicVoicing v;
    switch (m)
    {
        case MicModel::SM57:  // bright, mid-forward dynamic
            v = { 120.0f, 450.0f, 1.0f, -2.0f, 5200.0f, 1.0f, 3.0f, 12000.0f, -2.5f };
            break;
        case MicModel::SM7B:  // darker, fatter dynamic
            v = { 80.0f, 0.0f, 1.0f, 0.0f, 4500.0f, 1.2f, 2.0f, 9000.0f, -3.0f };
            break;
        case MicModel::CondenserLDC: // U87-style: full, airy
            v = { 45.0f, 0.0f, 1.0f, 0.0f, 8000.0f, 0.8f, 2.0f, 12000.0f, 3.0f };
            break;
        case MicModel::NoMic:        // bypassed by the engine; values unused
        case MicModel::CondenserSDC: // KM84-style: flat, extended, detailed
        default:
            v = { 55.0f, 0.0f, 1.0f, 0.0f, 9000.0f, 0.7f, 1.5f, 11000.0f, 4.0f };
            break;
    }
    return v;
}

} // namespace amp
