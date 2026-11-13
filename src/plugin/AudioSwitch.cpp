// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioSwitch.h"

#include "AutoSetup.h"
#include "DeviceLog.h"
#include "../engine/DeviceNames.h"

namespace rh
{
namespace
{
    constexpr double kProveMs = 2000.0; // how long a driver gets to deliver its first sound
    constexpr juce::uint32 kBlocksWanted = 4;
    constexpr double kBufferTryMs = 1600.0; // as long as setup listens to each buffer size
}

AudioSwitch::AudioSwitch (juce::AudioDeviceManager& d, RiffhouseProcessor& p) : dm (d), proc (p) {}

AudioSwitch::~AudioSwitch()
{
    probe.cancel();
    if (working && search != nullptr)
    {
        // closed while finding the buffer: keep the one already proven to play
        stopTimer();
        openExactly (dm, proven);
        search.reset();
        finish (true, {});
    }
    else if (working)
    {
        // closed in the middle of a try: leave what worked, and let whoever is waiting know
        stopTimer();
        goBack();
        finish (false, {});
    }
}

bool AudioSwitch::isShared (const juce::String& type)
{
    return type != "ASIO" && type != "Windows Audio (Exclusive Mode)";
}

juce::StringArray AudioSwitch::modesFor (juce::AudioDeviceManager& dm, const juce::String& type, const juce::String& input, bool shared)
{
    juce::StringArray modes;
    auto has = [&] (const juce::String& t)
    {
        for (auto* dt : dm.getAvailableDeviceTypes())
            if (dt->getTypeName() == t) return dt->getDeviceNames (true).contains (input);
        return false;
    };
    if (type == "ASIO") { modes.add ("ASIO"); return modes; }
    if (type.startsWith ("Windows Audio") || type == "DirectSound")
    {
        // DirectSound comes last: it is the slowest, and the one that starts when nothing else does
        for (auto* m : { "Windows Audio (Exclusive Mode)", "Windows Audio (Low Latency Mode)", "Windows Audio", "DirectSound" })
            if ((! shared || isShared (m)) && has (m)) modes.add (m);
        return modes;
    }
    modes.add (type); // other systems have one way in
    return modes;
}

juce::String AudioSwitch::friendlyType (const juce::String& t)
{
    if (t == "Windows Audio (Exclusive Mode)") return "Windows Audio, exclusive mode";
    if (t == "Windows Audio (Low Latency Mode)") return "Windows Audio, low latency mode";
    if (t == "Windows Audio") return "Windows Audio, shared mode";
    if (t == "DirectSound") return "DirectSound (old and slow)";
    if (t == "ASIO") return "ASIO (audio interface drivers)";
    return t;
}

juce::String AudioSwitch::outputToFollow (const juce::String& type, const juce::String& input, const juce::StringArray& outputs)
{
    if (type == "ASIO" || AutoSetup::looksLikeOnboardInput (input, type)) return {};
    return outputBeside (input, outputs);
}

juce::String AudioSwitch::running (juce::AudioDeviceManager& dm, const RiffhouseProcessor& proc)
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr) return "No audio device is open.";
    const double sr = std::max (1.0, dev->getCurrentSampleRate());
    const int buf = dev->getCurrentBufferSizeSamples();
    int lat = dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples();
    if (lat <= 0) lat = 2 * buf;
    lat += proc.getLatencySamples();
    return "Running: " + juce::String (buf) + " samples at " + juce::String (sr / 1000.0, 1) + " kHz, about "
           + juce::String (1000.0 * lat / sr, 1) + " ms from string to ear.";
}

void AudioSwitch::apply (const AudioChoice& c, std::function<void (Result)> done)
{
    if (working) return;
    working = true;
    want = c;
    onDone = std::move (done);
    before = AudioOpen::running (dm);
    tryIndex = 0;
    rateIndex = 0;
    lastError = {};
    sawNoInput = sawSilence = false;
    tryNext();
}

double AudioSwitch::rateAt (int typeIndex, int index) const
{
    const double asked = before.sampleRate > 0 ? before.sampleRate : 48000.0;
    // An ASIO driver can be held at one rate by its own settings or by Windows using the same
    // interface; it gets every usual rate, starting with the one it is already set to. Other
    // modes are tried once.
    const auto rates = want.types[typeIndex] == "ASIO" ? asioRatesToTry (asked) : juce::Array<double> { asked };
    return juce::isPositiveAndBelow (index, rates.size()) ? rates[index] : -1.0;
}

