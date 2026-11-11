// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"
#include "ToneStack.h"

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

// The DSP amp. Runs 8x oversampled (4x at high sample rates). Mono.
class Amp
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setSettings (const AmpSettings& s);
    void process (float* data, int n);
    float latencySamples() const;

private:
    void updateFilters();
    static float tube (float x, float bias) noexcept;

    struct Stage
    {
        OnePole hp, lp;
        float gain = 1.0f, bias = 0.0f;
        EnvelopeFollower blocking;
    };

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
    EnvelopeFollower supply;
    DcBlocker dcPre, dcPost;
    Smoother preGain, masterGain, outGain;
    float stackMakeup = 1.0f;
};
} // namespace rh
