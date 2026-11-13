// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// Finds steady whines on a noisy input and cuts each one out with a very
// narrow notch.
#pragma once

#include "Common.h"

#include <array>
#include <atomic>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace rh
{
// A cheap pickup, a phone charger, a laptop's power supply or a USB dongle can put a
// fixed high whine on the input (a few kHz up to past 10 kHz), or a comb of buzz far
// above where the hum filter works. A high-gain amp turns those into a constant
// whistle and fizz behind every note.
//
// It only acts on what cannot be playing. It listens only while the guitar is quiet
// (within 3 dB of the quietest the input has been lately, on an input whose quiet is
// below -45 dBFS) and only to 1 kHz and up. A line has to stand 15 dB clear of the
// noise around it, be as narrow as a pure tone, sit in at least 22 of 24 quiet looks
// at one pitch and a level that moves under 1.5 dB, and do that in two separate
// listening rounds (about 8 seconds of quiet in all) before anything is cut. A note
// fades, slides or changes; a whine does none of those. Anything unsure is left alone.
// Each notch is two 4 Hz notches in a row and fades in over half a second; with nothing found the
// sound passes through untouched.
class WhineRemover
{
public:
    static constexpr int kMaxNotches = 24;
    static constexpr int kFramesPerRound = 24;

    void prepare (double sampleRate);
    void reset();   // clears the filters, keeps what it has learnt about this input
    void forget();  // starts over (another input)
    void process (float* x, int n);

    // what is being removed now, for the screen
    int removing() const noexcept { return shownCount.load(); }
    float removingHz (int i) const noexcept { return i >= 0 && i < kMaxNotches ? shownHz[(size_t) i].load() : 0.0f; }

private:
    struct Line { double hz = 0.0; float prominenceDb = 0.0f; };
    void frameDone();
    void decide();
    void apply (const std::vector<Line>& lines);

    double sr = 48000.0, fsA = 48000.0, binHz = 1.0;
    int decim = 1, decimAt = 0;
    Biquad aa[2];

    static constexpr int kOrder = 13, kSize = 1 << kOrder, kBins = kSize / 2 + 1;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window, frame, work;
    int frameAt = 0;
    double framePower = 0.0;

    // the quietest frames lately (about 11 s)
    static constexpr int kHistory = 64;
    std::array<double, kHistory> history {};
    int historyAt = 0, historyHeard = 0;

    // this round's quiet frames, as power spectra
    std::vector<float> spectra; // kFramesPerRound * kBins
    int framesInRound = 0;
    bool roundReady = false;
    std::vector<float> scratch;

    std::vector<Line> lastRound;        // lines seen in the previous round, waiting to be seen again

    struct Notch
    {
        bool used = false;
        double hz = 0.0;
        double b0 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0, w1 = 0, w2 = 0; // band-pass (b1 = 0, b2 = -b0), run twice
        double depth = 0.0, target = 0.0;
        int misses = 0;
    };
    std::array<Notch, kMaxNotches> notches;
    double rampCoeff = 0.0;
    bool anyNotch = false;

    std::array<std::atomic<float>, kMaxNotches> shownHz {};
    std::atomic<int> shownCount { 0 };
};
} // namespace rh
