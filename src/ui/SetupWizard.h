// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/AutoSetup.h"
#include "Widgets.h"

namespace rh::ui
{
// First-run screen of the standalone app. One button, then it figures out
// the rest by listening to you play.
class SetupWizard : public juce::Component, private juce::Timer
{
public:
    explicit SetupWizard (RiffhouseProcessor&);
    ~SetupWizard() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void begin(); // show the welcome step
    std::function<void()> onFinished;

private:
    enum class Step { welcome, running, done, failed };
    void timerCallback() override;
    void startSetup();
    void layoutButtons();
    juce::Rectangle<int> cardBounds() const;
    void complete (bool keep);

    RiffhouseProcessor& proc;
    std::unique_ptr<AutoSetup> setup;
    Step step = Step::welcome;
    juce::TextButton primary, secondary, tertiary;
    Meter meter;
    float pulse = 0.0f;
    juce::StringArray inputsAtFailure; // what was plugged in when nothing was heard
    int ticksSinceScan = 0;
    juce::StringArray scanInputs() const;
};
} // namespace rh::ui
