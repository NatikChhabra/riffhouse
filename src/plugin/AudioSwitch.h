// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "PluginProcessor.h"
#include "AudioOpen.h"
#include "../engine/DeviceNames.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace rh
{
// What the player picked in the audio settings: where the guitar goes in and where the sound
// comes out. How the sound card is driven is not theirs to work out: `types` are the driver
// modes to try, fastest first, and the first one that really delivers sound is kept.
struct AudioChoice
{
    juce::StringArray types;
    juce::String input, output; // for ASIO the driver is both input and output
    int bufferSize = 0;         // 0: whatever the driver likes. Only asked of the first mode.
    int outputPair = -1;        // ASIO: which pair of the interface's outputs, -1 for its first two
    int inputPair = -1;         // which pair of inputs the guitar is on, -1 for the first two
};

// Changes the input, output or buffer the careful way. Each driver mode in the choice is opened
// and then has to prove itself: sound must actually arrive through it. A mode that will not
// start, or starts and stays silent, is closed again and the next one is tried. When none
// works, what was running before comes back, with a sentence that says what happened. The
// player is never left with a silent app.
// Message thread only. While it works, whoever watches the devices must stand aside
// (StandaloneHost::setupRunning).
class AudioSwitch : private juce::Timer
{
public:
    AudioSwitch (juce::AudioDeviceManager&, RiffhouseProcessor&);
    ~AudioSwitch() override;

    struct Result
    {
        bool ok = false;
        juce::String message;
        bool fastest = true; // the first mode asked for is the one that runs
    };
    // `done` is called once, later, on the message thread.
    void apply (const AudioChoice&, std::function<void (Result)> done);
    bool busy() const noexcept { return working; }

    // The driver modes worth trying for an input, fastest first. `shared`: other apps must be
    // able to play at the same time, so the mode that takes the sound card for itself is left out.
    static juce::StringArray modesFor (juce::AudioDeviceManager&, const juce::String& type, const juce::String& input, bool shared);
    static bool isShared (const juce::String& type); // other apps keep their sound in this mode
    static juce::String friendlyType (const juce::String& typeName);
    // Someone with an audio interface listens through it. When the input they pick is one, this
    // is the output to open with it; empty leaves the output alone (a mic jack, a headset, ASIO).
    static juce::String outputToFollow (const juce::String& type, const juce::String& input, const juce::StringArray& outputs);
    // One sentence on what is running now, for the status line.
    static juce::String running (juce::AudioDeviceManager&, const RiffhouseProcessor&);

private:
    void timerCallback() override;
    void tryNext();
    void goBack();
    juce::String wentBackTo() const;
    void finish (bool ok, const juce::String& message);

    juce::AudioDeviceManager& dm;
    RiffhouseProcessor& proc;
    AudioChoice want;
    std::function<void (Result)> onDone;
    AudioOpen before;
    int tryIndex = 0;
    bool working = false, sawNoInput = false;
    juce::uint32 beatAtOpen = 0;
    double openedAt = 0;
    // after a hand-picked input proves it plays: the smallest buffer that stays clean
    void startTuning();
    void tryBuffer (int size);
    void endTuning();
    std::unique_ptr<BufferSearch> search;
    AudioOpen proven;
    int xrunsAtTry = 0;
    double triedAt = 0;
    bool tryOpened = false;
};
} // namespace rh
