// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <vector>

namespace rh
{
// A guitar played by nobody: a plucked-string model through a pickup's response. The app
// measures its own tones with it, and the tests play it through everything.

// Twelve seconds of playing: open chords, a single-note line, drop-tuned power chords,
// palm mutes and a quiet arpeggio. Peaks at `level`. As it comes it is brighter than any
// real guitar, which is what a filter or aliasing test wants; `likeARecording` gives it the
// treble a recorded electric guitar has, for anything about how loud or bright a player is.
std::vector<float> playedGuitar (double sampleRate, double seconds = 12.0, float level = 0.32f, bool likeARecording = false);

// The two seconds every tone's loudness is judged with: a strummed chord, four single notes,
// a power chord and three palm mutes, at the level setup trims a real guitar to.
constexpr double kReferencePhraseSeconds = 2.0;
std::vector<float> referencePhrase (double sampleRate);
} // namespace rh
