// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

namespace rh
{
// Opens exactly this driver, input and output, and nothing else on the way. JUCE's own way of
// changing the driver type first opens that type's default devices, which on Windows means
// loading whatever ASIO driver happens to be first in the list or grabbing a sound card nobody
// asked for; here the sound card is opened once. Returns what went wrong, or nothing.
// buffer 0 and rate 0: whatever the driver likes. Message thread only.
struct AudioOpen
{
    juce::String type, input, output;
    int bufferSize = 0;
    double sampleRate = 0;
    // ASIO only: which pair of the interface's outputs the sound goes to, 0 for the first two.
    // -1 leaves it to the driver (its first two).
    int outputPair = -1;
    // Which pair of inputs the guitar comes in on, 0 for the first two (sockets 1 and 2). An
    // interface with four or more sockets can have the guitar in socket 3 or 4.
    int inputPair = -1;
    bool valid() const noexcept { return type.isNotEmpty() && (input.isNotEmpty() || output.isNotEmpty()); }
    static AudioOpen running (juce::AudioDeviceManager&); // what is open now; not valid() when nothing is
};
juce::String openExactly (juce::AudioDeviceManager&, const AudioOpen&);
} // namespace rh
