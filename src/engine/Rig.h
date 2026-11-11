// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../dsp/Amp.h"
#include "../dsp/Cabinet.h"
#include "../dsp/NeuralCapture.h"
#include "../dsp/Pedals.h"
#include "Params.h"

#include <atomic>

namespace rh
{
// Click-free on/off for a pedal: a short crossfade instead of a hard switch.
class Fade
{
public:
    void prepare (double sampleRate) { step = (float) (1.0 / (0.012 * sampleRate)); }
    void setOn (bool on) noexcept { target = on ? 1.0f : 0.0f; }
    void snap (bool on) noexcept { target = value = on ? 1.0f : 0.0f; }
    bool idle() const noexcept { return target == 0.0f && value == 0.0f; }
    bool full() const noexcept { return target == 1.0f && value == 1.0f; }
    bool justStarting() const noexcept { return target == 1.0f && value == 0.0f; }
    float next() noexcept
    {
        if (value < target) value = std::min (target, value + step);
        else if (value > target) value = std::max (target, value - step);
        return value;
    }

private:
    float value = 0.0f, target = 0.0f, step = 0.01f;
};

struct RigMeters
{
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    std::atomic<float> gateInPeak { 0.0f }; // level the gate sees (after input gain and hum filter)
    std::atomic<float> compReduction { 0.0f };
    std::atomic<bool> gateOpen { false };
    std::atomic<int> mains { 0 };
    std::atomic<int> clipEvents { 0 };
};

// The whole signal chain. Mono guitar in, stereo out.
class Rig
{
public:
    Rig();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void setParams (const ParamValues& v); // call once per block before process
    void setTempo (double bpm) noexcept { tempo = bpm > 20.0 ? bpm : 120.0; }
    void process (const float* in, float* outL, float* outR, int n);

    int latencySamples() const;
    RigMeters& meters() noexcept { return m; }
    Cabinet& cabinet() noexcept { return cab; }
    NeuralCapture& neural() noexcept { return nam; }
    TiltProbe& tiltProbe() noexcept { return probe; }

    // Offline use: make every generated IR active immediately.
    void finishPendingWork() { cab.rebuildNow(); }

private:
    void processMono (float* x, int n);
    void processStereo (float* l, float* r, int n);
    float delayTimeMs() const;

    double sr = 48000.0;
    int maxBlock = 512;
    double tempo = 120.0;
    ParamValues p {};
    bool firstParams = true;

    InputConditioner input;
    TiltProbe probe;
    NoiseGate gate;
    Compressor comp;
    Wah wah;
    PitchPedal pitch;
    DrivePedal drive;
    Amp amp;
    NeuralCapture nam;
    Biquad namBass, namMid, namTreble;
    Cabinet cab;
    GraphicEq eq;
    Modulation mod;
    Delay delay;
    Reverb reverb;
    LoFi lofi;
    Doubler doubler;
    SafetyLimiter limiter;

    Fade fGate, fComp, fWah, fPitch, fDrive, fAmp, fCab, fEq, fMod, fLofi;
    bool delayActive = false, reverbActive = false;
    int delayQuiet = 0, reverbQuiet = 0;

    Smoother inGain, outGain;
    std::vector<float> dry, dryR, tailL, tailR;
    RigMeters m;
};
} // namespace rh
