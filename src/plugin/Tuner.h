// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

namespace rh
{
// Chromatic tuner using the YIN pitch detector. The audio thread pushes
// samples; the UI thread calls analyse() and reads the result. Works for any
// tuning, from a drop-tuned 8-string (about 23 Hz) up to high frets.
class Tuner
{
public:
    struct Reading
    {
        bool valid = false;
        float hz = 0.0f;
        int midiNote = 0;
        float cents = 0.0f; // -50..+50
        juce::String noteName;
        int octave = 0;
    };

    void prepare (double sampleRate);
    void push (const float* x, int n) noexcept; // audio thread
    Reading analyse();                          // UI thread

    static Reading describe (float hz);

private:
    void write (const float* data, int count) noexcept;
    double sr = 48000.0;
    int decimation = 2;
    juce::AbstractFifo fifo { 1 << 15 };
    std::vector<float> ring = std::vector<float> (1 << 15);
    std::vector<float> window = std::vector<float> (4096), diff = std::vector<float> (2048);
    float lpState = 0.0f;
    int decimCounter = 0;
    float smoothedHz = 0.0f;
    int holdFrames = 0;
};
} // namespace rh
