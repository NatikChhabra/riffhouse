// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/AutoSetup.h"
#include "Widgets.h"

namespace rh::ui
{
// First-run screen of the standalone app. One button, then it figures out the rest by
// listening to you play: four steps, each saying what to do, each stopping for a moment
// to show what it found before the next one starts.
class SetupWizard : public juce::Component, private juce::Timer
{
public:
    explicit SetupWizard (RiffhouseProcessor&);
    ~SetupWizard() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    // Show the welcome step. `again`: setup was done by an older version and a new run is
    // worth it, which the welcome step then says.
    void begin (bool again = false);
    // For pictures of the app: a step as it looks half way through, with nothing running.
    // 0-3 the four steps, 4 the pause after the first, 5 the summary.
    void showExample (int which);
    std::function<void()> onFinished;

private:
    enum class Step { welcome, running, done, failed };
    // what the running step shows, from the setup at work or made up for a picture
    struct View
    {
        AutoSetup::Phase phase = AutoSetup::Phase::idle;
        bool pausing = false, measuringNoise = false;
        float progress = 0.0f;
        juce::String detail;
    };
    View view() const;
    void noteFound();
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
    float pulse = 0.0f, shownProgress = 0.0f;
    bool offeredAgain = false, example = false;
    View exampleView;
    AutoSetup::Summary exampleSummary;
    juce::String found[4]; // what each finished step found
    juce::StringArray inputsAtFailure; // what was plugged in when nothing was heard
    int ticksSinceScan = 0;
    juce::StringArray scanInputs() const;
};
} // namespace rh::ui