void AudioSwitch::tryNext()
{
    while (tryIndex < want.types.size())
    {
        const auto type = want.types[tryIndex];
        const double rate = rateAt (tryIndex, rateIndex);
        if (rate < 0) { ++tryIndex; rateIndex = 0; continue; }
        juce::String output = type == "ASIO" ? want.input : want.output;
        if (type != "ASIO")
            for (auto* dt : dm.getAvailableDeviceTypes())
                if (dt->getTypeName() == type)
                {
                    // the same output goes by another name under another kind of driver
                    const auto outs = dt->getDeviceNames (false);
                    if (! outs.contains (output))
                    {
                        const int idx = dt->getDefaultDeviceIndex (false);
                        output = juce::isPositiveAndBelow (idx, outs.size()) ? outs[idx] : outs[0];
                    }
                }
        if (type == "ASIO" && ! AudioProbe::provedThisRun (want.input))
        {
            // A test run outside the app first: an ASIO driver that takes seconds to fail, or
            // hangs, must not freeze the window. The driver needs the sound devices free for it.
            dm.closeAudioDevice();
            devlog::note ("Testing " + want.input + " in a test run first");
            probe.start (AudioProbe::commandFor (type, want.input), AudioProbe::kTimeoutMs, [this] (AudioProbe::Result r)
            {
                devlog::note (AudioProbe::describe (r, want.input));
                if (r.outcome != AudioProbe::Outcome::couldNotStart) AudioProbe::remember (want.input, r.ok());
                if (! r.ok())
                {
                    // one try per driver: it is not opened in the app at any rate
                    if (r.outcome == AudioProbe::Outcome::silent) sawSilence = true;
                    else lastError = r.detail;
                    ++tryIndex;
                    rateIndex = 0;
                }
                tryNext();
            });
            return;
        }
        // the buffer asked for belongs to the first mode; one tried in its place picks its own
        const double openStarted = juce::Time::getMillisecondCounterHiRes();
        const auto err = openExactly (dm, { type, want.input, output, tryIndex == 0 && rateIndex == 0 ? want.bufferSize : 0, rate,
                                            type == "ASIO" ? want.outputPair : -1, want.inputPair });
        auto* dev = dm.getCurrentAudioDevice();
        if (err.isEmpty() && dev != nullptr && ! dev->getActiveInputChannels().isZero())
        {
            beatAtOpen = proc.blockCounter.load();
            openedAt = juce::Time::getMillisecondCounterHiRes();
            startTimer (100);
            return;
        }
        if (err.isNotEmpty()) lastError = err;
        if (err.isNotEmpty() && type == "ASIO" && juce::Time::getMillisecondCounterHiRes() - openStarted > 2500.0)
        {
            // a driver that took seconds to fail fails the same way at every rate: not again
            ++tryIndex;
            rateIndex = 0;
            continue;
        }
        if (err.isEmpty() && dev != nullptr)
        {
            // it opened with no input at all: another rate will not grow one
            sawNoInput = true;
            ++tryIndex;
            rateIndex = 0;
            continue;
        }
        ++rateIndex;
    }
    goBack();
    finish (false, failureText());
}

juce::String AudioSwitch::failureText() const
{
    if (sawNoInput && lastError.isEmpty() && ! sawSilence) return want.input + " has no input to plug a guitar into. " + wentBackTo();
    if (lastError.containsIgnoreCase ("sample rate"))
        return "Windows runs " + want.input + " and your headphones at different rates, and "
               + (want.types.size() == 1 ? friendlyType (want.types[0]) : juce::String ("this mode"))
               + " needs one rate for both. In Windows sound settings, open each one's Properties, then Advanced, and set both to 48000 Hz. "
               + wentBackTo();
    if (want.types.contains ("ASIO")) return whyAsioFailed (want.input, lastError, sawSilence && lastError.isEmpty()) + " " + wentBackTo();
    const auto on = want.types.size() == 1 ? " on " + friendlyType (want.types[0]) : juce::String();
    return want.input + on + (sawSilence ? " opened but no sound came through it. " : " would not start. ")
         + (sawSilence ? juce::String() : juce::String ("Another program may be using it, often a voice call (Discord, Zoom, Meet): close it and try again. "))
         + wentBackTo();
}

