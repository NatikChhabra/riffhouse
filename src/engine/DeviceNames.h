// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

namespace rh
{
// What Windows calls the piece of hardware an input or output is on: it stands in brackets
// at the end of the name, "Analogue 1 + 2 (Focusrite USB Audio)". Empty when there is none.
juce::String hardwareOf (const juce::String& deviceName);

// The output on the same piece of hardware as an input. With several, the main pair, which
// is where headphones and speakers hang. Empty when the input has no output of its own
// (a USB guitar cable, a webcam).
juce::String outputBeside (const juce::String& input, const juce::StringArray& outputs);

// Two output channels of an interface, named the way a player reads them: "Headphones L" and
// "Headphones R" are "Headphones", "Out 1" and "Out 2" are "Out 1 + 2".
juce::String outputPairName (const juce::String& left, const juce::String& right);

// Whether an ASIO driver and a name Windows lists are the same interface: "Focusrite USB ASIO"
// and "Analogue 1 + 2 (Focusrite USB Audio)" are; the computer's own sound card is not.
bool sameHardware (const juce::String& asioDriver, const juce::String& windowsName);

// Why the sound runs through Windows' shared mode, in words a player can act on; empty when it
// does not (exclusive mode, ASIO, or another system). `type` is the driver mode that runs.
juce::String whyShared (const juce::String& type, const juce::String& input, const juce::String& output);

// The buffer sizes setup tries in one driver mode, smallest first, spread over the whole range.
juce::Array<int> buffersToTry (const juce::Array<int>& available);

// Picking an input by hand ends at the smallest buffer that plays clean, in a few tries: it
// starts near 128 samples, goes down while the sound stays clean and up when it crackles.
// Only sizes below the one already running clean are tried.
class BufferSearch
{
public:
    BufferSearch (const juce::Array<int>& sizes, int running);
    int next() const noexcept { return trying; } // 0 when the search is over
    void result (bool clean);
    int best() const noexcept { return bestSize; }

private:
    juce::Array<int> sizes; // smallest first
    int at = -1, trying = 0, bestSize = 0;
    bool foundClean = false, crackled = false;
};
} // namespace rh
