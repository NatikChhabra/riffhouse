// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/InputGuard.h"
#include "../plugin/PluginProcessor.h"
#include "Modules.h"
#include "Panels.h"
#include "PresetBrowser.h"
#include "SetupWizard.h"
#include "Theme.h"

namespace rh
{
// The main window. Everything is laid out on a fixed 1200 x 780 canvas and
// scaled as a whole, so it looks identical at any window size.
class RiffhouseEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit RiffhouseEditor (RiffhouseProcessor&);
    ~RiffhouseEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int baseWidth = 1200, baseHeight = 780;
    // the rig fills a window up to this shape (wider than 21:9, taller than 4:3), then centres
    static constexpr int maxWidth = 1900, maxHeight = 1800; // tall enough for a window snapped to half a screen

    // Drop a file on the window: a .nam capture becomes the amp, a short sound file the
    // cabinet, a long one the song to play along with, a tone file the tone.
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void openFile (const juce::File&);

    void showSetupWizard (bool again = false);
    void closeSetupWizard();
    // Goes through the end of a real setup with the clock running: the summary of a setup that
    // measured something, "Start playing", the card fading while the rig powers on. The pictures
    // of the app only ever showed a made-up summary, and the real one crashed in 0.1.1.
    void rehearseSetup();
    void showToast (const juce::String&);
    void applyLook (ui::Look, bool reopenSettings);

    // Renders every screen to PNG files (used for docs and visual checks).
    void saveSnapshots (const juce::File& folder, float scale, float cutouts = 1.0f);
    // Every move in the app as a strip of frames, with the motion clock driven by hand: riffhouse --motion <folder>
    void saveFilmstrips (const juce::File& folder);
    // The real window as film: a guitar recording (DI) goes through the rig offline while the
    // window is painted at `fps`, with the meters fed by that very audio and the script's steps
    // (tones, pages, knob moves) taken at their times. Writes frame_00000.png ... and audio.wav,
    // the processed sound of the same render. Returns an error, or nothing when it worked.
    // riffhouse --film <folder> --input <di.wav> [--script <json>] [--fps 30] [--scale 2] [--size WxH] [--seconds s]
    juce::String saveFilm (const juce::File& folder, const juce::File& input, const juce::var& script,
                           int fps, float scale, double seconds);

private:
    class Root;
    void timerCallback() override;
    void buildRoot();
    void watchInputLevel();

    RiffhouseProcessor& proc;
    ui::LookAndFeel lnf;
    std::unique_ptr<Root> root;
    juce::TooltipWindow tooltips { this, 600 };
    InputGuard inputGuard;
    int guardTick = 0;
    double firstPaintBegan = 0.0; // the first picture is timed for the diary
    bool firstPaintDone = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffhouseEditor)
};
} // namespace rh
