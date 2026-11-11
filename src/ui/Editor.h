// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

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
class RiffhouseEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RiffhouseEditor (RiffhouseProcessor&);
    ~RiffhouseEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int baseWidth = 1200, baseHeight = 780;

    void showSetupWizard();
    void showToast (const juce::String&);
    void applyLook (ui::Look, bool reopenSettings);

    // Renders every screen to PNG files (used for docs and visual checks).
    void saveSnapshots (const juce::File& folder, float scale);

private:
    class Root;
    void timerCallback() override;
    void buildRoot();

    RiffhouseProcessor& proc;
    ui::LookAndFeel lnf;
    std::unique_ptr<Root> root;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffhouseEditor)
};
} // namespace rh
