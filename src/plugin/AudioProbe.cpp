// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioProbe.h"

#include "DeviceLog.h"

#include <iostream>

namespace rh
{
namespace
{
    juce::PropertiesFile* store = nullptr;
    juce::StringArray provedNow; // drivers that played in this run of the app
    const char* const kBadKey = "driversThatFailed";

    double nowMs() { return juce::Time::getMillisecondCounterHiRes(); }

    struct Counter : juce::AudioIODeviceCallback
    {
        std::atomic<int> blocks { 0 };
        void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int n,
                                               const juce::AudioIODeviceCallbackContext&) override
        {
            for (int c = 0; c < numOut; ++c)
                if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);
            blocks.fetch_add (1);
        }
        void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
        void audioDeviceStopped() override {}
    };
} // namespace

AudioProbe::~AudioProbe()
{
    onDone = nullptr;
    cancel();
}

juce::StringArray AudioProbe::commandFor (const juce::String& type, const juce::String& device)
{
    return { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName(), "--probe-audio", type, device };
}

bool AudioProbe::start (const juce::StringArray& command, double timeoutMs, std::function<void (Result)> done)
{
    cancel();
    onDone = std::move (done);
    child = std::make_unique<juce::ChildProcess>();
    began = nowMs();
    timeout = timeoutMs;
    if (! child->start (command, juce::ChildProcess::wantStdOut))
    {
        // said by the timer on the next turn of the message loop, as if it had run, so that
        // cancel() and the destructor can still take it back
        child.reset();
        startFailed = true;
        startTimer (1);
        return false;
    }
    startTimer (100);
    return true;
}

void AudioProbe::cancel()
{
    stopTimer();
    startFailed = false;
    if (child != nullptr)
    {
        if (child->isRunning()) child->kill();
        child.reset();
    }
}

void AudioProbe::poll()
{
    if (startFailed)
    {
        stopTimer();
        startFailed = false;
        auto finished = std::move (onDone);
        onDone = nullptr;
        if (finished) finished ({ Outcome::couldNotStart, "The check could not be started", 0 });
        return;
    }
    if (child == nullptr) { stopTimer(); return; }
    const double ms = nowMs() - began;
    Result r;
    r.ms = ms;
    if (child->isRunning())
    {
        if (ms < timeout) return;
        child->kill();
        r.outcome = Outcome::timedOut;
        r.detail = "It did not answer within " + juce::String (juce::roundToInt (timeout / 1000.0)) + " seconds";
    }
    else
    {
        const auto said = child->readAllProcessOutput().trim();
        const auto code = child->getExitCode();
        // the last line the check printed: "ok ...", "silent ..." or "failed <what the driver said>"
        const auto line = juce::StringArray::fromLines (said).joinIntoString ("\n").fromLastOccurrenceOf ("RH-PROBE ", false, false)
                              .upToFirstOccurrenceOf ("\n", false, false).trim();
        r.detail = line.fromFirstOccurrenceOf (" ", false, false).trim();
        // the exit code decides: a window program's printing can be lost on the way
        if (code == 0) r.outcome = Outcome::ok;
        else if (code == 3 || line.startsWith ("silent")) r.outcome = Outcome::silent;
        else r.outcome = Outcome::failed;
        if (r.ok() && ! line.startsWith ("ok")) r.detail = {};
        if (r.detail.isEmpty() && ! r.ok()) r.detail = "It stopped without saying why (code " + juce::String ((int) code) + ")";
    }
    stopTimer();
    child.reset();
    auto finished = std::move (onDone);
    onDone = nullptr;
    if (finished) finished (r);
}

