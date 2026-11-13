// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Circuits.h"
#include "Common.h"
#include "Whine.h"

#include <atomic>
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace rh
{
// Takes mains hum out by subtracting it, not by notching. It keeps a model of the hum (the
// first six harmonics of the mains, each a level and a phase) and learns it only while the
// guitar is quiet, so the notes never move it. A notch filter on 50 Hz and its harmonics would
// also cut G, B and D (98, 196, 247, 294 Hz) and, worse, ring on for a tenth of a second after
// every note stops near those pitches: a high-gain amp turns that ringing into crunch on muted
// strings. The model also follows the mains drifting off 50 or 60 Hz. If it has not been able
// to learn for a long while (one long phrase) it lets go of the hum gently rather than guess.
class HumCanceller
{
public:
    static constexpr int kHarmonics = 40; // up to 2 kHz at 50 Hz mains: a ground loop or a dimmer buzzes far above the hum itself
    void prepare (double sampleRate);
    void reset();
    void setMains (int hz);
    int mains() const noexcept { return mainsHz; }
    float process (float x) noexcept;
    float driftHz() const noexcept { return (float) (wCorr * sr / 6.283185307179586); }
    bool learning() const noexcept { return adaptHold == 0; }

private:
    void stepDone();

    double sr = 48000.0;
    int mainsHz = 0;
    double w0 = 0.0, wCorr = 0.0;
    double zr = 1.0, zi = 0.0, rr = 1.0, ri = 0.0; // phasor of the mains, and its turn per sample
    double a[kHarmonics] {}, b[kHarmonics] {};
    double cs[kHarmonics] {}, sn[kHarmonics] {};
    double mu = 0.0;
    // when it may learn: the last 20 ms against the quietest it has been lately
    int stepLength = 960, stepFill = 0;
    double stepPower = 0.0, floorPower = -1.0, floorRise = 1.0;
    double stepPeak = 0.0, floorPeak = -1.0; // the loudest moment of each 20 ms, and the quietest of those lately
    float freezeAbove = 0.0f;
    int adaptHold = 0, holdSamples = 0, frozenFor = 0, letGoAfter = 0;
    double use = 0.0, useStep = 0.0; // how much of the model is subtracted (fades out when it cannot learn)
    double lastTheta = 0.0; bool haveTheta = false; int thetaHarmonic = 0;
    // the input's own quiet level: hum is part of it, so the model is never let be louder
    double inPower = 0.0, inFloor = -1.0;
    // the last 300 ms of 20 ms levels: hum and hiss hold still, a fading note does not
    static constexpr int kSteady = 15;
    float levels[kSteady] {};
    int levelAt = 0, levelsHeard = 0;
    void forget();
};

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
    // steady whines found while the guitar is quiet, and being cut out (with the hum filter on)
    const WhineRemover& whines() const noexcept { return whine; }

private:
    void setNotches (int mainsHz);
    void analyse (const float* x, int n);

    double sr = 48000.0;
    DcBlocker dc;
    HumCanceller hum;
    WhineRemover whine;
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

    // With Input Fix on, the hiss of a PC input comes up with the treble. A note's own treble
    // stands well above it and covers it; as the note fades, or between notes, it does not.
    // So the treble (above about 2.2 kHz) is split off and turned down while it is near the
    // hiss, the way tape noise reduction worked: the noise floor of that band is learnt from
    // steady quiet moments, and within 12 dB of it the band is expanded down, up to 18 dB.
    // Playing never waits for it: it opens in about a millisecond.
    struct HissReducer
    {
        void prepare (double sampleRate);
        void reset();
        float process (float x) noexcept;
        float floorDb() const noexcept;
        Biquad split;
        double sr = 48000.0;
        float env = 0.0f, gain = 1.0f;
        float envCoeff = 0.0f, openCoeff = 0.0f, closeCoeff = 0.0f;
        // the band's floor, learnt from steady 20 ms steps
        float stepPower = 0.0f, floorPower = -1.0f, runRef = 0.0f, runSum = 0.0f;
        int stepLength = 960, stepAt = 0, runLength = 0;
    };
    HissReducer hiss;

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
    // What was heard since the restart, kept by block level (1 dB steps): which levels count as
    // playing is decided afresh over everything heard, so a take that starts on a ringing chord
    // is judged by the quiet that comes later too.
    static constexpr int kLevels = 121; // -120 .. 0 dBFS
    std::array<double, kLevels> lowAt {}, highAt {}, secondsAt {};
    int loudest = -1, quietest = kLevels;
    std::atomic<bool> restartRequested { true };
    std::atomic<double> seconds { 0.0 };
    std::atomic<float> tilt { 0.0f };
};

