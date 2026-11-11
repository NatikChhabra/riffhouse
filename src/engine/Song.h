// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

namespace rh
{
// Practising with a song: play it slower without changing its pitch, in another key without
// changing its speed, and go round one part of it until the part sits under your fingers.

// Changes the speed and the key of the song you play along with.
// At normal speed in the original key it does nothing at all: the song passes through untouched.
class SongStretch
{
public:
    static constexpr float minSpeed = 0.5f, maxSpeed = 1.25f;
    static constexpr int maxKeyShift = 12; // semitones, up or down

    SongStretch();
    ~SongStretch();

    // Message thread, audio stopped.
    void prepare (double sampleRate);

    // Any thread. Picked up by the next block.
    void setSpeed (float timesNormal) noexcept;
    void setKey (int semitones) noexcept;
    void restart() noexcept; // after a jump in the song: forget what was sounding
    float speed() const noexcept { return speedWanted.load(); }
    int key() const noexcept { return keyWanted.load(); }
    bool plain() const noexcept { return speed() == 1.0f && key() == 0; }

    // Audio thread, in this order for every block:
    // how many frames of the song the next n frames of sound need...
    int wants (int n) noexcept;
    // ...and those frames turned into n frames of sound.
    void process (const float* inL, const float* inR, int inN, float* outL, float* outR, int n) noexcept;

    // The most wants (n) can ever ask for.
    static int most (int n) noexcept { return (int) ((float) n * maxSpeed) + 2; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::atomic<float> speedWanted { 1.0f };
    std::atomic<int> keyWanted { 0 };
    std::atomic<bool> restartWanted { false };
    double owed = 0.0, rate = 48000.0;
    int keyNow = 0;
    bool plainNow = true;
};

// One part of the song held in memory, so that going round it is exact to the sample:
// no gap where the part starts again, which is the one place a gap would throw you.
class SongSection
{
public:
    static constexpr double shortest = 0.5, longest = 180.0; // seconds

    // Message thread, with the song lock held. Takes the sound out of l and r and leaves
    // whatever it held before in them, so the old part is freed after the lock is let go.
    void swapIn (std::vector<float>& l, std::vector<float>& r, double startSeconds, double sampleRate);
    void clear() noexcept { on.store (false); }

    bool active() const noexcept { return on.load(); }
    double startSeconds() const noexcept { return start; }
    double endSeconds() const noexcept { return rate > 0.0 ? start + (double) bufL.size() / rate : start; }
    double positionSeconds() const noexcept { return rate > 0.0 ? start + (double) pos.load() / rate : start; }

    // With the song lock held.
    void seek (double seconds) noexcept;
    // Audio thread, with the song lock held: the next n frames, going round.
    void read (float* l, float* r, int n) noexcept;

private:
    std::vector<float> bufL, bufR;
    std::atomic<size_t> pos { 0 };
    std::atomic<bool> on { false };
    double start = 0.0, rate = 0.0;
};
}