int AudioProbe::run (const juce::StringArray& args)
{
    const int at = args.indexOf ("--probe-audio");
    const auto type = at >= 0 && at + 1 < args.size() ? args[at + 1].unquoted() : juce::String();
    const auto name = at >= 0 && at + 2 < args.size() ? args[at + 2].unquoted() : juce::String();
    auto say = [] (const juce::String& line, int code)
    {
        std::cout << "RH-PROBE " << line.replaceCharacters ("\r\n", "  ").toStdString() << std::endl;
        return code;
    };
    if (type.isEmpty() || name.isEmpty()) return say ("failed Nothing to check", 2);

    juce::AudioDeviceManager dm;
    juce::OwnedArray<juce::AudioIODeviceType> types;
    dm.createAudioDeviceTypes (types);
    juce::AudioIODeviceType* t = nullptr;
    for (auto* x : types)
        if (x->getTypeName() == type) t = x;
    if (t == nullptr) return say ("failed This computer has no " + type + " drivers", 2);
    t->scanForDevices();
    if (! t->getDeviceNames (true).contains (name) && ! t->getDeviceNames (false).contains (name))
        return say ("failed " + name + " is not on this computer", 2);

    const bool both = t->hasSeparateInputsAndOutputs() == false;
    std::unique_ptr<juce::AudioIODevice> dev (t->createDevice (both || t->getDeviceNames (false).contains (name) ? name : juce::String(), name));
    if (dev == nullptr) return say ("failed The driver would not load", 2);

    // 48 kHz first, as the app asks; if that fails, the rate the driver says it is held at (by
    // its own panel, say), as the app also tries, so a driver held at 44.1 kHz still passes
    const auto rates = dev->getAvailableSampleRates();
    const double first = rates.contains (48000.0) ? 48000.0 : (rates.isEmpty() ? 0.0 : rates[0]);
    juce::BigInteger ins, outs;
    ins.setRange (0, std::min (2, std::max (1, dev->getInputChannelNames().size())), true);
    outs.setRange (0, std::min (2, std::max (1, dev->getOutputChannelNames().size())), true);
    auto err = dev->open (ins, outs, first, 0);
    if (err.isNotEmpty())
    {
        // one more try, at the rate it reports, or else 44.1 kHz, the other common one
        const double held = dev->getCurrentSampleRate();
        dev->close();
        const double second = held > 0.0 && std::abs (held - first) > 1.0 ? held
                            : (rates.contains (44100.0) && std::abs (first - 44100.0) > 1.0 ? 44100.0 : 0.0);
        if (second > 0.0)
            err = dev->open (ins, outs, second, 0);
    }
    if (err.isNotEmpty()) return say ("failed " + err.trim().upToFirstOccurrenceOf ("\n", false, false), 2);

    Counter counter;
    dev->start (&counter);
    const double began = nowMs();
    while (counter.blocks.load() < 8 && nowMs() - began < 3000.0)
        juce::Thread::sleep (20);
    const int got = counter.blocks.load();
    const auto ran = juce::String (dev->getCurrentSampleRate(), 0) + " Hz, " + juce::String (dev->getCurrentBufferSizeSamples()) + " samples";
    dev->stop();
    dev->close();
    dev.reset();
    if (got < 8) return say ("silent It opened at " + ran + " but sent no sound", 3);
    return say ("ok " + ran, 0);
}

void AudioProbe::useSettings (juce::PropertiesFile* s) { store = s; }

bool AudioProbe::knownBad (const juce::String& device)
{
    if (store == nullptr || provedNow.contains (device)) return false;
    return juce::StringArray::fromLines (store->getValue (kBadKey)).contains (device);
}

bool AudioProbe::provedThisRun (const juce::String& device) { return provedNow.contains (device); }

void AudioProbe::remember (const juce::String& device, bool worked)
{
    if (worked) provedNow.addIfNotAlreadyThere (device);
    else provedNow.removeString (device);
    if (store == nullptr) return;
    auto bad = juce::StringArray::fromLines (store->getValue (kBadKey));
    bad.removeEmptyStrings();
    if (worked) bad.removeString (device);
    else bad.addIfNotAlreadyThere (device);
    store->setValue (kBadKey, bad.joinIntoString ("\n"));
    store->saveIfNeeded();
}

juce::String AudioProbe::describe (const Result& r, const juce::String& device)
{
    const auto secs = juce::String (r.ms / 1000.0, 1) + " s";
    switch (r.outcome)
    {
        case Outcome::ok:            return device + " played in a test run (" + r.detail + ", " + secs + ")";
        case Outcome::silent:        return device + " opened in a test run but sent no sound (" + secs + ")";
        case Outcome::timedOut:      return device + " did not answer in a test run within " + secs + " and was stopped";
        case Outcome::couldNotStart: return "The test run for " + device + " could not be started";
        case Outcome::failed:        break;
    }
    return device + " failed in a test run: " + r.detail + " (" + secs + ")";
}
} // namespace rh
