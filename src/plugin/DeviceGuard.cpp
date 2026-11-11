// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "DeviceGuard.h"

namespace rh
{
namespace
{
    constexpr int kTickMs = 500;
    constexpr int kStallMs = 3000;  // no audio callbacks for this long means the device has stopped
    constexpr int kScanMs = 2000;   // how often to look for the input while it is gone

    juce::AudioIODeviceType* findType (juce::AudioDeviceManager& dm, const juce::String& name)
    {
        for (auto* t : dm.getAvailableDeviceTypes())
            if (t->getTypeName() == name) return t;
        return nullptr;
    }
} // namespace

bool DeviceGuard::looksLikeBluetooth (const juce::String& name)
{
    const auto n = name.toLowerCase();
    for (auto* w : { "bluetooth", "airpods", "hands-free", "handsfree", "buds", "wh-1000", "wf-1000", "bose qc", "quietcomfort",
                     "jbl tune", "jbl live", "soundcore", "beats ", "powerbeats", "freebuds", "earbuds", "jabra", " a2dp", "bt audio" })
        if (n.contains (w)) return true;
    return false;
}

DeviceGuard::DeviceGuard (juce::AudioDeviceManager& d, RiffhouseProcessor& p) : dm (d), proc (p)
{
    dm.addChangeListener (this);
    adopt();
    knownInputs = allInputs (false);
    startTimer (kTickMs);
}

DeviceGuard::~DeviceGuard()
{
    stopTimer();
    dm.removeChangeListener (this);
    if (guardMuted) proc.muteOutput.store (false);
}

void DeviceGuard::setState (State s)
{
    if (s == current) return;
    const auto before = current;
    current = s;
    if (s == State::healthy && (before == State::lost || before == State::stalled) && onMessage)
        onMessage (before == State::lost ? "Your guitar input is back" : "Audio restarted");
    if (s != State::healthy) bluetoothDismissed = false;
}

void DeviceGuard::adopt()
{
    auto* dev = dm.getCurrentAudioDevice();
    if (dev == nullptr || dev->getActiveInputChannels().isZero()) return;
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    if (s.inputDeviceName.isEmpty()) return;
    wanted = { dm.getCurrentAudioDeviceType(), s.inputDeviceName, s.outputDeviceName, dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate() };
    bluetoothName = looksLikeBluetooth (s.outputDeviceName) ? s.outputDeviceName : juce::String();
    bluetoothDismissed = false;
    stalledMs = sinceTryMs = tries = 0;
    lastBeat = proc.blockCounter.load();
    if (guardMuted) { proc.muteOutput.store (false); guardMuted = false; }
    current = State::healthy;
}

void DeviceGuard::pause (bool p)
{
    paused = p;
    stalledMs = sinceTryMs = 0;
    lastBeat = proc.blockCounter.load();
    if (p && guardMuted) { proc.muteOutput.store (false); guardMuted = false; } // setup does its own muting
}

juce::StringArray DeviceGuard::allInputs (bool rescan) const
{
    juce::StringArray names;
    for (auto* t : dm.getAvailableDeviceTypes())
    {
        if (rescan) t->scanForDevices();
        for (auto& n : t->getDeviceNames (true))
            names.addIfNotAlreadyThere (n);
    }
    return names;
}

bool DeviceGuard::inputAvailable (const juce::String& type, const juce::String& input, bool rescan) const
{
    auto* t = findType (dm, type);
    if (t == nullptr) return false;
    if (rescan) t->scanForDevices();
    return t->getDeviceNames (true).contains (input);
}

bool DeviceGuard::reopen()
{
    if (! wanted.valid()) return false;
    auto* t = findType (dm, wanted.type);
    if (t == nullptr) return false;
    if (dm.getCurrentAudioDeviceType() != wanted.type)
        dm.setCurrentAudioDeviceType (wanted.type, true);
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    s.inputDeviceName = wanted.input;
    const auto outputs = t->getDeviceNames (false);
    if (wanted.type == "ASIO") s.outputDeviceName = wanted.input;
    else if (outputs.contains (wanted.output)) s.outputDeviceName = wanted.output;
    else
    {
        const int idx = t->getDefaultDeviceIndex (false);
        s.outputDeviceName = juce::isPositiveAndBelow (idx, outputs.size()) ? outputs[idx] : outputs[0];
    }
    s.useDefaultInputChannels = false;
    s.inputChannels.clear();
    s.inputChannels.setRange (0, 2, true);
    s.useDefaultOutputChannels = true;
    s.bufferSize = wanted.bufferSize;
    s.sampleRate = wanted.sampleRate;
    auto err = dm.setAudioDeviceSetup (s, true);
    if (err.isNotEmpty() || dm.getCurrentAudioDevice() == nullptr)
    {
        // the exact buffer or rate may be gone (another app holds the device): let the driver choose
        s.bufferSize = 0;
        s.sampleRate = 0;
        err = dm.setAudioDeviceSetup (s, true);
    }
    lastBeat = proc.blockCounter.load();
    stalledMs = 0;
    return err.isEmpty() && dm.getCurrentAudioDevice() != nullptr;
}

void DeviceGuard::changeListenerCallback (juce::ChangeBroadcaster*)
{
    sinceScanMs = kScanMs; // look again on the next tick instead of waiting
    if (! paused) timerCallback();
}

void DeviceGuard::timerCallback()
{
    const auto beat = proc.blockCounter.load();
    const bool moving = beat != lastBeat;
    lastBeat = beat;
    if (paused) { stalledMs = 0; return; }

    auto* dev = dm.getCurrentAudioDevice();
    juce::AudioDeviceManager::AudioDeviceSetup s;
    dm.getAudioDeviceSetup (s);
    sinceScanMs += kTickMs;

    if (! wanted.valid())
    {
        // Nothing has ever been set up. Watch for an input being plugged in.
        setState (dev != nullptr && ! dev->getActiveInputChannels().isZero() ? State::healthy : State::noDevice);
        if (current == State::healthy) { adopt(); return; }
        if (sinceScanMs >= kScanMs)
        {
            sinceScanMs = 0;
            const auto now = allInputs (true);
            bool fresh = false;
            for (auto& n : now)
                if (! knownInputs.contains (n)) fresh = true;
            knownInputs = now;
            if (fresh && onNeedSetup) onNeedSetup();
        }
        return;
    }

    const bool ours = dev != nullptr && s.inputDeviceName == wanted.input && dm.getCurrentAudioDeviceType() == wanted.type;
    if (ours)
    {
        if (moving)
        {
            stalledMs = 0;
            tries = 0;
            if (guardMuted) { proc.muteOutput.store (false); guardMuted = false; }
            setState (State::healthy);
            return;
        }
        stalledMs += kTickMs;
        if (stalledMs < kStallMs) return;
        // Open but silent: sleep and wake, or a driver that gave up. Start it again, less often each time.
        setState (inputAvailable (wanted.type, wanted.input, true) ? State::stalled : State::lost);
        sinceTryMs += kTickMs;
        if (sinceTryMs >= std::min (10000, 2000 + tries * 2000))
        {
            sinceTryMs = 0;
            ++tries;
            dm.closeAudioDevice();
            if (current == State::stalled) reopen();
        }
        return;
    }

    // The input is gone, or the system put a different one in its place. Never play through a
    // microphone the player did not choose.
    if (dev != nullptr && s.inputDeviceName != wanted.input)
        dm.closeAudioDevice();
    if (! guardMuted) { proc.muteOutput.store (true); guardMuted = true; }
    setState (State::lost);
    if (sinceScanMs >= kScanMs)
    {
        sinceScanMs = 0;
        if (inputAvailable (wanted.type, wanted.input, true) && reopen())
        {
            // the next ticks confirm audio is flowing before the mute comes off
            stalledMs = 0;
        }
    }
}

DeviceGuard::Notice DeviceGuard::notice() const
{
    if (paused) return {};
    switch (current)
    {
        case State::lost:
            return { "Your guitar input was unplugged. Plug it back in and Riffhouse carries on by itself.", "Use another input", true };
        case State::stalled:
            return { "Audio stopped. Riffhouse is starting it again.", {}, true };
        case State::noDevice:
            return { "No guitar input yet. Plug in and Riffhouse will find it.", "Run setup", false };
        case State::healthy:
            if (bluetoothName.isNotEmpty() && ! bluetoothDismissed)
                return { "Bluetooth headphones add a delay you can feel. Wired headphones play in time.", "Got it", true };
            return {};
    }
    return {};
}

void DeviceGuard::dismissNotice() { bluetoothDismissed = true; }
} // namespace rh
