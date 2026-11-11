// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rh::ui
{
// One place for every colour and font, so the whole app looks like one thing.
namespace colours
{
    // Set by setLook(). Read at paint time, so a look change only needs a repaint.
    inline juce::Colour bg        { 0xff121212 };
    inline juce::Colour bgTop     { 0xff121318 };
    inline juce::Colour panel     { 0xff16181d };
    inline juce::Colour raised    { 0xff1e2128 };
    inline juce::Colour raisedHi  { 0xff262a33 };
    inline juce::Colour line      { 0xff2a2e37 };
    inline juce::Colour well      { 0xff0a0b0d };
    inline juce::Colour text      { 0xfff1f1f3 };
    inline juce::Colour textDim   { 0xff9aa0ac };
    inline juce::Colour textFaint { 0xff5e6470 };
    inline juce::Colour accent    { 0xffff9f43 };
    inline juce::Colour accentHi  { 0xffffc56e };
    inline juce::Colour accentInk { 0xff0d0e11 };
    inline juce::Colour good      { 0xff4ade80 };
    inline juce::Colour warn      { 0xfffacc15 };
    inline juce::Colour hot       { 0xffffb020 }; // clipping and limiter warnings: amber, never red
    inline juce::Colour record    { 0xffe5483a }; // the record light only: the one red thing in the app
    inline juce::Colour lamp      { 0xffffa31a }; // pilot lights: the logo's jewel, on lights, the unsaved mark
    inline juce::Colour loud      { 0xff0f6b66 }; // pedalboard look: the one loud pedal
    inline juce::Colour silk      { 0xffece4cf }; // pedalboard look: silk-screen lettering
} // namespace colours

// Three visual directions were tried. Studio is the one in use: gear on a dark stage. Studio is
// the plain modern look; Pedalboard draws every module as a stomp box.
enum class Look { studio, signal, pedalboard };
inline constexpr Look defaultLook = Look::studio;
void setLook (Look);
Look look() noexcept;
juce::String lookName (Look);

// One colour per guitar string (low E to high e).
juce::Colour stringColour (int string);

// Literal text with non-ASCII characters (like the middle dot) must go through this.
inline juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

enum class Weight { regular, medium, semibold, display, numeric };

// Titles: lowercase silk lettering in the pedalboard look, as written otherwise.
juce::String titleCase (const juce::String&);

juce::Font font (float height, Weight w = Weight::regular);

// Hardware-style labels: small capitals with a little air between the letters.
juce::Font capsFont (float height, float tracking = 0.09f);
float capsWidth (const juce::String&, float height, float tracking = 0.09f);
// The Riffhouse mark: a roof over the letter H, with the amp's pilot light in the gable.
// Drawn in `ink` inside the largest square that fits `area`.
void drawBrandMark (juce::Graphics&, juce::Rectangle<float> area, juce::Colour ink, bool simple = false);
// The app icon: the mark on a dark tile. `simple` is the version for very small sizes.
juce::Image renderAppIcon (int size, bool simple);

void drawCaps (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float height, juce::Colour,
               juce::Justification = juce::Justification::centredLeft, float tracking = 0.09f);

// The backdrop behind the gear: a dark room with one soft light.
juce::Image renderStage (int width, int height);

// A specific bundled face, whatever the look: for lettering printed on gear.
enum class Face { inter, interBold, instrument, instrumentSemi, doto, shoulders, script, brush, slab };
juce::Font faceFont (Face, float height);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void applyColours(); // call after setLook()

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 8; }

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (14.0f, Weight::medium); }
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;

    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override { return font (15.0f); }
    int getPopupMenuBorderSize() override { return 6; }

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;

    void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&, int w, int h, int titleSpaceX,
                                     int titleSpaceW, const juce::Image* icon, bool drawTitleTextOnLeft) override;
};

// Small vector icons drawn in code, so they stay sharp at any size.
namespace icons
{
    juce::Path chevronLeft();
    juce::Path chevronRight();
    juce::Path chevronDown();
    juce::Path star (bool filled);
    juce::Path save();
    juce::Path tuningFork();
    juce::Path metronome();
    juce::Path gear();
    juce::Path search();
    juce::Path close();
    juce::Path folder();
    juce::Path play();
    juce::Path pause();
    juce::Path stop();
    juce::Path headphones();
    juce::Path power();
    juce::Path guitar();
    juce::Path share();
    juce::Path music();
    juce::Path check();
    juce::Path undo();
    juce::Path redo();
    juce::Path loop();
} // namespace icons

// Surfaces
void drawPanel (juce::Graphics&, juce::Rectangle<float>, float radius);      // a grouped surface
void drawWell (juce::Graphics&, juce::Rectangle<float>, float radius);       // an inset display window
void drawEnclosure (juce::Graphics&, juce::Rectangle<float>, float radius, juce::Colour body, bool screws);

void fillIcon (juce::Graphics&, const juce::Path&, juce::Rectangle<float> area, juce::Colour);
void strokeIcon (juce::Graphics&, const juce::Path&, juce::Rectangle<float> area, juce::Colour, float thickness = 1.8f);
} // namespace rh::ui
