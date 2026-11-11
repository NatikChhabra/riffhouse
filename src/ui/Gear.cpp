// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Gear.h"

namespace rh::ui
{
namespace
{
    using C = juce::Colour;
    using R = juce::Rectangle<float>;

    // fixed seeds: the same box always gets the same flecks and grain
    juce::Random seeded (juce::int64 seed) { return juce::Random (seed); }

    C mix (C a, C b, float t) { return a.interpolatedWith (b, t); }

    void fillMetal (juce::Graphics& g, R r, C base, float radius, bool vertical = true)
    {
        juce::ColourGradient grad (base.brighter (0.45f), r.getX(), r.getY(), base.darker (0.35f),
                                   vertical ? r.getX() : r.getRight(), vertical ? r.getBottom() : r.getY(), false);
        grad.addColour (0.45, base.brighter (0.1f));
        grad.addColour (0.55, base.darker (0.1f));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, radius);
    }

    void screw (juce::Graphics& g, juce::Point<float> c, float size, float turn)
    {
        const auto r = R (size, size).withCentre (c);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (r.translated (0.0f, 0.8f).expanded (0.6f));
        g.setGradientFill (juce::ColourGradient (C (0xffc9ccc8), r.getX(), r.getY(), C (0xff4a4d4b), r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        juce::Path slot;
        slot.addRectangle (-size * 0.36f, -size * 0.07f, size * 0.72f, size * 0.14f);
        g.setColour (C (0xff2a2c2b));
        g.fillPath (slot, juce::AffineTransform::rotation (turn).translated (c));
    }

    // grain, flecks and threads ------------------------------------------------
    void finishTexture (juce::Graphics& g, R r, Finish f, C body, juce::int64 seed)
    {
        auto rnd = seeded (seed);
        const float area = r.getWidth() * r.getHeight();
        switch (f)
        {
            case Finish::enamel:
                break; // smooth baked paint: its gloss is added with the lighting
            case Finish::hammertone:
                for (int i = 0; i < (int) (area / 70.0f); ++i)
                {
                    const float d = 2.0f + rnd.nextFloat() * 5.0f;
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    const bool light = rnd.nextBool();
                    g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f + rnd.nextFloat() * 0.06f));
                    g.fillEllipse (x, y, d, d * (0.7f + rnd.nextFloat() * 0.5f));
                }
                break;
            case Finish::sparkle:
                for (int i = 0; i < (int) (area / 9.0f); ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    const float a = rnd.nextFloat();
                    g.setColour ((a > 0.85f ? juce::Colours::white : body.brighter (0.9f)).withAlpha (0.12f + a * 0.45f));
                    const float d = a > 0.93f ? 1.6f : 0.9f;
                    g.fillRect (x, y, d, d);
                }
                break;
            case Finish::brushed:
                for (float y = r.getY(); y < r.getBottom(); y += 0.5f)
                {
                    const bool light = rnd.nextBool();
                    g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.02f + rnd.nextFloat() * 0.05f));
                    const float x0 = r.getX() + rnd.nextFloat() * r.getWidth() * 0.3f;
                    g.fillRect (x0, y, r.getWidth() * (0.5f + rnd.nextFloat() * 0.5f), 0.5f);
                }
                break;
            case Finish::crinkle:
                for (int i = 0; i < (int) (area / 3.0f); ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f + rnd.nextFloat() * 0.07f));
                    g.fillRect (x, y, 1.2f, 1.2f);
                }
                break;
            case Finish::anodised:
                for (int i = 0; i < (int) (area / 6.0f); ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    g.setColour (juce::Colours::white.withAlpha (0.015f + rnd.nextFloat() * 0.025f));
                    g.fillRect (x, y, 0.8f, 0.8f);
                }
                break;
        }
    }

    // One light for every piece of gear: above and to the left. Angles run clockwise from straight up.
    constexpr float lightAngle = -juce::MathConstants<float>::pi * 0.25f;
    inline float lit (float a) { return 0.5f + 0.5f * std::cos (a - lightAngle); } // 1 facing the light, 0 facing away

    // A ring (a disc when inner is 0) whose colour changes with the angle around it. This is what
    // makes a turned part look turned: light wraps round it instead of lying flat across it.
    template <typename Fn>
    void conic (juce::Graphics& g, juce::Point<float> c, float outer, float inner, Fn&& colourAt, int wedges)
    {
        const float step = juce::MathConstants<float>::twoPi / (float) wedges;
        for (int i = 0; i < wedges; ++i)
        {
            const float a0 = step * (float) i, a1 = step * ((float) i + 1.6f); // overlap the next wedge so no seam shows
            juce::Path p;
            p.startNewSubPath (c.getPointOnCircumference (outer, a0));
            p.lineTo (c.getPointOnCircumference (outer, a1));
            if (inner > 0.01f)
            {
                p.lineTo (c.getPointOnCircumference (inner, a1));
                p.lineTo (c.getPointOnCircumference (inner, a0));
            }
            else
                p.lineTo (c);
            p.closeSubPath();
            g.setColour (colourAt (step * ((float) i + 0.5f)));
            g.fillPath (p);
        }
        if (inner <= 0.01f)
        {
            // where every wedge meets, the points pile up: cap the pivot with the mid tone
            g.setColour (colourAt (lightAngle + juce::MathConstants<float>::halfPi * 0.5f));
            g.fillEllipse (R (outer * 0.14f, outer * 0.14f).withCentre (c));
        }
    }

    // Spun or polished metal seen face on: light and dark fans that stay put while the part turns.
    C metalAt (C base, float a)
    {
        const float d = a - lightAngle;
        const float v = juce::jlimit (0.0f, 1.0f, 0.5f + 0.36f * std::cos (2.0f * d) + 0.14f * std::cos (4.0f * d + 0.7f));
        return base.darker (0.8f).interpolatedWith (base.brighter (1.2f), v);
    }

    // A hex nut with a washer face: each flat takes its own light.
    void hexNut (juce::Graphics& g, juce::Point<float> c, float radius)
    {
        juce::Path hex;
        hex.addPolygon (c, 6, radius, 0.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), std::max (2, (int) (radius * 0.28f)), { 1, std::max (1, (int) (radius * 0.12f)) }).drawForPath (g, hex);
        g.setColour (C (0xff8e9296));
        g.fillPath (hex);
        for (int i = 0; i < 6; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / 6.0f, a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / 6.0f;
            juce::Path f;
            f.addTriangle (c, c.getPointOnCircumference (radius, a0), c.getPointOnCircumference (radius, a1));
            g.setColour (metalAt (C (0xffa3a7ab), (a0 + a1) * 0.5f));
            g.fillPath (f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.strokePath (hex, juce::PathStrokeType (0.8f));
        conic (g, c, radius * 0.8f, 0.0f, [] (float a) { return metalAt (C (0xffbfc3c6), a); }, 72);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawEllipse (R (radius * 1.6f, radius * 1.6f).withCentre (c), 0.8f);
    }

    // A quarter-inch jack socket in a panel: nut, washer and the dark hole.
    void jackSocket (juce::Graphics& g, juce::Point<float> c, float radius)
    {
        hexNut (g, c, radius);
        const auto hole = R (radius * 0.86f, radius * 0.86f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (C (0xff050505), hole.getX(), hole.getY(), C (0xff2a2b2c), hole.getRight(), hole.getBottom(), false));
        g.fillEllipse (hole);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (hole.expanded (1.2f), 0.8f);
    }

    // A pressed metal corner protector on a cabinet, held by two nails.
    void cabinetCorner (juce::Graphics& g, R h, bool right, bool bottom, float s)
    {
        const float x = right ? h.getRight() - s : h.getX(), y = bottom ? h.getBottom() - s : h.getY();
        juce::Path corner;
        corner.addRoundedRectangle (x, y, s, s, s * 0.28f, s * 0.28f, ! right && ! bottom, right && ! bottom, ! right && bottom, right && bottom);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 5, { 1, 2 }).drawForPath (g, corner);
        juce::ColourGradient grad (C (0xffeceeef), x, y, C (0xff5d6164), x + s, y + s, false);
        grad.addColour (0.45, C (0xffb4b8bb));
        grad.addColour (0.6, C (0xff8d9195));
        g.setGradientFill (grad);
        g.fillPath (corner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.6f), x, y, juce::Colours::black.withAlpha (0.5f), x + s, y + s, false));
        g.strokePath (corner, juce::PathStrokeType (1.0f));
        // the raised rib pressed into the middle, and a nail on each leg
        const juce::Point<float> mid (x + s * 0.5f, y + s * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillEllipse (R (s * 0.42f, s * 0.42f).withCentre (mid.translated (-0.6f, -0.6f)));
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillEllipse (R (s * 0.42f, s * 0.42f).withCentre (mid.translated (0.8f, 0.8f)));
        g.setColour (C (0xffa9adb0));
        g.fillEllipse (R (s * 0.40f, s * 0.40f).withCentre (mid));
        for (auto n : { juce::Point<float> (x + s * (right ? 0.22f : 0.78f), y + s * (bottom ? 0.8f : 0.2f)),
                        juce::Point<float> (x + s * (right ? 0.8f : 0.2f), y + s * (bottom ? 0.22f : 0.78f)) })
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (R (5.0f, 5.0f).withCentre (n.translated (0.4f, 0.6f)));
            g.setGradientFill (juce::ColourGradient (C (0xfff3f4f4), n.x - 1.5f, n.y - 1.5f, C (0xff6a6e71), n.x + 2.0f, n.y + 2.0f, true));
            g.fillEllipse (R (4.0f, 4.0f).withCentre (n));
        }
    }

    // Paint chipped to bare metal at one corner and scuffed by a boot near the switch. How worn a
    // box is, and which corner took the knocks, comes from its own seed, so it never changes.
    void pedalWear (juce::Graphics& g, R b, const PedalLayout& l, C body, juce::int64 seed)
    {
        auto rnd = seeded (seed);
        const int level = rnd.nextInt (3); // 0 looked after, 1 gigged, 2 road worn
        const bool lightBody = body.getPerceivedBrightness() > 0.55f;
        const C bare = lightBody ? C (0xff84888b) : C (0xffa4a8ab);
        auto chip = [&] (juce::Point<float> c, float size)
        {
            juce::Path p;
            const int n = 5 + rnd.nextInt (4);
            for (int i = 0; i < n; ++i)
            {
                const float a = juce::MathConstants<float>::twoPi * (float) i / (float) n;
                const auto v = c.getPointOnCircumference (size * (0.45f + rnd.nextFloat() * 0.75f), a);
                if (i == 0) p.startNewSubPath (v); else p.lineTo (v);
            }
            p.closeSubPath();
            g.setColour (juce::Colours::black.withAlpha (0.35f)); // the paint's thickness, in shadow
            g.fillPath (p, juce::AffineTransform::translation (-0.5f, -0.6f));
            g.setColour (bare.withAlpha (0.9f));
            g.fillPath (p);
        };
        // the corner that took the knocks, plus a few strays along the bottom edge
        const bool right = rnd.nextBool(), bottom = rnd.nextInt (4) != 0;
        const juce::Point<float> corner (right ? b.getRight() : b.getX(), bottom ? b.getBottom() : b.getY());
        const int chips = 2 + level * 5;
        for (int i = 0; i < chips; ++i)
        {
            const float along = rnd.nextFloat() * rnd.nextFloat() * 46.0f + 3.0f;
            const bool onSide = rnd.nextBool();
            const float inset = 1.5f + rnd.nextFloat() * 3.0f;
            const juce::Point<float> p (corner.x + (right ? -1.0f : 1.0f) * (onSide ? inset : along),
                                        corner.y + (bottom ? -1.0f : 1.0f) * (onSide ? along : inset));
            chip (p, 1.0f + rnd.nextFloat() * (1.2f + (float) level));
        }
        for (int i = 0; i < level * 2; ++i)
            chip ({ b.getX() + rnd.nextFloat() * b.getWidth(), b.getBottom() - 1.5f - rnd.nextFloat() * 2.5f }, 0.8f + rnd.nextFloat() * 1.4f);
        // boot scuffs: fine scratches fanning out around the switch
        const auto sc = l.footswitch.getCentre();
        const float sr = l.footswitch.getWidth();
        for (int i = 0; i < 6 + level * 14; ++i)
        {
            const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
            const auto p0 = sc.getPointOnCircumference (sr * (0.75f + rnd.nextFloat() * 0.9f), a);
            const float len = 4.0f + rnd.nextFloat() * 14.0f, dir = a + 1.2f + rnd.nextFloat() * 0.8f;
            g.setColour ((lightBody ? juce::Colours::black : juce::Colours::white).withAlpha (0.03f + rnd.nextFloat() * 0.06f));
            g.drawLine (juce::Line<float> (p0, p0.getPointOnCircumference (len, dir)), 0.6f);
        }
    }

    // the lettering fitted to a width
    void letter (juce::Graphics& g, const juce::String& text, R area, juce::Font f, C ink, juce::Justification j, bool raised = false)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, text);
        if (w > area.getWidth()) f.setHeight (f.getHeight() * area.getWidth() / w);
        g.setFont (f);
        if (raised)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawText (text, area.translated (0.0f, 1.5f), j, false);
        }
        g.setColour (ink);
        g.drawText (text, area, j, false);
    }
} // namespace

