// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "LevelMatch.h"

#include "History.h"
#include "ReferenceGuitar.h"

#include <cstring>

namespace rh
{
ParamValues toneForMeasuring (const ParamValues& v, bool playersPhrase)
{
    ParamValues t = v;
    auto set = [&t] (P id, float value) { t[(size_t) id] = value; };
    if (! playersPhrase)
    {
        // the house phrase is a guitar as it should reach the pedals already
        set (P::inGain, 0.0f);
        set (P::inputFix, 0.0f);
        set (P::inputFixAmt, 0.0f);
    }
    else if (t[(size_t) P::inputFix] < 0.5f)
        set (P::inputFixAmt, 0.0f); // switched off, its strength changes nothing
    set (P::humFilter, 0.0f);
    set (P::gateThresh, -70.0f);
    set (P::autoLevel, 0.0f);
    return t;
}

ToneChange changeBetween (const ParamValues& x, const ParamValues& y)
{
    auto change = ToneChange::none;
    for (const auto& s : paramSpecs())
    {
        const float u = x[(size_t) s.id], v = y[(size_t) s.id];
        // the player's own setup is theirs to turn: louder there means louder
        if (s.kind == ParamKind::continuous || belongsToSetup (s.id)) { if (u != v) change = std::max (change, ToneChange::knob); }
        else if (std::lround (u) != std::lround (v))
            change = std::max (change, s.kind == ParamKind::choice ? ToneChange::swapped : ToneChange::toggled);
    }
    return change;
}

juce::uint64 toneHash (const ParamValues& t)
{
    juce::uint64 h = 1469598103934665603ull;
    for (float v : t)
    {
        juce::uint32 bits = 0;
        std::memcpy (&bits, &v, sizeof (bits));
        h = (h ^ bits) * 1099511628211ull;
    }
    return h;
}

float measureLoudnessDb (Rig& rig, const ParamValues& tone, const LevelPhrase& phrase)
{
    if (phrase.samples.empty() || phrase.sampleRate <= 0.0) return -120.0f;
    const double sr = phrase.sampleRate;
    const int block = 256;
    rig.setMeasuring (true);
    rig.startFresh (tone, sr, block);

    const size_t n = phrase.samples.size();
    std::vector<float> in ((size_t) block), l (n), r (n);
    // ears are most sensitive in the upper mids and nearly deaf to rumble: weigh the reading the same way
    Biquad shelf[2], rumble[2];
    for (int ch = 0; ch < 2; ++ch)
    {
        shelf[ch].set (Biquad::Type::highShelf, sr, 1500.0, 0.7, 4.0);
        rumble[ch].set (Biquad::Type::highpass, sr, 60.0, 0.5);
    }
    for (size_t at = 0; at < n; at += (size_t) block)
    {
        const int k = (int) std::min ((size_t) block, n - at);
        for (int i = 0; i < k; ++i) in[(size_t) i] = phrase.samples[at + (size_t) i];
        rig.process (in.data(), l.data() + at, r.data() + at, k);
        for (size_t i = at; i < at + (size_t) k; ++i)
        {
            l[i] = rumble[0].process (shelf[0].process (l[i]));
            r[i] = rumble[1].process (shelf[1].process (r[i]));
        }
    }
    return loudHalfDb (l.data(), r.data(), n, sr);
}

float LevelKeeper::reading (float measuredDb, ToneChange change) noexcept
{
    if (measuredDb < kSilentDb) return trim; // nothing comes out: nothing to match, and nothing to learn
    if (fresh || ! haveTarget)
    {
        if (fresh) target = kHouseLoudnessDb;
        else       target = measuredDb + trim;   // a restored session: this is what it sounded like
        fresh = false;
        haveTarget = true;
    }
    else if (change == ToneChange::knob || change == ToneChange::none)
        target = measuredDb + trim;               // a knob was turned: it changed the volume, and that stands
    else if (change == ToneChange::toggled)       // a pedal on or off: what it does by itself, up to the leeway
        target = std::min (target + kSwitchLeewayDb, std::max (target - kSwitchLeewayDb, measuredDb + trim));
    trim = std::min (kMostBoostDb, std::max (kMostCutDb, target - measuredDb));
    target = measuredDb + trim;
    return trim;
}
} // namespace rh
