// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Theme.h"
#include "Motion.h"

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
    // The enclosure itself, so a pedalboard is not one box in different paint:
    // 0 cast box with a round stomp switch; 1 box with a recessed control deck and a hinged foot plate;
    // 2 folded sheet steel wedge on a base plate; 3 machined slab with a chamfered edge and a soft-touch switch;
    // 4 console with wooden end cheeks; 5 treadle pedal that rocks
    int form = 0;
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
    // What makes one amp a different object from the next, not a repaint of it:
    // 0 head, controls above the cloth; 1 head, controls along the bottom; 2 combo with a control strip on top;
    // 3 small metal head with vents; 4 head with two rows of big controls; 5 rack unit with a display
    int form = 0;
    int groups = 0;        // how the eight controls are spaced: 0 evenly, 1 as 1+3+2+2, 2 as 4+4, 3 as 1+3+4
    int lamp = 0;          // 0 jewel pilot light; 1 toggle switch under a small lamp; 2 lit rocker switch
    float width = 1.0f;    // share of the available width the cabinet takes
    // The panel as its maker would have laid it out. The eight controls are always gain, bass,
    // mid, treble, presence, depth, master and level, but each amp calls them what it likes,
    // puts them in its own order, and keeps the lesser ones as small trim knobs.
    const char* labels[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    juce::uint8 order[8] = { 0, 1, 2, 3, 4, 5, 6, 7 }; // which control stands at each place, left to right
    juce::uint8 smallMask = 0;                         // bit n set: control n is a small knob
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
    juce::Rectangle<float> treadle; // the rocking plate of a treadle pedal (empty on every other box)
    int variant = 0;
    int form = 0;
};
PedalLayout pedalLayout (juce::Rectangle<float> area, const PedalDesign&, int numKnobs, bool faders = false);
// Where knob i of n sits on the pedal (one row up to four knobs, two rows above that).
juce::Rectangle<float> pedalKnobCell (const PedalLayout&, int i, int n);
// The finished box (no knobs, no switch) as an image, drawn at `scale` for sharp output.
juce::Image renderPedal (const PedalDesign&, const PedalLayout&, juce::Rectangle<float> area, float scale, bool smallPrint = true);
// Pedals photographed by the 3D renderer: whether this one has a picture, and the patch of
// floor the picture covers for a box of any size (the box, its sockets, its front wall, its shadow).
bool pedalPictured (const PedalDesign&);
juce::Rectangle<float> pedalArtArea (juce::Rectangle<float> box);
// The print on the face alone (name, stripes, small print) on a clear ground, for the 3D renderer.
juce::Image renderPedalPrint (const PedalDesign&, const PedalLayout&, juce::Rectangle<float> area, float scale);
void drawPedalLed (juce::Graphics&, const PedalLayout&, const PedalDesign&, bool on);
// A light between off (0) and on (1), for the moment it takes to come on or go out.
void drawPedalLed (juce::Graphics&, const PedalLayout&, const PedalDesign&, float level);

// Amp head.
struct AmpLayout
{
    juce::Rectangle<float> area;               // the place the amp was given to stand in
    juce::Rectangle<float> head, panel, knobs, grille, badge, jewel, handle;
    juce::Rectangle<float> rack;               // the road case a rack unit is bolted into (empty for an amp)
    std::vector<juce::Rectangle<float>> cells; // where each of the eight controls sits: the knob with its label under it
    std::vector<juce::Rectangle<float>> slots; // the same cells in the order they stand on the panel
    juce::Point<float> jack;                   // the input socket
    float knobSize = 0.0f;
    int form = 0;
};
AmpLayout ampLayout (juce::Rectangle<float> area, const AmpDesign&);
juce::Image renderAmp (const AmpDesign&, const AmpLayout&, juce::Rectangle<float> area, float scale,
                       const juce::String& displayText);
// The amp page gives every amp the same place to stand in; the 3D photographs are taken at that size.
inline constexpr float ampCanvasW = 1080.0f, ampCanvasH = 420.0f;
bool ampPictured (const AmpDesign&);
juce::Rectangle<float> ampArtArea (juce::Rectangle<float> ampArea);
// For the 3D renderer: what is printed flat on the amp, and (as outlines) what stands up off its face.
juce::Image renderAmpPrint (const AmpDesign&, const AmpLayout&, juce::Rectangle<float> area, float scale, juce::var& raisedParts);
void drawAmpJewel (juce::Graphics&, const AmpLayout&, const AmpDesign&, bool on);
void drawAmpJewel (juce::Graphics&, const AmpLayout&, const AmpDesign&, float level);
// How long an amp's power light takes to come on and go out: a jewel lamp warms up, a switch just flips.
int ampLampMs (const AmpDesign&, bool comingOn);

