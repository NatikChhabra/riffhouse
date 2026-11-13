// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Tuner.h"

#include <cmath>

namespace rh
{
void Tuner::prepare (double sampleRate)
{
    sr = sampleRate;
    decimation = sampleRate > 60000.0 ? 4 : 2;
    window.assign (4096, 0.0f);
    diff.assign (2048, 0.0f);
    raw.assign (2048, 0.0f);
    fifo.reset();
}

void Tuner::push (const float* x, int n) noexcept
{
    // lowpass + decimate into a small local buffer, then write the exact count
    float local[1024];
    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        lpState += 0.35f * (x[i] - lpState);
        if (++decimCounter >= decimation)
        {
            decimCounter = 0;
            local[count++] = lpState;
            if (count == 1024)
            {
                write (local, count);
                count = 0;
            }
        }
    }
    if (count > 0)
        write (local, count);
}

void Tuner::write (const float* data, int count) noexcept
{
    const int k = std::min (count, fifo.getFreeSpace());
    const auto scope = fifo.write (k);
    int src = 0;
    scope.forEach ([&] (int dst) { ring[(size_t) dst] = data[src++]; });
}

Tuner::Reading Tuner::describe (float hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    Reading r;
    if (hz <= 0.0f) return r;
    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    r.valid = true;
    r.hz = hz;
    r.midiNote = (int) std::lround (midi);
    r.cents = (midi - (float) r.midiNote) * 100.0f;
    r.noteName = names[((r.midiNote % 12) + 12) % 12];
    r.octave = r.midiNote / 12 - 1;
    return r;
}

Tuner::Reading Tuner::analyse()
{
    const int N = (int) window.size();
    if (N == 0) return {};
    // pull everything new, keep the latest N samples
    int avail = fifo.getNumReady();
    while (avail > 0)
    {
        const int take = std::min (avail, N);
        const auto scope = fifo.read (take);
        std::vector<float> tmp ((size_t) take);
        for (int i = 0; i < scope.blockSize1; ++i) tmp[(size_t) i] = ring[(size_t) (scope.startIndex1 + i)];
        for (int i = 0; i < scope.blockSize2; ++i) tmp[(size_t) (scope.blockSize1 + i)] = ring[(size_t) (scope.startIndex2 + i)];
        std::move (window.begin() + take, window.end(), window.begin());
        std::copy (tmp.begin(), tmp.end(), window.end() - take);
        avail -= take;
    }

    const double rate = sr / decimation;
    float rms = 0.0f;
    for (float s : window) rms += s * s;
    rms = std::sqrt (rms / (float) N);

    float hz = 0.0f;
    if (rms > 0.0015f)
    {
        // YIN: cumulative mean normalised difference
        const int W = N / 2;
        const int tauMin = (int) (rate / 1500.0), tauMax = std::min (W - 2, (int) (rate / 22.0));
        diff[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float delta = window[(size_t) j] - window[(size_t) (j + tau)];
                d += delta * delta;
            }
            running += d;
            raw[(size_t) tau] = d;
            diff[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }
        int best = -1;
        for (int tau = std::max (2, tauMin); tau < tauMax; ++tau)
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && diff[(size_t) (tau + 1)] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best > 0)
        {
            // parabolic interpolation around the minimum
            const float a = diff[(size_t) (best - 1)], b = diff[(size_t) best], c = diff[(size_t) (best + 1)];
            const float denom = a - 2.0f * b + c;
            const float shift = std::abs (denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
            double period = (double) best + shift;
            // The same dip comes back at every whole number of periods, and an error of a
            // fraction of a sample there is that fraction over the count: measured at the
            // farthest repeat in reach, a high note is read to a tenth of a cent, where one
            // period at a time left it two cents out.
            const int k = (int) std::floor ((double) (tauMax - 3) / period);
            if (k >= 2)
            {
                const int centre = (int) std::lround (period * k);
                int at = centre;
                for (int t = std::max (2, centre - 2); t <= std::min (tauMax - 2, centre + 2); ++t)
                    if (raw[(size_t) t] < raw[(size_t) at]) at = t;
                const float ra = raw[(size_t) (at - 1)], rb = raw[(size_t) at], rc = raw[(size_t) (at + 1)];
                const float rd = ra - 2.0f * rb + rc;
                const double fine = ((double) at + (std::abs (rd) > 1e-12f ? 0.5 * (ra - rc) / rd : 0.0)) / k;
                // only if it agrees with the first look (a wrong repeat would pick another note)
                if (std::abs (fine / period - 1.0) < 0.01) period = fine;
            }
            hz = (float) (rate / period);
        }
    }

    if (hz > 0.0f)
    {
        // light smoothing for a steady needle, snap on big jumps (new string)
        if (smoothedHz <= 0.0f || std::abs (std::log2 (hz / smoothedHz)) > 0.04f) smoothedHz = hz;
        else smoothedHz = 0.7f * smoothedHz + 0.3f * hz;
        holdFrames = 12;
    }
    else if (holdFrames > 0)
        --holdFrames;
    else
        smoothedHz = 0.0f;

    return describe (smoothedHz);
}
} // namespace rh
