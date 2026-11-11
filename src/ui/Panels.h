// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Widgets.h"

namespace rh::ui
{
// Chromatic tuner. Works in any tuning: it just shows the nearest note.
class TunerView : public juce::Component
{
public:
    explicit TunerView (RiffhouseProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onClose;

private:
    RiffhouseProcessor& proc;
    Tuner::Reading reading;
    float shownCents = 0.0f;
    int lostFrames = 100;
    IconButton close { icons::close() };
    juce::ToggleButton mute { "Mute the sound while tuning" };
};

// A small floating card, shown under the button that opened it.
class Popover : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    juce::String title;
};

// Recording options and the play-along track.
class RecordPanel : public Popover, private juce::Timer
{
public:
    explicit RecordPanel (RiffhouseProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void visibilityChanged() override;
    std::function<void()> onShowTakes;
    void refresh() { timerCallback(); } // what the timer does while the window is open

private:
    void timerCallback() override;
    void chooseSong();
    RiffhouseProcessor& proc;
    juce::ToggleButton countIn { "Count in 4 beats before recording" };
    juce::ToggleButton saveDi { "Also save the clean guitar (re-amp later)" };
    juce::TextButton openFolder { "Listen to your takes" }, changeFolder { "Change folder..." };
    juce::TextButton loadSong { "Load a song to play along..." };
    IconButton playSong { icons::play(), {}, true };
    juce::Slider songLevel, songPos, songSpeed;
    juce::TextButton keyDown { "-" }, keyUp { "+" };
    juce::TextButton partFrom { "Start here" }, partTo { "End here" }, partClear { "Whole song" };
    double partStart = -1.0; // a start that is marked and still waits for its end
    juce::String partNote;
    std::unique_ptr<juce::FileChooser> chooser;
};

class MetronomePanel : public Popover
{
public:
    explicit MetronomePanel (RiffhouseProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    std::function<void()> onChange;

private:
    void tap();
    RiffhouseProcessor& proc;
    juce::ToggleButton enabled { "Click on" };
    juce::Slider bpm, level;
    juce::TextButton tapButton { "Tap tempo" }, minus { "-" }, plus { "+" };
};

// The looper: one big button that does what the pedal's switch does, then stop, clear and volume.
class LooperPanel : public Popover, private juce::Timer
{
public:
    explicit LooperPanel (RiffhouseProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;
    static juce::String stateLine (RiffhouseProcessor&); // "Recording 3.2 s", "Playing a loop of 8.0 s"

private:
    void timerCallback() override;
    RiffhouseProcessor& proc;
    juce::TextButton main { "Record" }, stopButton { "Stop" }, clearButton { "Clear" };
    juce::Slider level;
};

// Input, audio and about. Audio settings appear only in the standalone app.
class SettingsView : public juce::Component, private juce::Timer
{
public:
    explicit SettingsView (RiffhouseProcessor&);
    ~SettingsView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    std::function<void()> onClose, onRunSetup;
    std::function<void (Look)> onLookChange;
    std::function<void (const juce::String&)> onMessage;

private:
    void timerCallback() override;
    void saveReport();
    juce::String deviceSummary() const;
    void showAdvancedAudio();

    RiffhouseProcessor& proc;
    IconButton close { icons::close() };
    juce::TextButton runSetup { "Run automatic setup again" }, advanced { "Driver, input and buffer..." };
    juce::TextButton website { "Help and fixes" }, report { "Save a report" }, problem { "Report a problem" };
    juce::String reportText() const;
    juce::TextButton updateButton { "Check for updates" };
    juce::ToggleButton autoUpdate { "Install updates by itself when I close Riffhouse" };
    juce::Rectangle<int> updateHeading, updateStatus, motionHeading;
    juce::ToggleButton lessMotion { "Less motion: nothing slides, fades are short" };
    void refreshUpdate();
    juce::String lastUpdateText;
    Knob inGain, gate;
    juce::ToggleButton inputFix { "Input Fix: brings back the sparkle a PC mic jack takes away" };
    juce::ToggleButton gateOn { "Noise gate" };
    std::unique_ptr<ChipRow> hum, looks;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inputFixAtt, gateOnAtt;
    Meter inMeter;
    juce::Rectangle<int> meterCaption;
    juce::String summary;
    int summaryHeight() const; // the device summary is one line or four: what follows sits right under it
    // Foot control: teach a controller, and the list of what is taught
    class FootList;
    juce::TextButton teachControl { "Teach a knob or switch" }, teachJob { "Teach a button..." };
    juce::Viewport footView;
    std::unique_ptr<FootList> footList;
    juce::Rectangle<int> footHeading, footText;
    static constexpr int aboutH = 100;
};

// Ask for a name and save the current tone.
class SaveDialog : public Popover
{
public:
    explicit SaveDialog (RiffhouseProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;
    std::function<void (juce::String)> onSaved;

private:
    void save();
    RiffhouseProcessor& proc;
    juce::TextEditor name;
    juce::ComboBox category;
    juce::TextButton saveButton { "Save tone" }, cancel { "Cancel" };
};
} // namespace rh::ui
