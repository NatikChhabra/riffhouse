// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <cmath>

namespace rh
{
// The click: a short sine burst on every beat, higher on the one. Beats are counted in
// fractions of a sample, so a bar at any tempo stays on time however long it runs, and the
// click sounds the same at every sample rate.
class Metronome
{
public:
    void restart() noexcept { untilBeat = 0.0; beat = 0; }

    // Adds the click to l and r. onBeat (int beatIndex) is called as each beat starts.
    template <typename OnBeat>
    void render (float* l, float* r, int n, double sampleRate, double bpm, float level, OnBeat&& onBeat) noexcept
    {
        const double perBeat = sampleRate * 60.0 / (bpm < 30.0 ? 30.0 : (bpm > 300.0 ? 300.0 : bpm));
        // about 105 ms to fade at any rate (it was tuned at 48 kHz)
        const float decay = (float) std::pow (0.9985, 48000.0 / sampleRate);
        for (int i = 0; i < n; ++i)
        {
            if (untilBeat <= 0.0)
            {
                // start where the beat really falls, between this sample and the last
                const double late = -untilBeat;
                untilBeat += perBeat;
                env = 1.0f;
                freq = (beat % 4 == 0) ? 2000.0f : 1400.0f;
                phase = 2.0 * 3.14159265358979323846 * freq * late / sampleRate;
                onBeat (beat);
                ++beat;
            }
            untilBeat -= 1.0;
            if (env > 0.0005f)
            {
                const float s = (float) std::sin (phase) * env * 0.25f * level;
                phase += 2.0 * 3.14159265358979323846 * freq / sampleRate;
                env *= decay;
                l[i] += s;
                r[i] += s;
            }
        }
    }

    int beatsSoFar() const noexcept { return beat; }

private:
    double untilBeat = 0.0, phase = 0.0;
    int beat = 0;
    float env = 0.0f, freq = 1000.0f;
};
} // namespace rh
