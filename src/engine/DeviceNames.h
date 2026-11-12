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

// The sample rates an ASIO driver is opened at, in order, when the one asked for does not play:
// 0 is the rate the interface is already set to (in its own settings, or by Windows), which is
// the one most likely to work, then the usual rates. Each rate comes once.
juce::Array<double> asioRatesToTry (double asked);

// What to say when an ASIO driver would not start or stayed silent at every rate, in words a
// player can act on. `driver` is its name, `error` what the driver said (may be empty),
// `silent` true when it opened but no sound came through.
juce::String whyAsioFailed (const juce::String& driver, const juce::String& error, bool silent);

// What to say when the interface's own settings hold the buffer at one size: empty when the
// delay is short enough or Riffhouse can change it. `button` is the name of the button that
// opens the driver's own settings.
juce::String asioBufferAdvice (int bufferSize, double sampleRate, int sizesOffered, const juce::String& button);
// The delay as the big figure shows it: one decimal under 10 ms, a whole number from 10 up.
juce::String delayFigure (double ms);

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
