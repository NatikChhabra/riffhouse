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
} // namespace rh
