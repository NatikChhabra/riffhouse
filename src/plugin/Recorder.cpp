// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Recorder.h"
#include "Takes.h"

namespace rh
{
Recorder::Recorder() { thread.startThread(); }

Recorder::~Recorder()
{
    stop();
    thread.stopThread (2000);
}

juce::File Recorder::defaultFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Riffhouse Recordings");
}

bool Recorder::start (const juce::File& folder, double sampleRate, bool alsoSaveDi, const juce::String& toneName)
{
    stop();
    if (! folder.createDirectory()) return false;
    rate = sampleRate;

    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S");
    const auto base = juce::File::createLegalFileName (stamp + " " + toneName).trim();

    auto makeWriter = [&] (const juce::File& f, int channels) -> std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter>
    {
        f.deleteFile();
        auto stream = f.createOutputStream();
        if (stream == nullptr) return nullptr;
        juce::WavAudioFormat wav;
        juce::StringPairArray meta;
        meta.set ("ISFT", "Riffhouse");
        auto* writer = wav.createWriterFor (stream.get(), sampleRate, (unsigned int) channels, 24, meta, 0);
        if (writer == nullptr) return nullptr;
        stream.release(); // the writer owns it now
        return std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer, thread, 1 << 16);
    };

    lastFile = folder.getChildFile (base + ".wav");
    auto w = makeWriter (lastFile, 2);
    if (w == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> d;
    if (alsoSaveDi)
    {
        // the clean guitar goes in a subfolder so the main folder stays tidy
        const auto clean = cleanGuitarFile (folder, base);
        clean.getParentDirectory().createDirectory();
        d = makeWriter (clean, 1);
    }

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        wet = std::move (w);
        dry = std::move (d);
        samplesWritten.store (0);
        active.store (true);
    }
    return true;
}

void Recorder::stop()
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> w, d;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        active.store (false);
        w = std::move (wet);
        d = std::move (dry);
    }
    // destroying the writers flushes and closes the files
}

void Recorder::push (const float* di, const float* l, const float* r, int n) noexcept
{
    if (! active.load()) return;
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked() || wet == nullptr) return;
    const float* stereo[] = { l, r };
    wet->write (stereo, n);
    if (dry != nullptr)
    {
        const float* mono[] = { di };
        dry->write (mono, n);
    }
    samplesWritten.fetch_add (n);
}
} // namespace rh
