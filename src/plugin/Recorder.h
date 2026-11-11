// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

namespace rh
{
// Records the processed stereo sound and, at the same time, the clean guitar
// (DI) so a take can be re-amped later with a different tone. Writing happens
// on a background thread; the audio thread only copies into a FIFO.
class Recorder
{
public:
    Recorder();
    ~Recorder();

    static juce::File defaultFolder();

    // Message thread.
    bool start (const juce::File& folder, double sampleRate, bool alsoSaveDi, const juce::String& toneName);
    void stop();
    bool isRecording() const noexcept { return active.load(); }
    double secondsRecorded() const noexcept { return (double) samplesWritten.load() / std::max (1.0, rate); }
    juce::File lastTake() const { return lastFile; }

    // Audio thread.
    void push (const float* di, const float* l, const float* r, int n) noexcept;

private:
    juce::TimeSliceThread thread { "Riffhouse recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> wet, dry;
    juce::SpinLock lock;
    std::atomic<bool> active { false };
    std::atomic<juce::int64> samplesWritten { 0 };
    double rate = 48000.0;
    juce::File lastFile;
};
} // namespace rh
