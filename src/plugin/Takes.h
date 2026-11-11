// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/Params.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <functional>

namespace rh
{
// One recording in the recordings folder: the sound as it was played, and the
// clean guitar beside it when that was kept.
struct Take
{
    juce::File sound, clean;
    juce::String name; // the tone it was recorded with
    juce::Time when;
    double seconds = 0.0;
    bool hasClean() const { return clean.existsAsFile(); }
};

// Newest first.
std::vector<Take> listTakes (const juce::File& folder);

// Plays a take back inside the app. Two ways: the sound as recorded, or the
// clean guitar fed into the rig in place of the live input, so the tone can be
// changed after the fact while it plays.
class TakePlayer
{
public:
    enum class Mode { asRecorded, throughRig };

    TakePlayer();
    ~TakePlayer();

    // Message thread.
    bool load (const Take&, Mode);
    void unload();
    void prepare (double sampleRate, int blockSize);
    void release();
    void setPlaying (bool shouldPlay);
    bool isPlaying() const noexcept { return transport.isPlaying(); }
    bool isLoaded() const noexcept { return source != nullptr; }
    Mode mode() const noexcept { return (Mode) modeNow.load(); }
    const Take& take() const noexcept { return current; }
    double length() const { return transport.getLengthInSeconds(); }
    double position() const { return transport.getCurrentPosition(); }
    void setPosition (double seconds);

    // Audio thread. Each returns false, and leaves the buffers alone, when nothing is playing.
    bool readClean (float* dst, int n) noexcept;
    bool readSound (float* l, float* r, int n) noexcept;

private:
    bool pull (int n) noexcept;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread disk { "Riffhouse take reader" }; // file reading stays off the audio thread
    std::unique_ptr<juce::AudioFormatReaderSource> source;
    juce::AudioTransportSource transport;
    juce::CriticalSection lock;
    juce::AudioBuffer<float> scratch;
    std::atomic<int> modeNow { 0 };
    double rate = 48000.0;
    int block = 512;
    Take current;
};

// Runs a take's clean guitar through a rig of its own, faster than real time,
// and writes the result as a new take. Nothing is played while it works.
class ReampJob : private juce::Thread
{
public:
    struct Setup
    {
        ParamValues params {};
        juce::File capture, userIr; // what the amp and cab slots hold, if anything
        double tempo = 120.0;
        juce::String toneName;
    };

    ReampJob (Take source, Setup, std::function<void (juce::File result, juce::String error)> done);
    ~ReampJob() override;
    float progress() const noexcept { return done01.load(); }

    // The work itself, usable without a thread (and from the command line).
    static juce::String render (const juce::File& clean, const juce::File& out, const Setup&, std::atomic<float>* progress,
                                const std::function<bool()>& shouldStop);
    static juce::File outputFileFor (const Take& source, const juce::String& toneName);

private:
    void run() override;
    Take source;
    Setup setup;
    std::function<void (juce::File, juce::String)> onDone;
    std::atomic<float> done01 { 0.0f };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};
} // namespace rh
