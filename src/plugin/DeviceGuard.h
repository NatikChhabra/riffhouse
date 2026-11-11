// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace rh
{
// Keeps the guitar connected without the player doing anything.
//  - The input is unplugged: the rig goes quiet, and the moment the same input
//    shows up again it is reopened with the settings it had.
//  - The computer sleeps, or a driver stops calling back: the device is restarted.
//  - Windows swaps in another microphone when the guitar's input disappears: that
//    is refused, because a laptop mic through an amp into speakers howls.
//  - Nothing was plugged in at launch and an input appears later: setup is offered.
// It only touches the device manager from the message thread.
class DeviceGuard : private juce::Timer, private juce::ChangeListener
{
public:
    struct Notice
    {
        juce::String text, action; // action is the label of the button beside it, or empty
        bool warning = false;
        bool isEmpty() const noexcept { return text.isEmpty(); }
    };

    DeviceGuard (juce::AudioDeviceManager&, RiffhouseProcessor&);
    ~DeviceGuard() override;

    void adopt();          // the player or setup chose this device: it is the one to keep alive
    void pause (bool);     // setup is driving the devices itself
    Notice notice() const;
    void dismissNotice();  // hides a warning until something changes

    std::function<void()> onNeedSetup;                    // a new input appeared and there is nothing to return to
    std::function<void (const juce::String&)> onMessage;  // good news worth a toast

    static bool looksLikeBluetooth (const juce::String& deviceName);

    enum class State { noDevice, healthy, stalled, lost };
    State state() const noexcept { return current; }

private:
    struct Wanted
    {
        juce::String type, input, output;
        int bufferSize = 0;
        double sampleRate = 0;
        bool valid() const noexcept { return input.isNotEmpty(); }
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    bool reopen();
    bool inputAvailable (const juce::String& type, const juce::String& input, bool rescan) const;
    juce::StringArray allInputs (bool rescan) const;
    void setState (State);

    juce::AudioDeviceManager& dm;
    RiffhouseProcessor& proc;
    Wanted wanted;
    State current = State::noDevice;
    bool paused = false, guardMuted = false, bluetoothDismissed = false;
    juce::uint32 lastBeat = 0;
    int stalledMs = 0, sinceTryMs = 0, tries = 0, sinceScanMs = 0;
    juce::StringArray knownInputs;
    juce::String bluetoothName;
};
} // namespace rh
