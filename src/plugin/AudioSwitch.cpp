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
}

AudioSwitch::AudioSwitch (juce::AudioDeviceManager& d, RiffhouseProcessor& p) : dm (d), proc (p) {}

AudioSwitch::~AudioSwitch()
{
    if (working)
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
    sawNoInput = false;
    tryNext();
}

void AudioSwitch::tryNext()
{
    while (tryIndex < want.types.size())
    {
        const auto type = want.types[tryIndex];
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
        // the buffer asked for belongs to the first mode; one tried in its place picks its own
        const auto err = openExactly (dm, { type, want.input, output, tryIndex == 0 ? want.bufferSize : 0,
                                            before.sampleRate > 0 ? before.sampleRate : 48000.0,
                                            type == "ASIO" ? want.outputPair : -1 });
        auto* dev = dm.getCurrentAudioDevice();
        if (err.isEmpty() && dev != nullptr && ! dev->getActiveInputChannels().isZero())
        {
            beatAtOpen = proc.blockCounter.load();
            openedAt = juce::Time::getMillisecondCounterHiRes();
            startTimer (100);
            return;
        }
        if (err.isEmpty() && dev != nullptr) sawNoInput = true;
        ++tryIndex;
    }
    goBack();
    finish (false, want.input + (sawNoInput ? " has no input to plug a guitar into. " : " would not start. ") + wentBackTo());
}

juce::String AudioSwitch::wentBackTo() const
{
    return before.valid() ? "Riffhouse went back to " + before.input + "." : juce::String ("Pick another input, or run the automatic setup.");
}

void AudioSwitch::timerCallback()
{
    if (proc.blockCounter.load() - beatAtOpen >= kBlocksWanted)
    {
        stopTimer();
        finish (true, running (dm, proc));
        return;
    }
    if (juce::Time::getMillisecondCounterHiRes() - openedAt < kProveMs) return;
    stopTimer();
    devlog::note (want.input + " opened as " + want.types[tryIndex] + " but delivered no sound in " + juce::String ((int) kProveMs) + " ms");
    ++tryIndex;
    if (tryIndex < want.types.size()) { tryNext(); return; }
    goBack();
    finish (false, want.input + " opened but no sound came through it. " + wentBackTo());
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
    auto done = std::move (onDone);
    onDone = nullptr;
    if (done) done ({ ok, message, tryIndex == 0 });
}
} // namespace rh
