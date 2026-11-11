// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once
#include <vector>

namespace rh::test
{
// A synthetic electric guitar DI performance (plucked-string model with a
// pickup response): open chords, power chords in drop tuning, palm mutes and
// single notes. Peak level around -10 dBFS, like a typical interface input.
std::vector<float> guitarDi (double sampleRate, double seconds = 12.0, float level = 0.32f);
std::vector<float> sine (double sampleRate, double hz, double seconds, float amp);
std::vector<float> impulse (int length, int at = 0);
}