//==============================================================================
const PedalDesign& drivePedalDesign (int t)
{
    // body, ink, led, finish, knob, knob cap, face, upper, width, stripe
    static const PedalDesign d[] = {
        { "clean boost", "plus 20 dB of clean", 0xffaeb3b5, 0xff16191b, 0xffd9f1ff, Finish::brushed,    KnobStyle::dome,    0xff16191b, Face::instrumentSemi, false, 0.86f, 0, 1 },
        { "TREBLE BOOSTER", "range 1", 0xff5f6f78, 0xfff1ead8, 0xffffb547, Finish::hammertone, KnobStyle::chicken, 0xffece4cf, Face::shoulders,      true,  0.92f, 0, 2 },
        { "GREEN DRIVE", "mid overdrive", 0xff2e6a42, 0xfff0e8cc, 0xff7dffa0, Finish::enamel,     KnobStyle::pedal,   0xfff0e8cc, Face::shoulders,      true,  1.0f,  1 },
        { "blues drive", "low gain", 0xff1d4f9a, 0xffe9eef5, 0xff7fc4ff, Finish::sparkle,    KnobStyle::dome,    0xff1b1e24, Face::instrumentSemi, false, 1.0f,  0, 1 },
        { "RODENT", "distortion", 0xff1c1d1e, 0xfff2f0e8, 0xffffb547, Finish::crinkle,    KnobStyle::pedal,   0xfff2f0e8, Face::shoulders,      true,  1.0f,  0, 2 },
        { "big fuzz", "sustainer", 0xffe6dbc0, 0xff3b2a1a, 0xffffb547, Finish::enamel,     KnobStyle::skirted, 0xff1e1a16, Face::shoulders,      false, 1.28f, 3, 3 },
        { "METAL BOX", "high gain", 0xff34373b, 0xfff4a524, 0xfff3f3ee, Finish::crinkle,    KnobStyle::skirted, 0xff101112, Face::shoulders,      true,  1.0f,  2 },
    };
    return d[juce::jlimit (0, (int) std::size (d) - 1, t)];
}

const PedalDesign* pedalDesignFor (int module, int type)
{
    // name, kind, body, ink, led, finish, knob, knob cap, face, upper, width, stripe
    static const PedalDesign gate   { "SILENCER", "noise gate", 0xff3e4537, 0xfff0e8cc, 0xff7dffa0, Finish::hammertone, KnobStyle::pedal, 0xfff0e8cc, Face::shoulders, true, 0.95f, 0, 2 };
    static const PedalDesign comp   { "squeeze", "compressor", 0xff3f8fc4, 0xfff7f7f2, 0xffd9f1ff, Finish::enamel, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 1.22f, 0, 3 };
    static const PedalDesign wah    { "FUNK FILTER", "wah and envelope", 0xffd9a521, 0xff2a1d10, 0xff7dffa0, Finish::enamel, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, true, 1.0f, 1 };
    static const PedalDesign pitch  { "deep octave", "pitch shifter", 0xff5b3c8f, 0xffeae6f4, 0xffd9c4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 0.95f, 0, 1 };
    static const PedalDesign eq     { "GRAPHIC EQ", "seven band", 0xff18191b, 0xffeceae4, 0xff7dffa0, Finish::crinkle, KnobStyle::pedal, 0xffeceae4, Face::shoulders, true, 1.5f, 0 };
    static const PedalDesign lofi   { "dusty tape", "lo-fi machine", 0xff6b5a44, 0xfff1ead8, 0xffffb547, Finish::hammertone, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, false, 1.18f, 0 };
    static const PedalDesign mods[] = {
        { "liquid chorus", "chorus", 0xff6fc3b2, 0xff10302a, 0xffd9f1ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::instrumentSemi, false, 1.0f, 0, 1 },
        { "JET FLANGER", "flanger", 0xffaeb3b5, 0xff2e2350, 0xffd9c4ff, Finish::brushed, KnobStyle::pedal, 0xfff7f7f2, Face::shoulders, true, 1.0f, 0, 2 },
        { "PHASE FOUR", "phaser", 0xffb07a2a, 0xff1d140a, 0xffffb547, Finish::hammertone, KnobStyle::pedal, 0xfff1ead8, Face::shoulders, true, 0.9f, 0 },
        { "tremble", "tremolo", 0xffe6dbc0, 0xff1d1a16, 0xffffb547, Finish::enamel, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, false, 1.0f, 0, 2 },
        { "warble", "vibrato", 0xff1f4e5f, 0xffe6edf0, 0xff7fc4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 1.0f, 0, 1 },
        { "ROTARY VIBE", "vibe", 0xff4a3426, 0xfff1ead8, 0xffffb547, Finish::crinkle, KnobStyle::skirted, 0xff141516, Face::shoulders, true, 1.25f, 3, 3 },
    };
    static const PedalDesign delays[] = {
        { "digital echo", "delay", 0xffeceae4, 0xff16181a, 0xff7fc4ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::instrumentSemi, false, 1.18f, 0 },
        { "BUCKET DELAY", "analog delay", 0xff4c5a4f, 0xfff0e8cc, 0xffffb547, Finish::hammertone, KnobStyle::pedal, 0xfff0e8cc, Face::shoulders, true, 1.18f, 0, 3 },
        { "TAPE ECHO", "tape delay", 0xff8a6a48, 0xfff1ead8, 0xffffb547, Finish::crinkle, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, true, 1.3f, 3 },
        { "ping pong", "stereo delay", 0xff2d3e5c, 0xfff1f3f6, 0xff7fc4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 1.18f, 0, 2 },
    };
    static const PedalDesign reverbs[] = {
        { "small room", "reverb", 0xff5d7f99, 0xfff4f6f8, 0xffd9f1ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::instrumentSemi, false, 1.18f, 0, 2 },
        { "CONCERT HALL", "reverb", 0xff151517, 0xffd8b04a, 0xffffb547, Finish::sparkle, KnobStyle::hat, 0xffd8b04a, Face::shoulders, true, 1.18f, 0 },
        { "plate", "reverb", 0xffaeb3b5, 0xff16191b, 0xff7fc4ff, Finish::brushed, KnobStyle::dome, 0xff16191b, Face::instrumentSemi, false, 1.18f, 0, 3 },
        { "SPRING TANK", "reverb", 0xff2a2a2c, 0xffd0d4d8, 0xffffb547, Finish::crinkle, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, true, 1.3f, 1 },
        { "shimmer", "octave reverb", 0xffe8e6ef, 0xff4a3c78, 0xffd9c4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 1.18f, 0, 1 },
        { "cloud", "ambient reverb", 0xffb8cbe0, 0xff17243a, 0xffd9f1ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::instrumentSemi, false, 1.18f, 0, 3 },
    };
    auto pick = [type] (const PedalDesign* arr, int n) { return &arr[juce::jlimit (0, n - 1, type)]; };
    switch (module)
    {
        case 0:  return &gate;
        case 1:  return &comp;
        case 2:  return &wah;
        case 3:  return &pitch;
        case 4:  return &drivePedalDesign (type);
        case 7:  return &eq;
        case 8:  return pick (mods, (int) std::size (mods));
        case 9:  return pick (delays, (int) std::size (delays));
        case 10: return pick (reverbs, (int) std::size (reverbs));
        case 11: return &lofi;
        default: return nullptr;
    }
}

const AmpDesign& ampDesign (int m)
{
    // badge, tolex, tolex colour, panel, ink, weave, piping, knob, cap, jewel, face, plate, upper, display
    static const AmpDesign d[] = {
        { "glass",     Tolex::levant, 0xff151617, Panel::black,    0xffeeeeea, Weave::silver,     0xffc9ccd0, KnobStyle::skirted, 0xff141516, 0xff6fb7ff, Face::instrumentSemi, false, false },
        { "tweed",     Tolex::tweed,  0xffc9a66b, Panel::chrome,   0xff1b1a18, Weave::wheat,      0,          KnobStyle::pedal,   0xfff1ead8, 0xffffb547, Face::instrumentSemi, false, false },
        { "chime",     Tolex::cream,  0xffd9c9a6, Panel::brushed,  0xff2b1f14, Weave::diamond,    0xffcbb48a, KnobStyle::chicken, 0xfff1ead8, 0xffffc35a, Face::shoulders,      true,  false },
        { "PLEXI",     Tolex::levant, 0xff131313, Panel::gold,     0xff17130c, Weave::saltPepper, 0xffe9e4d6, KnobStyle::hat,     0xffd8b04a, 0xffffb547, Face::shoulders,      true,  true  },
        { "EIGHT HUNDRED", Tolex::levant, 0xff121212, Panel::gold, 0xff17130c, Weave::black,      0xffd8b04a, KnobStyle::hat,     0xffd8b04a, 0xffffb547, Face::shoulders,      true,  true  },
        { "hot rod",   Tolex::levant, 0xff141414, Panel::chrome,   0xff111214, Weave::black,      0xffb9bdc2, KnobStyle::dome,    0xff1b1d20, 0xff6fb7ff, Face::instrumentSemi, false, false },
        { "MODERN",    Tolex::levant, 0xff101010, Panel::plate,    0xffe9e7e2, Weave::mesh,       0,          KnobStyle::skirted, 0xff101112, 0xff7dffa0, Face::shoulders,      true,  true  },
        { "TIGHT",     Tolex::smooth, 0xff0d0d0e, Panel::anodised, 0xfff4a524, Weave::mesh,       0,          KnobStyle::pedal,   0xfff4a524, 0xfff4a524, Face::shoulders,      false, true  },
        { "bass station", Tolex::levant, 0xff1d2530, Panel::brushed, 0xff141618, Weave::grid,     0xff8fa3b8, KnobStyle::skirted, 0xff141618, 0xff6fb7ff, Face::instrumentSemi, true,  false },
        { "studio direct", Tolex::smooth, 0xff1a1b1d, Panel::brushed, 0xff141618, Weave::none,    0,          KnobStyle::dome,    0xff141618, 0xff7dffa0, Face::instrumentSemi, false, false, true },
        { "capture",   Tolex::smooth, 0xff141518, Panel::anodised, 0xffe9e7e2, Weave::none,       0,          KnobStyle::modern,  0xffe9e7e2, 0xffff9f1c, Face::instrumentSemi, false, false, true },
    };
    return d[juce::jlimit (0, (int) std::size (d) - 1, m)];
}

