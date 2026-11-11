// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/AutoSetup.h"
#include "Widgets.h"

namespace rh::ui
{
// First-run screen of the standalone app. You say where the guitar is plugged in, or let
// Riffhouse find it, and it figures out the rest by listening to you play: four steps, each
// saying what to do, each stopping for a moment to show what it found before the next one starts.
class SetupWizard : public juce::Component, private juce::Timer
{
public:
    explicit SetupWizard (RiffhouseProcessor&);
    ~SetupWizard() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    // Show the welcome step. `again`: setup was done by an older version and a new run is
    // worth it, which the welcome step then says.
    // `pretend`: inputs to list in place of this computer's, for pictures of the app.
    void begin (bool again = false, const std::vector<AutoSetup::Input>* pretend = nullptr);
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    // For pictures of the app: a step as it looks half way through, with nothing running.
    // 0-3 the four steps, 4 the pause after the first, 5 the summary.
    // `moving`: get there from the example shown before, the way the real thing would.
    void showExample (int which, bool moving = false);
    std::function<void()> onFinished;
    bool ranToTheEnd = false; // closed from the summary: setup really did its work

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
    void go (Step);
    void animate();
    int heightFor (Step) const;

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
    // The card grows and shrinks between steps and what is on it fades in; a finished step's
    // check mark is drawn, left to right; on the summary the lines arrive one after another.
    motion::Glide cardH, contentIn { 1.0f }, titleIn { 1.0f }, rowDone[4];
    juce::String lastTitle;
    double doneAt = -1.0e12;
    juce::StringArray scanInputs() const;
    // The welcome step lists every input: "Find it for me" first, then each one by name.
    std::vector<AutoSetup::Input> inputs;
    int picked = -1, hoverRow = -1, firstRow = 0; // picked: -1 is "find it for me"
    static constexpr int kRowH = 34, kRowsShown = 6;
    int listRows() const { return inputs.empty() ? 0 : std::min (kRowsShown, (int) inputs.size() + 1); }
    juce::Rectangle<int> listArea() const;
    int rowAt (juce::Point<int>) const; // which line of the list, -1 for none
};
} // namespace rh::ui
