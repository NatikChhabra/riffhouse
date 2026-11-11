// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Widgets.h"

namespace rh::ui
{
// The signal chain as the player sees it, in order.
struct ModuleInfo
{
    const char* id;
    const char* title;
    juce::Colour colour;
    P onParam;
    int typeParam; // (int) P or -1
    std::vector<P> knobs;
    const char* blurb;
};

enum ModuleIndex { mGate, mComp, mWah, mPitch, mDrive, mAmp, mCab, mEq, mMod, mDelay, mReverb, mLofi, numModules };

// A module's colour in the current look.
juce::Colour moduleColour (int index);

const std::vector<ModuleInfo>& modules();
juce::String describeChoice (P param, int index);
juce::String ampTags (int model);

// One tile per module in the chain strip at the top.
class ChainStrip : public juce::Component
{
public:
    explicit ChainStrip (RiffhouseProcessor&);
    ~ChainStrip() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void setSelected (int index);
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onSelect;
    void refresh(); // called by the editor's timer

private:
    class Tile;
    RiffhouseProcessor& proc;
    float cableY = 0.0f;
    juce::OwnedArray<Tile> tiles;
    int selected = mAmp;
};

// The speaker and the mic drawn from above, on the cabinet page. It shows what the two
// mic knobs do, and the mic can be dragged: sideways across the cone, back into the room.
class MicMap : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MicMap (RiffhouseProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    struct Plan { juce::Rectangle<float> area; float cx, baffleY, coneR, travel, near, far; };
    Plan plan() const;
    void moveTo (juce::Point<float>);
    RiffhouseProcessor& proc;
    bool dragging = false;
};

// The page under the chain strip that edits one module: the piece of gear itself,
// its type picker above it, and nothing else in the way.
class ModulePage : public juce::Component
{
public:
    ModulePage (RiffhouseProcessor&, int moduleIndex);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    void updateTypeText();
    void updateEnablement();
    const PedalDesign& design() const;

    RiffhouseProcessor& proc;
    const ModuleInfo& info;
    int index;
    PowerSwitch power;
    std::unique_ptr<ChoiceSelector> types, sync;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Fader> faders;
    std::unique_ptr<IconButton> loadIr;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String typeText, extraText;
    int lastType = -1, lastSync = -1;
    juce::Rectangle<int> titleArea, descArea, syncLabel;

    bool cabMode = false;
    CabLayout cab;
    juce::Rectangle<float> cabArea, preArea;
    MicPreLayout pre;
    juce::Image preImage;
    float lastMic = -1.0f, lastDist = -1.0f;
    std::unique_ptr<MicMap> micMap;
    std::unique_ptr<Footswitch> foot;
    std::unique_ptr<Treadle> treadle;
    PedalLayout pedal;
    juce::Rectangle<float> pedalArea, pedalDraw;
    juce::Image pedalImage;
};

// The amp gets its own page: it is the heart of the tone.
class AmpPage : public juce::Component
{
public:
    explicit AmpPage (RiffhouseProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

private:
    void loadCapture();
    juce::String nameFor (int model) const;

    RiffhouseProcessor& proc;
    int current = 0;
    PowerSwitch power, jewel;
    ChoiceSelector selector;
    juce::OwnedArray<Knob> knobs;
    IconButton loadNam { icons::folder(), "Load capture" };
    juce::Rectangle<int> titleArea, descArea, tagArea;
    juce::Rectangle<float> ampArea;
    AmpLayout layout;
    juce::Image ampImage;
    juce::String status;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace rh::ui
