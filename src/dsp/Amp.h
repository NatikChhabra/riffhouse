// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"
#include "ToneStack.h"
#include "Tube.h"

#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace rh
{
// Voicing data for one amp model. The models are original designs built from
// generic tube-amp building blocks (gain stages, a passive tone stack and a
// push-pull power stage); they are not copies of any product.
struct AmpVoicing
{
    const char* name;
    float inputHpHz;          // coupling cap at the input
    float brightDb;           // bright cap boost at low gain settings
    float gainMinDb, gainMaxDb; // range of the gain knob
    int numStages;            // 1..5 preamp stages
    float stageGain[5];       // fixed gain of each stage after the first
    float stageBias[5];       // asymmetry (even harmonics)
    float stageHpHz[5];       // interstage coupling (tightness)
    float stageLpHz[5];       // Miller-cap rolloff
    bool baxandall;           // active EQ instead of a passive tone stack
    ToneStackComponents stack;
    float midBoostHz;         // where the extra mid control sits
    float powerDrive;         // how hard the power stage is hit at master 10
    float powerBias;          // power stage asymmetry (class A feel)
    float sag;                // supply sag amount
    float presenceHz, depthHz;
    float outputHpHz, outputLpHz; // output transformer bandwidth
    float trimDb;             // loudness calibration
};

const AmpVoicing& ampVoicing (int model);
int numAmpVoicings();

struct AmpSettings
{
    int model = 0;
    float gain = 5, bass = 5, mid = 5, treble = 5, presence = 5, depth = 5, master = 5; // 0..10
    float levelDb = 0.0f;
};

// The DSP amp. Runs 4x oversampled (2x at high sample rates), its curves antialiased. Mono.
class Amp
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setSettings (const AmpSettings& s);
    void process (float* data, int n);
    float latencySamples() const;
    // The preamp as real 12AX7 stages (off for now: at high gain they fold back more grit and
    // cost three times as much), or the curve model; the lab holds the two side by side.
    void setTubeStages (bool on) noexcept { tubeStages = on; dirty = true; updateFilters(); }

private:
    void updateFilters();
    static float tube (float x, float bias) noexcept;
    // antialiased curves; each keeps where the signal was a sample ago and the curve's bend there
    struct AaState { float u = 0.0f, bend = 0.0f; bool fresh = true; }; // fresh: no last sample yet (after a reset)
    static float tubeAA (float x, float bias, AaState& last) noexcept;
    static float powerAA (float x, float bias, float head, AaState& last) noexcept;

    struct Stage
    {
        OnePole hp, lp;
        float gain = 1.0f, bias = 0.0f;
        AaState last; // where the stage's curve was a sample ago, for the antialiasing
        EnvelopeFollower blocking;
    };
    AaState powerLast;

    double sr = 48000.0, osr = 192000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    AmpSettings settings;
    const AmpVoicing* v = nullptr;
    bool dirty = true;

    OnePole inputHp;
    Biquad bright, midBoost, presence, depth, outHp, outLp, antiFizz;
    Biquad baxLow, baxMid, baxHigh;
    ToneStack stack;
    Stage stages[5];
    tube::Stage tubes[5];
    double into[5] {};       // what reaches each tube's grid, per volt from the stage before
    bool tubeStages = false;
    EnvelopeFollower supply;
    DcBlocker dcPre, dcPost;
    Smoother preGain, masterGain, outGain;
    float stackMakeup = 1.0f;

    // The tone knobs (bass, mid, treble, presence, depth, and gain for the bright cap) glide to
    // where they are turned, the filters following every 32 samples: a filter that jumps clicks.
    enum { kBass, kMid, kTreble, kPresence, kDepth, kGain, kTone };
    float toneNow[kTone] {}, toneGoal[kTone] {};
    float toneGlide = 0.0f;
    bool toneSnap = true; // until sound runs, the knobs land at once
    void updateTone();
};
} // namespace rh
