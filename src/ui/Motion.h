// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Everything in Riffhouse that moves, moves through here: one clock, one set of
// durations, two curves. Motion is feedback, never decoration: a press answers the
// finger, a panel shows where it came from, a lamp warms up like a lamp. Nothing
// moves at rest, no control takes longer than 300 ms (a jewel lamp dying down and the
// end of setup are the two slower things), and a new input always wins over a running move.
namespace rh::ui::motion
{
// A film (Riffhouse --film) draws a pointer of its own over the window. The control it rests on
// shows its real hover look, and its pressed look while the film clicks it.
inline const juce::Component* filmPointer = nullptr;
inline bool filmPress = false;
inline bool pointedAt (const juce::Component& c) noexcept { return filmPointer == &c; }
// Milliseconds. A control never takes longer than `overlay`.
constexpr int press = 90, release = 150, hover = 170, toggle = 170, ui = 200, overlay = 240, leave = 150, travel = 260;
constexpr int lampUp = 240, lampDown = 420, ledUp = 60, ledDown = 140; // an amp's jewel lamp warms up; an LED just lights

double now();                  // the motion clock, in ms
void setClock (double ms);     // tests and filmstrips drive the clock by hand ...
void freeClock();              // ... and hand it back
void step();                   // run one frame of everything in flight (the display does this by itself)

// "Less motion": moves become cuts and fades become short. Starts from what Windows is set to.
bool reduced();
void setReduced (bool);
bool systemPrefersReduced();
// Stills: while frozen nothing is in flight, so a picture of the app shows only finished states.
void freeze (bool);
bool still(); // frozen, or "less motion": nothing is staged, everything is simply there

enum class Curve { out, inOut, linear };
float ease (Curve, float t);

// A number that glides to wherever it is sent, from wherever it is at that moment.
class Glide
{
public:
    explicit Glide (float v = 0.0f) : from (v), goal (v) {}
    float get() const;
    float target() const noexcept { return goal; }
    bool moving() const;
    // `fade` moves (opacity, colour) are kept short under "less motion"; all others become cuts.
    void to (float v, int ms, Curve c = Curve::out, bool fade = false);
    void snap (float v) { from = goal = v; length = 0; }

private:
    float from, goal;
    double start = 0.0;
    int length = 0;
    Curve curve = Curve::out;
};

// For the rare staged moment (the end of setup): how far along a move is that starts `delay` ms
// after `since` (a reading of now()) and takes `ms`. 0 before, 1 after, and always 1 when still().
float after (double since, int delay, int ms, Curve c = Curve::out);

// Repaints `c` on every frame for as long as `moving` says so, then once more.
// Safe to call again and again for the same component: the newest call wins.
void run (juce::Component& c, std::function<bool()> moving, std::function<void()> onFrame = {});

// The display's frame tick is borrowed from a component that is on screen (the editor).
void attach (juce::Component& home);
void detach (juce::Component& home);

// How a panel arrives.
enum class Enter
{
    fade,   // a page that replaces another: it only fades
    rise,   // a full page laid over the rig: fades in, rising 6 px
    card,   // a card in the middle: fades in from 98 % size
    drop    // something that hangs off a button: fades in from 97 %, growing from its top edge
};
void enter (juce::Component&, Enter);
// A picture of `c` as it looks now is left where it stands and fades away, while `c` itself is
// free to be hidden or changed at once. Clicks go to what is really there.
void ghost (juce::Component& c, int ms = leave);
// `justAbove`: the sibling the picture lies directly on top of; none means on top of everything.
void ghost (juce::Component& parent, juce::Rectangle<int> area, int ms = leave, juce::Component* justAbove = nullptr);

// Gives a component its entrance and exit by itself, whenever it is shown or hidden.
class Presence : private juce::ComponentListener
{
public:
    Presence (juce::Component&, Enter);
    ~Presence() override;
    bool enabled = true;

private:
    void componentVisibilityChanged (juce::Component&) override;
    juce::Component& comp;
    Enter kind;
    bool wasVisible;
};

// Hover and press of a button, kept with the button itself. Made on first use.
struct Feel : juce::ReferenceCountedObject
{
    Glide hover, down, on;
    bool primed = false;
    static Feel& of (juce::Component&);
    // feeds the button's state in, keeps it repainting while anything glides
    void follow (juce::Component&, bool over, bool pressed, bool toggled);
    float scale() const { return 1.0f - 0.03f * down.get(); }
};

// Preset loads, undo and redo announce themselves, so knobs know to travel to their new
// place and not jump. Anything else that moves a knob (a foot pedal, a DAW) is followed 1:1.
void announceJump();
bool jumpJustHappened();
} // namespace rh::ui::motion
