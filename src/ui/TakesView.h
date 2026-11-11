// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/PluginProcessor.h"
#include "Widgets.h"

namespace rh::ui
{
// The strip that shows while a take plays: play and pause, where you are, and
// the one thing you can do next. Listening: "Change the tone". Re-amping: the
// take's clean guitar runs through the rig as it is set now, every knob works,
// and "Save as new take" writes the result faster than real time.
class TakeBar : public juce::Component, private juce::Timer
{
public:
    explicit TakeBar (RiffhouseProcessor&);
    ~TakeBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void start (const Take&, bool changeTone);
    void stop();
    bool isActive() const noexcept { return isVisible(); }
    bool isChangingTone() const noexcept { return proc.takes().isLoaded() && proc.takes().mode() == TakePlayer::Mode::throughRig; }
    juce::File current() const { return proc.takes().take().sound; }

    std::function<void (juce::String)> onMessage;
    std::function<void()> onChanged; // a take appeared, or the bar opened or closed

private:
    void timerCallback() override;
    void save();
    static juce::String clock (double seconds);

    RiffhouseProcessor& proc;
    IconButton play { icons::play(), {}, true }, close { icons::close() };
    juce::Slider pos;
    juce::TextButton action;
    std::unique_ptr<ReampJob> job;
    juce::Rectangle<int> nameArea, timeArea;
};

// Every recording, newest first. Click one to hear it.
class TakesView : public juce::Component, private juce::ListBoxModel
{
public:
    TakesView (RiffhouseProcessor&, TakeBar&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void refresh();

    std::function<void()> onClose;
    std::function<void (juce::String)> onMessage;
    std::function<void (const Take&, bool changeTone)> onPlay;

private:
    struct Cells { juce::Rectangle<int> play, name, when, length, tone, reveal, remove; };
    Cells cells (int width, int height, bool hasClean) const;
    int getNumRows() override { return (int) takes.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    juce::MouseCursor getMouseCursorForRow (int) override { return juce::MouseCursor::PointingHandCursor; }

    RiffhouseProcessor& proc;
    TakeBar& bar;
    std::vector<Take> takes;
    juce::ListBox list;
    IconButton close { icons::close() };
    juce::TextButton openFolder { "Open the folder" };
};
} // namespace rh::ui
