// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
//
// Finds steady whines on a noisy input and cuts each one out with a very
// narrow notch. Why it can be trusted not to touch playing: see Whine.h.
#include "Whine.h"

#include <algorithm>
#include <cmath>

namespace rh
{
namespace
{
constexpr double kQuietInputPower = 3.16e-5;  // -45 dBFS: an input noisier than this is not judged
constexpr double kSilencePower = 1.0e-13;     // digital silence: nothing to learn from
constexpr double kLowestHz = 1000.0;          // below this the hum filter works, and notes live
constexpr int kAround = 40;                   // bins each side for the noise around a line
constexpr float kStandsOut = 31.6f;           // 15 dB over that noise
constexpr float kNarrow = 16.0f;              // 12 dB down three bins either side, like a pure tone
constexpr double kLeastAmplitude = 1.0e-5;    // -100 dBFS: quieter is never heard
constexpr int kMostLooksMissed = 2;           // of 24
constexpr float kMostLevelSwingDb = 1.5f;
constexpr int kMostPitchSpreadBins = 2;
constexpr double kSameLineHz = 2.0;
}

void WhineRemover::prepare (double sampleRate)
{
    sr = sampleRate;
    decim = std::max (1, (int) std::lround (sr / 48000.0));
    fsA = sr / decim;
    binHz = fsA / kSize;
    for (auto& f : aa) f.reset();
    if (decim > 1)
    {
        aa[0].set (Biquad::Type::lowpass, sr, 0.42 * fsA, 0.5412);
        aa[1].set (Biquad::Type::lowpass, sr, 0.42 * fsA, 1.3066);
    }
    if (fft == nullptr) fft = std::make_unique<juce::dsp::FFT> (kOrder);
    window.resize (kSize);
    for (int i = 0; i < kSize; ++i)
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (kTwoPi * i / kSize));
    frame.assign (kSize, 0.0f);
    work.assign (2 * kSize, 0.0f);
    spectra.assign ((size_t) kFramesPerRound * kBins, 0.0f);
    scratch.assign (2 * kAround + 1, 0.0f);
    lastRound.reserve (kMaxNotches * 4);
    rampCoeff = std::exp (-1.0 / (0.15 * sr));
    forget();
}

void WhineRemover::forget()
{
    frameAt = 0; framePower = 0.0; decimAt = 0;
    history.fill (0.0); historyAt = historyHeard = 0;
    framesInRound = 0; roundReady = false;
    lastRound.clear();
    for (auto& n : notches) n = Notch {};
    anyNotch = false;
    for (auto& h : shownHz) h.store (0.0f);
    shownCount.store (0);
}

void WhineRemover::reset()
{
    for (auto& f : aa) f.reset();
    for (auto& n : notches) n.z1 = n.z2 = n.w1 = n.w2 = 0.0;
}

void WhineRemover::frameDone()
{
    const double p = framePower / kSize;
    history[(size_t) historyAt] = p;
    historyAt = (historyAt + 1) % kHistory;
    historyHeard = std::min (historyHeard + 1, kHistory);
    if (p < kSilencePower || historyHeard < 8 || roundReady) return;

    double quietest = 1.0e9;
    for (int i = 0; i < historyHeard; ++i)
        if (history[(size_t) i] >= kSilencePower) quietest = std::min (quietest, history[(size_t) i]);
    if (quietest > kQuietInputPower || p > 2.0 * quietest) return; // playing, or an input too noisy to judge

    for (int i = 0; i < kSize; ++i) work[(size_t) i] = frame[(size_t) i] * window[(size_t) i];
    std::fill (work.begin() + kSize, work.end(), 0.0f);
    fft->performRealOnlyForwardTransform (work.data(), true);
    float* s = spectra.data() + (size_t) framesInRound * kBins;
    for (int k = 0; k < kBins; ++k)
    {
        const float re = work[(size_t) (2 * k)], im = work[(size_t) (2 * k + 1)];
        s[k] = re * re + im * im;
    }
    if (++framesInRound >= kFramesPerRound) roundReady = true;
}

