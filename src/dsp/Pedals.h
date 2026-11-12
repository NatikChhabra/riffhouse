// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"

#include <atomic>
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace rh
{
// Cleans up the raw input: removes DC and mains hum (auto-detects 50 or 60 Hz)
// and, when Input Fix is on, restores the highs a low-impedance mic or line
// input steals from a passive pickup.
class InputConditioner
{
public:
    enum HumMode { humOff, humAuto, hum50, hum60 };

    void prepare (double sampleRate);
    void reset();
    void set (int humMode, bool inputFix, float fixAmountDb = 6.0f);
    void process (float* x, int n);

    // 0 = none detected yet, otherwise 50 or 60
    int detectedMains() const noexcept { return detected.load(); }

private:
    void setNotches (int mainsHz);
    void analyse (const float* x, int n);

    double sr = 48000.0;
    DcBlocker dc;
    static constexpr int kHarmonics = 6;
    Biquad notches[kHarmonics];
    int activeMains = 0;
    int mode = humAuto;
    bool fix = false;
    float fixAmount = -1.0f;
    void setFixAmount (float db);
    // (1 + s/w1) / (1 + s/w2): the exact opposite of a pickup loaded by a low input
    // impedance, which is a first-order low-pass
    struct Shelf
    {
        double b0 = 1, b1 = 0, a1 = 0, z = 0;
        void set (double sampleRate, double lowHz, double highHz) noexcept;
        void reset() noexcept { z = 0; }
        float process (float x) noexcept
        {
            const double y = b0 * x + z;
            z = b1 * x - a1 * y;
            if (std::abs (z) < 1e-25) z = 0;
            return (float) y;
        }
    };
    Shelf fixShelf;
    Biquad fixTop;

    // Goertzel accumulators for 50/60 Hz families
    double g50 = 0, g60 = 0, total = 0;
    int counted = 0;
    struct Goertzel { double coeff = 0, s1 = 0, s2 = 0; };
    Goertzel gz[6];
    std::atomic<int> detected { 0 };
    int votes50 = 0, votes60 = 0;
};

// Listens to the raw input while you play and measures how much treble it has
// (the 1.8 to 3.6 kHz band against the band around 250 Hz). A passive pickup
// into a low-impedance PC mic jack loses a lot of treble; setup compares this
// with a guitar on a proper instrument input to decide how much Input Fix you need.
// process() runs on the audio thread; restart() and the readers are safe from any thread.
class TiltProbe
{
public:
    void prepare (double sampleRate);
    void process (const float* x, int n);
    void restart() noexcept { restartRequested.store (true); }
    double secondsMeasured() const noexcept { return seconds.load(); }
    // high band minus low band in dB; only meaningful once secondsMeasured() > 1
    float tiltDb() const noexcept { return tilt.load(); }

    // What a real electric guitar reads on an instrument (Hi-Z) input: three minutes
    // of a clean recorded performance measured -14 dB, and ten-second stretches of it
    // mostly -12 to -18.
    static constexpr float referenceTiltDb = -14.0f;
    static constexpr float kMostFixDb = 24.0f;
    // Input Fix amount (dB) that brings a measured tilt back to the reference. Nothing
    // is mended below `smallestLossDb`: 5 dB covers the spread between guitars, an input
    // known to load a pickup (a PC mic jack) can be held to less.
    static float fixAmountFor (float measuredTiltDb, float smallestLossDb = 5.0f) noexcept;
    // How far that amount moves the reading (dB), for a pickup loaded the usual way.
    static float tiltGainOf (float fixAmountDb) noexcept;

private:
    double sr = 48000.0;
    Biquad lowBand, highBand[4];
    double lowE = 0, highE = 0;
    std::atomic<bool> restartRequested { true };
    std::atomic<double> seconds { 0.0 };
    std::atomic<float> tilt { 0.0f };
};

// The threshold the player (or setup) chose is the least the gate asks for. On top of that
// it follows the room: when what it hears has been the same steady hiss or hum for a good
// second and a half, that is the noise floor, and the gate stays a little above it. So a
// noisier evening, a guitar turned up or more Input Fix do not let hiss through, and
// nobody has to run setup again for it. A ringing note fades and is never taken for noise.
class NoiseGate
{
public:
    static constexpr float kAboveFloorDb = 15.0f;   // threshold over the floor's level (its peaks sit about 10 dB up)
    static constexpr float kHighestFollowDb = -36.0f; // following never asks for more than this: notes must still ring out

    void prepare (double sampleRate);
    void reset();
    void set (float thresholdDb, float releaseMs);
    void setFollowsNoise (bool f) noexcept { follows = f; }
    void process (float* x, int n);
    bool isOpen() const noexcept { return open; }
    float thresholdNowDb() const noexcept { return thrNowDb; }
    float noiseFloorDb() const noexcept { return floorDb; } // -120 until one has been heard

private:
    void windowDone (float levelDb);
    void applyThreshold();

    double sr = 48000.0;
    EnvelopeFollower env;
    float userThrDb = -62.0f, thrNowDb = -62.0f, floorDb = -120.0f;
    bool follows = true;
    static constexpr int kWindows = 16;             // 1.6 s of 0.1 s windows
    float windows[kWindows] {};
    int windowAt = 0, windowsHeard = 0, windowLength = 4800, windowFill = 0;
    double windowPower = 0.0;
    float thrOpen = 0.001f, thrClose = 0.0005f;
    float gain = 0.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f;
    int holdSamples = 0, holdCounter = 0;
    bool open = false;
};

class Compressor
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (float sustain, float attack, float level, float blend); // 0..10 knobs
    void process (float* x, int n);
    float gainReductionDb() const noexcept { return grDb; }

private:
    double sr = 48000.0;
    float thresholdDb = -20, ratio = 4, makeup = 1, mix = 1;
    float envDb = -120.0f, att = 0, rel = 0, grDb = 0;
};