//==============================================================================
void drawGearKnob (juce::Graphics& g, R area, float angle, float start, float end, KnobStyle style, C cap, C ink, bool enabled, bool hot, bool numbers)
{
    const float size = std::min (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    numbers = numbers && size >= 64.0f;
    // printed scale on the panel around the knob: numerals on an amp, eleven dots on a pedal
    if (ink.getAlpha() > 0)
    {
        const float rScale = size * 0.5f - (numbers ? 6.0f : 3.0f);
        g.setColour (ink.withAlpha (enabled ? 0.85f : 0.35f));
        if (numbers) g.setFont (faceFont (Face::instrumentSemi, juce::jlimit (7.0f, 10.5f, size * 0.088f)));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = start + (end - start) * (float) i / 10.0f;
            const auto p = c.getPointOnCircumference (rScale, a);
            if (numbers)
                g.drawText (juce::String (i), R (20.0f, 12.0f).withCentre (p), juce::Justification::centred, false);
            else
            {
                const float d = (i == 0 || i == 10 || i == 5) ? 3.0f : 2.0f;
                g.fillEllipse (R (d, d).withCentre (p));
            }
        }
    }
    const float r = size * 0.5f - (numbers ? 15.0f : 9.0f);
    const auto body = R (r * 2.0f, r * 2.0f).withCentre (c);
    {
        // the knob stands proud of the panel: a long soft shadow away from the light,
        // and a tight dark one where it meets the panel
        juce::Path s;
        s.addEllipse (body.translated (r * 0.10f, r * 0.16f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), std::max (2, (int) (r * 0.4f)), { (int) (r * 0.05f), (int) (r * 0.1f) }).drawForPath (g, s);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (body.expanded (1.2f).translated (0.4f, 1.0f));
    }
    auto rot = juce::AffineTransform::rotation (angle).translated (c);
    const float alpha = enabled ? 1.0f : 0.5f;
    const C cream (0xfff1ead8);

    // turned plastic: one broad highlight toward the light and a faint bounce opposite it
    auto plastic = [] (C base, float gloss)
    {
        return [base, gloss] (float a)
        {
            const float v = lit (a);
            return base.darker (0.6f).interpolatedWith (base.brighter (gloss), v * v)
                       .interpolatedWith (base.brighter (gloss * 0.5f), 0.22f * std::pow (1.0f - v, 5.0f));
        };
    };
    // the same, cut with grooves that turn with the knob
    auto ridged = [angle] (C base, float gloss, int n, float depth)
    {
        return [=] (float a)
        {
            const float v = lit (a);
            const auto col = base.darker (0.6f).interpolatedWith (base.brighter (gloss), v * v);
            const float groove = std::sin ((a - angle) * (float) n);
            return groove > 0.0f ? col.brighter (depth * groove * (0.4f + v)) : col.darker (-depth * groove * 1.6f);
        };
    };
    auto metal = [] (C base) { return [base] (float a) { return metalAt (base, a); }; };
    // the turned edge where a top face meets its side: bright toward the light, dark away from it
    auto chamfer = [&] (float radius, float width, float strength)
    {
        conic (g, c, radius, radius - width, [strength] (float a)
        {
            const float v = lit (a) * 2.0f - 1.0f;
            return (v > 0.0f ? juce::Colours::white : juce::Colours::black).withAlpha (strength * std::abs (v));
        }, 72);
    };
    auto topFace = [&] (float radius, C base, float gloss)
    {
        const auto b = R (radius * 2.0f, radius * 2.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (base.brighter (gloss), b.getX() + radius * 0.5f, b.getY() + radius * 0.4f,
                                                 base.darker (0.3f), b.getRight(), b.getBottom(), true));
        g.fillEllipse (b);
    };
    // a pointer line set into the surface: filled paint with a hair of shadow on its low side
    auto pointer = [&] (float from, float to, float w, C col)
    {
        juce::Path p;
        p.addRoundedRectangle (-w * 0.5f, -to, w, to - from, w * 0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (p, rot.translated (0.5f, 0.8f));
        g.setColour (col.withMultipliedAlpha (alpha));
        g.fillPath (p, rot);
    };
    auto contact = [&] (float radius, float strength)
    {
        // the shadow a raised part throws onto the part below it
        const auto b = R (radius * 2.0f, radius * 2.0f).withCentre (c).translated (radius * 0.07f, radius * 0.12f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (strength), b.getCentreX(), b.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.0f), b.getCentreX() + radius * 1.18f, b.getCentreY(), true));
        g.fillEllipse (b.expanded (radius * 0.18f));
    };
    const float lineW = std::max (1.8f, r * 0.075f);

    switch (style)
    {
        case KnobStyle::pedal:
        {
            // the everyday stomp box knob: a knurled grip and a flat top with a long line
            conic (g, c, r, r * 0.78f, ridged (C (0xff1c1d1f), 0.5f, 45, 0.32f), 180);
            topFace (r * 0.8f, C (0xff202224), 0.28f);
            chamfer (r * 0.81f, std::max (1.0f, r * 0.04f), 0.38f);
            pointer (r * 0.16f, r * 0.76f, lineW * 1.15f, cap);
            break;
        }
        case KnobStyle::skirted:
        {
            // amp knob: a flared skirt with the index line, a fluted grip, a spun metal disc in the top
            const bool darkCap = cap.getPerceivedBrightness() < 0.3f;
            const C skirt = darkCap ? C (0xff1b1c1e) : cap;
            conic (g, c, r, r * 0.56f, plastic (skirt, darkCap ? 0.5f : 0.7f), 96);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawEllipse (body.reduced (0.4f), 0.9f);
            pointer (r * 0.68f, r * 0.97f, lineW, darkCap ? cream : C (0xff151617));
            contact (r * 0.6f, 0.55f);
            conic (g, c, r * 0.6f, r * 0.36f, ridged (C (0xff18191b), 0.55f, 16, 0.55f), 128);
            conic (g, c, r * 0.4f, 0.0f, metal (C (0xffa7abaf)), 96);
            chamfer (r * 0.41f, std::max (1.0f, r * 0.035f), 0.5f);
            break;
        }
        case KnobStyle::chicken:
        {
            // chicken-head pointer: a round base and a long tapered beak, moulded in one glossy piece
            conic (g, c, r * 0.9f, 0.0f, plastic (C (0xff1d1b19), 0.45f), 72);
            juce::Path head;
            head.startNewSubPath (0.0f, -r * 1.02f);
            head.quadraticTo (r * 0.34f, -r * 0.2f, r * 0.26f, r * 0.55f);
            head.quadraticTo (0.0f, r * 0.72f, -r * 0.26f, r * 0.55f);
            head.quadraticTo (-r * 0.34f, -r * 0.2f, 0.0f, -r * 1.02f);
            head.closeSubPath();
            {
                auto moved = head;
                moved.applyTransform (rot);
                juce::DropShadow (juce::Colours::black.withAlpha (0.6f), std::max (2, (int) (r * 0.16f)), { (int) (r * 0.04f), (int) (r * 0.07f) }).drawForPath (g, moved);
            }
            {
                // rounded across its width: the ridge down the middle catches the light
                juce::Graphics::ScopedSaveState s (g);
                g.addTransform (rot);
                juce::ColourGradient grad (C (0xff0d0c0b), -r * 0.34f, 0.0f, C (0xff090808), r * 0.34f, 0.0f, false);
                grad.addColour (0.38, C (0xff4a4642));
                grad.addColour (0.5, C (0xff2b2927));
                g.setGradientFill (grad);
                g.fillPath (head);
                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.strokePath (head, juce::PathStrokeType (0.8f));
            }
            pointer (r * 0.42f, r * 0.96f, lineW * 0.9f, cap);
            break;
        }
        case KnobStyle::dome:
        {
            // a smooth dome: spun aluminium, or the box's own moulded colour
            const bool chrome = cap.getPerceivedBrightness() < 0.3f;
            if (chrome) conic (g, c, r, 0.0f, metal (C (0xffa9adb2)), 120);
            else        conic (g, c, r, 0.0f, plastic (cap, 0.7f), 96);
            // the curve of the dome: a soft bright patch toward the light, falling away at the rim
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (chrome ? 0.5f : 0.35f), c.x - r * 0.3f, c.y - r * 0.36f,
                                                     juce::Colours::white.withAlpha (0.0f), c.x + r * 0.5f, c.y + r * 0.5f, true));
            g.fillEllipse (body);
            juce::ColourGradient rim (juce::Colours::black.withAlpha (0.0f), c.x, c.y, juce::Colours::black.withAlpha (0.42f), c.x + r, c.y, true);
            rim.addColour (0.72, juce::Colours::black.withAlpha (0.0f));
            g.setGradientFill (rim);
            g.fillEllipse (body);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (body.reduced (0.4f), 0.9f);
            pointer (r * 0.42f, r * 0.9f, lineW * 1.1f, chrome ? C (0xff16181a) : cream);
            break;
        }
        case KnobStyle::hat:
        {
            // top hat: a ribbed black skirt with a brushed metal cap pressed into the crown
            conic (g, c, r, r * 0.5f, ridged (C (0xff191919), 0.5f, 24, 0.4f), 144);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawEllipse (body.reduced (0.4f), 0.9f);
            pointer (r * 0.62f, r * 0.97f, lineW, cream);
            contact (r * 0.54f, 0.5f);
            conic (g, c, r * 0.54f, 0.0f, metal (cap), 96);
            chamfer (r * 0.55f, std::max (1.0f, r * 0.045f), 0.45f);
            break;
        }
        case KnobStyle::modern:
        {
            // a plain satin knob for the rack gear and the bottom bar
            conic (g, c, r, r * 0.84f, plastic (C (0xff26282b), 0.55f), 96);
            topFace (r * 0.86f, C (0xff1e2023), 0.25f);
            chamfer (r * 0.87f, std::max (1.0f, r * 0.045f), 0.35f);
            pointer (r * 0.3f, r * 0.8f, lineW * 1.1f, cap);
            break;
        }
    }
    if (hot && enabled)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillEllipse (body);
    }
}

