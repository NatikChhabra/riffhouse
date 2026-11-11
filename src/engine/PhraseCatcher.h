// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

#include <atomic>
#include <vector>

namespace rh
{
// A few seconds of guitar that tones are measured with: the house phrase nobody played, or
// better, the player's own guitar exactly as it comes off the cable, before the input level
// and Input Fix (tones are then measured through those as they are set now). Their pickups,
// their strings and how hard they hit decide how loud a clean tone is next to a distorted
// one, so only their own playing levels the two for them.
struct LevelPhrase
{
    std::vector<float> samples;
    double sampleRate = 0.0;
    float inGainDb = 0.0f;   // unused, kept for the file format
    float loudDb = -120.0f;  // loudness of its loud half, unweighted
    bool fromPlayer = false;
    juce::uint64 id = 0;     // changes whenever the samples do
};

LevelPhrase housePhrase (double sampleRate);
LevelPhrase atRate (const LevelPhrase&, double sampleRate);

// How loud something sounds to a player: the level of its louder half, in 0.4 s stretches.
// A ringing clean chord and a wall of distortion that hit equally hard read about the same,
// where a plain average would call the clean one much quieter for dying away.
float loudHalfDb (const float* left, const float* right, size_t n, double sampleRate);

// True when these seconds are a guitar being played and not a cable being plugged in, a
// cough or the gaps between songs: sounding most of the time, struck at least once, never
// clipped. `x` is the raw input; `inGainDb` is the input level it will be played through,
// which decides whether it is loud enough to be the player digging in. `loudDb` is of the raw input.
bool worthKeeping (const std::vector<float>& x, double sampleRate, float& loudDb, float inGainDb = 0.0f);

// Keeps the last seconds of the raw input. push() runs on the audio thread and
// never waits; latest() is for any other thread.
class PhraseCatcher
{
public:
    static constexpr double kSeconds = 3.0;

    void prepare (double sampleRate);
    void push (const float* x, int n) noexcept;
    // The most recent kSeconds, oldest first. False until that much has been heard.
    bool latest (std::vector<float>& out) const;
    double rate() const noexcept { return sr; }

private:
    double sr = 48000.0;
    std::vector<float> ring;
    std::atomic<juce::int64> written { 0 };
};

// For keeping the player's phrase between sessions: 16-bit, a few hundred kilobytes.
juce::MemoryBlock phraseToBytes (const LevelPhrase&);
bool phraseFromBytes (const void* data, size_t size, LevelPhrase& out);
} // namespace rh
