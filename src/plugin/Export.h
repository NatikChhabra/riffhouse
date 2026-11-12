// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>

namespace rh
{
// A copy of a recording in a file other apps and phones take: MP3 to send and post, FLAC to
// keep every detail in a smaller file than WAV.
enum class ExportFormat { mp3, flac };

juce::String exportExtension (ExportFormat); // ".mp3"
juce::String exportName (ExportFormat);      // "MP3"

// Where a copy of `source` goes: a folder beside the takes whose name says what it is for.
juce::File exportFileFor (const juce::File& recordingsFolder, const juce::File& source, ExportFormat);

// Writes `source` (any sound file Riffhouse can read) as `out`. MP3 is 320 kbps, the best a
// fixed rate allows, with the title in its tag; FLAC keeps 24 bits. Returns what went wrong
// in plain words, or nothing. Any thread; `progress` goes from 0 to 1.
juce::String exportSound (const juce::File& source, const juce::File& out, ExportFormat, const juce::String& title,
                          std::atomic<float>* progress = nullptr, const std::function<bool()>& shouldStop = {});

// The same on its own thread. `onDone` is called on the message thread with the file made, or
// with what went wrong; never after the job is gone.
class ExportJob : private juce::Thread
{
public:
    ExportJob (juce::File source, juce::File out, ExportFormat, juce::String title,
               std::function<void (juce::File, juce::String)> onDone);
    ~ExportJob() override;
    float progress() const noexcept { return done01.load(); }
    const juce::File& output() const noexcept { return out; }

private:
    void run() override;
    juce::File source, out;
    ExportFormat format;
    juce::String title;
    std::function<void (juce::File, juce::String)> onDone;
    std::atomic<float> done01 { 0.0f };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};
} // namespace rh
