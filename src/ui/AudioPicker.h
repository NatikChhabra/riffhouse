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

    // For pictures of the app: a PC with a built-in sound card.
    void showExample();

private:
    enum class Change { input, output, buffer, sharing };
    void refresh(); // the lists and the selection, from what is running
    void apply (Change);
    const AutoSetup::Input* picked() const;

    RiffhouseProcessor& proc;
    std::unique_ptr<AudioSwitch> changer;
    juce::ComboBox input, output, buffer;
    juce::ToggleButton sharing { "Let other apps play sound at the same time" };
    juce::TextButton panel { "Open the driver's own settings" };
    std::vector<AutoSetup::Input> inputs;
    juce::Array<int> bufferSizes;
    juce::String status, note; // what happened last; and a standing remark about how it runs now
    bool statusIsProblem = false, filling = false, isAsio = false, example = false;
    bool followed = false; // the output was moved to the interface that was just picked as the input
};
} // namespace rh::ui
