// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Song.h"

#include <algorithm>
#include <cmath>

#if defined (_MSC_VER)
 #pragma warning (push, 0)
#endif
#include "signalsmith-stretch/signalsmith-stretch.h"
#if defined (_MSC_VER)
 #pragma warning (pop)
#endif

namespace rh
{
struct SongStretch::Impl
{
    // a fixed seed: the same song at the same speed sounds the same every time
    signalsmith::stretch::SignalsmithStretch<float> s { 20261111L };
};

SongStretch::SongStretch() : impl (std::make_unique<Impl>()) { prepare (48000.0); }
SongStretch::~SongStretch() = default;

void SongStretch::prepare (double sampleRate)
{
    rate = sampleRate > 0.0 ? sampleRate : 48000.0;
    // the work is spread over the blocks instead of landing on one of them
    impl->s.presetDefault (2, (float) rate, true);
    impl->s.reset();
    keyNow = 0;
    plainNow = true;
    owed = 0.0;
}

void SongStretch::setSpeed (float timesNormal) noexcept
{
    float s = std::clamp (timesNormal, minSpeed, maxSpeed);
    if (std::abs (s - 1.0f) < 0.004f) s = 1.0f; // normal speed is exactly normal
    speedWanted.store (s);
}

void SongStretch::setKey (int semitones) noexcept { keyWanted.store (std::clamp (semitones, -maxKeyShift, maxKeyShift)); }
void SongStretch::restart() noexcept { restartWanted.store (true); }

int SongStretch::wants (int n) noexcept
{
    const float sp = speedWanted.load();
    const int key = keyWanted.load();
    const bool nowPlain = sp == 1.0f && key == 0;
    const bool again = restartWanted.exchange (false);
    if (nowPlain != plainNow || again)
    {
        impl->s.reset();
        owed = 0.0;
    }
    plainNow = nowPlain;
    if (key != keyNow)
    {
        // above about 8 kHz a shifted song turns glassy, so the top is left where it was
        impl->s.setTransposeSemitones ((float) key, (float) (8000.0 / rate));
        keyNow = key;
    }
    if (plainNow) return n;
    owed += (double) n * (double) sp;
    const int need = (int) owed;
    owed -= (double) need;
    return need;
}

void SongStretch::process (const float* inL, const float* inR, int inN, float* outL, float* outR, int n) noexcept
{
    if (plainNow)
    {
        std::copy (inL, inL + n, outL);
        std::copy (inR, inR + n, outR);
        return;
    }
    const float* in[2] = { inL, inR };
    float* out[2] = { outL, outR };
    impl->s.process (in, inN, out, n);
}

//==============================================================================
void SongSection::swapIn (std::vector<float>& l, std::vector<float>& r, double startSeconds, double sampleRate)
{
    // a few milliseconds of fade at each end: the join is a cut in the song, and a bare cut clicks
    const size_t len = std::min (l.size(), r.size());
    l.resize (len);
    r.resize (len);
    const size_t fade = std::min (len / 2, (size_t) (sampleRate * 0.004));
    for (size_t i = 0; i < fade; ++i)
    {
        const float g = (float) (i + 1) / (float) (fade + 1);
        l[i] *= g;
        r[i] *= g;
        l[len - 1 - i] *= g;
        r[len - 1 - i] *= g;
    }
    bufL.swap (l);
    bufR.swap (r);
    start = startSeconds;
    rate = sampleRate;
    pos.store (0);
    on.store (! bufL.empty());
}

void SongSection::seek (double seconds) noexcept
{
    if (bufL.empty() || rate <= 0.0) return;
    const double at = std::clamp ((seconds - start) * rate, 0.0, (double) (bufL.size() - 1));
    pos.store ((size_t) at);
}

void SongSection::read (float* l, float* r, int n) noexcept
{
    const size_t len = bufL.size();
    if (len == 0)
    {
        std::fill (l, l + n, 0.0f);
        std::fill (r, r + n, 0.0f);
        return;
    }
    size_t p = pos.load() % len;
    for (int i = 0; i < n; ++i)
    {
        l[i] = bufL[p];
        r[i] = bufR[p];
        if (++p == len) p = 0;
    }
    pos.store (p);
}
}
