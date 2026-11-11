// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Common.h"

namespace rh
{
// Streaming windowed-sinc sample-rate converter. Push input samples at one
// rate, pull output samples at another. Used to run neural captures at the
// rate they were trained at when the audio device runs at a different rate.
class StreamResampler
{
public:
    static constexpr int kHalf = 16;      // taps either side
    static constexpr int kPhases = 256;   // table resolution

    void prepare (double inRate, double outRate)
    {
        step = inRate / outRate;
        // when lowering the rate, lower the cutoff to avoid aliasing
        const double cutoff = 0.92 * std::min (1.0, outRate / inRate);
        scale = (float) cutoff;
        table.assign ((size_t) (kPhases + 1) * (2 * kHalf), 0.0f);
        for (int p = 0; p <= kPhases; ++p)
        {
            const double frac = (double) p / kPhases;
            for (int k = 0; k < 2 * kHalf; ++k)
            {
                const double x = (double) (k - kHalf + 1) - frac; // distance from the read point
                const double arg = kPi * x * cutoff;
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (arg) / arg;
                const double wx = x / (kHalf + 1);
                const double win = std::abs (wx) >= 1.0 ? 0.0 : 0.42 + 0.5 * std::cos (kPi * wx) + 0.08 * std::cos (2 * kPi * wx);
                table[(size_t) (p * 2 * kHalf + k)] = (float) (sinc * win * cutoff);
            }
        }
        history.assign (4096, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (history.begin(), history.end(), 0.0f);
        count = 0;
        readPos = (double) kHalf; // start once enough history exists
    }

    // Converts n input samples; returns how many output samples were written (<= maxOut).
    int process (const float* in, int n, float* out, int maxOut)
    {
        int produced = 0;
        for (int i = 0; i < n; ++i)
        {
            push (in[i]);
            // emit every output sample whose window is now fully available
            while (readPos + kHalf <= (double) (count - 1) && produced < maxOut)
            {
                out[produced++] = interpolate (readPos);
                readPos += step;
            }
        }
        // keep the counters bounded
        if (count > (1 << 30))
        {
            const int shift = 1 << 29;
            count -= shift;
            readPos -= shift;
        }
        return produced;
    }

    // Approximate latency in input samples.
    int latency() const { return kHalf + 1; }

private:
    void push (float x)
    {
        history[(size_t) (count & (int) (history.size() - 1))] = x;
        ++count;
    }

    float interpolate (double pos) const
    {
        const int base = (int) std::floor (pos);
        const double frac = pos - base;
        const int p = (int) std::round (frac * kPhases);
        const float* k = &table[(size_t) (p * 2 * kHalf)];
        const int mask = (int) history.size() - 1;
        float acc = 0.0f;
        for (int t = 0; t < 2 * kHalf; ++t)
            acc += history[(size_t) ((base - kHalf + 1 + t) & mask)] * k[t];
        return acc;
    }

    std::vector<float> table;
    std::vector<float> history;
    int count = 0;
    double readPos = 0.0, step = 1.0;
    float scale = 1.0f;
};
} // namespace rh
