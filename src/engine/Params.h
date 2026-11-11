// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace rh
{
// Every automatable parameter in the rig. The order is the storage order, so
// only append new parameters at the end of a section and never reorder.
enum class P : int
{
    // Input
    inGain, inputFix, humFilter, gateOn, gateThresh, gateRelease, inputFixAmt,
    // Compressor
    compOn, compSustain, compAttack, compLevel, compBlend,
    // Wah
    wahOn, wahMode, wahPos, wahSens, wahQ,
    // Pitch
    pitchOn, pitchMode, pitchSemis, pitchMix,
    // Drive pedal
    drvOn, drvType, drvGain, drvTone, drvLevel,
    // Amp
    ampOn, ampModel, ampGain, ampBass, ampMid, ampTreble, ampPresence, ampDepth, ampMaster, ampLevel,
    // Cab
    cabOn, cabModel, cabMic, cabDistance, cabLowCut, cabHighCut,
    // Post EQ
    eqOn, eq100, eq200, eq400, eq800, eq1k6, eq3k2, eq6k4, eqLevel,
    // Modulation
    modOn, modType, modRate, modDepth, modMix,
    // Delay
    dlyOn, dlyType, dlyTime, dlySync, dlyFeedback, dlyTone, dlyMix,
    // Reverb
    revOn, revType, revDecay, revPreDelay, revTone, revMix,
    // Lo-fi
    lofiOn, lofiBits, lofiRate, lofiWobble, lofiNoise, lofiFilter, lofiMix,
    // Output
    doubler, space, outLevel,
    // Auto level: tones load at one loudness and switches never change the volume (part of the setup, not the tone)
    autoLevel,
    count
};

constexpr int kNumParams = (int) P::count;

enum class ParamKind { toggle, choice, continuous };

struct ParamSpec
{
    P id;
    const char* key;       // stable id used in presets and the host
    const char* name;      // display name
    ParamKind kind;
    float min, max, def;
    float skewCentre;      // value at the knob's midpoint (0 = linear)
    const char* unit;
    std::vector<const char*> choices;
};

const std::vector<ParamSpec>& paramSpecs();
const ParamSpec& spec (P id);
int findParamByKey (std::string_view key);

using ParamValues = std::array<float, kNumParams>;
ParamValues defaultParamValues();

// Choice lists shared with the UI.
namespace choices
{
    enum AmpModel { glassClean, tweedEdge, chimeTop, britPlexi, brit800, hotRodLead, modernHigh, tightMetal, bassStation, studioDirect, neuralCapture, numAmpModels };
    enum CabModel { cabOpen1x12, cabBlue2x12, cabVintage4x12, cabModern4x12, cabUs2x12, cabTiny1x8, cabBass4x10, cabBass1x15, cabUserIr, numCabModels };
    enum DriveType { drvBoost, drvTrebleBoost, drvGreen, drvBlues, drvRat, drvFuzz, drvMetal, numDriveTypes };
    enum ModType { modChorus, modFlanger, modPhaser, modTremolo, modVibrato, modVibe, numModTypes };
    enum DelayType { dlyDigital, dlyAnalog, dlyTape, dlyPingPong, numDelayTypes };
    enum DelaySync { syncOff, sync1_2, sync1_4d, sync1_4, sync1_8d, sync1_8, sync1_8t, sync1_16, numSyncs };
    enum ReverbType { revRoom, revHall, revPlate, revSpring, revShimmer, revCloud, numReverbTypes };
    enum PitchMode { pitchSubOctave, pitchOctaveUp, pitchShift, numPitchModes };
    enum WahMode { wahManual, wahAuto, numWahModes };
} // namespace choices
} // namespace rh
