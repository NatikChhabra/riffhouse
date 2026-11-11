// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>

namespace rh
{
// Keeps the guitar connected without the player doing anything.
//  - The input is unplugged: the rig goes quiet, and the moment the same input
//    shows up again it is reopened with the settings it had.
//  - The computer sleeps, or a driver stops calling back: the device is restarted.
//  - Windows swaps in another microphone when the guitar's input disappears: that
//    is refused, because a laptop mic through an amp into speakers howls.
//  - Nothing was plugged in at launch and an input appears later: setup is offered.
// It only touches the device manager from the message thread, which is also the thread the
// window is drawn on. So it is careful with it: a sound card that will not start is asked a few
// times, further and further apart, and then left alone until something changes or the player
// says so. It never keeps the window waiting on a card that does not answer.
class DeviceGuard : private juce::Timer, private juce::ChangeListener
{
public:
    struct Notice
    {
        juce::String text, action; // action is the label of the button beside it, or empty
        bool warning = false;
        bool isEmpty() const noexcept { return text.isEmpty(); }
    };

    // `beats` ticks once per block of audio, which is how the guard knows sound is flowing;
    // `mute` silences the rig while an input the player did not choose would otherwise be heard.
    DeviceGuard (juce::AudioDeviceManager&, std::atomic<juce::uint32>& beats, std::atomic<bool>& mute);
    ~DeviceGuard() override;

    void adopt();          // the player or setup chose this device: it is the one to keep alive
    void retry();          // the player asked for another go after the guard gave up
    bool gaveUp() const noexcept { return attempts >= kMaxAttempts; }
    void pause (bool);     // setup is driving the devices itself
    Notice notice() const;
    void dismissNotice();  // hides a warning until something changes

    std::function<void()> onNeedSetup;                    // a new input appeared and there is nothing to return to
    std::function<void (const juce::String&)> onMessage;  // good news worth a toast

    static bool looksLikeBluetooth (const juce::String& deviceName);

    enum class State { noDevice, healthy, stalled, lost };
    State state() const noexcept { return current; }

    // One look at the connection, at this time in ms. A timer does this twice a second; the
    // checks call it with a clock of their own.
    void tick (double now);
    void somethingChanged() noexcept { listChanged = true; } // the device manager said so

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
    void adoptSaved();
    bool reopen();
    bool tryNow (double now);
    bool inputAvailable (const juce::String& type, const juce::String& input, bool rescan) const;
    juce::StringArray allInputs() const;
    void setState (State);

    static constexpr int kMaxAttempts = 5; // then the card is left alone

    juce::AudioDeviceManager& dm;
    std::atomic<juce::uint32>& beats;
    std::atomic<bool>& mute;
    double clock = 0; // the time of the tick in progress
    Wanted wanted;
    State current = State::noDevice;
    bool paused = false, guardMuted = false, bluetoothDismissed = false;
    bool listChanged = false; // the device manager said something changed: look on the next tick
    juce::uint32 lastBeat = 0;
    double lastMovedAt = 0, nextTryAt = 0, nextLookAt = 0, lastTryAt = -1.0e12;
    int attempts = 0, looks = 0;
    juce::StringArray knownInputs;
    juce::String bluetoothName;
};
} // namespace rh