juce::String AudioSwitch::wentBackTo() const
{
    if (! before.valid()) return "Pick another input, or run the automatic setup.";
    // name the driver too: going back to the same input on another driver is a real change
    const auto what = before.input.isNotEmpty() ? before.input : before.output;
    if (! want.types.contains (before.type) || what == want.input)
        return "Riffhouse went back to " + what + " on " + friendlyType (before.type) + ", which works.";
    return "Riffhouse went back to " + what + ".";
}

void AudioSwitch::timerCallback()
{
    if (search != nullptr)
    {
        if (juce::Time::getMillisecondCounterHiRes() - triedAt < kBufferTryMs) return;
        auto* dev = dm.getCurrentAudioDevice();
        const bool clean = tryOpened && dev != nullptr && dm.getXRunCount() - xrunsAtTry <= 0
                           && proc.blockCounter.load() - beatAtOpen >= kBlocksWanted && dm.getCpuUsage() < 0.7;
        search->result (clean);
        if (search->next() > 0) tryBuffer (search->next());
        else endTuning();
        return;
    }
    if (proc.blockCounter.load() - beatAtOpen >= kBlocksWanted)
    {
        stopTimer();
        // an input picked by hand, with no buffer asked for: end at the lowest delay that plays
        // clean, the way setup does, instead of the driver's own (often 10 ms or more)
        if (want.bufferSize == 0) { startTuning(); return; }
        finish (true, running (dm, proc));
        return;
    }
    if (juce::Time::getMillisecondCounterHiRes() - openedAt < kProveMs) return;
    stopTimer();
    devlog::note (want.input + " opened as " + want.types[tryIndex] + " but delivered no sound in " + juce::String ((int) kProveMs) + " ms");
    sawSilence = true;
    ++rateIndex; // the next rate, or the next mode once this one has had them all
    tryNext();
}

void AudioSwitch::goBack()
{
    // what was running comes back; when nothing was, nothing is left half open
    if (before.valid()) openExactly (dm, before);
    else dm.closeAudioDevice();
}

void AudioSwitch::finish (bool ok, const juce::String& message)
{
    working = false;
    auto text = message;
    auto* dev = dm.getCurrentAudioDevice();
    if (ok && rateIndex > 0 && dev != nullptr && dev->getTypeName() == "ASIO")
    {
        // the rate asked for would not play: say which one does, so a change nobody made is explained
        const double asked = rateAt (tryIndex, 0);
        text += " Your interface would not play at " + juce::String (asked / 1000.0, 1) + " kHz, so it runs at "
                + juce::String (dev->getCurrentSampleRate() / 1000.0, 1) + " kHz, the rate it is set to.";
    }
    auto done = std::move (onDone);
    onDone = nullptr;
    if (done) done ({ ok, text, tryIndex == 0 });
}

void AudioSwitch::startTuning()
{
    proven = AudioOpen::running (dm);
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr || ! proven.valid()) { finish (true, running (dm, proc)); return; }
    search = std::make_unique<BufferSearch> (buffersToTry (dev->getAvailableBufferSizes()), dev->getCurrentBufferSizeSamples());
    if (search->next() <= 0) { search.reset(); finish (true, running (dm, proc)); return; }
    devlog::note ("Finding the lowest buffer that plays clean on " + proven.input + ", from " + juce::String (proven.bufferSize));
    tryBuffer (search->next());
    startTimer (100);
}

void AudioSwitch::tryBuffer (int size)
{
    auto o = proven;
    o.bufferSize = size;
    tryOpened = openExactly (dm, o).isEmpty() && dm.getCurrentAudioDevice() != nullptr
                && dm.getCurrentAudioDevice()->getCurrentBufferSizeSamples() == size;
    xrunsAtTry = dm.getXRunCount();
    beatAtOpen = proc.blockCounter.load();
    triedAt = juce::Time::getMillisecondCounterHiRes();
}

void AudioSwitch::endTuning()
{
    stopTimer();
    const int best = search->best();
    search.reset();
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr || dev->getCurrentBufferSizeSamples() != best)
    {
        auto o = proven;
        o.bufferSize = best;
        if (openExactly (dm, o).isNotEmpty()) openExactly (dm, proven);
    }
    devlog::note ("Settled on " + juce::String (best) + " samples");
    finish (true, running (dm, proc));
}
} // namespace rh
