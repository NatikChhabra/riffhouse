// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "AudioSwitch.h"

#include "AutoSetup.h"

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
    if (isTimerRunning())
    {
        // closed in the middle of a try: leave what worked, and let whoever is waiting know
        stopTimer();
        goBack();
        finish (false, {});
    }
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
    if (isTimerRunning()) return;
    want = c;
    onDone = std::move (done);
    before = dm.createStateXml();
    typeBefore = dm.getCurrentAudioDeviceType();

    if (dm.getCurrentAudioDeviceType() != c.type)
        dm.setCurrentAudioDeviceType (c.type, true);
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    s.inputDeviceName = c.input;
    s.outputDeviceName = c.type == "ASIO" ? c.input : c.output;
    s.useDefaultInputChannels = false;
    s.inputChannels.clear();
    s.inputChannels.setRange (0, 2, true);
    s.useDefaultOutputChannels = true;
    s.bufferSize = c.bufferSize;
    const auto err = dm.setAudioDeviceSetup (s, true);
    auto* dev = dm.getCurrentAudioDevice();
    if (err.isNotEmpty() || dev == nullptr)
    {
        goBack();
        finish (false, c.input + " would not start" + (err.isNotEmpty() ? " (" + err.trim().upToFirstOccurrenceOf ("\n", false, false) + ")" : juce::String())
                           + ". Riffhouse went back to " + friendlyType (typeBefore) + ".");
        return;
    }
    if (dev->getActiveInputChannels().isZero())
    {
        goBack();
        finish (false, c.input + " has no input to plug a guitar into. Riffhouse went back to " + friendlyType (typeBefore) + ".");
        return;
    }
    beatAtOpen = proc.blockCounter.load();
    openedAt = juce::Time::getMillisecondCounterHiRes();
    startTimer (100);
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
    goBack();
    juce::String why = want.input + " opened but no sound came through it, so Riffhouse went back to " + friendlyType (typeBefore) + ".";
    if (want.type == "ASIO" && AutoSetup::isAsioWrapper (want.input))
        why << " It is a stand-in driver, not an audio interface's own. Exclusive mode is as fast and steadier.";
    finish (false, why);
}

void AudioSwitch::goBack()
{
    dm.closeAudioDevice();
    if (typeBefore.isNotEmpty() && dm.getCurrentAudioDeviceType() != typeBefore)
        dm.setCurrentAudioDeviceType (typeBefore, true);
    dm.initialise (2, 2, before.get(), true);
}

void AudioSwitch::finish (bool ok, const juce::String& message)
{
    auto done = std::move (onDone);
    onDone = nullptr;
    if (done) done ({ ok, message });
}
} // namespace rh