//==============================================================================
PedalLayout pedalLayout (R area, const PedalDesign& d, int numKnobs, bool faders)
{
    PedalLayout l;
    const float h = std::min (area.getHeight(), 420.0f);
    const float w = std::min (area.getWidth(), h * 0.64f * d.width);
    l.box = R (w, h).withCentre (area.getCentre());
    const auto b = l.box;
    const bool tall = faders || numKnobs > 4;
    l.variant = tall ? 0 : d.layout;
    const float fs = std::min (w * 0.27f, h * (tall ? 0.13f : 0.15f));
    auto at = [&] (float fx, float fy) { return juce::Point<float> (b.getX() + w * fx, b.getY() + h * fy); };
    const float knobH = faders ? 0.50f : (tall ? 0.42f : 0.32f);
    l.knobs = R (b.getX() + w * 0.06f, b.getY() + h * 0.045f, w * 0.88f, h * knobH);
    l.name = R (b.getX() + w * 0.08f, b.getY() + h * (tall ? 0.56f : 0.41f), w * 0.84f, h * (tall ? 0.13f : 0.17f));
    l.led = R (12.0f, 12.0f).withCentre (at (0.5f, tall ? 0.725f : 0.645f));
    l.footswitch = R (fs, fs).withCentre (at (0.5f, tall ? 0.855f : 0.81f));
    switch (l.variant)
    {
        case 1: // light at the top edge; a row of three becomes a triangle
            l.knobs = R (b.getX() + w * 0.06f, b.getY() + h * 0.085f, w * 0.88f, h * (numKnobs == 3 ? 0.40f : 0.30f));
            l.name = l.name.withY (b.getY() + h * (numKnobs == 3 ? 0.50f : 0.44f));
            l.led = l.led.withCentre (at (0.5f, 0.05f));
            l.footswitch = l.footswitch.withCentre (at (0.5f, 0.82f));
            break;
        case 2: // light up and to the left of the switch
            l.name = l.name.withY (b.getY() + h * 0.43f);
            l.led = l.led.withCentre (at (0.2f, 0.7f));
            break;
        case 3: // switch left of centre, light level with it on the right
            l.name = l.name.withY (b.getY() + h * 0.43f);
            l.footswitch = l.footswitch.withCentre (at (0.3f, 0.8f));
            l.led = l.led.withCentre (at (0.72f, 0.8f));
            break;
        default: break;
    }
    return l;
}

R pedalKnobCell (const PedalLayout& l, int i, int n)
{
    if (l.variant == 1 && n == 3)
    {
        // two up, one down between them
        const float cw = l.knobs.getWidth() / 3.0f, ch = l.knobs.getHeight() * 0.7f;
        return { l.knobs.getX() + cw * (float) i, l.knobs.getY() + (i == 1 ? l.knobs.getHeight() - ch : 0.0f), cw, ch };
    }
    const int rows = n > 4 ? 2 : 1;
    const int perRow = (n + rows - 1) / rows;
    const int row = i / perRow, col = i % perRow;
    const int inRow = row == rows - 1 ? n - perRow * (rows - 1) : perRow;
    const float cw = l.knobs.getWidth() / (float) perRow;
    const float ch = l.knobs.getHeight() / (float) rows;
    const float x0 = l.knobs.getX() + (l.knobs.getWidth() - cw * (float) inRow) * 0.5f;
    return { x0 + cw * (float) col, l.knobs.getY() + ch * (float) row, cw, ch };
}

juce::Image renderPedal (const PedalDesign& d, const PedalLayout& l, R area, float scale)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    const auto b = l.box;
    const float rad = b.getWidth() * 0.045f;
    const C body (d.body);

    // shadow on the floor: wide and soft away from the light, tight and dark right under the box
    juce::Path face;
    face.addRoundedRectangle (b, rad);
    {
        juce::Path p;
        p.addRoundedRectangle (b.translated (3.0f, 8.0f), rad);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 30, { 4, 12 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 7, { 1, 5 }).drawForPath (g, p);
    }
    // the box's side, seen below the top face
    g.setGradientFill (juce::ColourGradient (body.darker (0.55f), b.getX(), 0, body.darker (1.1f), b.getRight(), 0, false));
    g.fillRoundedRectangle (b.translated (0.0f, 6.0f), rad);
    g.setGradientFill (juce::ColourGradient (body.brighter (0.16f), b.getX(), b.getY(), body.darker (0.24f), b.getRight(), b.getBottom(), false));
    g.fillPath (face);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face);
        finishTexture (g, b, d.finish, body, (juce::int64) d.body * 31 + 7);
        // edges: light along the top, a darker band at the sides
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.22f), b.getX(), 0, juce::Colours::black.withAlpha (0.0f), b.getX() + 18.0f, 0, false));
        g.fillRect (b.withWidth (18.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), b.getRight() - 18.0f, 0, juce::Colours::black.withAlpha (0.25f), b.getRight(), 0, false));
        g.fillRect (b.withLeft (b.getRight() - 18.0f));

        const C ink (d.ink);
        if (d.stripe == 1)
        {
            g.setColour (ink.withAlpha (0.9f));
            g.fillRect (b.getX(), l.name.getY() - 10.0f, b.getWidth(), 2.0f);
            g.fillRect (b.getX(), l.name.getBottom() + 26.0f, b.getWidth(), 2.0f);
        }
        else if (d.stripe == 2)
        {
            // tread plate along the bottom edge
            const auto strip = b.withTop (b.getBottom() - b.getHeight() * 0.09f);
            fillMetal (g, strip, C (0xff8d9196), 0.0f);
            for (float x = strip.getX() - 20.0f; x < strip.getRight(); x += 14.0f)
                for (float y = strip.getY() + 4.0f; y < strip.getBottom(); y += 10.0f)
                {
                    juce::Path lozenge;
                    lozenge.addRoundedRectangle (-5.0f, -1.3f, 10.0f, 2.6f, 1.3f);
                    const bool alt = ((int) ((x + y) / 7.0f)) % 2 == 0;
                    g.setColour (juce::Colours::white.withAlpha (0.35f));
                    g.fillPath (lozenge, juce::AffineTransform::rotation (alt ? 0.8f : -0.8f).translated (x + (alt ? 0.0f : 7.0f), y));
                }
        }
        else if (d.stripe == 3)
        {
            g.setColour (ink.withAlpha (0.92f));
            g.fillRect (b.getX(), l.name.getY() - 6.0f, b.getWidth(), l.name.getHeight() + 12.0f);
        }
        // gloss paint picks up the room: a soft patch of light on the upper left, no hard edge
        if (d.finish == Finish::enamel || d.finish == Finish::sparkle)
        {
            const juce::Point<float> gc (b.getX() + b.getWidth() * 0.22f, b.getY() + b.getHeight() * 0.06f);
            juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.16f), gc.x, gc.y,
                                        juce::Colours::white.withAlpha (0.0f), gc.x + b.getWidth() * 0.9f, gc.y, true);
            gloss.addColour (0.5, juce::Colours::white.withAlpha (0.05f));
            g.setGradientFill (gloss);
            g.fillRect (b);
        }
        // a cast box has rolled edges: they catch the light on the top and left, and fall dark on the bottom and right
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.5f), b.getRight(), b.getBottom(), false));
        g.drawRoundedRectangle (b.reduced (1.2f), rad, 2.6f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.10f), b.getRight(), b.getBottom(), false));
        g.drawRoundedRectangle (b.reduced (3.4f), rad * 0.85f, 1.2f);
    }

    // lettering
    {
        const C ink (d.ink);
        const C letterInk = d.stripe == 3 ? body : ink;
        juce::String name (d.name);
        letter (g, name, l.name.withTrimmedBottom (l.name.getHeight() * 0.25f), faceFont (d.face, l.name.getHeight() * 0.62f),
                letterInk, juce::Justification::centred);
        letter (g, juce::String (d.kind), l.name.withTrimmedTop (l.name.getHeight() * 0.74f).translated (0.0f, d.stripe == 3 ? 2.0f : 4.0f),
                faceFont (Face::instrument, 12.5f), letterInk.withAlpha (0.8f), juce::Justification::centred);
    }

    // a little history: every box has been played, and each one shows it in its own place
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (face);
        pedalWear (g, b, l, body, (juce::int64) d.body * 13 + (juce::int64) d.ink);
    }

    // jacks on both sides (input on the right, output on the left, like real boxes),
    // a chrome bezel for the light and a nut under the switch
    for (bool right : { false, true })
    {
        const float jy = b.getY() + b.getHeight() * 0.22f;
        const auto nut = R (10.0f, 26.0f).withCentre ({ right ? b.getRight() + 4.0f : b.getX() - 4.0f, jy });
        fillMetal (g, nut, C (0xffb9bdc0), 2.0f, false);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        for (float y = nut.getY() + 4.0f; y < nut.getBottom() - 2.0f; y += 4.0f)
            g.fillRect (nut.getX(), y, nut.getWidth(), 1.0f);
    }
    // the light sits in a turned chrome bezel
    {
        const auto lc = l.led.getCentre();
        const float lr = l.led.getWidth() * 0.5f + 4.5f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (R (lr * 2.0f + 2.0f, lr * 2.0f + 2.0f).withCentre (lc.translated (0.6f, 1.2f)));
        conic (g, lc, lr, 0.0f, [] (float a) { return metalAt (C (0xffb4b8bb), a); }, 48);
        g.setColour (C (0xff0a0a0a));
        g.fillEllipse (l.led.expanded (1.0f));
    }
    hexNut (g, l.footswitch.getCentre(), l.footswitch.getWidth() * 0.62f);
    return img;
}

void drawPedalLed (juce::Graphics& g, const PedalLayout& l, const PedalDesign& d, bool on)
{
    const C led (d.led);
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (led.withAlpha (0.45f), l.led.getCentreX(), l.led.getCentreY(),
                                                 led.withAlpha (0.0f), l.led.getCentreX() + 22.0f, l.led.getCentreY(), true));
        g.fillEllipse (l.led.expanded (22.0f));
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colours::white : led.withMultipliedSaturation (0.4f).darker (1.6f),
                                             l.led.getCentreX() - 2.0f, l.led.getCentreY() - 2.0f,
                                             on ? led : C (0xff151515), l.led.getRight(), l.led.getBottom(), true));
    g.fillEllipse (l.led);
    // the lens is a little dome: one small bright point where the room's light lands
    g.setColour (juce::Colours::white.withAlpha (on ? 0.85f : 0.3f));
    g.fillEllipse (R (l.led.getWidth() * 0.26f, l.led.getWidth() * 0.2f).withCentre (l.led.getCentre().translated (-l.led.getWidth() * 0.17f, -l.led.getWidth() * 0.2f)));
}

