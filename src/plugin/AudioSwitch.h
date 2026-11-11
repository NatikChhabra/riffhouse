// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace rh
{
// What the player picked in the audio settings.
struct AudioChoice
{
    juce::String type, input, output; // for ASIO the driver is both input and output
    int bufferSize = 0;               // 0: whatever the driver likes
};

// Changes the audio driver, input, output or buffer the careful way. The choice is opened
// and then has to prove itself: sound must actually arrive through it. A driver that will
// not start, or starts and stays silent (which is what a stand-in ASIO driver on a built-in
// sound card tends to do), is closed again and what was running before comes back, with a
// sentence that says what happened. The player is never left with a silent app.
// Message thread only. While it works, whoever watches the devices must stand aside
// (StandaloneHost::setupRunning).
class AudioSwitch : private juce::Timer
{
public:
    AudioSwitch (juce::AudioDeviceManager&, RiffhouseProcessor&);
    ~AudioSwitch() override;

    struct Result { bool ok = false; juce::String message; };
    // `done` is called once, later, on the message thread.
    void apply (const AudioChoice&, std::function<void (Result)> done);
    bool busy() const noexcept { return isTimerRunning(); }

    static juce::String friendlyType (const juce::String& typeName);
    // Someone with an audio interface listens through it. When the input they pick is one, this
    // is the output to open with it; empty leaves the output alone (a mic jack, a headset, ASIO).
    static juce::String outputToFollow (const juce::String& type, const juce::String& input, const juce::StringArray& outputs);
    // One sentence on what is running now, for the status line.
    static juce::String running (juce::AudioDeviceManager&, const RiffhouseProcessor&);

private:
    void timerCallback() override;
    void goBack();
    void finish (bool ok, const juce::String& message);

    juce::AudioDeviceManager& dm;
    RiffhouseProcessor& proc;
    AudioChoice want;
    std::function<void (Result)> onDone;
    std::unique_ptr<juce::XmlElement> before;
    juce::String typeBefore;
    juce::uint32 beatAtOpen = 0;
    double openedAt = 0;
};
} // namespace rh
