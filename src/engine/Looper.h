// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <atomic>
#include <vector>

namespace rh
{
// A phrase looper at the end of the rig: play a part, let it go round, play over it.
// One button does nearly everything, the way a looper pedal does: the first press starts
// recording, the second closes the loop and plays it, the next ones switch layering on and off.
// It hears the rig's output and adds the loop to what goes to your ears; takes do not contain it.
class Looper
{
public:
    enum class State { empty, recording, playing, overdubbing, stopped };

    // Message thread, with audio stopped or before the first press: the memory for the longest loop.
    void prepare (double sampleRate, double maxSeconds = 120.0);

    // Message thread. Each is picked up by the next audio block.
    void press();  // record, then play, then layer, then play
    void stop();   // stop the loop but keep it; press() plays it again from its start
    void clear();  // throw the loop away

    State state() const noexcept { return (State) st.load(); }
    double lengthSeconds() const noexcept { return rate > 0.0 ? (double) length.load() / rate : 0.0; }
    double positionSeconds() const noexcept { return rate > 0.0 ? (double) pos.load() / rate : 0.0; }
    double maxSeconds() const noexcept { return rate > 0.0 ? (double) capacity / rate : 0.0; }
    double sampleRate() const noexcept { return rate; }
    std::atomic<float> level { 0.85f };

    // Audio thread.
    void process (float* l, float* r, int n) noexcept;

private:
    enum Request { none = 0, pressed = 1, stopped = 2, cleared = 4 };
    void closeLoop() noexcept;

    std::vector<float> bufL, bufR;
    double rate = 0.0;
    int capacity = 0, fade = 1;
    std::atomic<int> st { (int) State::empty }, requests { 0 }, length { 0 }, pos { 0 };
    float layering = 0.0f; // 0..1: how much of the input is being written into the loop right now
};
} // namespace rh
