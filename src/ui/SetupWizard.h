// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/AutoSetup.h"
#include "Gear.h"
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
    bool keyPressed (const juce::KeyPress&) override;
    // For pictures of the app: a step as it looks half way through, with nothing running.
    // 0-3 the four steps, 4 the pause after the first, 5 the summary, 6 the summary on two
    // USB devices, 7 the sounds to start with.
    // `moving`: get there from the example shown before, the way the real thing would.
    void showExample (int which, bool moving = false);
    // For the checks: a setup that ran for real has just finished with this result.
    void finishedWith (const AutoSetup::Summary&);
    void startPlaying() { complete (true); } // setup is over, whatever step it is on
    std::function<void()> onFinished;
    bool ranToTheEnd = false; // closed from the summary: setup really did its work

private:
    // sound: the first time only, after the summary (or after skipping setup), a handful of
    // whole rigs to start from, so a player who has never used an amp sim hears a good tone at once
    enum class Step { welcome, running, done, failed, sound };
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
    // What the summary step shows. A copy: the setup that measured it is gone the moment
    // "Start playing" is pressed, and the card is still drawn while it fades.
    AutoSetup::Summary shownSummary;
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

    // The sounds to start with: one factory tone for each kind of playing, shown by its amp.
    struct Sound
    {
        juce::String style, tone;
        int preset = -1;
        int amp = 0;
        GearThumb thumb;
    };
    std::vector<Sound> sounds;
    int hoverSound = -1, chosenSound = -1;
    bool firstRun = false, reachedDone = false;
    void makeSounds();
    bool offerSounds() const { return firstRun && ! sounds.empty(); }
    void toSounds(); // the step after the summary, or after skipping setup, on the first run
    void choose (int which);
    static constexpr int kSoundW = 173, kSoundH = 160, kSoundGap = 12; // three across the card's 544
    static juce::Rectangle<float> soundArt(); // where the amp is drawn in a tile
    juce::Rectangle<int> soundArea (int which) const;
    int soundAt (juce::Point<int>) const;
};
} // namespace rh::ui