// The threshold the player (or setup) chose is the least the gate asks for. On top of that
// it follows the room: when what it hears has been the same steady hiss or hum for a good
// second and a half, that is the noise floor, and the gate stays a little above it. So a
// noisier evening, a guitar turned up or more Input Fix do not let hiss through, and
// nobody has to run setup again for it. A ringing note fades and is never taken for noise.
//
// It hears the loudest the guitar has been over the last 20 ms (one swing of the lowest
// string), so it neither flutters on a low note nor lags behind it. When the strings are
// stopped by a hand, the level falls 18 dB or more inside about 30 ms, faster than any note
// fades on its own: the gate closes in about 12 ms then, without waiting for the threshold,
// and only a new pick opens it again. That is what makes muted chugs tight. The Rig also
// applies the same gain after the amp (gainsOut), because a high-gain amp squashes a fade
// made in front of it: 20 dB less going in is a few dB less coming out, and the hiss of a
// closing gate turns into crunch.
class NoiseGate
{
public:
    static constexpr float kAboveFloorDb = 17.0f;   // threshold over the floor's level (hiss peaks sit about 10 dB up; it closes 4 dB lower)
    static constexpr float kAbovePeaksDb = 6.0f;    // ... and over the floor's own loudest peaks: buzzy hum peaks far above its level
    // following never asks for more than the tone's own threshold plus this, kept between -40
    // and -30 dB: notes must still ring out, and a clean tone (set low for soft playing) is let
    // hum a little before it cuts quiet notes
    static constexpr float kMostFollowAboveDb = 28.0f, kFollowCapLowDb = -40.0f, kFollowCapHighDb = -30.0f;
    static constexpr float kStopDropDb = 18.0f;     // this much down inside about 30 ms is a hand on the strings
    static constexpr float kStopRangeDb = 24.0f;    // ... when it lands within this of the threshold

    void prepare (double sampleRate);
    void reset();
    void set (float thresholdDb, float releaseMs);
    void setFollowsNoise (bool f) noexcept { follows = f; }
    // Gates x in place. gainsOut, when given, gets the gain used on each sample.
    void process (float* x, int n, float* gainsOut = nullptr);
    bool isOpen() const noexcept { return open; }
    float thresholdNowDb() const noexcept { return thrNowDb; }
    float noiseFloorDb() const noexcept { return floorDb; } // -120 until one has been heard

private:
    void windowDone (float levelDb, float peakDb);
    void applyThreshold();
    void stepDone();

    double sr = 48000.0;
    float userThrDb = -62.0f, thrNowDb = -62.0f, floorDb = -120.0f, floorPeakDb = -120.0f;
    bool follows = true;
    static constexpr int kWindows = 16;             // 1.6 s of 0.1 s windows
    static constexpr int kFirstWindows = 8;         // 0.8 s for the very first floor
    float windows[kWindows] {}, windowPeaks[kWindows] {};
    float windowPeak = 0.0f;
    int windowAt = 0, windowsHeard = 0, windowLength = 4800, windowFill = 0;
    double windowPower = 0.0;
    // the detector: peaks of 2.5 ms steps, the last 16 of them
    static constexpr int kSteps = 16;
    float steps[kSteps] {};
    int stepAt = 0, stepLength = 120, stepFill = 0;
    float stepPeak = 0.0f, heldPeak = 0.0f, recentPeak = 0.0f, earlierPeak = 0.0f;
    float reopenAbove = 0.0f, reopenDecay = 1.0f;
    float thrOpen = 0.001f, thrClose = 0.0005f;
    float gain = 0.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f, stopCoeff = 0.0f;
    int holdSamples = 0, holdCounter = 0;
    bool open = false, stopping = false;
};