//==============================================================================
AmpLayout ampLayout (R area)
{
    AmpLayout l;
    l.handle = R (200.0f, 18.0f).withCentre ({ area.getCentreX(), area.getY() + 9.0f });
    l.head = area.withTrimmedTop (13.0f).withTrimmedBottom (10.0f); // room for the handle above and the feet below
    const auto h = l.head;
    l.panel = R (h.getX() + 24.0f, h.getY() + 20.0f, h.getWidth() - 48.0f, std::max (128.0f, h.getHeight() * 0.42f));
    // the input jack sits left of the knobs, the pilot light right of them
    l.knobs = l.panel.reduced (18.0f, 8.0f).withTrimmedLeft (66.0f).withTrimmedRight (62.0f);
    l.jewel = R (24.0f, 24.0f).withCentre ({ l.panel.getRight() - 42.0f, l.knobs.getCentreY() - 11.0f });
    l.grille = R (h.getX() + 24.0f, l.panel.getBottom() + 16.0f, h.getWidth() - 48.0f, h.getBottom() - 22.0f - (l.panel.getBottom() + 16.0f));
    l.badge = R (l.grille.getX() + 34.0f, l.grille.getY() + 22.0f, 300.0f, std::min (70.0f, l.grille.getHeight() * 0.45f));
    return l;
}
namespace
{
    void tolexTexture (juce::Graphics& g, R r, const AmpDesign& d)
    {
        const C base (d.tolexColour);
        g.setColour (base);
        g.fillRect (r);
        auto rnd = seeded ((juce::int64) d.tolexColour + 3);
        const float area = r.getWidth() * r.getHeight();
        if (d.tolex == Tolex::tweed)
        {
            // diagonal twill: light and dark threads in alternating bands
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 3.0f)
            {
                const int band = (int) ((x - r.getX()) / 3.0f) % 4;
                g.setColour ((band < 2 ? base.brighter (0.25f) : base.darker (0.45f)).withAlpha (0.55f));
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.4f);
            }
            for (int i = 0; i < (int) (area / 30.0f); ++i)
            {
                g.setColour (C (0xff3b2a14).withAlpha (0.10f + rnd.nextFloat() * 0.1f));
                g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.5f, 1.5f);
            }
            // a lacquer yellowing toward the edges
            g.setGradientFill (juce::ColourGradient (C (0x00000000), r.getCentreX(), r.getCentreY(), C (0x40452c10), r.getX(), r.getY(), true));
            g.fillRect (r);
            return;
        }
        if (d.tolex == Tolex::smooth)
        {
            for (int i = 0; i < (int) (area / 8.0f); ++i)
            {
                g.setColour (juce::Colours::white.withAlpha (0.012f + rnd.nextFloat() * 0.02f));
                g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.0f, 1.0f);
            }
            return;
        }
        // levant grain: small pebbles, each lit from above
        const float step = 3.2f;
        for (float y = r.getY(); y < r.getBottom(); y += step)
            for (float x = r.getX(); x < r.getRight(); x += step)
            {
                const float jx = x + rnd.nextFloat() * step, jy = y + rnd.nextFloat() * step;
                const float s = 1.4f + rnd.nextFloat() * 1.6f;
                g.setColour (juce::Colours::white.withAlpha (d.tolex == Tolex::cream ? 0.10f : 0.045f));
                g.fillEllipse (jx, jy, s, s * 0.8f);
                g.setColour (juce::Colours::black.withAlpha (d.tolex == Tolex::cream ? 0.10f : 0.25f));
                g.fillEllipse (jx + 0.6f, jy + 0.9f, s * 0.8f, s * 0.6f);
            }
    }

    void panelFinish (juce::Graphics& g, R r, const AmpDesign& d)
    {
        juce::Path shape;
        shape.addRoundedRectangle (r, 4.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 10, { 0, 3 }).drawForPath (g, shape);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shape);
        switch (d.panel)
        {
            case Panel::black:
                g.setGradientFill (juce::ColourGradient (C (0xff26272a), r.getX(), r.getY(), C (0xff0d0e0f), r.getX(), r.getBottom(), false));
                g.fillRect (r);
                break;
            case Panel::chrome:
            {
                juce::ColourGradient grad (C (0xfff2f3f3), 0, r.getY(), C (0xff7b8086), 0, r.getBottom(), false);
                grad.addColour (0.35, C (0xffc6cacd));
                grad.addColour (0.5, C (0xff8f959a));
                grad.addColour (0.62, C (0xffdfe2e4));
                g.setGradientFill (grad);
                g.fillRect (r);
                break;
            }
            case Panel::gold:
            {
                juce::ColourGradient grad (C (0xffe9cf86), 0, r.getY(), C (0xff9c7a2c), 0, r.getBottom(), false);
                grad.addColour (0.45, C (0xffd2b05a));
                g.setGradientFill (grad);
                g.fillRect (r);
                finishTexture (g, r, Finish::brushed, C (0xffd2b05a), 11);
                break;
            }
            case Panel::brushed:
                g.setGradientFill (juce::ColourGradient (C (0xffd4d7d8), 0, r.getY(), C (0xff9da2a5), 0, r.getBottom(), false));
                g.fillRect (r);
                finishTexture (g, r, Finish::brushed, C (0xffc0c4c6), 12);
                break;
            case Panel::anodised:
                g.setGradientFill (juce::ColourGradient (C (0xff2e3135), 0, r.getY(), C (0xff1a1c1f), 0, r.getBottom(), false));
                g.fillRect (r);
                finishTexture (g, r, Finish::anodised, C (0xff26292c), 13);
                break;
            case Panel::plate:
            {
                fillMetal (g, r, C (0xff9ba0a5), 0.0f);
                for (float y = r.getY() + 6.0f; y < r.getBottom(); y += 16.0f)
                    for (float x = r.getX() + 6.0f; x < r.getRight(); x += 20.0f)
                    {
                        const bool alt = ((int) ((y - r.getY()) / 16.0f)) % 2 == 1;
                        juce::Path lozenge;
                        lozenge.addRoundedRectangle (-7.0f, -1.8f, 14.0f, 3.6f, 1.8f);
                        const auto t = juce::AffineTransform::rotation (alt ? -0.75f : 0.75f).translated (x + (alt ? 10.0f : 0.0f), y);
                        g.setColour (juce::Colours::black.withAlpha (0.25f));
                        g.fillPath (lozenge, t.translated (0.0f, 1.2f));
                        g.setColour (juce::Colours::white.withAlpha (0.45f));
                        g.fillPath (lozenge, t);
                    }
                // a black label strip where the controls sit
                g.setColour (C (0xf0141516));
                g.fillRoundedRectangle (r.reduced (10.0f, 12.0f), 4.0f);
                break;
            }
        }
        // the panel sits back under the cabinet's top edge, which shades its upper part
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.42f), 0, r.getY(), juce::Colours::black.withAlpha (0.0f), 0, r.getY() + 16.0f, false));
        g.fillRect (r.withHeight (16.0f));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawHorizontalLine ((int) r.getBottom() - 2, r.getX(), r.getRight());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());
    }

    void weave (juce::Graphics& g, R r, const AmpDesign& d)
    {
        auto rnd = seeded ((juce::int64) d.weave * 97 + 5);
        const float area = r.getWidth() * r.getHeight();
        switch (d.weave)
        {
            case Weave::silver:
            case Weave::wheat:
            case Weave::black:
            {
                const C base = d.weave == Weave::silver ? C (0xff2a2c2f) : d.weave == Weave::wheat ? C (0xff7d6136) : C (0xff131314);
                const C thread = d.weave == Weave::silver ? C (0xffa9adb2) : d.weave == Weave::wheat ? C (0xffc9a96c) : C (0xff4a4c50);
                g.setColour (base);
                g.fillRect (r);
                for (float x = r.getX(); x < r.getRight(); x += 2.5f)
                {
                    g.setColour (thread.withAlpha (0.14f + rnd.nextFloat() * 0.10f));
                    g.fillRect (x, r.getY(), 1.0f, r.getHeight());
                }
                for (float y = r.getY(); y < r.getBottom(); y += 2.5f)
                {
                    g.setColour (thread.withAlpha (0.10f + rnd.nextFloat() * 0.08f));
                    g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
                }
                if (d.weave == Weave::silver)
                    for (int i = 0; i < (int) (area / 420.0f); ++i)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.08f + rnd.nextFloat() * 0.16f));
                        g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.0f, 1.0f);
                    }
                break;
            }
            case Weave::diamond:
            {
                g.setColour (C (0xff3a2b1f));
                g.fillRect (r);
                const float p = 22.0f;
                for (float x = r.getX() - r.getHeight(); x < r.getRight() + r.getHeight(); x += p)
                {
                    g.setColour (C (0xffb08e5c).withAlpha (0.55f));
                    g.drawLine (x, r.getY(), x + r.getHeight(), r.getBottom(), 3.0f);
                    g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 3.0f);
                }
                for (int i = 0; i < (int) (area / 6.0f); ++i)
                {
                    g.setColour (juce::Colours::black.withAlpha (0.08f + rnd.nextFloat() * 0.1f));
                    g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.0f, 1.0f);
                }
                break;
            }
            case Weave::saltPepper:
                g.setColour (C (0xff1d1d1d));
                g.fillRect (r);
                for (int i = 0; i < (int) (area / 3.5f); ++i)
                {
                    const float v = rnd.nextFloat();
                    g.setColour (C (0xffe8e6e0).withAlpha (v * v * v * 0.45f));
                    g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.2f, 1.2f);
                }
                break;
            case Weave::mesh:
            {
                g.setColour (C (0xff2a2c2f));
                g.fillRect (r);
                const float p = 9.0f;
                int row = 0;
                for (float y = r.getY() + 4.0f; y < r.getBottom(); y += p * 0.866f, ++row)
                    for (float x = r.getX() + 4.0f + (row % 2) * p * 0.5f; x < r.getRight(); x += p)
                    {
                        g.setColour (C (0xff050506));
                        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
                        g.setColour (juce::Colours::white.withAlpha (0.10f));
                        g.drawEllipse (x - 3.0f, y - 2.5f, 6.0f, 6.0f, 0.6f);
                    }
                break;
            }
            case Weave::grid:
                g.setColour (C (0xff15171a));
                g.fillRect (r);
                for (float x = r.getX(); x < r.getRight(); x += 4.0f) { g.setColour (C (0xff2a2e33)); g.fillRect (x, r.getY(), 1.0f, r.getHeight()); }
                for (float y = r.getY(); y < r.getBottom(); y += 4.0f) { g.setColour (C (0xff2a2e33)); g.fillRect (r.getX(), y, r.getWidth(), 1.0f); }
                break;
            case Weave::none:
                break;
        }
        // light falls from above: brighter in the upper middle, darker at the edges
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getCentreX(), r.getY() + r.getHeight() * 0.25f,
                                                 juce::Colours::black.withAlpha (0.35f), r.getX(), r.getBottom(), true));
        g.fillRect (r);
        // the panel above casts a soft shadow onto the cloth
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), 0, r.getY(), juce::Colours::black.withAlpha (0.0f), 0, r.getY() + 22.0f, false));
        g.fillRect (r.withHeight (22.0f));
    }
} // namespace

