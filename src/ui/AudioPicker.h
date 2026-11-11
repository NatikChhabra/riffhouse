// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/AudioSwitch.h"
#include "Widgets.h"

namespace rh::ui
{
// Driver, input, output and buffer, for the player who wants to choose them by hand (setup
// does it for everyone else). Every change is tried and has to deliver sound; if it does
// not, the last working choice comes back and the reason is written underneath.
class AudioPicker : public juce::Component
{
public:
    explicit AudioPicker (RiffhouseProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

    // For pictures of the app: a PC with a built-in sound card and no ASIO driver.
    void showExample();

private:
    void refresh();          // the lists and the selection, from what is running
    void fillDevices();      // inputs and outputs of the driver type picked
    void typePicked();
    void apply();
    juce::String pickedType() const;

    RiffhouseProcessor& proc;
    std::unique_ptr<AudioSwitch> changer;
    juce::ComboBox type, input, output, buffer;
    juce::TextButton panel { "Open the driver's own settings" };
    juce::StringArray typeNames;
    juce::Array<int> bufferSizes;
    juce::String status, note; // what happened last; and a standing remark about the driver type
    bool statusIsProblem = false, filling = false, isAsio = false, example = false;
};
} // namespace rh::ui
