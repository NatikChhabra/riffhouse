// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "TestSignals.h"

#include "dsp/Common.h"

namespace rh::test
{
namespace
{
    struct Note { double start, freq, dur; float vel; bool mute; };

    double midiHz (int m) { return 440.0 * std::pow (2.0, (m - 69) / 12.0); }

    void pluck (std::vector<float>& out, double sr, const Note& n, uint32_t seed)
    {
        const double period = sr / n.freq;
        const int len = (int) period;
        const float frac = (float) (period - len);
        std::vector<float> line ((size_t) len + 2, 0.0f);
        Noise rnd (seed);
        // pluck: filtered noise burst with a pick-position comb
        const int pos = std::max (1, (int) (len * 0.13));
        for (int i = 0; i < len; ++i) line[(size_t) i] = rnd.next();
        for (int i = len - 1; i >= pos; --i) line[(size_t) i] -= line[(size_t) (i - pos)];
        float lp = 0.0f;
        const float brightness = 0.35f + 0.5f * n.vel;
        for (auto& s : line) { lp += brightness * (s - lp); s = lp; }

        const int start = (int) (n.start * sr);
        const int total = (int) ((n.dur + 1.5) * sr);
        const float loss = n.mute ? 0.86f : 0.996f;
        const float damp = n.mute ? 0.25f : 0.5f;
        int idx = 0;
        float prev = 0.0f;
        const int releaseAt = (int) (n.dur * sr);
        for (int i = 0; i < total && start + i < (int) out.size(); ++i)
        {
            const int j = idx;
            const int j1 = (idx + 1) % len;
            float s = line[(size_t) j] * (1.0f - frac) + line[(size_t) j1] * frac;
            float nv = loss * ((1.0f - damp) * s + damp * prev);
            if (i > releaseAt) nv *= 0.995f; // fret hand lets go
            prev = s;
            line[(size_t) j] = nv;
            idx = j1;
            out[(size_t) (start + i)] += s * n.vel;
        }
    }
} // namespace

std::vector<float> guitarDi (double sr, double seconds, float level)
{
    std::vector<float> out ((size_t) (seconds * sr), 0.0f);
    std::vector<Note> notes;
    auto chord = [&] (double t, std::initializer_list<int> midi, double dur, float vel, bool mute = false)
    {
        double off = 0.0;
        for (int m : midi) { notes.push_back ({ t + off, midiHz (m), dur, vel, mute }); off += 0.012; }
    };
    // open E major and C major strums
    chord (0.0, { 40, 47, 52, 56, 59, 64 }, 1.6, 0.8f);
    chord (1.8, { 48, 52, 55, 60, 64 }, 1.6, 0.7f);
    // single-note line
    const int line[] = { 64, 67, 69, 71, 74, 76, 74, 71 };
    for (int i = 0; i < 8; ++i) notes.push_back ({ 3.6 + i * 0.22, midiHz (line[i]), 0.2, 0.75f, false });
    // drop-D power chords and palm-muted chugs (any tuning should work)
    chord (5.6, { 38, 45, 50 }, 0.9, 0.9f);
    for (int i = 0; i < 8; ++i) chord (6.6 + i * 0.14, { 38, 45 }, 0.1, 0.85f, true);
    // seven-string low B
    chord (7.9, { 35, 42, 47 }, 1.2, 0.9f);
    // quiet clean arpeggio for dynamics
    const int arp[] = { 45, 52, 57, 60, 64, 60, 57, 52 };
    for (int i = 0; i < 8; ++i) notes.push_back ({ 9.3 + i * 0.25, midiHz (arp[i]), 0.6, 0.35f, false });

    uint32_t seed = 1;
    for (const auto& n : notes) pluck (out, sr, n, seed++);

    // pickup: resonant peak around 3 kHz then rolloff
    Biquad pickup, hp;
    pickup.set (Biquad::Type::lowpass, sr, 3200.0, 1.6);
    hp.set (Biquad::Type::highpass, sr, 40.0, 0.7);
    float peak = 0.0f;
    for (auto& s : out) { s = hp.process (pickup.process (s)); peak = std::max (peak, std::abs (s)); }
    const float g = peak > 0 ? level / peak : 1.0f;
    for (auto& s : out) s *= g;
    return out;
}

std::vector<float> sine (double sr, double hz, double seconds, float amp)
{
    std::vector<float> out ((size_t) (seconds * sr));
    for (size_t i = 0; i < out.size(); ++i) out[i] = amp * (float) std::sin (kTwoPi * hz * (double) i / sr);
    return out;
}

std::vector<float> impulse (int length, int at)
{
    std::vector<float> out ((size_t) length, 0.0f);
    out[(size_t) at] = 1.0f;
    return out;
}
} // namespace rh::test
