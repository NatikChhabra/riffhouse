// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Takes.h"
#include "../engine/Rig.h"

namespace rh
{
namespace
{
// A re-amped take is named "<the original take> ~ <tone>", so the clean guitar
// it came from is found by name and it can be re-amped again.
const char* const reampMark = " ~ ";

juce::String originalBase (const juce::String& base)
{
    return base.contains (reampMark) ? base.upToFirstOccurrenceOf (reampMark, false, false) : base;
}
} // namespace

std::vector<Take> listTakes (const juce::File& folder)
{
    std::vector<Take> takes;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.wav"))
    {
        Take t;
        t.sound = f;
        const auto base = f.getFileNameWithoutExtension();
        const auto first = originalBase (base);
        t.clean = folder.getChildFile ("DI").getChildFile (first + " (DI).wav");
        // "2026-11-11 21-04-33 Tone name": the stamp is 19 characters
        const bool stamped = first.length() > 19 && first[4] == '-' && first[10] == ' ' && first[13] == '-';
        t.name = base.contains (reampMark) ? base.fromFirstOccurrenceOf (reampMark, false, false)
                                           : (stamped ? first.substring (20) : first);
        t.when = f.getLastModificationTime();
        if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor (f) })
            t.seconds = r->sampleRate > 0.0 ? (double) r->lengthInSamples / r->sampleRate : 0.0;
        if (t.seconds > 0.05) takes.push_back (std::move (t)); // an empty file is a take that never started
    }
    // newest first. Two files written in the same second (a quick re-amp) fall back to their
    // names, which start with the time of the take and put a re-amp after its original.
    std::sort (takes.begin(), takes.end(), [] (const Take& a, const Take& b)
    {
        if (a.when != b.when) return a.when > b.when;
        return a.sound.getFileNameWithoutExtension().compare (b.sound.getFileNameWithoutExtension()) > 0;
    });
    return takes;
}

//==============================================================================
TakePlayer::TakePlayer()
{
    formats.registerBasicFormats();
    disk.startThread();
}

TakePlayer::~TakePlayer()
{
    unload();
    disk.stopThread (2000);
}

bool TakePlayer::load (const Take& t, Mode m)
{
    const auto file = m == Mode::throughRig ? t.clean : t.sound;
    std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (file) };
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const double fileRate = reader->sampleRate;
    auto next = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    const juce::ScopedLock sl (lock);
    transport.stop();
    transport.setSource (next.get(), 32768, &disk, fileRate, 2);
    source = std::move (next);
    current = t;
    modeNow.store ((int) m);
    transport.prepareToPlay (block, rate);
    return true;
}

void TakePlayer::unload()
{
    const juce::ScopedLock sl (lock);
    transport.stop();
    transport.setSource (nullptr);
    source.reset();
    current = {};
}

void TakePlayer::prepare (double sampleRate, int blockSize)
{
    const juce::ScopedLock sl (lock);
    rate = sampleRate;
    block = std::max (16, blockSize);
    scratch.setSize (2, block);
    transport.prepareToPlay (block, rate);
}

void TakePlayer::release()
{
    const juce::ScopedLock sl (lock);
    transport.releaseResources();
}

void TakePlayer::setPlaying (bool shouldPlay)
{
    if (source == nullptr) return;
    if (shouldPlay)
    {
        if (transport.hasStreamFinished() || transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.02)
            transport.setPosition (0.0);
        transport.start();
    }
    else
        transport.stop();
}

void TakePlayer::setPosition (double seconds)
{
    transport.setPosition (juce::jlimit (0.0, std::max (0.0, transport.getLengthInSeconds()), seconds));
}

bool TakePlayer::pull (int n) noexcept
{
    if (! transport.isPlaying() || scratch.getNumSamples() < n) return false;
    juce::AudioSourceChannelInfo info (&scratch, 0, n);
    transport.getNextAudioBlock (info);
    return true;
}

bool TakePlayer::readClean (float* dst, int n) noexcept
{
    if (mode() != Mode::throughRig) return false;
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || ! pull (n)) return false;
    // the clean guitar is one channel; a stereo file someone dropped in gets folded
    const float* a = scratch.getReadPointer (0);
    const float* b = scratch.getReadPointer (1);
    const bool mono = source != nullptr && source->getAudioFormatReader()->numChannels < 2;
    for (int i = 0; i < n; ++i) dst[i] = mono ? a[i] : 0.5f * (a[i] + b[i]);
    return true;
}

bool TakePlayer::readSound (float* l, float* r, int n) noexcept
{
    if (mode() != Mode::asRecorded) return false;
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || ! pull (n)) return false;
    const bool mono = source != nullptr && source->getAudioFormatReader()->numChannels < 2;
    std::copy (scratch.getReadPointer (0), scratch.getReadPointer (0) + n, l);
    std::copy (scratch.getReadPointer (mono ? 0 : 1), scratch.getReadPointer (mono ? 0 : 1) + n, r);
    return true;
}

