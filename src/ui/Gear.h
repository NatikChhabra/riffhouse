// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace rh::ui
{
// Every pedal and amp in Riffhouse is drawn as its own piece of gear: its own
// finish, lettering, knobs and light. All designs are original; none copies a
// real product's look or name.

enum class KnobStyle { modern, pedal, skirted, chicken, dome, hat };
enum class Finish { enamel, hammertone, sparkle, brushed, crinkle, anodised };

struct PedalDesign
{
    const char* name;      // printed on the box
    const char* kind;      // small line under the name
    juce::uint32 body, ink, led;
    Finish finish;
    KnobStyle knob;
    juce::uint32 knobCap;  // pointer or cap colour
    Face face;
    bool upper = false;    // capitals or lowercase lettering
    float width = 1.0f;    // relative enclosure width (fuzz boxes run wide)
    int stripe = 0;        // 0 none, 1 pinstripe band, 2 tread plate strip, 3 wide band
    // Where the parts sit, so the boxes are not all one template:
    // 0 light under the name; 1 light at the top edge, three knobs in a triangle;
    // 2 light up and left of the switch; 3 switch left of centre with the light on the right.
    int layout = 0;
};

enum class Tolex { levant, tweed, smooth, cream };
enum class Panel { black, chrome, gold, plate, anodised, brushed };
enum class Weave { silver, wheat, diamond, saltPepper, black, mesh, grid, none };

struct AmpDesign
{
    const char* badge;     // logo lettering on the grille
    Tolex tolex;
    juce::uint32 tolexColour;
    Panel panel;
    juce::uint32 ink;      // panel lettering
    Weave weave;
    juce::uint32 piping;   // thin cord around the grille (0 = none)
    KnobStyle knob;
    juce::uint32 knobCap;
    juce::uint32 jewel;    // pilot light colour
    Face face;
    bool badgePlate;       // lettering on a metal plate, or straight on the cloth
    bool upper;
    bool display = false;  // a display window instead of a badge (direct and capture)
};

const PedalDesign& drivePedalDesign (int driveType);
// The pedal for a module (chain order: gate, comp, wah, pitch, drive, amp, cab,
// eq, mod, delay, reverb, lo-fi) and its current type; nullptr for the amp and cab.
const PedalDesign* pedalDesignFor (int module, int type);
const AmpDesign& ampDesign (int ampModel);

// Knob drawing for the gear styles (called from the LookAndFeel).
void drawGearKnob (juce::Graphics&, juce::Rectangle<float> area, float angle, float startAngle, float endAngle,
                   KnobStyle, juce::Colour cap, juce::Colour ink, bool enabled, bool hot, bool numbers = false);

// Pedal enclosure. Returns nothing; the layout helper says where parts go.
struct PedalLayout
{
    juce::Rectangle<float> box, knobs, name, led, footswitch;
    int variant = 0;
};
PedalLayout pedalLayout (juce::Rectangle<float> area, const PedalDesign&, int numKnobs, bool faders = false);
// Where knob i of n sits on the pedal (one row up to four knobs, two rows above that).
juce::Rectangle<float> pedalKnobCell (const PedalLayout&, int i, int n);
// The finished box (no knobs, no switch) as an image, drawn at `scale` for sharp output.
juce::Image renderPedal (const PedalDesign&, const PedalLayout&, juce::Rectangle<float> area, float scale);
void drawPedalLed (juce::Graphics&, const PedalLayout&, const PedalDesign&, bool on);

// Amp head.
struct AmpLayout
{
    juce::Rectangle<float> head, panel, knobs, grille, badge, jewel, handle;
};
AmpLayout ampLayout (juce::Rectangle<float> area);
juce::Image renderAmp (const AmpDesign&, const AmpLayout&, juce::Rectangle<float> area, float scale,
                       const juce::String& displayText);
void drawAmpJewel (juce::Graphics&, const AmpLayout&, const AmpDesign&, bool on);

// Speaker cabinet seen from the front with the grille off, and the mic on it.
struct CabLayout
{
    juce::Rectangle<float> box, baffle;
    std::vector<juce::Rectangle<float>> speakers;
};
CabLayout cabLayout (juce::Rectangle<float> area, int cabModel);
juce::Image renderCab (int cabModel, const CabLayout&, juce::Rectangle<float> area, float scale, const juce::String& irName);
// micPos 0..10 (centre of the cone to its edge), distance 0..10 (touching to a foot away)
void drawCabMic (juce::Graphics&, const CabLayout&, float micPos, float distance);

// Small finished pictures of the gear (knobs and switch included) for the chain strip.
// Everything is in nominal units: draw `image` over `area`, and place it by `box`.
struct GearThumb
{
    juce::Image image;
    juce::Rectangle<float> area, box;
    PedalLayout pedal;
    AmpLayout amp;
};
GearThumb pedalThumb (const PedalDesign&, int numKnobs, bool faders, float scale);
GearThumb ampThumb (const AmpDesign&, const juce::String& displayText, float scale);
GearThumb cabThumb (int cabModel, float scale);

// The switch itself, for thumbnails as well as the live control.
void drawFootswitch (juce::Graphics&, juce::Rectangle<float> bounds, bool pressed, bool hover);

// A metal stomp switch bound to a module's on/off parameter.
class Footswitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    Footswitch (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isOn() const noexcept { return on; }
    std::function<void (bool)> onChange;

private:
    juce::ParameterAttachment attachment;
    bool on = false, pressed = false;
};
} // namespace rh::ui
