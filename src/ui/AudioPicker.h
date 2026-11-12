// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/AudioSwitch.h"
#include "../plugin/AutoSetup.h"
#include "Widgets.h"

namespace rh::ui
{
// Where the guitar goes in and where the sound comes out, for the player who wants to say so
// by hand (setup does it for everyone else). There is no list of drivers to understand: the
// player picks the input, and Riffhouse tries the fastest way of driving it first and keeps
// the first one that really delivers sound. If none does, the last working choice comes back
// and the reason is written underneath.
class AudioPicker : public juce::Component
{
public:
    explicit AudioPicker (RiffhouseProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    // For pictures of the app: a PC with a built-in sound card, or an interface on its ASIO driver
    // (its outputs by name, then the PC's own outputs under a heading).
    void showExample (bool asio = false);

private:
    enum class Change { input, output, buffer, sharing };
    void refresh(); // the lists and the selection, from what is running
    void apply (Change);
    const AutoSetup::Input* picked() const;
    // ASIO: the interface's outputs in pairs come first in "Sound out", then every other output.
    // Picking one of those others moves the guitar to the same interface's Windows driver, since
    // ASIO can only play through the interface itself.
    void pickOtherOutputForAsio();
    static constexpr int kOtherOutputs = 1000; // item ids from here on are outputs of other hardware

    RiffhouseProcessor& proc;
    std::unique_ptr<AudioSwitch> changer;
    juce::ComboBox input, output, buffer;
    juce::ToggleButton sharing { "Let other apps play sound at the same time" };
    juce::TextButton panel { "Delay: open the driver's own settings" };
    std::vector<AutoSetup::Input> inputs;
    juce::Array<int> bufferSizes;
    int pairs = 0; // ASIO: how many pairs of outputs the interface has
    juce::String status, note, afterNote; // what happened last; and a standing remark about how it runs now
    bool statusIsProblem = false, filling = false, isAsio = false, example = false;
    bool followed = false; // the output was moved to the interface that was just picked as the input
};
// The window above, opened over `near`. Standalone only.
void showAudioPicker (RiffhouseProcessor&, juce::Component* near);
} // namespace rh::ui