void WhineRemover::decide()
{
    std::vector<float>& avg = work; // free until the next frame
    std::fill (avg.begin(), avg.begin() + kBins, 0.0f);
    for (int f = 0; f < kFramesPerRound; ++f)
    {
        const float* s = spectra.data() + (size_t) f * kBins;
        for (int k = 0; k < kBins; ++k) avg[(size_t) k] += s[k];
    }
    for (int k = 0; k < kBins; ++k) avg[(size_t) k] /= (float) kFramesPerRound;

    std::vector<Line> found;
    const int kLo = std::max (kAround + 1, (int) std::ceil (kLowestHz / binHz));
    const int kHi = std::min (kBins - kAround - 2, (int) std::floor (0.45 * fsA / binHz));
    for (int k = kLo; k <= kHi; ++k)
    {
        const float c = avg[(size_t) k];
        if (c < avg[(size_t) k - 1] || c <= avg[(size_t) k + 1]) continue;
        if (c < 10.0f * 0.5f * (avg[(size_t) k - 6] + avg[(size_t) k + 6])) continue;
        std::copy (avg.begin() + (k - kAround), avg.begin() + (k + kAround + 1), scratch.begin());
        std::nth_element (scratch.begin(), scratch.begin() + 24, scratch.end());
        const float noise = scratch[24]; // the 30th percentile: other lines nearby do not lift it
        if (noise <= 0.0f || c < kStandsOut * noise) continue;
        if (avg[(size_t) k - 3] > c / kNarrow || avg[(size_t) k + 3] > c / kNarrow) continue;
        if (4.0 * std::sqrt ((double) c) / kSize < kLeastAmplitude) continue;

        // in (nearly) every quiet look, at one pitch and one level
        int present = 0, lowBin = k + 99, highBin = k - 99;
        double sum = 0.0, sum2 = 0.0;
        for (int f = 0; f < kFramesPerRound; ++f)
        {
            const float* s = spectra.data() + (size_t) f * kBins;
            const float lv = s[k - 1] + s[k] + s[k + 1];
            if (lv > 3.0f * 6.31f * noise) ++present; // 8 dB clear
            const double db = 10.0 * std::log10 (std::max (lv, 1.0e-30f));
            sum += db; sum2 += db * db;
            int at = k - 2;
            for (int j = k - 1; j <= k + 2; ++j) if (s[j] > s[at]) at = j;
            lowBin = std::min (lowBin, at); highBin = std::max (highBin, at);
        }
        const double mean = sum / kFramesPerRound;
        const double swing = std::sqrt (std::max (0.0, sum2 / kFramesPerRound - mean * mean));
        if (present < kFramesPerRound - kMostLooksMissed || swing > kMostLevelSwingDb
            || highBin - lowBin > kMostPitchSpreadBins) continue;

        const double l = std::log (std::max (avg[(size_t) k - 1], 1.0e-30f)), m = std::log (c),
                     r = std::log (std::max (avg[(size_t) k + 1], 1.0e-30f));
        const double den = l - 2.0 * m + r;
        const double shift = den < 0.0 ? std::clamp (0.5 * (l - r) / den, -0.5, 0.5) : 0.0;
        found.push_back ({ (k + shift) * binHz, (float) (10.0 * std::log10 (c / noise)) });
    }
    std::sort (found.begin(), found.end(), [] (const Line& a, const Line& b) { return a.prominenceDb > b.prominenceDb; });
    if ((int) found.size() > kMaxNotches) found.resize (kMaxNotches);
    apply (found);
}

void WhineRemover::apply (const std::vector<Line>& lines)
{
    auto near = [] (double a, double b) { return std::abs (a - b) <= kSameLineHz; };

    // a notch stays while its line is still there; two rounds without it and it fades out
    for (auto& n : notches)
    {
        if (! n.used || n.target <= 0.0) continue;
        const bool still = std::any_of (lines.begin(), lines.end(), [&] (const Line& l) { return near (l.hz, n.hz); });
        n.misses = still ? 0 : n.misses + 1;
        if (n.misses >= 2) n.target = 0.0;
    }

    // a new notch only for a line seen in this round and the one before
    for (const auto& l : lines)
    {
        const bool before = std::any_of (lastRound.begin(), lastRound.end(), [&] (const Line& o) { return near (o.hz, l.hz); });
        if (! before) continue;
        const bool have = std::any_of (notches.begin(), notches.end(), [&] (const Notch& n) { return n.used && n.target > 0.0 && near (n.hz, l.hz); });
        if (have) continue;
        auto slot = std::find_if (notches.begin(), notches.end(), [] (const Notch& n) { return ! n.used; });
        if (slot == notches.end()) break;
        // two 4 Hz notches in a row: as much taken from the playing as one 6 Hz notch, but
        // twice as deep, and still deep if the whine drifts a little
        const double bw = std::max (4.0, 0.0005 * l.hz);
        const double w0 = kTwoPi * l.hz / sr;
        const double alpha = std::sin (w0) * bw / (2.0 * l.hz);
        Notch n;
        n.used = true; n.hz = l.hz;
        n.b0 = alpha / (1.0 + alpha);
        n.a1 = -2.0 * std::cos (w0) / (1.0 + alpha);
        n.a2 = (1.0 - alpha) / (1.0 + alpha);
        n.target = 1.0;
        *slot = n;
    }
    lastRound = lines;

    int shown = 0;
    for (const auto& n : notches)
        if (n.used && n.target > 0.0) shownHz[(size_t) shown++].store ((float) n.hz);
    shownCount.store (shown);
    anyNotch = std::any_of (notches.begin(), notches.end(), [] (const Notch& n) { return n.used; });
}

void WhineRemover::process (float* x, int n)
{
    if (roundReady)
    {
        decide();
        framesInRound = 0;
        roundReady = false;
    }

    for (int i = 0; i < n; ++i)
    {
        const float in = x[i];
        float v = in;
        bool take = true;
        if (decim > 1)
        {
            v = aa[1].process (aa[0].process (in));
            if (++decimAt < decim) take = false; else decimAt = 0;
        }
        if (take)
        {
            frame[(size_t) frameAt] = v;
            framePower += (double) v * v;
            if (++frameAt >= kSize)
            {
                frameDone();
                frameAt = 0;
                framePower = 0.0;
            }
        }

        if (anyNotch)
        {
            // one after another, so close neighbours (a buzz comb) never leak into each other's null
            double s = in;
            for (auto& nt : notches)
            {
                if (! nt.used) continue;
                nt.depth = nt.target + rampCoeff * (nt.depth - nt.target);
                const double y = nt.b0 * s + nt.z1;
                nt.z1 = -nt.a1 * y + nt.z2;
                nt.z2 = -nt.b0 * s - nt.a2 * y;
                s -= nt.depth * y;
                const double y2 = nt.b0 * s + nt.w1;
                nt.w1 = -nt.a1 * y2 + nt.w2;
                nt.w2 = -nt.b0 * s - nt.a2 * y2;
                s -= nt.depth * y2;
            }
            x[i] = (float) s;
        }
    }

    if (anyNotch)
    {
        bool still = false;
        for (auto& nt : notches)
        {
            if (nt.used && nt.target <= 0.0 && nt.depth < 1.0e-4) nt = Notch {};
            still = still || nt.used;
        }
        anyNotch = still;
    }
}
} // namespace rh