juce::Image renderAmp (const AmpDesign& d, const AmpLayout& l, R area, float scale, const juce::String& displayText)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    const auto h = l.head;

    {
        juce::Path p;
        p.addRoundedRectangle (h.translated (0.0f, 8.0f), 10.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 30, { 0, 12 }).drawForPath (g, p);
    }
    // handle: a leather strap seen edge on, bowed up between two metal end caps
    {
        const auto s = l.handle;
        const float base = h.getY() + 1.0f, peak = s.getY() + 3.0f;
        juce::Path strap;
        strap.startNewSubPath (s.getX() + 6.0f, base - 3.0f);
        strap.quadraticTo (s.getCentreX(), 2.0f * peak - base + 3.0f, s.getRight() - 6.0f, base - 3.0f);
        g.setColour (C (0xff060505));
        g.strokePath (strap, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (C (0xff2a2521));
        g.strokePath (strap, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (C (0xff7a6f63).withAlpha (0.85f));
        g.strokePath (strap, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (0.0f, -2.4f));
        for (auto x : { s.getX() - 4.0f, s.getRight() - 18.0f })
        {
            const R capR (x, base - 9.0f, 22.0f, 10.0f);
            fillMetal (g, capR, C (0xffb6babd), 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (capR, 3.0f, 0.8f);
            screw (g, capR.getCentre(), 4.5f, 0.4f);
        }
    }
    juce::Path shell;
    shell.addRoundedRectangle (h, 10.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shell);
        tolexTexture (g, h, d);
        // the cabinet's rounded edges catch the light
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.12f), 0, h.getY(), juce::Colours::white.withAlpha (0.0f), 0, h.getY() + 14.0f, false));
        g.fillRect (h.withHeight (14.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), 0, h.getBottom() - 16.0f, juce::Colours::black.withAlpha (0.45f), 0, h.getBottom(), false));
        g.fillRect (h.withTop (h.getBottom() - 16.0f));
    }
    // rubber feet, then the metal corners
    for (auto x : { h.getX() + 64.0f, h.getRight() - 64.0f - 54.0f })
    {
        g.setGradientFill (juce::ColourGradient (C (0xff1c1c1c), 0, h.getBottom(), C (0xff070707), 0, h.getBottom() + 9.0f, false));
        g.fillRoundedRectangle (x, h.getBottom() - 2.0f, 54.0f, 10.0f, 3.0f);
    }
    for (int i = 0; i < 4; ++i)
        cabinetCorner (g, h, i % 2 == 1, i >= 2, 34.0f);

    panelFinish (g, l.panel, d);
    for (auto x : { l.panel.getX() + 10.0f, l.panel.getRight() - 10.0f })
        screw (g, { x, l.panel.getY() + 12.0f }, 6.0f, 0.6f);
    // what is printed and fitted on the panel beside the knobs: the input jack and the pilot light
    {
        const float cell = l.knobs.getWidth() / 8.0f;
        const float size = std::min (cell - 6.0f, l.knobs.getHeight() - 22.0f);
        const float labelY = l.knobs.getCentreY() + size * 0.5f - 11.0f;
        const auto f = faceFont (d.face == Face::shoulders ? Face::shoulders : Face::instrument, d.face == Face::shoulders ? 12.0f : 11.4f);
        auto label = [&] (const char* t, float cx)
        {
            const juce::String text = d.upper ? juce::String (t).toUpperCase() : juce::String (t);
            letter (g, text, R (80.0f, 16.0f).withCentre ({ cx, labelY + 8.0f }), f, C (d.ink), juce::Justification::centred);
        };
        const juce::Point<float> jack (l.panel.getX() + 52.0f, l.knobs.getCentreY() - 11.0f);
        jackSocket (g, jack, 15.0f);
        label ("input", jack.x);
        const auto jc = l.jewel.getCentre();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (l.jewel.expanded (6.5f).translated (0.6f, 1.4f));
        conic (g, jc, l.jewel.getWidth() * 0.5f + 5.5f, 0.0f, [] (float a) { return metalAt (C (0xffb9bdc0), a); }, 64);
        g.setColour (C (0xff0a0a0a));
        g.fillEllipse (l.jewel.expanded (1.0f));
        label ("power", jc.x);
    }

    if (d.weave != Weave::none)
    {
        juce::Path cloth;
        cloth.addRoundedRectangle (l.grille, 6.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (cloth);
            weave (g, l.grille, d);
        }
        if (d.piping != 0)
        {
            g.setColour (C (d.piping));
            g.strokePath (cloth, juce::PathStrokeType (2.2f));
        }
    }
    else
    {
        // a front face instead of cloth, with a display window
        const auto face = l.grille;
        panelFinish (g, face, d);
    }

    // one soft light over the whole head
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shell);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), h.getCentreX(), h.getY(),
                                                 juce::Colours::black.withAlpha (0.18f), h.getX(), h.getBottom(), true));
        g.fillRect (h);
    }
    // badge
    if (d.display)
    {
        const auto win = l.grille.withSizeKeepingCentre (l.grille.getWidth() * 0.56f, l.grille.getHeight() * 0.62f);
        drawWell (g, win, 10.0f);
        const auto ink = C (d.jewel);
        letter (g, displayText.isNotEmpty() ? displayText : juce::String (d.badge), win.reduced (24.0f, 0.0f),
                faceFont (Face::doto, std::min (40.0f, win.getHeight() * 0.42f)), ink, juce::Justification::centred);
        letter (g, juce::String (d.badge).toUpperCase(), R (win.getX(), win.getBottom() + 6.0f, win.getWidth(), 18.0f),
                faceFont (Face::instrumentSemi, 12.0f), C (d.ink).withAlpha (0.8f), juce::Justification::centred);
    }
    else
    {
        const juce::String text = d.upper ? juce::String (d.badge).toUpperCase() : juce::String (d.badge);
        if (d.badgePlate)
        {
            auto f = faceFont (d.face, l.badge.getHeight() * 0.62f);
            const float tw = juce::GlyphArrangement::getStringWidth (f, text);
            const auto plate = R (std::min (l.badge.getWidth(), tw + 40.0f), l.badge.getHeight()).withPosition (l.badge.getPosition());
            const C plateColour = d.panel == Panel::gold ? C (0xffe7e1d2) : C (0xffd9dcde);
            juce::Path pp;
            pp.addRoundedRectangle (plate, 5.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 3 }).drawForPath (g, pp);
            fillMetal (g, plate, plateColour, 5.0f);
            letter (g, text, plate.reduced (16.0f, 6.0f), f, C (0xff15130f), juce::Justification::centred);
        }
        else
        {
            // a cast metal logo: the letters as a shape, chrome filled, with an edge and a shadow
            auto f = faceFont (d.face, l.badge.getHeight() * 0.8f);
            juce::GlyphArrangement ga;
            ga.addLineOfText (f, text, 0.0f, 0.0f);
            juce::Path logo;
            ga.createPath (logo);
            const auto lb = logo.getBounds();
            const float k = std::min (1.0f, l.badge.getWidth() / std::max (1.0f, lb.getWidth()));
            logo.applyTransform (juce::AffineTransform::translation (-lb.getX(), -lb.getY()).scaled (k)
                                     .translated (l.badge.getX(), l.badge.getCentreY() - lb.getHeight() * k * 0.5f));
            const auto b2 = logo.getBounds();
            juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 5, { 0, 3 }).drawForPath (g, logo);
            const C metal = d.piping != 0 ? C (d.piping) : C (0xffdcd6c6);
            juce::ColourGradient chrome (metal.brighter (0.9f), 0, b2.getY(), metal.darker (0.7f), 0, b2.getBottom(), false);
            chrome.addColour (0.48, metal.brighter (0.3f));
            chrome.addColour (0.52, metal.darker (0.35f));
            g.setGradientFill (chrome);
            g.fillPath (logo);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.strokePath (logo, juce::PathStrokeType (0.8f));
        }
    }
    return img;
}

void drawAmpJewel (juce::Graphics& g, const AmpLayout& l, const AmpDesign& d, bool on)
{
    const C jewel (d.jewel);
    const auto r = l.jewel;
    if (on)
    {
        g.setGradientFill (juce::ColourGradient (jewel.withAlpha (0.5f), r.getCentreX(), r.getCentreY(), jewel.withAlpha (0.0f), r.getCentreX() + 34.0f, r.getCentreY(), true));
        g.fillEllipse (r.expanded (34.0f));
    }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colours::white : jewel.darker (2.0f), r.getCentreX() - 3.0f, r.getCentreY() - 3.0f,
                                             on ? jewel : jewel.darker (3.5f), r.getRight(), r.getBottom(), true));
    g.fillEllipse (r);
    // cut facets catch the light
    g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.15f));
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
        g.drawLine (juce::Line<float> (r.getCentre(), r.getCentre().getPointOnCircumference (r.getWidth() * 0.45f, a)), 0.6f);
    }
    g.setColour (juce::Colours::white.withAlpha (on ? 0.8f : 0.3f));
    g.fillEllipse (R (r.getWidth() * 0.24f, r.getWidth() * 0.18f).withCentre (r.getCentre().translated (-r.getWidth() * 0.17f, -r.getWidth() * 0.2f)));
}