class Wah
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (int mode, float pos, float sens, float q);
    void process (float* x, int n);

private:
    void processClassic (float* x, int n);
    double sr = 48000.0;
    int mode = 1;
    float pos = 0.5f, sens = 0.6f, res = 0.5f;
    float ic1 = 0, ic2 = 0, hp = 0;
    EnvelopeFollower env;
    Smoother posSmooth;
};

// Analog-style octave divider plus a low-latency pitch shifter.
class PitchPedal
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (int mode, float semitones, float mix);
    void process (float* x, int n);
    int latencySamples() const noexcept { return 0; } // dry path is never delayed

private:
    double sr = 48000.0;
    int mode = 0;
    float ratio = 0.5f, mix = 0.5f;
    // sub octave
    OnePole trackLp, subLp1, subLp2, upHp;
    EnvelopeFollower subEnv;
    float flip = 1.0f, lastSign = 0.0f;
    // shifter
    DelayLine line;
    float phase = 0.0f;
    int window = 1024;
};

class DrivePedal
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void set (int type, float drive, float tone, float level);
    void process (float* x, int n);
    float latencySamples() const;

private:
    void update();
    double sr = 48000.0, osr = 192000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int type = 2;
    float drive = 0.4f, tone = 0.5f, level = 0.5f;
    bool dirty = true;
    Biquad preHp, preLp, midHump, toneLp, toneHp, postLp, scoop;
    OnePole clipHp, gbwLp;
    DcBlocker dc;
    Smoother gainS, levelS;
    float clean = 0.0f;
};

class GraphicEq
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (const float* gainsDb7, float levelDb);
    void process (float* x, int n);

private:
    double sr = 48000.0;
    Biquad bands[7];
    float gains[7] {};
    float level = 1.0f;
    bool initialised = false;
};

// Everything from here on is stereo.
class Modulation
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (int type, float rate, float depth, float mix);
    void process (float* l, float* r, int n);

private:
    double sr = 48000.0;
    int type = 0;
    float rateHz = 1.0f, depth = 0.5f, mix = 0.5f;
    Lfo lfo;
    DelayLine dl, dr;
    float fbL = 0, fbR = 0;
    float apL[6] {}, apR[6] {};
    Smoother depthS, mixS;
};

class Delay
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (int type, float timeMs, float feedback, float tone, float mix);
    void process (float* l, float* r, int n);

private:
    double sr = 48000.0;
    int type = 0;
    float feedback = 0.3f, mix = 0.3f;
    DelayLine dl, dr;
    Smoother timeS, mixS;
    OnePole lpL, lpR, hpL, hpR;
    Lfo wow, flutter;
    float toneHz = 8000.0f;
};

class Reverb
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (int type, float decay, float preDelayMs, float tone, float mix);
    void process (float* l, float* r, int n);

private:
    void configure();
    static constexpr int N = 8;
    double sr = 48000.0;
    int type = 0;
    float decay = 0.4f, preMs = 10.0f, tone = 0.5f, mix = 0.2f;
    bool dirty = true;

    DelayLine pre;
    DelayLine diff[4];
    float diffLen[4] {};
    DelayLine lines[N];
    float lineLen[N] {}, lineGain[N] {};
    OnePole damp[N];
    OnePole lowCut[N];
    Lfo mod;
    float modDepth = 0.0f, size = 1.0f, diffusion = 0.7f, inputLp = 12000.0f;
    OnePole bandwidth;
    // spring
    float springAp[12] {};
    DelayLine springLine;
    OnePole springLp, springHp;
    // shimmer
    DelayLine shimmerLine;
    float shimPhase = 0.0f, shimFb = 0.0f;
    float lastOut[N] {};
    Smoother mixS;
};

class LoFi
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (float crush, float downsample, float wobble, float noise, float radio, float mix);
    void process (float* l, float* r, int n);

private:
    double sr = 48000.0;
    float bits = 24, holdRatio = 1, wobble = 0, noiseAmt = 0, mix = 1;
    float holdCounter = 0, heldL = 0, heldR = 0;
    DelayLine dl, dr;
    Lfo wow, flutter;
    Noise noise { 0xBEEF };
    OnePole hissLp, crackleLp;
    Biquad radioHpL, radioHpR, radioLpL, radioLpR, preLpL, preLpR;
    float drift = 0.0f;
};

class Doubler
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (float amount);
    void process (float* l, float* r, int n);

private:
    double sr = 48000.0;
    float amount = 0.0f;
    DelayLine line;
    Lfo lfo1, lfo2;
    OnePole tone;
    Smoother amountS;
};

// Output safety: transparent below -6 dBFS, never exceeds about -0.3 dBFS.
class SafetyLimiter
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n);

private:
    float env = 0.0f, rel = 0.0f;
};
} // namespace rh
