// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "TestSignals.h"

#include "dsp/Common.h"
#include "engine/ReferenceGuitar.h"

namespace rh::test
{
std::vector<float> guitarDi (double sr, double seconds, float level) { return playedGuitar (sr, seconds, level); }

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
