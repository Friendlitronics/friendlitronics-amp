# Friendlitronics Amp — developer notes

_Last updated: 2026-09-23 (v0.8.0)_

A JUCE/C++ guitar **amp emulator** plugin (AU + VST3 + Standalone, macOS universal),
scaffolded after a sibling `tape_emulator` project. Freely swap amp / cab / mic and move the
mic around the speaker continuously. All DSP is **parametric** (no impulse responses).

## Current status: WORKING ✅

- Builds clean (0 errors) and **passes `auval`** (AU VALIDATION SUCCEEDED).
- AU + VST3 auto-installed; Standalone `.app` built.
- Last feature added (v0.3.0): **four bass amps** + four bass cabs + a bi-amp
  bass drive stage + a **tape colour** stage borrowed from the sibling tape_emulator project,
  plus 10 presets. See "Bass + tape" below.
- Last change (v0.8.0): **scoped presets** (amp / pedalboard / everything) +
  10 **factory pedalboards**, and the pitch tracker rebuilt on autocorrelation.
- v0.7.0: **drag-to-reorder chain** (pedals + the amp as a tile),
  pitch-shifter rebuild, pedal top-end tamed. See "Chain order" below.
- v0.6.0: **user presets** (save/load/delete) and a **distortion
  rework** aimed at the harshness complaint. See "Distortion rework" below.
- v0.5.0 added an **8-pedal pedalboard** on its own UI tab,
  each pedal switchable between INTO AMP and AFTER CAB. See "Pedalboard" below.
- v0.4.0 added the **Line / Colour** path (no amp/cab/mic) + a **LoFi** stage
  (crush / SR hold / noise / narrow) + 4 `Master:` presets, for using the plugin
  as a bus or master colour box. See "Line / colour mode" below.
- v0.2.0 added the **Acoustic (piezo DI)** amp + Acoustic Full-Range cab.

## Resume steps after restart

```bash
# JUCE is fetched automatically (pinned 7.0.12 + the macOS 15 patch), or a
# local checkout at ./JUCE is used if one exists.
cmake -B build -G "Xcode"
cmake --build build --config Release

# Validate the AU:
auval -v aufx Amp1 Frnd

# Run the standalone:
open "build/FriendlitronicsAmp_artefacts/Release/Standalone/Amp Sim.app"
```

Installed plugin locations (from COPY_PLUGIN_AFTER_BUILD):
- AU:   `~/Library/Audio/Plug-Ins/Components/Friendlitronics Amp.component`
- VST3: `~/Library/Audio/Plug-Ins/VST3/Friendlitronics Amp.vst3`

## Architecture (where things live)

Signal chain (`Source/dsp/AmpEngine.h`):
```
Input HPF+bright → AcousticImager (acoustic amp)
                 → [4× oversample: BassStage (bass amps) → Preamp → ToneStack → PowerAmp]
                 → SpringReverb (Twin/Champ/Acoustic) → Cabinet → Mic → MicPosition
                 → RoomSim → TapeColour (any amp) → output trim + wet/dry mix
```

| File | Responsibility |
|------|----------------|
| `Source/dsp/AmpEngine.h` | top-level chain; applies voicing on amp/cab/mic change; oversampling region |
| `Source/dsp/AmpVoicing.h` | per-amp / per-cab / per-mic coefficient tables (the "tuning") |
| `Source/dsp/PreampStage.h` | cascaded asym tube stages (`setAsymScale` = sweetness) |
| `Source/dsp/ToneStack.h` | FMV / Vox / Champ / acoustic / 3 bass EQ topologies |
| `Source/dsp/AcousticImager.h` | piezo body / de-quack / comp / colour (acoustic amp) |
| `Source/dsp/BassStage.h` | bass bi-amp split, fuzz + blend, octave divider |
| `Source/dsp/TapeColour.h` | tape saturation, head bump, HF loss, wow/flutter |
| `Source/dsp/PowerAmp.h` | sag, class-A/AB clip, presence, resonance (`setSagScale` = sweetness) |
| `Source/dsp/Cabinet.h` | speaker biquads (`setExtraPeakDb` = sweetness bell lift) |
| `Source/dsp/Mic.h`, `MicPosition.h`, `RoomSim.h` | mic voicing + continuous placement + room |
| `Source/dsp/BiquadHelpers.h`, `Waveshapers.h`, `Oversampler.h` | DSP primitives |
| `Source/PluginProcessor.{h,cpp}` | APVTS params, cached atomics, wet/dry, metering |
| `Source/PluginEditor.{h,cpp}` | selectors, knobs, meters, model-specific greying |
| `Source/ui/AmpLookAndFeel.{h,cpp}`, `LevelMeter.h` | styling + meters |
| `Source/Presets.h` | 19 factory presets (6 electric, 3 acoustic, 9 bass, 1 tape) |