//==============================================================================
namespace
{
    struct CabSpec { int cols, rows; float speakerInches; juce::uint32 frame, tolex; float aspect; };
    CabSpec cabSpec (int m)
    {
        switch (m)
        {
            case 0: return { 1, 1, 12.0f, 0xff8c9196, 0xff1a1a1b, 1.15f };  // 1x12 open back
            case 1: return { 2, 1, 12.0f, 0xff2f5d9a, 0xff141414, 1.75f };  // 2x12, blue frames
            case 2: return { 2, 2, 12.0f, 0xff6e6a62, 0xff151515, 1.0f };   // 4x12 vintage
            case 3: return { 2, 2, 12.0f, 0xff1c1d1f, 0xff101010, 1.0f };   // 4x12 modern
            case 4: return { 2, 1, 12.0f, 0xffb9bdc2, 0xff121212, 1.75f };  // 2x12 American
            case 5: return { 1, 1, 8.0f,  0xff8c9196, 0xff2b2118, 1.05f };  // 1x8 tiny combo
            case 6: return { 2, 2, 10.0f, 0xff2a2c2f, 0xff1d2530, 1.0f };   // 4x10 bass
            case 7: return { 1, 1, 15.0f, 0xff2a2c2f, 0xff1d2530, 0.92f };  // 1x15 bass
            default: return { 1, 1, 12.0f, 0xff5d6166, 0xff161616, 1.6f };  // your IR: one speaker beside a display
        }
    }
    // A loudspeaker seen from the front with the grille off: pressed steel rim, fibre gasket and
    // bolts, the cloth edge in two rolls, the paper cone falling away to the dust cap.
    void drawSpeaker (juce::Graphics& g, R sp, C frame, bool alloyCap, juce::int64 seed)
    {
        const auto c = sp.getCentre();
        const float r = sp.getWidth() * 0.5f;
        auto ring = [&] (float outer, float inner, C dark, C light, bool faceOut)
        {
            conic (g, c, outer, inner, [=] (float a) { return dark.interpolatedWith (light, faceOut ? lit (a) : 1.0f - lit (a)); }, 96);
        };
        {
            juce::Path hole;
            hole.addEllipse (sp.expanded (1.5f));
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f), std::max (4, (int) (r * 0.07f)), { 2, 3 }).drawForPath (g, hole);
        }
        // rim: the rolled edge catches the light on one side
        ring (r, r * 0.955f, frame.darker (0.7f), frame.brighter (0.5f), true);
        ring (r * 0.955f, r * 0.93f, frame.darker (0.75f), frame.brighter (0.1f), false);
        // gasket: grey fibre board in eight pieces
        ring (r * 0.935f, r * 0.845f, C (0xff232323), C (0xff3d3c3a), true);
        {
            auto rnd = seeded (seed + 77);
            for (int i = 0; i < (int) (r * 2.2f); ++i)
            {
                const auto p = c.getPointOnCircumference (r * (0.85f + 0.08f * rnd.nextFloat()), rnd.nextFloat() * juce::MathConstants<float>::twoPi);
                g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f + 0.05f * rnd.nextFloat()));
                g.fillRect (p.x, p.y, 1.0f, 1.0f);
            }
        }
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * ((float) i + 0.5f) / 8.0f;
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine ({ c.getPointOnCircumference (r * 0.848f, a), c.getPointOnCircumference (r * 0.933f, a) }, 1.0f);
        }
        const int bolts = r > 70.0f ? 8 : 4;
        for (int i = 0; i < bolts; ++i)
            screw (g, c.getPointOnCircumference (r * 0.89f, juce::MathConstants<float>::twoPi * (float) i / (float) bolts + (bolts == 4 ? juce::MathConstants<float>::pi * 0.25f : 0.0f)),
                   std::max (3.0f, r * 0.05f), 0.7f * (float) i + (float) (seed % 5));
        // the well the cone sits in
        g.setColour (C (0xff060606));
        g.fillEllipse (R (r * 1.69f, r * 1.69f).withCentre (c));
        // cloth edge: two rolls, each lit on the side that faces the light
        const float s0 = r * 0.84f, s1 = r * 0.735f, roll = (s0 - s1) * 0.5f;
        for (int k = 0; k < 2; ++k)
        {
            const float o = s0 - roll * (float) k;
            ring (o, o - roll * 0.5f, C (0xff0a0a0a), C (0xff3c3b39), true);
            ring (o - roll * 0.5f, o - roll, C (0xff070707), C (0xff2b2a29), false);
        }
        // paper cone: a hollow, so the far side takes the light and the near side sits in shade
        const float capR = r * 0.235f;
        conic (g, c, s1, capR * 0.8f, [] (float a) { return C (0xff17140f).interpolatedWith (C (0xff4a4238), 0.15f + 0.85f * (1.0f - lit (a))); }, 144);
        {
            const auto cone = R (s1 * 2.0f, s1 * 2.0f).withCentre (c);
            juce::ColourGradient deep (juce::Colours::black.withAlpha (0.62f), c.x, c.y, juce::Colours::black.withAlpha (0.0f), c.x + s1, c.y, true);
            deep.addColour (0.45, juce::Colours::black.withAlpha (0.3f));
            g.setGradientFill (deep);
            g.fillEllipse (cone);
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addEllipse (cone);
            g.reduceClipRegion (clip);
            // pulp fibres
            auto rnd = seeded (seed + 5);
            for (int i = 0; i < (int) (s1 * s1 * 0.5f); ++i)
            {
                const auto p = c.getPointOnCircumference (s1 * std::sqrt (rnd.nextFloat()), rnd.nextFloat() * juce::MathConstants<float>::twoPi);
                g.setColour ((rnd.nextInt (3) == 0 ? juce::Colours::black : juce::Colours::white).withAlpha (0.02f + 0.035f * rnd.nextFloat()));
                g.fillRect (p.x, p.y, 1.0f + rnd.nextFloat(), 1.0f);
            }
            // pressed ribs: a dark groove with a lit lip beside it
            for (int k = 0; k < 5; ++k)
            {
                const float rr = s1 * (0.5f + 0.1f * (float) k);
                g.setColour (juce::Colours::black.withAlpha (0.32f));
                g.drawEllipse (R (rr * 2.0f, rr * 2.0f).withCentre (c), std::max (0.8f, r * 0.008f));
                g.setColour (juce::Colours::white.withAlpha (0.055f));
                g.drawEllipse (R (rr * 2.0f, rr * 2.0f).withCentre (c.translated (0.9f, 0.9f)), std::max (0.7f, r * 0.006f));
            }
        }
        // where the edge is glued to the cone
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (R (s1 * 2.0f, s1 * 2.0f).withCentre (c), std::max (1.0f, r * 0.012f));
        // dust cap: a dome standing up out of the hollow, with its shadow thrown down and right
        const auto cap = R (capR * 2.0f, capR * 2.0f).withCentre (c);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), c.x + capR * 0.2f, c.y + capR * 0.2f,
                                                 juce::Colours::black.withAlpha (0.0f), c.x + capR * 1.55f, c.y + capR * 0.2f, true));
        g.fillEllipse (cap.expanded (capR * 0.5f).translated (capR * 0.2f, capR * 0.2f));
        if (alloyCap)
        {
            conic (g, c, capR, 0.0f, [] (float a) { return metalAt (C (0xffa9adb1), a); }, 120);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), c.x - capR * 0.35f, c.y - capR * 0.4f,
                                                     juce::Colours::white.withAlpha (0.0f), c.x + capR * 0.3f, c.y + capR * 0.3f, true));
            g.fillEllipse (cap);
        }
        else
        {
            juce::ColourGradient dome (C (0xff5b544b), c.x - capR * 0.38f, c.y - capR * 0.42f, C (0xff0f0d0b), c.x + capR * 0.9f, c.y + capR * 0.9f, true);
            dome.addColour (0.5, C (0xff2a2622));
            g.setGradientFill (dome);
            g.fillEllipse (cap);
        }
        // the bead of glue round the cap
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (cap, std::max (1.0f, r * 0.014f));
        juce::Path glint;
        glint.addCentredArc (c.x, c.y, capR, capR, 0.0f, lightAngle - 0.5f, lightAngle + 0.5f, true);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (glint, juce::PathStrokeType (std::max (1.0f, r * 0.012f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

CabLayout cabLayout (R area, int model)
{
    const auto spec = cabSpec (model);
    CabLayout l;
    // small cabs are drawn smaller, so the size reads
    const float scale = spec.speakerInches < 10.0f ? 0.72f : 1.0f;
    float h = area.getHeight() * scale, w = h * spec.aspect;
    if (w > area.getWidth()) { w = area.getWidth(); h = w / spec.aspect; }
    l.box = R (w, h).withCentre (area.getCentre());
    l.baffle = l.box.reduced (std::max (14.0f, w * 0.045f));
    const float cw = l.baffle.getWidth() / (float) spec.cols, ch = l.baffle.getHeight() / (float) spec.rows;
    const float d = std::min (cw, ch) * 0.86f * (spec.speakerInches == 10.0f ? 0.94f : 1.0f);
    for (int r = 0; r < spec.rows; ++r)
        for (int c = 0; c < spec.cols; ++c)
            l.speakers.push_back (R (d, d).withCentre ({ l.baffle.getX() + cw * ((float) c + 0.5f), l.baffle.getY() + ch * ((float) r + 0.5f) }));
    if (model == 8) // the speaker moves right to leave room for the display that names the loaded file
        l.speakers.front() = l.speakers.front().withCentre ({ l.baffle.getRight() - l.baffle.getHeight() * 0.5f, l.baffle.getCentreY() });
    return l;
}

juce::Image renderCab (int model, const CabLayout& l, R area, float scale, const juce::String& irName)
{
    const auto spec = cabSpec (model);
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    const auto b = l.box;
    {
        juce::Path p;
        p.addRoundedRectangle (b.translated (0.0f, 8.0f), 10.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 30, { 0, 12 }).drawForPath (g, p);
    }
    // the box: tolex all round
    AmpDesign shell {};
    shell.tolex = model == 5 ? Tolex::tweed : Tolex::levant;
    shell.tolexColour = model == 5 ? 0xffc9a66b : spec.tolex;
    juce::Path outer;
    outer.addRoundedRectangle (b, 10.0f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (outer);
        tolexTexture (g, b, shell);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, b.getY(), juce::Colours::white.withAlpha (0.0f), 0, b.getY() + 14.0f, false));
        g.fillRect (b.withHeight (14.0f));
    }
    // baffle board: black painted wood, lit from above
    {
        juce::Path baffle;
        baffle.addRoundedRectangle (l.baffle, 4.0f);
        g.setGradientFill (juce::ColourGradient (C (0xff1f2022), 0, l.baffle.getY(), C (0xff0b0b0c), 0, l.baffle.getBottom(), false));
        g.fillPath (baffle);
        finishTexture (g, l.baffle, Finish::anodised, C (0xff151516), 21);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), 0, l.baffle.getY(), juce::Colours::black.withAlpha (0.0f), 0, l.baffle.getY() + 16.0f, false));
        g.fillRect (l.baffle.withHeight (16.0f));
    }
    // a thin cord of piping round the baffle on the older boxes
    if (model == 0 || model == 2 || model == 4 || model == 5)
    {
        const auto pipe = l.baffle.expanded (2.0f);
        g.setGradientFill (juce::ColourGradient (C (0xffe9e2cf), pipe.getX(), pipe.getY(), C (0xff7d7767), pipe.getRight(), pipe.getBottom(), false));
        g.drawRoundedRectangle (pipe, 5.0f, 1.8f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (pipe.reduced (1.4f), 4.0f, 0.8f);
    }
    // the box's rim stands proud of the baffle and shades its top and left
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), l.baffle.getX(), 0, juce::Colours::black.withAlpha (0.0f), l.baffle.getX() + 14.0f, 0, false));
    g.fillRect (l.baffle.withWidth (14.0f));
    for (int i = 0; i < 4; ++i)
        screw (g, { i % 2 == 0 ? l.baffle.getX() + 9.0f : l.baffle.getRight() - 9.0f, i < 2 ? l.baffle.getY() + 9.0f : l.baffle.getBottom() - 9.0f },
               5.0f, 0.5f + 0.9f * (float) i);
    // speakers
    for (size_t i = 0; i < l.speakers.size(); ++i)
        drawSpeaker (g, l.speakers[i], C (spec.frame), model == 6, (juce::int64) model * 16 + (juce::int64) i);
    // corners
    if (model != 5)
        for (int i = 0; i < 4; ++i)
            cabinetCorner (g, b, i % 2 == 1, i >= 2, std::min (32.0f, b.getWidth() * 0.075f));
    if (model == 8)
    {
        const float room = l.speakers.front().getX() - l.baffle.getX();
        const auto win = R (room - 66.0f, std::min (48.0f, l.baffle.getHeight() * 0.2f)).withCentre ({ l.baffle.getX() + room * 0.5f + 4.0f, l.baffle.getCentreY() });
        const auto plate = win.expanded (22.0f, 8.0f);
        {
            juce::Path p;
            p.addRoundedRectangle (plate, 5.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 1, 2 }).drawForPath (g, p);
        }
        fillMetal (g, plate, C (0xff2c2e30), 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (plate.reduced (0.5f), 5.0f, 0.8f);
        for (auto x : { plate.getX() + 11.0f, plate.getRight() - 11.0f })
            screw (g, { x, plate.getCentreY() }, 5.0f, x * 0.37f);
        drawWell (g, win, 8.0f);
        letter (g, irName.isNotEmpty() ? irName : juce::String ("load an IR"), win.reduced (12.0f, 0.0f), faceFont (Face::doto, 18.0f),
                C (0xffff9f1c), juce::Justification::centred);
    }
    return img;
}