// Speaker cabinet seen from the front with the grille off, and the mic on it.
struct CabLayout
{
    juce::Rectangle<float> box, baffle;
    std::vector<juce::Rectangle<float>> speakers;
};
CabLayout cabLayout (juce::Rectangle<float> area, int cabModel);
juce::Image renderCab (int cabModel, const CabLayout&, juce::Rectangle<float> area, float scale, const juce::String& irName);
// The microphone's channel strip: the box its cable runs to, with the four controls that place
// the mic and trim its lows and highs.
struct MicPreLayout
{
    juce::Rectangle<float> box;
    std::vector<juce::Rectangle<float>> cells; // position, distance, low cut, high cut
    juce::Point<float> socket;                 // where the mic cable plugs in
    float knobSize = 0.0f;
};
MicPreLayout micPreLayout (juce::Rectangle<float> box);
juce::Image renderMicPre (const MicPreLayout&, juce::Rectangle<float> area, float scale);
// micPos 0..10 (centre of the cone to its edge), distance 0..10 (touching to a foot away).
// With a socket given, the cable runs along the floor and plugs into it.
void drawCabMic (juce::Graphics&, const CabLayout&, float micPos, float distance, juce::Point<float> socket = {});

// Small finished pictures of the gear (knobs and switch included) for the chain strip.
// Everything is in nominal units: draw `image` over `area`, and place it by `box`.
struct GearThumb
{
    juce::Image image;
    juce::Rectangle<float> area, box;
    PedalLayout pedal;
    AmpLayout amp;
};
// With `labels`, the picture is a full portrait: every control is lettered and the small print is on it.
GearThumb pedalThumb (const PedalDesign&, int numKnobs, bool faders, float scale, const juce::StringArray* labels = nullptr);
GearThumb ampThumb (const AmpDesign&, const juce::String& displayText, float scale, const juce::StringArray* labels = nullptr);
GearThumb cabThumb (int cabModel, float scale);
// Draws a thumb as large as fits in `target`, standing on its bottom edge like gear on a floor.
void drawThumb (juce::Graphics&, const GearThumb&, juce::Rectangle<float> target);
// The scale to render a thumb at so that it is sharp at twice the size of `target`.
float thumbScaleFor (const GearThumb& probe, juce::Rectangle<float> target);

// The switch itself, for thumbnails as well as the live control.
// style 0 round stomp switch, 1 hinged foot plate in the box's own paint, 2 low soft-touch switch
void drawFootswitch (juce::Graphics&, juce::Rectangle<float> bounds, bool pressed, bool hover, int style = 0, juce::Colour body = {});
int footswitchStyle (const PedalLayout&);
// The rocking plate of a treadle pedal. position 0 is heel down, 1 is toe down.
// With `pictured`, the pedal has a picture and the treadle is drawn from its strip of photographs,
// which reach a little past the well (treadleArtArea): boxHeight is the height of the pedal's box.
void drawTreadle (juce::Graphics&, juce::Rectangle<float> well, float position, bool pressed, bool hover,
                  const PedalDesign* pictured = nullptr, float boxHeight = 420.0f);
juce::Rectangle<float> treadleArtArea (juce::Rectangle<float> well, float boxHeight);

// A metal stomp switch bound to a module's on/off parameter.
class Footswitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    Footswitch (juce::RangedAudioParameter&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isOn() const noexcept { return on; }
    void setStyle (int s, juce::Colour bodyColour, bool inPicture = false) { style = s; body = bodyColour; pictured = inPicture; repaint(); }
    std::function<void (bool)> onChange;

private:
    juce::ParameterAttachment attachment;
    bool on = false, pressed = false;
    bool pictured = false; // the switch is part of the pedal's picture: only a press is drawn
    int style = 0;
    juce::Colour body;
};

// The treadle of a wah: drag it to rock the pedal, click it to switch the pedal on or off.
class Treadle : public juce::Component, public juce::SettableTooltipClient
{
public:
    Treadle (juce::RangedAudioParameter& position, juce::RangedAudioParameter& onOff);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    // When the pedal follows the picking by itself there is nothing to rock: it sits level.
    void setRocking (bool canRock) { rocking = canRock; refresh(); }
    // The pedal this treadle belongs to has a picture: the treadle is drawn from its photographs,
    // which stand a little outside the component.
    void setPicture (const PedalDesign* d, float boxH) { design = d; boxHeight = boxH; setPaintingIsUnclipped (d != nullptr); refresh(); }
    std::function<void (bool)> onChange;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment pos, power;
    float value = 0.5f, startValue = 0.5f;
    bool on = false, pressed = false, dragging = false, rocking = true;
    const PedalDesign* design = nullptr;
    float boxHeight = 420.0f;
    void refresh();
};
} // namespace rh::ui