class Compressor
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (float sustain, float attack, float level, float blend); // 0..10 knobs
    void process (float* x, int n);
    // While switched off: follow the guitar's level, so switching on squeezes from the first
    // sample instead of letting a loud note through for an attack time and then clamping down.
    void listen (const float* x, int n);
    // Switching on: the knobs land where they are, the level it followed is kept.
    void wake() noexcept { makeupSmooth.reset (makeupDb); mixSmooth.reset (mix); threshSmooth.reset (thresholdDb); ratioSmooth.reset (ratio); }
    float gainReductionDb() const noexcept { return grDb; }

private:
    double sr = 48000.0;
    float thresholdDb = -20, ratio = 4, makeupDb = 0, mix = 1;
    float envDb = -120.0f, att = 0, rel = 0, grDb = 0;
    Smoother makeupSmooth, mixSmooth, threshSmooth, ratioSmooth; // turned knobs glide, never step;
    // the makeup glides in dB, as the squeeze does, so the two keep step and nothing overshoots
    void follow (float in) noexcept;
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
    Smoother mixS; // the blend glides: a fast turn of Mix must not click
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
    std::unique_ptr<juce::dsp::Oversampling<float>> os, osHigh; // 8x, and 16x for the hard clippers (rodent, fuzz, metal)
    juce::dsp::Oversampling<float>* active() const noexcept { return (type == 4 || type == 5 || type == 6) && osHigh ? osHigh.get() : os.get(); }
    int type = 2;
    float drive = 0.4f, tone = 0.5f, level = 0.5f;
    // where the circuit's drive and tone pots are on the way to the knobs: a fast turn glides
    // (about 20 ms) instead of stepping the circuit once a block, which clicked
    double driveNow = -1.0, toneNow = -1.0;
    void setPots (double d, double t, bool all);
    bool dirty = true;
    Smoother levelS;
    // the pedals, each built as its circuit (see Circuits.h and the pedal lab)
    circuit::GreenDrive green;
    circuit::RodentDist rodent;
    circuit::BigFuzz fuzz;
    circuit::CleanBoost boost;
    circuit::TrebleBooster trebleBoost;
    circuit::BluesDrive blues;
    circuit::MetalBox metal;
    double runCircuit (double v) noexcept;
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
    float gains[7] {};    // where the sliders are, dB
    float gainsNow[7] {}; // where the bands are on the way there: a jumped band clicks
    float level = 1.0f, glide = 0.0f;
    Smoother levelS;
    bool initialised = false, snap = true;
    void setBand (int b, float db);
};

// Everything from here on is stereo.
class Modulation
{
public:
    void prepare (double sampleRate);
    void reset();
    void set (int type, float rate, float depth, float mix);
    void process (float* l, float* r, int n);
    // While switched off: keep the delay lines full of what is playing, so a chorus switched on
    // starts with its voices already sounding instead of entering a few milliseconds late as a click.
    void listen (const float* l, const float* r, int n);
    // Switched on again: clear what rings, keep the delay lines (see listen).
    void wake();

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
    // Digital and ping-pong move to a new time by crossfading two read heads, as digital
    // delays do; analog and tape glide there, pitch bend and all, as the real ones do.
    float headA = -1.0f, headB = 0.0f, xf = 0.0f, xfStep = 0.0f;
    bool crossing = false;
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
    float gainNow[N] {}, preNow = -1.0f, glideK = 0.0f; // Decay and Pre-delay glide there
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
    void listen (const float* l, const float* r, int n); // as Modulation::listen, for the tape wobble
    void wake();

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
    // Wobble, Radio and Mix glide to a turned knob: jumped, each one clicks on a held note
    Smoother wobS, radioS, mixS, bitsS;
    float radioNow = -1.0f, radioTarget = 0.0f;
    void setRadio (float rd);
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