void drawCabMic (juce::Graphics& g, const CabLayout& l, float micPos, float distance)
{
    if (l.speakers.empty()) return;
    const auto sp = l.speakers.front();
    const float r = sp.getWidth() * 0.5f;
    // 0 = dead centre of the cap, 10 = out at the edge of the cone
    const auto c = sp.getCentre().translated (r * 0.72f * juce::jlimit (0.0f, 10.0f, micPos) / 10.0f, 0.0f);
    const float away = juce::jlimit (0.0f, 10.0f, distance) / 10.0f;
    // We look past the back of the mic at the cone. Pulled back from the speaker it comes toward
    // us: bigger, and its shadow on the cone grows softer and slides away.
    const float hr = r * (0.135f + 0.05f * away);
    const juce::Point<float> axis (0.62f, 0.785f); // the body runs down and to the right, toward the viewer
    const juce::Point<float> across (axis.y, -axis.x);
    const auto tail = c + axis * (hr * 3.3f);
    const float bodyHead = hr * 0.7f, bodyTail = hr * 0.56f;
    juce::Path body;
    body.startNewSubPath (c + across * bodyHead);
    body.lineTo (tail + across * bodyTail);
    body.lineTo (tail - across * bodyTail);
    body.lineTo (c - across * bodyHead);
    body.closeSubPath();
    juce::Path head;
    head.addEllipse (R (hr * 2.0f, hr * 2.0f).withCentre (c));
    {
        juce::Path shade = body;
        shade.addPath (head);
        const float off = r * (0.03f + 0.16f * away);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f - 0.35f * away), (int) (r * (0.03f + 0.12f * away)), { (int) off, (int) off }).drawForPath (g, shade);
    }
    // stand: a rod up from the floor to a clip on the body
    const auto clip = c + axis * (hr * 2.0f);
    {
        const float w = std::max (3.0f, hr * 0.26f);
        const R rod (clip.x - w * 0.5f, clip.y, w, l.box.getBottom() - 2.0f - clip.y);
        juce::ColourGradient m (C (0xff111213), rod.getX(), 0, C (0xff0a0a0b), rod.getRight(), 0, false);
        m.addColour (0.35, C (0xff54585c));
        g.setGradientFill (m);
        g.fillRect (rod);
    }
    // cable out of the tail, hanging to the floor
    {
        juce::Path cable;
        cable.startNewSubPath (tail);
        cable.cubicTo (tail + axis * (hr * 1.6f), { tail.x + hr * 1.9f, tail.y + hr * 2.2f }, { tail.x + hr * 1.7f, l.box.getBottom() - 2.0f });
        g.setColour (C (0xff070707));
        g.strokePath (cable, juce::PathStrokeType (std::max (2.0f, hr * 0.2f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.strokePath (cable, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (-0.8f, -0.4f));
    }
    // the grille ball, facing the cone
    {
        juce::ColourGradient ball (C (0xffe4e6e8), c.x - hr * 0.4f, c.y - hr * 0.45f, C (0xff3d4144), c.x + hr * 0.9f, c.y + hr * 0.9f, true);
        ball.addColour (0.55, C (0xff8a8e92));
        g.setGradientFill (ball);
        g.fillPath (head);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (head);
        const float pitch = std::max (2.2f, hr * 0.16f);
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        for (float d = -hr * 2.0f; d < hr * 2.0f; d += pitch)
        {
            g.drawLine (c.x + d - hr, c.y - hr, c.x + d + hr, c.y + hr, 0.8f);
            g.drawLine (c.x + d + hr, c.y - hr, c.x + d - hr, c.y + hr, 0.8f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (R (hr * 2.0f, hr * 2.0f).withCentre (c).reduced (0.5f), 1.0f);
    }
    // the body: a satin tube, lit along its upper side
    {
        const auto mid = c + axis * (hr * 1.65f);
        const auto e0 = mid + across * bodyHead, e1 = mid - across * bodyHead;
        juce::ColourGradient tube (C (0xff1b1c1e), e0.x, e0.y, C (0xff0c0d0e), e1.x, e1.y, false);
        tube.addColour (0.22, C (0xff8b9095));
        tube.addColour (0.42, C (0xff3a3d40));
        g.setGradientFill (tube);
        g.fillPath (body);
        // the collar where the ball screws on, and the clip
        for (float t : { 0.62f, 1.75f, 2.25f })
        {
            const auto p = c + axis * (hr * t);
            const float w = bodyHead + (bodyTail - bodyHead) * t / 3.3f;
            g.setColour (juce::Colours::black.withAlpha (t < 1.0f ? 0.55f : 0.75f));
            g.drawLine ({ p + across * w, p - across * w }, t < 1.0f ? 1.2f : std::max (1.6f, hr * 0.16f));
        }
    }
    // the tail end faces us: the socket and its plug
    {
        const auto end = R (bodyTail * 2.0f, bodyTail * 2.0f).withCentre (tail);
        conic (g, tail, bodyTail, 0.0f, [] (float a) { return metalAt (C (0xff5d6165), a); }, 72);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (end, 1.0f);
        g.setColour (C (0xff0b0b0c));
        g.fillEllipse (end.reduced (bodyTail * 0.34f));
    }
}

//==============================================================================
Footswitch::Footswitch (juce::RangedAudioParameter& p)
    : attachment (p, [this] (float v) { on = v >= 0.5f; repaint(); if (onChange) onChange (on); })
{
    attachment.sendInitialUpdate();
    setTooltip ("Stomp to switch on or off");
}

void Footswitch::mouseDown (const juce::MouseEvent&) { pressed = true; repaint(); }

void Footswitch::mouseUp (const juce::MouseEvent& e)
{
    pressed = false;
    repaint();
    if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void drawFootswitch (juce::Graphics& g, R b, bool pressed, bool hover)
{
    b = b.reduced (2.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f * (pressed ? 0.95f : 1.0f);
    const auto c = b.getCentre().translated (0.0f, pressed ? 1.2f : 0.0f);
    const auto disc = R (r * 2.0f, r * 2.0f).withCentre (c);
    // the nut under it already throws the long shadow; here only the dark line where the collar meets it
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (disc.expanded (1.2f).translated (0.4f, pressed ? 0.4f : 1.0f));
    // threaded collar
    conic (g, c, r, r * 0.72f, [] (float a)
    {
        const float groove = std::sin (a * 30.0f);
        const auto col = metalAt (C (0xff9da1a5), a);
        return groove > 0.0f ? col.brighter (0.3f * groove) : col.darker (-0.6f * groove);
    }, 180);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (disc.reduced (0.4f), 0.9f);
    // the plunger: a polished dome
    const float cr = r * 0.72f;
    const auto capR = R (cr * 2.0f, cr * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (capR.expanded (1.0f).translated (0.4f, 0.9f));
    conic (g, c, cr, 0.0f, [] (float a) { return metalAt (C (0xffc4c8cb), a); }, 96);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), c.x - cr * 0.3f, c.y - cr * 0.36f,
                                             juce::Colours::white.withAlpha (0.0f), c.x + cr * 0.45f, c.y + cr * 0.45f, true));
    g.fillEllipse (capR);
    juce::ColourGradient rim (juce::Colours::black.withAlpha (0.0f), c.x, c.y, juce::Colours::black.withAlpha (0.45f), c.x + cr, c.y, true);
    rim.addColour (0.7, juce::Colours::black.withAlpha (0.0f));
    g.setGradientFill (rim);
    g.fillEllipse (capR);
    if (hover && ! pressed)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillEllipse (disc);
    }
}
void Footswitch::paint (juce::Graphics& g)
{
    drawFootswitch (g, getLocalBounds().toFloat(), pressed, isMouseOver());
}


//==============================================================================
namespace
{
    constexpr float thumbStart = juce::MathConstants<float>::pi * 1.25f, thumbEnd = juce::MathConstants<float>::pi * 2.75f;
    float thumbAngle (int i) { return thumbStart + (thumbEnd - thumbStart) * (0.38f + 0.08f * (float) ((i * 37) % 5)); }
}

GearThumb pedalThumb (const PedalDesign& d, int n, bool faders, float scale)
{
    GearThumb t;
    const R nominal (0.0f, 0.0f, 300.0f, 250.0f);
    t.pedal = pedalLayout (nominal, d, n, faders);
    t.box = t.pedal.box;
    t.area = nominal.withTrimmedTop (-12.0f).withTrimmedBottom (-30.0f);
    t.image = renderPedal (d, t.pedal, t.area, scale);
    juce::Graphics g (t.image);
    g.addTransform (juce::AffineTransform::translation (-t.area.getX(), -t.area.getY()).scaled (scale));
    if (faders)
    {
        const float cw = t.pedal.knobs.getWidth() / ((float) n + 0.4f);
        for (int i = 0; i < n; ++i)
        {
            const float x = t.pedal.knobs.getX() + cw * ((float) i + (i == n - 1 ? 0.4f : 0.0f)) + cw * 0.5f;
            const auto slot = R (x - 2.5f, t.pedal.knobs.getY() + 20.0f, 5.0f, t.pedal.knobs.getHeight() - 42.0f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (slot, 2.5f);
            const float y = slot.getY() + slot.getHeight() * (0.3f + 0.1f * (float) ((i * 3) % 5));
            const auto cap = R (cw * 0.55f, 9.0f).withCentre ({ x, y });
            g.setColour (C (0xff1b1b1b));
            g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.0f);
            g.setColour (C (d.knobCap));
            g.fillRoundedRectangle (cap, 2.0f);
        }
    }
    else
    {
        for (int i = 0; i < n; ++i)
        {
            const auto cell = pedalKnobCell (t.pedal, i, n);
            const float size = std::min (cell.getWidth() - 6.0f, std::min (cell.getHeight() - 20.0f, 96.0f));
            const auto k = cell.withSizeKeepingCentre (size, size + 20.0f).removeFromTop (size);
            drawGearKnob (g, k, thumbAngle (i), thumbStart, thumbEnd, d.knob, C (d.knobCap), C (d.ink), true, false);
        }
    }
    drawFootswitch (g, t.pedal.footswitch, false, false);
    return t;
}

GearThumb ampThumb (const AmpDesign& d, const juce::String& displayText, float scale)
{
    GearThumb t;
    t.area = R (0.0f, 0.0f, 1000.0f, 400.0f);
    t.amp = ampLayout (t.area);
    t.box = t.amp.head.getUnion (t.amp.handle);
    t.image = renderAmp (d, t.amp, t.area, scale, displayText);
    juce::Graphics g (t.image);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto kr = t.amp.knobs;
    const float cell = kr.getWidth() / 8.0f;
    const float size = std::min (cell - 6.0f, kr.getHeight() - 22.0f);
    for (int i = 0; i < 8; ++i)
    {
        const auto k = R (kr.getX() + cell * (float) i, kr.getY(), cell, kr.getHeight()).withSizeKeepingCentre (size, size + 22.0f).removeFromTop (size);
        drawGearKnob (g, k, thumbAngle (i), thumbStart, thumbEnd, d.knob, C (d.knobCap), C (d.ink), true, false, true);
    }
    return t;
}

GearThumb cabThumb (int model, float scale)
{
    GearThumb t;
    t.area = R (0.0f, 0.0f, 420.0f, 340.0f);
    const auto l = cabLayout (t.area.reduced (10.0f), model);
    t.box = l.box;
    t.image = renderCab (model, l, t.area, scale, {});
    return t;
}
} // namespace rh::ui