DSP modules are **header-only**; only `PluginProcessor.cpp`, `PluginEditor.cpp`,
`ui/AmpLookAndFeel.cpp` are compiled units (see `CMakeLists.txt`).

## Models
- **Amps:** Fender Twin, Vox AC30, Marshall Stack, Fender Champ, Acoustic (piezo DI),
  Bass SVT Fridge, Bass Flip-Top B-15, Bass Funk 360, Bass Modern DI
- **Cabs:** 2×12 Twin, 2×12 Alnico Blue, 4×12 Greenback, 1×10 Champ,
  Acoustic Full-Range, Bass 8×10, Bass 1×15, Bass 1×18 horn, Bass 4×10+tweeter
- **Mics:** SM57, SM7B, condenser LDC (U87-style), condenser SDC (KM84-style)
- Amp/cab/mic enums are **append-only** so saved sessions keep their indices.

## Sweetness knob
Macro centered at **5 = neutral** (today's sound). Scales together:
even-harmonic asymmetry (preamp), cab bell-peak (±3 dB), and class-A bloom (sag).
Wired in `AmpEngine::process` via `sweetDelta = (sweetness-5)/5`. AC30 Chime preset
ships at Sweetness 7.

## Acoustic amp (v0.2.0)
Amp index 4 / cab index 4 (appended, so saved sessions keep their indices).
`AcousticImager.h` runs before the oversampled core only when
`AmpVoicing::acousticImager` is set, so electric presets are unchanged.
Params `body`, `deQuack`, `comp`, `colour` (0..10) drive it; editor greys them
for other amps. Voicing: 1 clean preamp stage (`gainRangeDb` = 24), AER-style
`ToneStackType::AcousticHiFi`, almost no sag, `outputTrimDb` -3 so presets land
near unity vs. the DI. Verified offline (harness, not in repo): stable at
44.1/48/96k, De-Quack cuts ~-7 dB on onsets vs ~-3 dB sustained at 6,
Comp 6.5 narrows loud/soft strum spread 11.7 -> 7 dB.
Tuning ideas: modal frequencies are dreadnought-ish (98/196/235/520 Hz) — a
"body size" switch (parlor/OM/dread) could shift them.

## Bass + tape (v0.3.0)
Amps 5-8 (`BassSVT`, `BassFlipTop`, `BassFunk`, `BassModern`), cabs 5-8. Bass amps
set `AmpVoicing::bassStage`, which enables `BassStage.h` at the *head of the
oversampled region* (so the fuzz and the divider's square edges are band-limited).
Key design points, all verified offline:
- The crossover keeps the low band clean and derives the high band by
  SUBTRACTION (high = x - low), so Blend 0 is bit-exact unity.
- The driven band is auto-levelled to the clean band's envelope (Grit changes
  character, not loudness) and high-passed at 0.8x the split, because clipping
  intermodulates well below its input band and was muddying the clean lows
  (measured +10 dB at 40-60 Hz before the fix, ~0 dB after).
- `Sub` is a zero-crossing flip-flop divider with hysteresis + envelope + gate,
  high-passed at 30 Hz. Tracks E2 (82 Hz) and up well; on a low E1 the octave
  lands at 20 Hz and is mostly filtered away, which is honest to an OC-2.

`TapeColour.h` runs LAST for every amp (params default 0 = bit-exact bypass, so
no existing preset changed). Adapted from the sibling tape_emulator project's dsp/
(TapeSaturation/HeadBumpFilter/HFLoss/WowFlutter): derivative-normalised tanh,
pre/de-emphasis pair, programme-dependent drive, 70 Hz head-bump BELL, gap-loss
rolloff, and wow/flutter from a Lagrange3rd delay line. Flutter > 0 adds ~1.5 ms
of unreported latency (documented; keep Mix at 100%).

Offline check harness (scratchpad, not in the repo — the session dir is wiped
between sessions, so rebuild it if needed): asserted blend-0/tape-0 bypass
exactness, fundamental preservation under fuzz, divider tracking per note, tape
response curves, and per-preset level/stability at 44.1/48/96k. Bass presets are
level-matched to land within ~2 dB of the DI's RMS via each preset's Output.

## Line / colour mode (v0.4.0)
Amp 9 (`Line`, `AmpVoicing::lineMode`), cab 9 (`NoCab`), mic 4 (`NoMic`) — all
appended. `lineMode` makes the engine skip the preamp and power amp (the
oversampling bracket still runs, so reported latency never changes with the amp
choice); `cabIsLine`/`micIsLine` drop the cab, mic and mic-placement stages.
**Gotcha found by measurement:** JUCE's 2-stage polyphase oversampling round
trip returns 2x hot (+6.02 dB). Every amp voicing had silently absorbed that in
its tuning, but line mode has no gain stage to absorb it, so the engine now
multiplies by 0.5 right after the downsample when `lineMode` — before LoFi and
Tape, whose quantisation and saturation are both level-dependent. Verified flat
within 0.03 dB, 40 Hz - 16 kHz.

`LoFiStage.h` (crush / SR hold / hiss+crackle / mid-side narrowing) sits before
`TapeColour` so the tape's HF loss smooths crush aliasing and shapes the hiss.
All four controls default 0 = bit-exact bypass.

`Presets::apply` now resets every parameter to its default before applying the
preset's list, so presets are self-contained — otherwise a preset that doesn't
mention Tape/Crush/Sub would inherit them from whatever was loaded before.

## Voicing sources (v0.4.0)
Bass voicings were reconciled against a research pass (Ampeg SVT/B-15 manuals
and schematics, the Acoustic 360/361 factory service manual, Bass Gear measured
cab curves, OC-2 schematics). Notable corrections applied: SVT mid is asymmetric
(+10/-20 dB), B-15 Baxandall is 40 Hz / 5 kHz (not 60 Hz / 2.5 kHz), the 360's
Variamp cut is a near-null while its boost is modest, the 8x10's audible top is
a 2.2 kHz breakup lobe after a -8 dB dip at 1.2 kHz, and the OC-2 derives its
sub from the rescaled dry signal (hence dynamics tracking) rather than a square.
Known-unknowable: no B-15 response has ever been published, no Ampeg document
gives an SVT mid Q, and no anechoic data exists for any bass guitar cab.

## Pedalboard (v0.5.0)
`Source/dsp/pedals/`: `PedalCommon.h` (LFO, allpass, envelope, Hermite delay
line), `DrivePedals.h` (Screamer, Fuzz), `FilterPedals.h` (Wah, Sustainer),
`PitchPedal.h` (Dive), `TimePedals.h` (Chorus, Phaser, Delay), `PedalBoard.h`
(ordering, per-slot instances, parameter metadata).

`PedalBoard` keeps a **separate instance of all eight pedals per slot**, so
moving a pedal between INTO AMP and AFTER CAB never drags a delay tail with it.
Pre runs at the head of the oversampled region; post runs at host rate after
RoomSim. Parameter names are generated from `pedalTag()`: `<tag>On`,
`<tag>Post`, `<tag>A`..`<tag>D` — so adding a pedal means adding an enum entry
plus its metadata, and the params and UI follow automatically.

UI: `Source/ui/PedalComponent.h` — `PedalSkin` (per-pedal colours + subtitle),
`PedalLookAndFeel` (tinted knobs, chrome stomp switch with LED, placement pill),
`PedalComponent`, `PedalBoardView`. The editor now has an AMP / PEDALS tab strip;
`ampViewComponents` is collected in the constructor and toggled wholesale, and
the model-dependent greying moved into `applyModelVisibility()` so switching
tabs re-applies it immediately instead of waiting for the timer.

**Bug worth remembering (pitch shifter):** the textbook two-taps-half-a-window-
apart granular shifter cancels itself on steady tones — with a 50 ms window the
grains are 5.5 cycles apart at 220 Hz, i.e. antiphase, and the note vanished
(measured -68 dB). Now a single moving tap with a short equal-power splice
crossfade at the wrap; verified ±1 octave to within a fraction of a dB.

## Distortion rework (v0.6.0)
Driven by the first real ear feedback ("harsh and ice-picky"). Four changes,
each measured with `scratchpad/harness/aliasprobe.cpp` (rebuild it if the
scratchpad has been cleaned):

1. **`pushPull` was non-monotonic** — `tanh(x - 0.15x^3)` folds back above
   x = 1.49, so any Master above ~halfway was wavefolding. Now
   `(1-m)tanh + m tanh^3`, monotonic for all input.
2. **Miller low-pass per preamp stage** (`AmpVoicing::stageLpHz`, x0.78 per
   stage). There was NO low-pass anywhere in the preamp before, so every stage
   generated harmonics from full-bandwidth input. This was the single biggest
   contributor to the fizz.
3. **Output-transformer low-pass** (`AmpVoicing::outputLpHz`) after the power
   stage clip.
4. **ADAA on every shaper** (`shape::Adaa1` + closed-form antiderivatives).
   Measured in isolation: 29 dB -> 62 dB alias rejection at 1x. Inside the 4x
   core it adds little (the oversampler already covers it) — the reason to keep
   it is the drive pedals, which run at 1x in the AFTER CAB slot.

Also trimmed the fixed brightness: bright caps (Twin 2.5->1.5, AC30 4->3,
Marshall 5->3.5 dB) and mic presence peaks (SM57 +4.5@5.5k -> +3@5.2k).
Brightness tilt (2-6 kHz vs 200-800 Hz): Marshall 8.9 -> 4.3 dB, Lead 7.1 ->
2.5, AC30 7.1 -> 5.9, Twin 8.8 -> 7.1. If it is now too dark, the cheapest
places to give top back are `stageLpHz`/`outputLpHz` and the mic presence peak.

## User presets (v0.6.0)
`Source/PresetManager.h` — one XML file per preset (the whole APVTS state) in
`~/Library/Application Support/Friendlitronics Amp/Presets`. Factory presets remain host
programs; user presets are editor-only, listed under a MY PRESETS section in the
same combo, with SAVE / DEL buttons in the header.

**Bug found while testing:** `juce::String("ABCD"[k])` resolves to the *numeric*
String constructor, so the pedal knob parameters had registered as `wah65`,
`wah66`, `wah67`. Self-consistent (UI and processor used the same expression) so
nothing misbehaved, but those ids go into saved sessions. Now built by
`pedal::pedalKnobId()`. Sessions saved with v0.5.0 will lose pedal knob values.

## Chain order (v0.7.0)
The chain is a `pedal::ChainOrder` — 9 slots, 8 pedals plus `kAmpToken`. It is
NOT a parameter: it lives as a `chainOrder` property on the APVTS state tree (so
it travels with sessions and user presets) and is mirrored into an atomic packed
4-bits-per-slot, because the audio thread must never read a ValueTree property.
`PedalBoard` walks that order; each pedal's pre/post slot is *derived* from its
position relative to the amp token, so the old `<tag>Post` parameter is gone.
`chainOrderValid()` guards against a malformed order, since it indexes arrays.
UI is `ChainStrip` in `ui/PedalComponent.h` (drag to reorder, click to toggle).

## Chasing the "popping static" (v0.7.0)
Worth recording, because most of the suspects were innocent:
- **ADAA float precision — NOT the cause.** The conditioning test shows up to
  0.003 absolute error in the float difference quotient, but the inharmonic
  floor measured identically (-77.9 dB) for float and double. Moved to double
  anyway as cheap insurance; it fixed nothing measurable.
- **Block-size dependence — clean.** `blockprobe` renders the real processor at
  fixed vs wildly varying block sizes (7..1024) and the outputs are bit
  identical. Mono instantiation also fine.
- **Amp path — clean.** No click-like discontinuities on any amp preset. (The
  click detector reports ~600 "clicks" on Champ Breakup: that is a false
  positive, it is detecting the sharp edges of a heavily clipped wave.)
- **The pitch shifter WAS discontinuous** — see below. Per-pedal inharmonic
  junk: every other pedal sits at -70 dB or better; Dive was the outlier.

If harshness or popping is still reported with all pedals off, the next things
to instrument are the per-block coefficient recomputation in ToneStack/PowerAmp
(fast automation) and the sag envelope, neither of which showed up here.

## Pitch shifter rebuild (v0.7.0)
Two bugs, both worth remembering:
1. Grains must be **pitch-synchronous** — snapped to whole periods of the note —
   or every splice is a phase discontinuity (10-20 of them a second).
2. Once they are phase-locked the grains are correlated, so the crossfade must
   be **equal gain**. Equal power (correct for uncorrelated material) sums
   correlated grains to +3 dB and bumps the level at every splice; that was most
   of the remaining choppiness. Junk at -1 oct: -22 dB -> -49 dB, wobble 76% ->
   32% (and much of that residual is the metric tracking the 110 Hz carrier).

Pedal top end was also pulled in: Screamer gets a 5.5 kHz pre-clip low-pass and
its tone tops out at 4 kHz instead of 6; Fuzz tone tops out at 5 kHz instead of
9; Wah's peak is +12 dB (was +15), Q maxes at 5.5 (was 7), it has a 6 kHz
low-pass, and its centre frequency is now smoothed rather than stepped every 32
samples.

## Scoped presets + factory boards (v0.8.0)
`PresetManager` now saves a scope with each preset (Full / Amp / Pedals) and
encodes it in the filename prefix (`full_`, `amp_`, `board_`), so the same name
can exist in two scopes. A scoped load walks the saved PARAM children and only
applies ids that pass `inScope()` — `isPedalParam()` is the split, keyed off the
pedal tags. An Amp-scope load deliberately discards the saved chain order.
`PedalPresets.h` holds 10 factory boards (pedal values + a full chain order);
`PedalPresets::apply` resets every pedal to default/off first so boards are
self-contained, and never touches amp parameters.

## Pitch tracking, second attempt (v0.8.0)
The v0.7.0 zero-crossing tracker was measured and it **failed on real notes**:
on a plucked low E it reported 140 samples against a true period of 582 (it had
locked onto a harmonic), and at 147 Hz it did not lock at all. It only worked on
the pure sine the first test used — a good lesson about test signals being too
kind. Replaced with `pedal::PeriodTracker`: coarse normalised autocorrelation on
a ~12 kHz decimated copy (with the "first peak above 0.85 x best" rule to avoid
octave errors), then a narrow correlation search at full rate for precision,
since grain alignment needs the period to well under 1%. Coarse-only was
measured at 220.7 vs a true 218.2 and sounded worse than v0.7.0; with refinement
it is 218.5, and junk at -1 oct went -22 -> -77 dB. PITCH is smoothed per sample
so sweeps bend instead of stepping.

## Possible next steps (not done yet)
- Tune voicing coefficients by ear (drive amounts, tone centres, cab/mic curves) —
  the bass amp/cab numbers in particular are from spec-sheet reasoning, not ears.
- Optionally let Sweetness also reach the mic air shelf for more top sparkle.
- Add per-sample smoothing for tone/placement filters (currently recompute per block →
  fast EQ-knob automation can zipper).
- Optional `tests/` unit-test target (tone-stack / cab response checks). Not added.
- Discretized passive FMV/Vox tone stack (currently a voiced-biquad approximation).
- Optional graphical mic-position widget (draggable mic over a speaker).

See `README.md` for the user-facing description.
