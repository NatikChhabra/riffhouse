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
    std::atomic<float> gateThreshNow { -62.0f }; // where the gate sits at the moment (it follows the noise floor)
    std::atomic<float> stagePeak { 0.0f };  // the loudest that level has been since someone last took it (exchange with 0)
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
    // A whole new tone is on its way (a tone picked, undo, a shared tone). The rig dips for a
    // few milliseconds, swaps every setting while it is silent, and comes back up, so nothing
    // clicks; delay and reverb tails of the old tone ring on into the new one. Audio thread.
    void beginToneChange() noexcept { if (! firstParams) swapWanted = true; }
    void setTempo (double bpm) noexcept { tempo = bpm > 20.0 ? bpm : 120.0; }
    void process (const float* in, float* outL, float* outR, int n);

    int latencySamples() const;
    RigMeters& meters() noexcept { return m; }
    Cabinet& cabinet() noexcept { return cab; }
    NeuralCapture& neural() noexcept { return nam; }
    TiltProbe& tiltProbe() noexcept { return probe; }

    // Offline use: make every generated IR active immediately.
    void finishPendingWork() { cab.rebuildNow(); }
    // The same, only when a setting has changed since (cheap to call every block offline).
    void finishChangedWork() { if (cab.workPending()) cab.rebuildCrossfaded(); }

    // Auto level's correction, applied after the rig and before the volume knob. Any thread.
    void setLevelTrimDb (float db) noexcept { levelTrimDb.store (db); }
    float levelTrim() const noexcept { return levelTrimDb.load(); }
    // A rig that only measures: no safety limiter, so a loud tone reads as loud as it is.
    void setMeasuring (bool on) noexcept { measuring = on; cab.setOffline (on); gate.setFollowsNoise (! on); }
    // A rig that renders off stage: nothing is built in the background, so snapParams and
    // startFresh leave it complete. Call before prepare().
    void setOffline (bool o) noexcept { cab.setOffline (o); }
    // Put the rig in this state at once, from silence, with nothing fading or gliding.
    void snapParams (const ParamValues&);
    // The same, and as if it had just been built: every delay line empty, every sweep at its
    // start. Two renders after this are the same sample for sample. Not for the audio thread.
    void startFresh (const ParamValues&, double sampleRate, int maxBlock);
    // Where the player's guitar is caught after the input stage (see PhraseCatcher). Set before audio runs.
    void setInputTap (class PhraseCatcher* t) noexcept { tap = t; }

private:
    void applyParams (const ParamValues& v, bool toneChange);
    bool needsSwap (const ParamValues& next) const;
    void processMono (float* x, int n);
    void processStereo (float* l, float* r, int n);
    float delayTimeMs() const;

    double sr = 48000.0;
    int maxBlock = 512;
    double tempo = 120.0;
    ParamValues p {};
    bool firstParams = true;
    // the dip around a tone change: out, swap, back in
    enum class Swap { none, out, hold, in };
    Swap swap = Swap::none;
    bool swapWanted = false;
    ParamValues swapTo {};
    float swapGain = 1.0f, swapOutStep = 0.1f, swapInStep = 0.05f;
    int swapOutLeft = 0, swapHoldLeft = 0, swapHoldLen = 0;

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
    // A new mod type crossfades from the old one (a copy that plays on for 20 ms) instead of
    // cutting its ringing off, which clicked.
    Modulation modOld;
    int modXfade = 0, modXfadeLen = 1, modTypeNow = -1;
    std::vector<float> modTmpL, modTmpR;
    // Two of each, so the old tone's echoes and room ring on while the new tone starts afresh.
    Delay delayA, delayB;
    Reverb reverbA, reverbB;
    Delay* dly = &delayA;
    Delay* dlyTail = &delayB;
    Reverb* rev = &reverbA;
    Reverb* revTail = &reverbB;
    bool dlyTailBusy = false, revTailBusy = false;
    int dlyTailQuiet = 0, revTailQuiet = 0;
    // a tail from two tones back fades out (straight line, 20 ms) to free its pair for the next change
    float dlyTailGain = 1.0f, revTailGain = 1.0f, tailFadeStep = 0.001f;
    bool dlyTailFading = false, revTailFading = false;
    LoFi lofi;
    Doubler doubler;
    SafetyLimiter limiter;

    Fade fGate, fComp, fWah, fPitch, fDrive, fAmp, fCab, fEq, fMod, fLofi;
    bool delayActive = false, reverbActive = false;
    int delayQuiet = 0, reverbQuiet = 0;

    Smoother inGain, outGain, trimGain;
    class PhraseCatcher* tap = nullptr;
    std::atomic<float> levelTrimDb { 0.0f };
    bool measuring = false;
    std::vector<float> dry, dryR, tailL, tailR, gateGains;
    static constexpr int kGateDelay = 2048; // the gate's gains, waiting for the drive and amp to catch up
    float gateDelay[kGateDelay] {};
    int gateDelayAt = 0;
    RigMeters m;
};
} // namespace rh
