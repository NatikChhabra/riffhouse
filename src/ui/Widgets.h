// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../plugin/PluginProcessor.h"
#include "Gear.h"
#include "Theme.h"

namespace rh::ui
{
void drawCard (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill, float radius);
inline void drawCard (juce::Graphics& g, juce::Rectangle<float> r) { drawCard (g, r, colours::panel, 18.0f); }

// A rotary knob with its name underneath. While you touch it, the name turns
// into the value. Double-click returns it to the default.
class Knob : public juce::Component
{
public:
    Knob (RiffhouseProcessor&, P id, juce::Colour colour, juce::String labelOverride = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void setColour (juce::Colour c);
    juce::Slider& getSlider() noexcept { return slider; }
    void setLabelHeight (int h) { labelH = h; resized(); }
    void setTooltip (const juce::String& t) { slider.setTooltip (t); }
    // Draw as a hardware knob printed on a piece of gear: its style, pointer or
    // cap colour, the panel's lettering colour and face.
    void setGear (KnobStyle, juce::Colour cap, juce::Colour ink, Face face, bool upper);
    void setNumbers (bool on) { slider.getProperties().set ("numbers", on); } // 0 to 10 round the knob, like an amp panel

private:
    struct S : juce::Slider
    {
        std::function<void()> onHover;
        void mouseEnter (const juce::MouseEvent& e) override { Slider::mouseEnter (e); if (onHover) onHover(); }
        void mouseExit (const juce::MouseEvent& e) override { Slider::mouseExit (e); if (onHover) onHover(); }
    } slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String label;
    int labelH = 34;
    bool gear = false, gearUpper = false;
    juce::Colour gearInk;
    Face gearFace = Face::instrument;
};

// Vertical fader (used by the EQ).
class Fader : public juce::Component
{
public:
    Fader (RiffhouseProcessor&, P id, juce::Colour colour, juce::String labelOverride = {});
    void resized() override;
    void paint (juce::Graphics&) override;
    void setGear (juce::Colour ink); // a slide pot in a slot, lettering printed in `ink`

private:
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::String label;
    bool gear = false;
    juce::Colour gearInk;
};

// Round power switch bound to an on/off parameter.
class PowerSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    PowerSwitch (RiffhouseProcessor&, P id, juce::Colour colour);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    bool isOn() const noexcept { return on; }
    void toggle() { attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f); }
    std::function<void (bool)> onChange;
    // round: the power icon in a circle. led: a small status light with ON or OFF
    // beside it, like the bypass light on a rack unit. hidden: draws nothing, only
    // takes the click (laid over a light that is part of a piece of gear).
    enum class Style { round, led, hidden };
    Style style = Style::round;

private:
    juce::ParameterAttachment attachment;
    juce::Colour colour;
    bool on = false;
};

// A row of pill buttons for a choice parameter, e.g. reverb types.
class ChipRow : public juce::Component
{
public:
    ChipRow (RiffhouseProcessor&, P id, juce::Colour colour);
    ChipRow (juce::StringArray items, juce::Colour colour, std::function<void (int)> onPick);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void setSelected (int i) { selected = i; repaint(); }
    int getSelected() const noexcept { return selected; }
    int preferredWidth() const;
    std::function<void (int)> onChange;
    float chipHeight = 32.0f;

private:
    std::vector<juce::Rectangle<float>> layoutChips() const;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::StringArray items;
    juce::Colour colour;
    int selected = 0, hover = -1;
    std::function<void (int)> pick;
};

// Flat button with an icon and optional text.
class IconButton : public juce::Button
{
public:
    IconButton (juce::Path icon, juce::String text = {}, bool filledIcon = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void setIcon (juce::Path p, bool filled) { icon = std::move (p); filled_ = filled; repaint(); }
    void setText (juce::String t) { setButtonText (std::move (t)); repaint(); }
    juce::Colour iconColour = colours::textDim, activeColour = colours::accent;
    bool drawBackground = true;
    float iconSize = 20.0f;

private:
    juce::Path icon;
    bool filled_ = false;
};

// The type picker on a piece of gear: < NAME v >. Click the name for the full list.
class ChoiceSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    ChoiceSelector (RiffhouseProcessor&, P id);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    int getSelected() const noexcept { return selected; }
    void select (int i) { attachment.setValueAsCompleteGesture ((float) i); if (onPick) onPick (i); }
    void showMenu();
    float textHeight = 22.0f;
    std::function<void (int)> onChange;                     // whenever the value changes, also from a preset or automation
    std::function<void (int)> onPick;                       // only when the player chose it here
    std::function<juce::String (int)> displayName;          // defaults to the choice's own name
    std::function<int (int current, int delta)> stepper;    // where the arrows go; defaults to wrapping
    std::function<void (juce::PopupMenu&)> buildMenu;       // defaults to every choice; ids are index + 1
    std::function<void (int)> onMenuItem;                   // for ids the default list doesn't use

private:
    juce::String nameFor (int i) const;
    juce::ParameterAttachment attachment;
    juce::StringArray items;
    int selected = 0;
    juce::Rectangle<int> nameArea;
    IconButton prev { icons::chevronLeft() }, next { icons::chevronRight() };
};

// Horizontal level meter with peak hold, read from an atomic by the editor's timer.
class Meter : public juce::Component
{
public:
    void setLevel (float linearPeak);
    void paint (juce::Graphics&) override;
    bool vertical = false;

private:
    float levelDb = -100.0f, holdDb = -100.0f;
    int holdTicks = 0;
};
} // namespace rh::ui