//==============================================================================
juce::File ReampJob::outputFileFor (const Take& source, const juce::String& toneName)
{
    const auto folder = source.sound.getParentDirectory();
    const auto base = originalBase (source.sound.getFileNameWithoutExtension());
    const auto tone = juce::File::createLegalFileName (toneName.isNotEmpty() ? toneName : juce::String ("New tone")).trim();
    for (int n = 1; n < 1000; ++n)
    {
        const auto f = folder.getChildFile (base + reampMark + tone + (n > 1 ? " " + juce::String (n) : juce::String()) + ".wav");
        if (! f.exists()) return f;
    }
    return folder.getNonexistentChildFile (base + reampMark + tone, ".wav");
}

juce::String ReampJob::render (const juce::File& clean, const juce::File& out, const Setup& setup, std::atomic<float>* progress,
                               const std::function<bool()>& shouldStop)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader { formats.createReaderFor (clean) };
    if (reader == nullptr || reader->lengthInSamples <= 0) return "The clean guitar for this take could not be read.";

    const double sr = reader->sampleRate;
    const int block = 512;
    auto rig = std::make_unique<Rig>();
    if (setup.capture.existsAsFile())
    {
        const auto err = rig->neural().load (setup.capture.getFullPathName().toStdString());
        if (! err.empty()) return "The amp capture could not be loaded: " + juce::String (err);
    }
    if (setup.userIr.existsAsFile()) rig->cabinet().loadUserIr (setup.userIr);
    rig->setOffline (true); // nothing built behind this thread's back while it renders
    rig->setParams (setup.params);
    rig->prepare (sr, block);
    rig->setParams (setup.params);
    rig->finishPendingWork();
    rig->prepare (sr, block);
    rig->finishPendingWork();
    rig->setTempo (setup.tempo);
    rig->setLevelTrimDb (setup.levelTrimDb);

    const auto temp = out.getSiblingFile (out.getFileName() + ".part");
    temp.deleteFile();
    std::unique_ptr<juce::AudioFormatWriter> writer;
    {
        auto stream = temp.createOutputStream();
        if (stream == nullptr) return "Could not write to the recordings folder.";
        juce::WavAudioFormat wav;
        juce::StringPairArray meta;
        meta.set ("ISFT", "Riffhouse");
        writer.reset (wav.createWriterFor (stream.get(), sr, 2, 24, meta, 0));
        if (writer == nullptr) return "Could not write to the recordings folder.";
        stream.release(); // the writer owns it now
    }

    juce::AudioBuffer<float> in ((int) std::max (1u, reader->numChannels), block), outBuf (2, block);
    std::vector<float> mono ((size_t) block);
    const juce::int64 total = reader->lengthInSamples;
    // after the last note the delay and reverb ring on: keep going until they have died away
    const juce::int64 maxTail = (juce::int64) (12.0 * sr), quietNeeded = (juce::int64) (0.4 * sr);
    juce::int64 done = 0, tail = 0, quiet = 0;
    bool ok = true;
    while (ok)
    {
        if (shouldStop && shouldStop()) { writer.reset(); temp.deleteFile(); return "stopped"; }
        const int n = done < total ? (int) std::min<juce::int64> (block, total - done) : block;
        if (done < total)
        {
            reader->read (&in, 0, n, done, true, true);
            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                for (int ch = 0; ch < in.getNumChannels(); ++ch) s += in.getSample (ch, i);
                mono[(size_t) i] = s / (float) in.getNumChannels();
            }
            done += n;
        }
        else
        {
            std::fill (mono.begin(), mono.end(), 0.0f);
            tail += n;
        }
        rig->setParams (setup.params);
        rig->process (mono.data(), outBuf.getWritePointer (0), outBuf.getWritePointer (1), n);
        ok = writer->writeFromAudioSampleBuffer (outBuf, 0, n);
        if (done >= total)
        {
            const float peak = outBuf.getMagnitude (0, n);
            quiet = peak < 1.0e-4f ? quiet + n : 0; // -80 dB
            if ((quiet >= quietNeeded && tail >= quietNeeded) || tail >= maxTail) break;
        }
        if (progress != nullptr) progress->store ((float) ((double) done / (double) total));
    }
    writer.reset();
    if (! ok) { temp.deleteFile(); return "The disk is full or the recordings folder cannot be written."; }
    out.deleteFile();
    if (! temp.moveFileTo (out)) { temp.deleteFile(); return "Could not write to the recordings folder."; }
    if (progress != nullptr) progress->store (1.0f);
    return {};
}

ReampJob::ReampJob (Take s, Setup su, std::function<void (juce::File, juce::String)> d)
    : juce::Thread ("Riffhouse re-amp"), source (std::move (s)), setup (std::move (su)), onDone (std::move (d))
{
    startThread (juce::Thread::Priority::low);
}

ReampJob::~ReampJob()
{
    alive->store (false);
    stopThread (15000);
}

void ReampJob::run()
{
    const auto out = outputFileFor (source, setup.toneName);
    const auto err = render (source.clean, out, setup, &done01, [this] { return threadShouldExit(); });
    juce::MessageManager::callAsync ([cb = onDone, out, err, ok = alive] { if (ok->load() && cb) cb (err.isEmpty() ? out : juce::File(), err); });
}
} // namespace rh
