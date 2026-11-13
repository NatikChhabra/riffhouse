// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Gear.h"
#include "GearArt.h"

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
                // hammered paint: a fine mottle of shallow pools a millimetre or two across,
                // each lit on the side facing the light. Small and dense, so it reads as a
                // surface and not as spots.
                for (int i = 0; i < (int) (area / 16.0f); ++i)
                {
                    const float d = 2.6f + rnd.nextFloat() * 4.2f;
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    const R cell (x - d * 0.5f, y - d * 0.5f, d * (0.8f + rnd.nextFloat() * 0.7f), d * (0.7f + rnd.nextFloat() * 0.5f));
                    g.setColour (juce::Colours::black.withAlpha (0.05f + rnd.nextFloat() * 0.05f));
                    g.fillEllipse (cell.translated (0.6f, 0.7f));
                    g.setColour (juce::Colours::white.withAlpha (0.035f + rnd.nextFloat() * 0.04f));
                    g.fillEllipse (cell.reduced (0.5f).translated (-0.4f, -0.5f));
                }
                break;
            case Finish::sparkle:
                // metalflake under lacquer: a dense field of flakes close to the paint's own colour,
                // and only a few that catch the light
                for (int i = 0; i < (int) (area / 4.0f); ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    const float a = rnd.nextFloat();
                    if (a > 0.985f)
                    {
                        g.setColour (body.brighter (1.6f).withAlpha (0.55f));
                        g.fillEllipse (x, y, 1.5f, 1.5f);
                        continue;
                    }
                    g.setColour (a > 0.5f ? body.brighter (0.55f).withAlpha (0.05f + (a - 0.5f) * 0.36f) : body.darker (0.6f).withAlpha (0.06f + a * 0.24f));
                    g.fillEllipse (x, y, 1.2f, 1.2f);
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
                // wrinkle paint: short ridges that wander, a light edge and a dark one. Drawn as
                // little bent strokes, because single dots read as television static.
                for (int i = 0; i < (int) (area / 11.0f); ++i)
                {
                    const float x = r.getX() + rnd.nextFloat() * r.getWidth(), y = r.getY() + rnd.nextFloat() * r.getHeight();
                    const float a = rnd.nextFloat() * juce::MathConstants<float>::twoPi, len = 2.2f + rnd.nextFloat() * 3.4f;
                    const float bend = (rnd.nextFloat() - 0.5f) * 2.4f;
                    juce::Path ridge;
                    ridge.startNewSubPath (x, y);
                    ridge.quadraticTo (x + std::cos (a) * len * 0.5f - std::sin (a) * bend, y + std::sin (a) * len * 0.5f + std::cos (a) * bend,
                                       x + std::cos (a) * len, y + std::sin (a) * len);
                    g.setColour (juce::Colours::black.withAlpha (0.10f + rnd.nextFloat() * 0.08f));
                    g.strokePath (ridge, juce::PathStrokeType (1.1f), juce::AffineTransform::translation (0.5f, 0.6f));
                    g.setColour (juce::Colours::white.withAlpha (0.035f + rnd.nextFloat() * 0.045f));
                    g.strokePath (ridge, juce::PathStrokeType (0.8f));
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

    // The soft shadow a round part standing on a panel throws away from the light. Drawn as a
    // gradient, not a blurred picture, so it has no edge of its own at any size or scale.
    void contactShadow (juce::Graphics& g, juce::Point<float> c, float radius, float strength)
    {
        const auto sc = c.translated (radius * 0.12f, radius * 0.2f);
        juce::ColourGradient sh (juce::Colours::black.withAlpha (strength), sc.x, sc.y, juce::Colours::black.withAlpha (0.0f), sc.x + radius * 1.45f, sc.y, true);
        sh.addColour (0.55, juce::Colours::black.withAlpha (strength * 0.55f));
        sh.addColour (0.8, juce::Colours::black.withAlpha (strength * 0.15f));
        g.setGradientFill (sh);
        g.fillEllipse (R (radius * 2.9f, radius * 2.9f).withCentre (sc));
    }

    // A hex nut on a washer: each flat takes its own light, and the edges between flats and
    // round the chamfered top catch it or fall into shade.
    void hexNut (juce::Graphics& g, juce::Point<float> c, float radius)
    {
        const float hair = std::max (0.6f, radius * 0.05f);
        contactShadow (g, c, radius * 1.1f, 0.42f);
        // the washer, a turned ring just wider than the nut
        const float washer = radius * 1.08f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (R (washer * 2.0f + 1.2f, washer * 2.0f + 1.2f).withCentre (c.translated (0.2f, 0.5f)));
        conic (g, c, washer, 0.0f, [] (float a) { return metalAt (C (0xff8f9397), a); }, 96);
        juce::Path hex;
        hex.addPolygon (c, 6, radius, 0.0f);
        // the flats
        for (int i = 0; i < 6; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / 6.0f, a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / 6.0f;
            juce::Path f;
            f.addTriangle (c, c.getPointOnCircumference (radius, a0), c.getPointOnCircumference (radius, a1));
            const float v = lit ((a0 + a1) * 0.5f);
            g.setColour (C (0xff55595d).interpolatedWith (C (0xffe9ebec), v * v * 0.9f + 0.05f));
            g.fillPath (f);
            g.strokePath (f, juce::PathStrokeType (0.5f)); // no hairline gaps between neighbouring flats
        }
        // the outer edge of each flat: lit edges a bright line, shaded ones dark
        for (int i = 0; i < 6; ++i)
        {
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / 6.0f, a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / 6.0f;
            const float v = lit ((a0 + a1) * 0.5f) * 2.0f - 1.0f;
            g.setColour ((v > 0.0f ? juce::Colours::white : juce::Colours::black).withAlpha (0.55f * std::abs (v)));
            g.drawLine ({ c.getPointOnCircumference (radius - hair * 0.5f, a0), c.getPointOnCircumference (radius - hair * 0.5f, a1) }, hair);
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (hex, juce::PathStrokeType (std::max (0.5f, hair * 0.7f)));
        // the chamfer turned round the top, then the flat top face, spun
        conic (g, c, radius * 0.86f, radius * 0.76f, [] (float a)
        {
            const float v = lit (a) * 2.0f - 1.0f;
            return v > 0.0f ? C (0xffb9bcbf).interpolatedWith (juce::Colours::white, v) : C (0xffb9bcbf).interpolatedWith (C (0xff3b3e41), -v);
        }, 96);
        conic (g, c, radius * 0.77f, 0.0f, [] (float a) { return metalAt (C (0xffc4c7ca), a); }, 96);
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

    // A quarter-inch socket seen from above, coming out of the box's side wall: washer, nut,
    // threaded bush. Round bar lit from above: a line of light near the top, dark underneath.
    void sideJack (juce::Graphics& g, float edgeX, float y, bool right, float u)
    {
        auto piece = [&] (float from, float len, float height, C base, bool threads)
        {
            const R r (right ? edgeX + from * u : edgeX - (from + len) * u, y - height * u * 0.5f, len * u, height * u);
            juce::ColourGradient grad (base.brighter (0.5f), 0.0f, r.getY(), base.darker (0.9f), 0.0f, r.getBottom(), false);
            grad.addColour (0.2, base.brighter (1.3f));
            grad.addColour (0.5, base);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 1.0f * u);
            if (threads)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                for (float x = r.getX() + 1.0f * u; x < r.getRight() - 0.5f * u; x += 2.0f * u)
                    g.fillRect (x, r.getY(), 0.8f * u, r.getHeight());
            }
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 1.0f * u, 0.6f);
            return r;
        };
        piece (11.0f, 7.0f, 19.0f, C (0xff8d9195), true);
        const auto nut = piece (3.0f, 8.0f, 31.0f, C (0xffa0a4a8), false);
        // the nut's flats: the one facing up is bright, the edges between them are dark lines
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRect (nut.getX(), nut.getY() + nut.getHeight() * 0.27f, nut.getWidth(), 0.8f);
        g.fillRect (nut.getX(), nut.getY() + nut.getHeight() * 0.73f, nut.getWidth(), 0.8f);
        piece (0.0f, 3.0f, 36.0f, C (0xffb4b8bb), false);
    }

    // A varnished hardwood end cheek: long grain running the length of the box.
    void woodCheek (juce::Graphics& g, const juce::Path& shape, R r, juce::int64 seed)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        juce::ColourGradient base (C (0xff6b4326), r.getX(), 0.0f, C (0xff3c2313), r.getRight(), 0.0f, false);
        base.addColour (0.35, C (0xff5a3720));
        g.setGradientFill (base);
        g.fillRect (r);
        auto rnd = seeded (seed);
        for (int i = 0; i < (int) (r.getWidth() * 1.6f); ++i)
        {
            const float x0 = r.getX() + rnd.nextFloat() * r.getWidth();
            const float phase = rnd.nextFloat() * 6.0f, sway = 0.6f + rnd.nextFloat() * 2.2f, freq = 0.008f + rnd.nextFloat() * 0.012f;
            juce::Path grain;
            bool started = false;
            for (float y = r.getY() - 4.0f; y <= r.getBottom() + 12.0f; y += 12.0f)
            {
                const float x = x0 + std::sin (y * freq + phase) * sway;
                if (started) grain.lineTo (x, y); else grain.startNewSubPath (x, y);
                started = true;
            }
            const bool light = rnd.nextInt (5) == 0;
            g.setColour ((light ? C (0xffb9834f) : C (0xff24130a)).withAlpha (0.10f + rnd.nextFloat() * (light ? 0.12f : 0.22f)));
            g.strokePath (grain.createPathWithRoundedCorners (8.0f), juce::PathStrokeType (0.5f + rnd.nextFloat() * 1.1f));
        }
        // varnish: one soft line of light down the rounded top of the cheek
        const float hx = r.getX() + r.getWidth() * 0.32f;
        juce::ColourGradient shine (juce::Colours::white.withAlpha (0.0f), hx - r.getWidth() * 0.3f, 0.0f, juce::Colours::white.withAlpha (0.0f), hx + r.getWidth() * 0.3f, 0.0f, false);
        shine.addColour (0.5, juce::Colours::white.withAlpha (0.16f));
        g.setGradientFill (shine);
        g.fillRect (r);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.55f), r.getRight(), r.getBottom(), false));
        g.strokePath (shape, juce::PathStrokeType (2.0f));
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
    juce::String titled (const juce::String& s) // "bass station" becomes "Bass Station"
    {
        juce::StringArray words;
        words.addTokens (s.toLowerCase(), " ", "");
        for (auto& w : words) w = w.substring (0, 1).toUpperCase() + w.substring (1);
        return words.joinIntoString (" ");
    }
} // namespace

//==============================================================================
const PedalDesign& drivePedalDesign (int t)
{
    // name, kind, body, ink, led, finish, knob, knob cap, face, upper, width, stripe, layout, form
    static const PedalDesign d[] = {
        { "clean boost", "plus 20 dB of clean", 0xffaeb3b5, 0xff16191b, 0xffd9f1ff, Finish::brushed,    KnobStyle::dome,    0xff16191b, Face::instrumentSemi, false, 0.86f, 0, 1, 3 },
        { "TREBLE BOOSTER", "range 1", 0xff5f6f78, 0xfff1ead8, 0xffffb547, Finish::hammertone, KnobStyle::chicken, 0xffece4cf, Face::shoulders,      true,  0.92f, 0, 2, 0 },
        { "GREEN DRIVE", "mid overdrive", 0xff2e6a42, 0xfff0e8cc, 0xff7dffa0, Finish::enamel,     KnobStyle::pedal,   0xfff0e8cc, Face::shoulders,      true,  1.0f,  1, 0, 0 },
        { "blues drive", "low gain", 0xff1d4f9a, 0xffe9eef5, 0xff7fc4ff, Finish::sparkle,    KnobStyle::dome,    0xff1b1e24, Face::script, false, 1.0f,  0, 1, 0 },
        { "RODENT", "distortion", 0xff1c1d1e, 0xfff2f0e8, 0xffffb547, Finish::crinkle,    KnobStyle::pedal,   0xfff2f0e8, Face::shoulders,      true,  1.0f,  0, 2, 0 },
        { "big fuzz", "sustainer", 0xffe6dbc0, 0xff3b2a1a, 0xffffb547, Finish::enamel,     KnobStyle::skirted, 0xff1e1a16, Face::shoulders,      false, 1.28f, 3, 3, 2 },
        { "METAL BOX", "high gain", 0xff34373b, 0xfff4a524, 0xfff3f3ee, Finish::crinkle,    KnobStyle::skirted, 0xff101112, Face::shoulders,      true,  1.0f,  2, 0, 0 },
    };
    return d[juce::jlimit (0, (int) std::size (d) - 1, t)];
}

const PedalDesign* pedalDesignFor (int module, int type)
{
    // name, kind, body, ink, led, finish, knob, knob cap, face, upper, width, stripe, layout, form
    static const PedalDesign gate   { "SILENCER", "noise gate", 0xff3e4537, 0xfff0e8cc, 0xff7dffa0, Finish::hammertone, KnobStyle::pedal, 0xfff0e8cc, Face::shoulders, true, 0.95f, 0, 0, 1 };
    static const PedalDesign comp   { "squeeze", "compressor", 0xff3f8fc4, 0xfff7f7f2, 0xffd9f1ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::slab, false, 1.12f, 0, 3, 0 };
    static const PedalDesign wah    { "FUNK FILTER", "wah and envelope", 0xffd9a521, 0xff2a1d10, 0xff7dffa0, Finish::enamel, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, true, 0.9f, 0, 0, 5 };
    static const PedalDesign classic { "THROAT", "classic wah", 0xff27443a, 0xfff0e8cc, 0xffffb547, Finish::hammertone, KnobStyle::chicken, 0xfff0e8cc, Face::shoulders, true, 0.9f, 0, 0, 5 };
    static const PedalDesign pitch  { "deep octave", "pitch shifter", 0xff5b3c8f, 0xffeae6f4, 0xffd9c4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::slab, false, 0.95f, 0, 2, 0 };
    static const PedalDesign eq     { "GRAPHIC EQ", "seven band", 0xff18191b, 0xffeceae4, 0xff7dffa0, Finish::crinkle, KnobStyle::pedal, 0xffeceae4, Face::shoulders, true, 1.5f, 0, 0, 2 };
    static const PedalDesign lofi   { "dusty tape", "lo-fi machine", 0xff6b5a44, 0xfff1ead8, 0xffffb547, Finish::hammertone, KnobStyle::chicken, 0xfff1ead8, Face::brush, false, 1.34f, 0, 0, 4 };
    static const PedalDesign mods[] = {
        { "liquid chorus", "chorus", 0xff6fc3b2, 0xff10302a, 0xffd9f1ff, Finish::enamel, KnobStyle::skirted, 0xff14201d, Face::brush, false, 1.0f, 0, 1, 0 },
        { "JET FLANGER", "flanger", 0xffaeb3b5, 0xff2e2350, 0xffd9c4ff, Finish::brushed, KnobStyle::pedal, 0xfff7f7f2, Face::shoulders, true, 1.0f, 0, 2, 3 },
        { "PHASE FOUR", "phaser", 0xffb07a2a, 0xff1d140a, 0xffffb547, Finish::hammertone, KnobStyle::pedal, 0xfff1ead8, Face::shoulders, true, 0.9f, 0, 0, 0 },
        { "tremble", "tremolo", 0xffe6dbc0, 0xff1d1a16, 0xffffb547, Finish::enamel, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, false, 1.0f, 0, 0, 1 },
        { "warble", "vibrato", 0xff1f4e5f, 0xffe6edf0, 0xff7fc4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::script, false, 1.0f, 0, 1, 0 },
        { "ROTARY VIBE", "vibe", 0xff4a3426, 0xfff1ead8, 0xffffb547, Finish::crinkle, KnobStyle::skirted, 0xff141516, Face::shoulders, true, 1.25f, 3, 3, 2 },
    };
    static const PedalDesign delays[] = {
        { "digital echo", "delay", 0xffeceae4, 0xff16181a, 0xff7fc4ff, Finish::enamel, KnobStyle::hat, 0xff16181a, Face::instrumentSemi, false, 1.18f, 2, 2, 2 },
        { "BUCKET DELAY", "analog delay", 0xff4c5a4f, 0xfff0e8cc, 0xffffb547, Finish::hammertone, KnobStyle::pedal, 0xfff0e8cc, Face::shoulders, true, 1.18f, 0, 0, 1 },
        { "TAPE ECHO", "tape delay", 0xff8a6a48, 0xfff1ead8, 0xffffb547, Finish::crinkle, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, true, 1.36f, 3, 0, 4 },
        { "ping pong", "stereo delay", 0xff2d3e5c, 0xfff1f3f6, 0xff7fc4ff, Finish::sparkle, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 1.18f, 0, 2, 0 },
    };
    static const PedalDesign reverbs[] = {
        { "small room", "reverb", 0xff5d7f99, 0xfff4f6f8, 0xffd9f1ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::instrumentSemi, false, 1.18f, 0, 0, 1 },
        { "CONCERT HALL", "reverb", 0xff151517, 0xffd8b04a, 0xffffb547, Finish::anodised, KnobStyle::hat, 0xffd8b04a, Face::shoulders, true, 1.36f, 0, 0, 4 },
        { "plate", "reverb", 0xffaeb3b5, 0xff16191b, 0xff7fc4ff, Finish::brushed, KnobStyle::dome, 0xff16191b, Face::instrumentSemi, false, 1.18f, 0, 3, 3 },
        { "SPRING TANK", "reverb", 0xff2a2a2c, 0xffd0d4d8, 0xffffb547, Finish::crinkle, KnobStyle::chicken, 0xfff1ead8, Face::shoulders, true, 1.36f, 1, 3, 4 },
        { "shimmer", "octave reverb", 0xffe8e6ef, 0xff4a3c78, 0xffd9c4ff, Finish::anodised, KnobStyle::dome, 0xff1b1d20, Face::instrumentSemi, false, 1.18f, 0, 1, 3 },
        { "cloud", "ambient reverb", 0xffb8cbe0, 0xff17243a, 0xffd9f1ff, Finish::enamel, KnobStyle::pedal, 0xfff7f7f2, Face::script, false, 1.18f, 0, 3, 0 },
    };
    auto pick = [type] (const PedalDesign* arr, int n) { return &arr[juce::jlimit (0, n - 1, type)]; };
    switch (module)
    {
        case 0:  return &gate;
        case 1:  return &comp;
        case 2:  return type == 2 ? &classic : &wah;
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
    // Badges are our own names, never a maker's model or its nickname.
    // badge, tolex, tolex colour, panel, ink, weave, piping, knob, cap, jewel, face, plate, upper, display, form, groups, lamp, width
    static const AmpDesign d[] = {
        { "glass",     Tolex::levant, 0xff151617, Panel::black,    0xffeeeeea, Weave::silver,     0xffc9ccd0, KnobStyle::skirted, 0xff141516, 0xfff2351f, Face::shoulders,      false, false, false, 0, 1, 0, 1.0f,
          { "volume", "bass", "middle", "treble", "bright", "deep", "master", "output" }, { 0, 3, 2, 1, 4, 5, 6, 7 }, 0x30 },
        { "tweed",     Tolex::tweed,  0xffc9a66b, Panel::chrome,   0xff1b1a18, Weave::wheat,      0,          KnobStyle::chicken, 0xffece4cf, 0xfff2351f, Face::instrumentSemi, true,  false, false, 2, 0, 1, 0.66f,
          { "volume", "bass", "middle", "treble", "presence", "body", "loud", "output" }, { 4, 2, 1, 3, 0, 6, 5, 7 }, 0xa0 },
        { "chime",     Tolex::cream,  0xffd9c9a6, Panel::brushed,  0xff2b1f14, Weave::diamond,    0xffcbb48a, KnobStyle::chicken, 0xfff1ead8, 0xffffc35a, Face::shoulders,      true,  false, false, 0, 3, 1, 0.94f,
          { "drive", "bass", "middle", "treble", "brilliance", "weight", "master", "output" }, { 0, 1, 2, 3, 4, 5, 6, 7 }, 0x30 },
        { "SOVEREIGN", Tolex::levant, 0xff131313, Panel::gold,     0xff17130c, Weave::none,       0xffe9e4d6, KnobStyle::hat,     0xffd8b04a, 0xffec2f1b, Face::shoulders,      false, true,  false, 1, 0, 2, 1.0f,
          { "VOLUME I", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "RESONANCE", "VOLUME II", "OUTPUT" }, { 4, 1, 2, 3, 0, 6, 5, 7 }, 0xa0 },
        { "SOVEREIGN 800", Tolex::levant, 0xff121212, Panel::gold, 0xff17130c, Weave::black,      0xffe9e4d6, KnobStyle::hat,     0xffd8b04a, 0xffec2f1b, Face::shoulders,      true,  true,  false, 1, 0, 2, 1.0f,
          { "PREAMP", "BASS", "MIDDLE", "TREBLE", "PRESENCE", "RESONANCE", "MASTER", "OUTPUT" }, { 4, 1, 2, 3, 6, 0, 5, 7 }, 0xa0 },
        { "redline",   Tolex::levant, 0xff141414, Panel::chrome,   0xff111214, Weave::black,      0xffb9bdc2, KnobStyle::chicken, 0xffe9e7e2, 0xfff2351f, Face::shoulders,      false, false, false, 2, 0, 0, 0.84f,
          { "drive", "bass", "middle", "treble", "presence", "thump", "master", "volume" }, { 7, 0, 3, 1, 2, 6, 4, 5 }, 0x30 },
        { "MODERN",    Tolex::levant, 0xff101010, Panel::plate,    0xffe9e7e2, Weave::mesh,       0,          KnobStyle::skirted, 0xff101112, 0xff7dffa0, Face::shoulders,      true,  true,  false, 1, 2, 1, 1.0f },
        { "TIGHT",     Tolex::smooth, 0xff0d0d0e, Panel::anodised, 0xfff4a524, Weave::mesh,       0,          KnobStyle::pedal,   0xfff4a524, 0xfff4a524, Face::shoulders,      false, true,  false, 3, 3, 2, 0.84f,
          { "GAIN", "LOW", "MID", "HIGH", "PRES", "DEEP", "MASTER", "OUT" }, { 0, 1, 2, 3, 4, 5, 6, 7 }, 0x30 },
        { "bass station", Tolex::levant, 0xff1d2530, Panel::brushed, 0xff141618, Weave::grid,     0xff8fa3b8, KnobStyle::skirted, 0xff141618, 0xff6fb7ff, Face::shoulders,      true,  true,  false, 4, 0, 0, 0.78f },
        { "studio direct", Tolex::smooth, 0xff1a1b1d, Panel::brushed, 0xff141618, Weave::none,    0,          KnobStyle::dome,    0xff141618, 0xff7dffa0, Face::instrumentSemi, false, false, true,  5, 2, 2, 1.0f },
        { "capture",   Tolex::smooth, 0xff141518, Panel::anodised, 0xffe9e7e2, Weave::none,       0,          KnobStyle::modern,  0xffe9e7e2, 0xffff9f1c, Face::instrumentSemi, false, false, true,  5, 0, 1, 1.0f },
    };
    return d[juce::jlimit (0, (int) std::size (d) - 1, m)];
}

//==============================================================================
// A knob photographed by the 3D renderer, standing on its own shadow, with the pointer line
// drawn on it where it would be seen from the player's side. False when there is no picture.
static bool drawKnobArt (juce::Graphics& g, juce::Point<float> c, float r, float angle, float start, float end,
                         KnobStyle style, C cap, bool enabled, bool hot)
{
    const auto canvas = R (r * 4.0f, r * 4.0f).withCentre (c); // every knob picture is four radii wide round the foot of the shaft
    const C cream (0xfff1ead8);
    juce::String name;
    C line = cap;
    float from = 0.0f, to = 0.0f, top = 1.0f, width = std::max (1.8f, r * 0.075f);
    float (*height) (float) = nullptr; // how high the knob stands at a distance from its middle, in radii
    switch (style)
    {
        case KnobStyle::modern:  name = "knob_modern"; from = 0.3f; to = 0.8f; top = 1.01f; width *= 1.1f; break;
        case KnobStyle::pedal:   name = "knob_pedal"; from = 0.16f; to = 0.72f; top = 1.3f; width *= 1.15f; break;
        case KnobStyle::skirted:
            if (cap.getPerceivedBrightness() >= 0.3f) return false;
            name = "knob_skirted"; line = cream; from = 0.69f; to = 0.95f; top = 1.29f;
            height = [] (float rho) { return 0.16f + (0.97f - rho) / 0.31f * 0.26f; };
            break;
        case KnobStyle::dome:
            if (cap.getPerceivedBrightness() >= 0.3f) return false;
            name = "knob_dome"; line = C (0xff16181a); from = 0.42f; to = 0.86f; top = 0.92f; width *= 1.1f;
            height = [] (float rho) { return 0.5f + 0.42f * std::sqrt (std::max (0.0f, 1.0f - rho * rho)); };
            break;
        case KnobStyle::hat:
            name = "knob_hat_" + juce::String::toHexString ((int) (cap.getARGB() & 0xffffff)).paddedLeft ('0', 6);
            line = cream; from = 0.64f; to = 0.94f; top = 1.23f;
            height = [] (float rho) { return 0.2f + (0.96f - rho) / 0.36f * 0.32f; };
            break;
        case KnobStyle::chicken: name = "knob_chicken"; top = 1.02f; break;
    }
    if (! art::has (name)) return false;
    if (style == KnobStyle::chicken)
    {
        const int frames = 65;
        const int f = juce::roundToInt ((angle - start) / (end - start) * (float) (frames - 1));
        if (! art::drawFrame (g, name, f, frames, 13, canvas)) return false;
    }
    else
    {
        if (! art::draw (g, name, canvas)) return false;
        juce::Path p;
        const float sn = std::sin (angle), cs = std::cos (angle);
        for (int i = 0; i <= 6; ++i)
        {
            const float rho = from + (to - from) * (float) i / 6.0f;
            const float h = height != nullptr ? height (rho) : top;
            const juce::Point<float> pt (c.x + rho * r * sn, c.y - rho * r * cs * art::squash - h * r * art::rise);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        const juce::PathStrokeType stroke (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (p, stroke, juce::AffineTransform::translation (0.4f, 0.8f));
        g.setColour (line);
        g.strokePath (p, stroke);
    }
    if ((hot && enabled) || ! enabled)
    {
        // the whole knob as seen: its foot, its side and its top
        juce::Path shape;
        shape.addEllipse (R (r * 2.0f, r * 2.0f * art::squash).withCentre (c));
        shape.addEllipse (R (r * 2.0f, r * 2.0f * art::squash).withCentre (c.translated (0.0f, -top * r * art::rise)));
        shape.addRectangle (R (c.x - r, c.y - top * r * art::rise, r * 2.0f, top * r * art::rise));
        shape.setUsingNonZeroWinding (true);
        g.setColour (enabled ? juce::Colours::white.withAlpha (0.05f) : juce::Colours::black.withAlpha (0.5f));
        g.fillPath (shape);
    }
    return true;
}

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
        if (numbers) g.setFont (faceFont (Face::instrumentSemi, juce::jlimit (8.5f, 12.0f, size * 0.105f)));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = start + (end - start) * (float) i / 10.0f;
            const auto p = c.getPointOnCircumference (rScale, a);
            if (numbers)
                g.drawText (juce::String (i), R (24.0f, 14.0f).withCentre (p), juce::Justification::centred, false);
            else
            {
                const float d = (i == 0 || i == 10 || i == 5) ? 3.0f : 2.0f;
                g.fillEllipse (R (d, d).withCentre (p));
            }
        }
    }
    const float r = size * 0.5f - (numbers ? 15.0f : 9.0f);
    if (drawKnobArt (g, c, r, angle, start, end, style, cap, enabled, hot)) return;
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
    const float alpha = 1.0f; // a control that does nothing right now is still a solid part: it sits in shade instead
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
    if (! enabled)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (body.expanded (1.0f));
    }
}

//==============================================================================
PedalLayout pedalLayout (R area, const PedalDesign& d, int numKnobs, bool faders)
{
    PedalLayout l;
    const float h = std::min (area.getHeight(), 420.0f);
    const bool tall = faders || numKnobs > 4;
    l.form = (d.form == 1 && tall) ? 0 : d.form;
    const float w = std::min (area.getWidth(), h * 0.64f * d.width);
    l.box = R (w, h).withCentre (area.getCentre());
    const auto b = l.box;
    l.variant = tall ? 0 : d.layout;
    const float fs = std::min (w * 0.27f, h * (tall ? 0.13f : 0.15f));
    auto at = [&] (float fx, float fy) { return juce::Point<float> (b.getX() + w * fx, b.getY() + h * fy); };
    const float knobH = faders ? 0.50f : (tall ? 0.42f : 0.32f);
    l.knobs = R (b.getX() + w * 0.06f, b.getY() + h * 0.045f, w * 0.88f, h * knobH);
    l.name = R (b.getX() + w * 0.08f, b.getY() + h * (tall ? 0.56f : 0.41f), w * 0.84f, h * (tall ? 0.13f : 0.17f));
    l.led = R (12.0f, 12.0f).withCentre (at (0.5f, tall ? 0.748f : 0.645f));
    l.footswitch = R (fs, fs).withCentre (at (0.5f, tall ? 0.868f : 0.81f));
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
    // the enclosure families move the parts again
    switch (l.form)
    {
        case 1: // control deck on top, the name on the bar under it, then the foot plate
            l.variant = 0;
            l.knobs = R (b.getX() + w * 0.09f, b.getY() + h * 0.05f, w * 0.82f, h * 0.29f);
            l.name = R (b.getX() + w * 0.10f, b.getY() + h * 0.385f, w * 0.80f, h * 0.125f);
            l.led = l.led.withCentre (at (0.5f, 0.548f));
            l.footswitch = R (b.getX() + w * 0.09f, b.getY() + h * 0.59f, w * 0.82f, h * 0.365f);
            break;
        case 4: // the wooden cheeks take the outer edges
        {
            const float cheek = w * 0.095f;
            l.knobs = l.knobs.withX (b.getX() + cheek + w * 0.03f).withWidth (w - 2.0f * cheek - w * 0.06f);
            l.name = l.name.withX (b.getX() + cheek + w * 0.04f).withWidth (w - 2.0f * cheek - w * 0.08f);
            break;
        }
        case 5: // two small controls and the light on the nose, the treadle below
            l.variant = 0;
            l.knobs = R (b.getX() + w * 0.08f, b.getY() + h * 0.03f, w * 0.84f, h * 0.205f);
            l.name = R (b.getX() + w * 0.10f, b.getY() + h * 0.245f, w * 0.80f, h * 0.105f);
            l.led = l.led.withCentre (at (0.5f, 0.085f));
            l.treadle = R (b.getX() + w * 0.17f, b.getY() + h * 0.385f, w * 0.66f, h * 0.585f);
            l.footswitch = l.treadle.withHeight (l.treadle.getHeight() * 0.3f); // the toe: where the switch under it is
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
    if (l.form == 5)
    {
        // one each side of the light
        const float cw = l.knobs.getWidth() * 0.4f;
        return { i == 0 ? l.knobs.getX() : l.knobs.getRight() - cw, l.knobs.getY(), cw, l.knobs.getHeight() };
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

int footswitchStyle (const PedalLayout& l) { return l.form == 1 ? 1 : (l.form == 3 ? 2 : 0); }

bool pedalPictured (const PedalDesign& d) { return art::has ("pedal_" + art::slug (d.name)); }

R pedalArtArea (R box)
{
    const float k = box.getHeight() / 420.0f;
    return box.expanded (70.0f * k, 0.0f).withTop (box.getY() - 24.0f * k).withBottom (box.getBottom() + 56.0f * k);
}

// The ink on a pedal's face: its name, which socket is which, and whose it is. Printed on paint,
// cut into bare metal. With `flat` it is the print alone, as the 3D renderer lays it on the box.
static void pedalLettering (juce::Graphics& g, const PedalDesign& d, const PedalLayout& l, bool smallPrint, bool flat)
{
    const auto b = l.box;
    const int form = l.form;
    const float w = b.getWidth(), h = b.getHeight();
    const float u = h / 420.0f;
    const C body (d.body), ink (d.ink);
    {
        const C letterInk = d.stripe == 3 ? body : ink;
        const auto nameR = l.name.withTrimmedBottom (l.name.getHeight() * 0.12f).withTrimmedTop (l.name.getHeight() * 0.12f);
        // a hand-written name starts its words with capitals and stands taller in its line
        const bool hand = d.face == Face::script || d.face == Face::brush;
        const juce::String nameText = hand ? titled (d.name) : juce::String (d.name);
        const auto nameFont = faceFont (d.face, l.name.getHeight() * (hand ? 0.78f : d.face == Face::slab ? 0.54f : 0.62f));
        if (form == 3 && d.stripe != 3 && ! flat)
        {
            const bool lightMetal = body.getPerceivedBrightness() > 0.5f;
            const C edge = lightMetal ? juce::Colours::white.withAlpha (0.55f) : juce::Colours::black.withAlpha (0.5f);
            letter (g, nameText, nameR.translated (0.0f, 1.0f * u), nameFont, edge, juce::Justification::centred);
        }
        letter (g, nameText, hand ? l.name : nameR, nameFont, letterInk, juce::Justification::centred);

        // the small print every real pedal carries: which socket is which, and whose it is
        auto small = faceFont (d.face == Face::shoulders ? Face::shoulders : Face::instrumentSemi, 11.0f * u);
        small.setExtraKerningFactor (0.1f);
        if (form != 4 && smallPrint)
        {
            const float jy = b.getY() + h * (form == 5 ? 0.13f : (form == 1 ? 0.19f : 0.22f));
            const bool room = l.knobs.getBottom() < jy - 4.0f * u || form == 1 || form == 5; // not where a knob already is
            float ly = room ? jy : l.knobs.getBottom() + 12.0f * u;
            if (d.stripe == 1) // never on the pinstripes
                for (const float sy : { l.name.getY() - 9.0f * u, l.name.getBottom() + 27.0f * u })
                    if (std::abs (ly - sy) < 10.0f * u) ly = sy - 12.0f * u;
            letter (g, "IN", R (b.getX() + 9.0f * u, ly - 7.0f * u, 30.0f * u, 14.0f * u), small, ink.withAlpha (0.7f), juce::Justification::centredLeft);
            letter (g, "OUT", R (b.getRight() - 49.0f * u, ly - 7.0f * u, 40.0f * u, 14.0f * u), small, ink.withAlpha (0.7f), juce::Justification::centredRight);
        }
        if (smallPrint && (form == 0 || form == 2 || form == 3) && d.stripe != 2) // a tread plate leaves no room for it
        {
            // a model number made from the pedal's own name, so each one has its own
            int sum = 0;
            for (const char* ch = d.name; *ch != 0; ++ch) sum += *ch;
            const juce::String mark = "RIFFHOUSE  " + juce::String::charToString ((juce::juce_wchar) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) d.name[0]))
                                      + "-" + juce::String (10 + sum % 89);
            const float my = b.getBottom() - 19.0f * u;
            letter (g, mark, R (b.getX(), my, w, 13.0f * u), small, ink.withAlpha (0.55f), juce::Justification::centred);
        }
    }
}

juce::Image renderPedalPrint (const PedalDesign& d, const PedalLayout& l, R area, float scale)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    const auto b = l.box;
    const float u = b.getHeight() / 420.0f;
    g.setColour (juce::Colour (d.ink));
    if (d.stripe == 1)
    {
        g.fillRect (b.getX(), l.name.getY() - 10.0f * u, b.getWidth(), 2.0f * u);
        g.fillRect (b.getX(), l.name.getBottom() + 26.0f * u, b.getWidth(), 2.0f * u);
    }
    else if (d.stripe == 3)
        g.fillRect (b.getX(), l.name.getY() - 6.0f * u, b.getWidth(), l.name.getHeight() + 12.0f * u);
    pedalLettering (g, d, l, true, true);
    return img;
}

juce::Image renderPedal (const PedalDesign& d, const PedalLayout& l, R area, float scale, bool smallPrint)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    if (pedalPictured (d) && art::draw (g, "pedal_" + art::slug (d.name), pedalArtArea (l.box)))
        return img;
    const auto b = l.box;
    const int form = l.form;
    const float w = b.getWidth(), h = b.getHeight();
    const float u = h / 420.0f; // every fixed size below was drawn for the full-size pedal
    const float rad = form == 2 ? 4.0f * u : (form == 3 ? w * 0.028f : (form == 4 ? 10.0f * u : w * 0.045f));
    const C body (d.body), ink (d.ink);

    // shadow on the floor: wide and soft away from the light, tight and dark right under the box
    juce::Path face;
    face.addRoundedRectangle (b, rad);
    {
        juce::Path p;
        p.addRoundedRectangle (b.translated (3.0f, 8.0f), rad);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 30, { 4, 12 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 7, { 1, 5 }).drawForPath (g, p);
    }
    // the sockets come out of the side walls, so they go down before the top face covers their roots
    if (form != 4)
    {
        const float jy = b.getY() + h * (form == 5 ? 0.13f : (form == 1 ? 0.19f : 0.22f));
        sideJack (g, b.getX() + 1.0f, jy, false, u);
        sideJack (g, b.getRight() - 1.0f, jy, true, u);
    }
    // the box's side, seen below the top face; a folded top sits on a plain steel base plate
    if (form == 2)
    {
        fillMetal (g, b.translated (0.0f, 7.0f * u).expanded (1.5f * u, 0.0f), C (0xff84888c), rad);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (b.getX(), b.getBottom() - 1.0f, w, 1.6f * u);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (body.darker (0.55f), b.getX(), 0, body.darker (1.1f), b.getRight(), 0, false));
        g.fillRoundedRectangle (b.translated (0.0f, (form == 3 ? 4.5f : 6.0f) * u), rad);
    }
    g.setGradientFill (juce::ColourGradient (body.brighter (0.16f), b.getX(), b.getY(), body.darker (0.24f), b.getRight(), b.getBottom(), false));
    g.fillPath (face);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face);
        finishTexture (g, b, d.finish, body, (juce::int64) d.body * 31 + 7);
        // the face falls away a little toward the sides
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.22f), b.getX(), 0, juce::Colours::black.withAlpha (0.0f), b.getX() + 18.0f * u, 0, false));
        g.fillRect (b.withWidth (18.0f * u));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), b.getRight() - 18.0f * u, 0, juce::Colours::black.withAlpha (0.25f), b.getRight(), 0, false));
        g.fillRect (b.withLeft (b.getRight() - 18.0f * u));

        if (form == 2)
        {
            // below the fold the face slopes down toward the player and takes less light
            const float foldY = l.name.getY() - h * 0.022f;
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.10f), 0.0f, foldY, juce::Colours::black.withAlpha (0.26f), 0.0f, b.getBottom(), false));
            g.fillRect (b.withTop (foldY));
            g.setColour (juce::Colours::white.withAlpha (0.32f));
            g.fillRect (b.getX(), foldY - 1.0f * u, w, 1.0f * u);
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRect (b.getX(), foldY, w, 1.4f * u);
        }
        if (form == 1)
        {
            // the control deck is pressed a little below the rim, so heels miss the knobs
            const auto deck = R (b.getX() + w * 0.055f, b.getY() + h * 0.03f, w * 0.89f, l.name.getY() - b.getY() - h * 0.05f);
            g.setColour (juce::Colours::black.withAlpha (0.20f));
            g.fillRoundedRectangle (deck, 7.0f * u);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.38f), 0.0f, deck.getY(), juce::Colours::black.withAlpha (0.0f), 0.0f, deck.getY() + 10.0f * u, false));
            g.fillRoundedRectangle (deck, 7.0f * u);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), deck.getX(), deck.getY(), juce::Colours::white.withAlpha (0.3f), deck.getRight(), deck.getBottom(), false));
            g.drawRoundedRectangle (deck, 7.0f * u, 1.2f * u);
            // the well the foot plate drops into
            const auto well = l.footswitch.expanded (3.5f * u);
            g.setColour (C (0xff070707));
            g.fillRoundedRectangle (well, 6.0f * u);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), well.getX(), well.getY(), juce::Colours::white.withAlpha (0.28f), well.getRight(), well.getBottom(), false));
            g.drawRoundedRectangle (well, 6.0f * u, 1.2f * u);
        }
        if (form == 5)
        {
            // the treadle rocks in a trough cut into the casting
            const auto well = l.treadle.expanded (5.0f * u);
            g.setColour (C (0xff090909));
            g.fillRoundedRectangle (well, 8.0f * u);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.6f), well.getX(), well.getY(), juce::Colours::white.withAlpha (0.3f), well.getRight(), well.getBottom(), false));
            g.drawRoundedRectangle (well, 8.0f * u, 1.3f * u);
        }

        if (d.stripe == 1)
        {
            g.setColour (ink.withAlpha (0.9f));
            g.fillRect (b.getX(), l.name.getY() - 10.0f * u, w, 2.0f * u);
            g.fillRect (b.getX(), l.name.getBottom() + 26.0f * u, w, 2.0f * u);
        }
        else if (d.stripe == 2)
        {
            // tread plate along the bottom edge
            const auto strip = b.withTop (b.getBottom() - h * 0.09f);
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
            g.fillRect (b.getX(), l.name.getY() - 6.0f * u, w, l.name.getHeight() + 12.0f * u);
        }
        // gloss paint picks up the room: a soft patch of light on the upper left, no hard edge
        if (d.finish == Finish::enamel || d.finish == Finish::sparkle)
        {
            const juce::Point<float> gc (b.getX() + w * 0.22f, b.getY() + h * 0.06f);
            juce::ColourGradient gloss (juce::Colours::white.withAlpha (0.16f), gc.x, gc.y,
                                        juce::Colours::white.withAlpha (0.0f), gc.x + w * 0.9f, gc.y, true);
            gloss.addColour (0.5, juce::Colours::white.withAlpha (0.05f));
            g.setGradientFill (gloss);
            g.fillRect (b);
        }
        if (form == 4)
        {
            // the steel panel sits a little below the hardwood end cheeks: the left one shades it
            const float cheek = w * 0.095f;
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), b.getX() + cheek, 0.0f, juce::Colours::black.withAlpha (0.0f), b.getX() + cheek + 10.0f * u, 0.0f, false));
            g.fillRect (b.getX() + cheek, b.getY(), 10.0f * u, h);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillRect (b.getX() + cheek, b.getY(), w - 2.0f * cheek, 1.2f * u);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (b.getX() + cheek, b.getBottom() - 1.4f * u, w - 2.0f * cheek, 1.4f * u);
        }
        else if (form == 3)
        {
            // machined from a block: a flat chamfer all round, bright where it faces the light
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.62f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.5f), b.getRight(), b.getBottom(), false));
            g.drawRoundedRectangle (b.reduced (2.2f * u), rad, 4.4f * u);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.28f), b.getX(), b.getY(), juce::Colours::white.withAlpha (0.22f), b.getRight(), b.getBottom(), false));
            g.drawRoundedRectangle (b.reduced (4.8f * u), rad * 0.8f, 0.9f * u);
        }
        else if (form == 2)
        {
            // bent sheet: a tight bright bend at the top and left, nothing soft about it
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.45f), b.getRight(), b.getBottom(), false));
            g.drawRoundedRectangle (b.reduced (0.7f * u), rad, 1.4f * u);
        }
        else
        {
            // a cast box has rolled edges: they catch the light on the top and left, and fall dark on the bottom and right
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.5f), b.getRight(), b.getBottom(), false));
            g.drawRoundedRectangle (b.reduced (1.2f * u), rad, 2.6f * u);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.10f), b.getRight(), b.getBottom(), false));
            g.drawRoundedRectangle (b.reduced (3.4f * u), rad * 0.85f, 1.2f * u);
        }
    }

    if (form == 4)
    {
        // hardwood end cheeks, standing a little proud of the panel at both ends
        const float cheek = w * 0.095f;
        for (bool right : { false, true })
        {
            const auto ck = (right ? b.withLeft (b.getRight() - cheek) : b.withWidth (cheek)).expanded (0.0f, 4.0f * u);
            juce::Path cp;
            cp.addRoundedRectangle (ck, 7.0f * u);
            juce::DropShadow (juce::Colours::black.withAlpha (0.5f), std::max (2, (int) (5.0f * u)), { (int) (2.0f * u), (int) (3.0f * u) }).drawForPath (g, cp);
            woodCheek (g, cp, ck, (juce::int64) d.body * 17 + (right ? 5 : 3));
        }
    }

    pedalLettering (g, d, l, smallPrint, false);

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
    if (form == 5)
    {
        // the bolts the treadle pivots on, one each side of the trough
        for (float x : { l.treadle.getX() - 15.0f * u, l.treadle.getRight() + 15.0f * u })
            hexNut (g, { x, l.treadle.getCentreY() }, 6.5f * u);
    }
    else if (footswitchStyle (l) == 0)
        hexNut (g, l.footswitch.getCentre(), l.footswitch.getWidth() * 0.47f);
    else if (footswitchStyle (l) == 2)
    {
        // a soft-touch switch sits in a counterbore, flush with the face
        const auto bore = l.footswitch.expanded (3.0f * u);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), bore.getX(), bore.getY(), juce::Colours::white.withAlpha (0.35f), bore.getRight(), bore.getBottom(), false));
        g.fillEllipse (bore);
        g.setColour (C (0xff0b0b0b));
        g.fillEllipse (bore.reduced (1.6f * u));
    }
    return img;
}

void drawPedalLed (juce::Graphics& g, const PedalLayout& l, const PedalDesign& d, bool on)
{
    const C led (d.led);
    if (pedalPictured (d))
    {
        // the lens and its bezel are in the picture, dark: only the light itself is drawn
        if (! on) return;
        const float k = l.box.getHeight() / 420.0f;
        const float lr = 6.2f * k;
        const auto c = l.led.getCentre().translated (0.0f, -lr * 1.1f * art::rise);
        const auto lens = R (lr * 2.0f, lr * 2.0f * art::squash).withCentre (c);
        g.setGradientFill (juce::ColourGradient (led.withAlpha (0.5f), c.x, c.y, led.withAlpha (0.0f), c.x + 26.0f * k, c.y, true));
        g.fillEllipse (lens.expanded (26.0f * k));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x - lr * 0.3f, c.y - lr * 0.3f, led, lens.getRight(), lens.getBottom(), true));
        g.fillEllipse (lens);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.fillEllipse (R (lr * 0.5f, lr * 0.4f).withCentre (c.translated (-lr * 0.34f, -lr * 0.4f)));
        return;
    }
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
namespace
{
    constexpr float ampLabelH = 22.0f;

    // Eight controls, spaced the way the panel groups them, or stacked in two rows of four.
    void placeControls (AmpLayout& l, R strip, const AmpDesign& d, int rows)
    {
        static const int patterns[4][4] = { { 8, 0, 0, 0 }, { 1, 3, 2, 2 }, { 4, 4, 0, 0 }, { 1, 3, 4, 0 } };
        l.knobs = strip;
        l.cells.clear();
        l.slots.clear();
        if (rows == 2)
        {
            const float rowH = strip.getHeight() * 0.5f, cell = strip.getWidth() / 4.0f;
            l.knobSize = std::min (cell - 16.0f, rowH - ampLabelH - 6.0f);
            for (int i = 0; i < 8; ++i)
                l.cells.push_back (R (strip.getX() + cell * (float) (i % 4), strip.getY() + rowH * (float) (i / 4), cell, rowH)
                                       .withSizeKeepingCentre (l.knobSize, l.knobSize + ampLabelH));
            l.slots = l.cells;
            return;
        }
        const int* p = patterns[juce::jlimit (0, 3, d.groups)];
        int gaps = 0;
        for (int i = 1; i < 4; ++i) if (p[i] > 0) ++gaps;
        const float gapShare = 0.4f, smallShare = 0.7f;
        float units = gapShare * (float) gaps;
        for (int slot = 0; slot < 8; ++slot)
            units += (d.smallMask >> d.order[slot]) & 1 ? smallShare : 1.0f;
        const float cell = strip.getWidth() / units;
        l.knobSize = std::min (cell - 6.0f, strip.getHeight() - ampLabelH);
        l.cells.assign (8, R());
        l.slots.clear();
        float x = strip.getX();
        int slot = 0;
        const float bottom = strip.getCentreY() + (l.knobSize + ampLabelH) * 0.5f; // every label on one line
        for (int gi = 0; gi < 4 && p[gi] > 0; ++gi)
        {
            if (gi > 0) x += cell * gapShare;
            for (int j = 0; j < p[gi]; ++j, ++slot)
            {
                const int control = d.order[slot];
                const bool small = (d.smallMask >> control) & 1;
                const float cw = cell * (small ? smallShare : 1.0f), size = small ? l.knobSize * 0.66f : l.knobSize;
                const R r (x + (cw - size) * 0.5f, bottom - ampLabelH - size, size, size + ampLabelH);
                l.cells[(size_t) control] = r;
                l.slots.push_back (r);
                x += cw;
            }
        }
    }
}

AmpLayout ampLayout (R area, const AmpDesign& d)
{
    AmpLayout l;
    l.form = d.form;
    l.area = area;
    const float w = area.getWidth() * juce::jlimit (0.4f, 1.0f, d.width);
    // every amp stands on the same floor: the handle sits above the cabinet, the feet below it
    const auto full = R (w, area.getHeight() - 23.0f).withCentre ({ area.getCentreX(), area.getY() + 13.0f + (area.getHeight() - 23.0f) * 0.5f });
    auto controlCentre = [&l] { return l.cells.empty() ? l.knobs.getCentreY() : l.cells.front().getY() + l.knobSize * 0.5f; };
    const bool grouped = d.groups != 0;

    switch (d.form)
    {
        default:
        case 0: // head: controls across the top, cloth below
        {
            l.head = full;
            const auto h = l.head;
            l.panel = R (h.getX() + 24.0f, h.getY() + 20.0f, h.getWidth() - 48.0f, std::max (128.0f, h.getHeight() * (d.groups == 3 ? 0.38f : 0.42f)));
            placeControls (l, l.panel.reduced (18.0f, 8.0f).withTrimmedLeft (66.0f).withTrimmedRight (62.0f).withTrimmedTop (grouped ? 12.0f : 0.0f), d, 1);
            l.grille = R (h.getX() + 24.0f, l.panel.getBottom() + 16.0f, h.getWidth() - 48.0f, h.getBottom() - 22.0f - (l.panel.getBottom() + 16.0f));
            l.badge = R (l.grille.getX() + 34.0f, l.grille.getY() + 22.0f, 300.0f, std::min (70.0f, l.grille.getHeight() * 0.45f));
            break;
        }
        case 1: // head: a tall face with the name on it, controls along the bottom
        {
            l.head = full;
            const auto h = l.head;
            l.grille = R (h.getX() + 24.0f, h.getY() + 22.0f, h.getWidth() - 48.0f, h.getHeight() * 0.44f);
            l.panel = R (h.getX() + 24.0f, l.grille.getBottom() + 14.0f, h.getWidth() - 48.0f, h.getBottom() - 22.0f - (l.grille.getBottom() + 14.0f));
            placeControls (l, l.panel.reduced (18.0f, 8.0f).withTrimmedLeft (66.0f).withTrimmedRight (62.0f).withTrimmedTop (grouped ? 12.0f : 0.0f), d, 1);
            l.badge = R (l.grille.getX() + 44.0f, l.grille.getCentreY() - 38.0f, 380.0f, 76.0f);
            if (d.weave == Weave::none) // nothing but covering on this face: the name stands in the middle of it
                l.badge = R (std::min (420.0f, l.grille.getWidth() * 0.5f), std::min (112.0f, l.grille.getHeight() * 0.5f)).withCentre (l.grille.getCentre());
            break;
        }
        case 2: // combo: a narrow control strip on top, the speaker behind a big cloth
        {
            l.head = full;
            const auto h = l.head;
            l.panel = R (h.getX() + 22.0f, h.getY() + 18.0f, h.getWidth() - 44.0f, 108.0f);
            placeControls (l, l.panel.reduced (14.0f, 6.0f).withTrimmedLeft (58.0f).withTrimmedRight (56.0f), d, 1);
            l.grille = R (h.getX() + 22.0f, l.panel.getBottom() + 14.0f, h.getWidth() - 44.0f, h.getBottom() - 22.0f - (l.panel.getBottom() + 14.0f));
            l.badge = R (l.grille.getX() + 26.0f, l.grille.getY() + 18.0f, 240.0f, 52.0f);
            break;
        }
        case 3: // small metal head: vents over the valves, controls below
        {
            l.head = full.withTrimmedTop (full.getHeight() * 0.24f);
            const auto h = l.head;
            l.grille = R (h.getX() + 26.0f, h.getY() + 22.0f, h.getWidth() - 52.0f, h.getHeight() * 0.36f);
            l.panel = R (h.getX() + 20.0f, l.grille.getBottom() + 12.0f, h.getWidth() - 40.0f, h.getBottom() - 16.0f - (l.grille.getBottom() + 12.0f));
            placeControls (l, l.panel.reduced (16.0f, 6.0f).withTrimmedLeft (64.0f).withTrimmedRight (60.0f).withTrimmedTop (grouped ? 12.0f : 0.0f), d, 1);
            l.badge = R (l.grille.getX() + 18.0f, l.grille.getY() + 8.0f, 250.0f, l.grille.getHeight() - 16.0f);
            break;
        }
        case 4: // two rows of four big controls under a strip of cloth
        {
            l.head = full;
            const auto h = l.head;
            l.grille = R (h.getX() + 24.0f, h.getY() + 22.0f, h.getWidth() - 48.0f, h.getHeight() * 0.22f);
            l.panel = R (h.getX() + 24.0f, l.grille.getBottom() + 14.0f, h.getWidth() - 48.0f, h.getBottom() - 22.0f - (l.grille.getBottom() + 14.0f));
            placeControls (l, l.panel.reduced (18.0f, 10.0f).withTrimmedLeft (84.0f).withTrimmedRight (80.0f), d, 2);
            l.badge = R (l.grille.getX() + 28.0f, l.grille.getY() + 14.0f, 320.0f, l.grille.getHeight() - 28.0f);
            break;
        }
        case 5: // rack unit: one steel face with ears, a display, one row of controls
        {
            // bolted into a road case that stands on the same floor as every amp, between two blank panels
            l.rack = full;
            const auto bay = full.reduced (22.0f);
            const float hh = full.getHeight() * (d.groups == 0 ? 0.50f : 0.58f);
            l.head = R (bay.getWidth(), hh).withCentre (bay.getCentre());
            l.panel = l.head;
            const auto in = l.head.reduced (46.0f, 0.0f); // between the ears
            const float dw = in.getWidth() * 0.25f;
            l.grille = R (in.getX() + 74.0f, in.getY() + 34.0f, dw, in.getHeight() - 68.0f); // the display window
            placeControls (l, R (l.grille.getRight() + 26.0f, in.getY() + 8.0f, in.getRight() - 62.0f - (l.grille.getRight() + 26.0f), in.getHeight() - 16.0f)
                                  .withTrimmedTop (grouped ? 12.0f : 0.0f), d, 1);
            l.badge = R (l.grille.getX(), l.grille.getBottom() + 10.0f, dw, 18.0f);
            break;
        }
    }

    const float cy = d.form == 4 ? l.panel.getCentreY() - 11.0f : controlCentre();
    l.jack = { l.panel.getX() + (d.form == 5 ? 84.0f : d.form == 4 ? 60.0f : 52.0f), cy };
    l.jewel = R (24.0f, 24.0f).withCentre ({ l.panel.getRight() - (d.form == 4 ? 58.0f : d.form == 5 ? 82.0f : 42.0f), cy });
    l.handle = d.form == 5 ? R() : R (d.form == 3 ? 280.0f : 200.0f, 18.0f).withCentre ({ l.head.getCentreX(), l.head.getY() - 4.0f });
    return l;
}
namespace
{
    void tolexGrain (juce::Graphics& g, R r, const AmpDesign& d)
    {
        const C base (d.tolexColour);
        g.setColour (base);
        g.fillRect (r);
        auto rnd = seeded ((juce::int64) d.tolexColour + 3);
        const float area = r.getWidth() * r.getHeight();
        if (d.tolex == Tolex::tweed)
        {
            // diagonal twill: light and dark threads in alternating bands
            // fine, like the cloth: a light and a dark thread side by side, with a slow change of
            // shade every few threads where the weave is uneven
            int thread = 0;
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 2.1f, ++thread)
            {
                const float uneven = 0.05f * std::sin ((float) thread * 0.37f) + 0.04f * std::sin ((float) thread * 0.091f);
                g.setColour ((thread % 2 == 0 ? base.brighter (0.22f) : base.darker (0.5f)).withAlpha (0.42f + uneven));
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.15f);
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

    // The grain is tens of thousands of small marks. Drawn straight onto the amp, each one is
    // cut to the amp's rounded outline on its own, which made the amp the slowest thing in the
    // app to draw. They are drawn on a plain sheet at the same resolution instead, and the
    // sheet is laid on the amp once.
    void tolexTexture (juce::Graphics& g, R r, const AmpDesign& d)
    {
        // tweed is a few hundred threads that blend with what is under them: it stays as it was
        if (d.tolex == Tolex::tweed) { tolexGrain (g, r, d); return; }
        const float scale = juce::jlimit (0.05f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int w = (int) std::ceil (r.getWidth() * scale), h = (int) std::ceil (r.getHeight() * scale);
        if (w <= 0 || h <= 0) return;
        if ((juce::int64) w * h > 16 * 1024 * 1024) { tolexGrain (g, r, d); return; }
        juce::Image sheet (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics sg (sheet);
            sg.addTransform (juce::AffineTransform::scale (scale));
            tolexGrain (sg, r.withPosition (0.0f, 0.0f), d);
        }
        g.drawImageTransformed (sheet, juce::AffineTransform::scale (1.0f / scale).translated (r.getX(), r.getY()));
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
                {
                    // the bright thread: short runs of tinsel that surface along the weft, some of them blue
                    for (int i = 0; i < (int) (area / 70.0f); ++i)
                    {
                        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
                        const float y = r.getY() + 2.5f * std::floor (rnd.nextFloat() * r.getHeight() / 2.5f);
                        g.setColour ((rnd.nextInt (5) == 0 ? C (0xff9fc3e6) : C (0xffffffff)).withAlpha (0.10f + rnd.nextFloat() * 0.34f));
                        g.fillRect (x, y, 2.0f + rnd.nextFloat() * 4.0f, 1.0f);
                    }
                }
                else if (d.weave == Weave::wheat)
                {
                    // darker stripes woven in, a hand apart
                    for (float x = r.getX() + 9.0f; x < r.getRight(); x += 18.0f)
                    {
                        g.setColour (C (0xff3a2610).withAlpha (0.28f));
                        g.fillRect (x, r.getY(), 2.4f, r.getHeight());
                        g.setColour (C (0xffe8cf96).withAlpha (0.20f));
                        g.fillRect (x + 3.4f, r.getY(), 1.0f, r.getHeight());
                    }
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
            {
                // basket weave: pairs of pale threads over pairs of dark ones, turn and turn about
                g.setColour (C (0xff161616));
                g.fillRect (r);
                const float cell = 3.4f;
                int row = 0;
                for (float y = r.getY(); y < r.getBottom(); y += cell, ++row)
                {
                    int col = 0;
                    for (float x = r.getX(); x < r.getRight(); x += cell, ++col)
                    {
                        const float v = 0.75f + 0.25f * rnd.nextFloat();
                        if ((row + col) % 2 == 0)
                        {
                            g.setColour (C (0xffd9d5c8).withAlpha (0.50f * v));
                            g.fillRect (x, y + 0.4f, cell, 0.9f);
                            g.fillRect (x, y + 2.0f, cell, 0.9f);
                        }
                        else
                        {
                            g.setColour (C (0xff6a685f).withAlpha (0.55f * v));
                            g.fillRect (x + 0.4f, y, 0.9f, cell);
                            g.fillRect (x + 2.0f, y, 0.9f, cell);
                        }
                    }
                }
                break;
            }
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

namespace
{
    // A leather strap handle bowed up between two metal end caps. We look at it from the front and a
    // little above, so its top face shows as a band along the bow, stitched down both edges.
    void strapHandle (juce::Graphics& g, R s, float cabinetTop)
    {
        const float base = cabinetTop - 1.0f, peak = s.getY() + 2.0f;
        const float x0 = s.getX() + 8.0f, x1 = s.getRight() - 8.0f;
        auto bow = [&] (float lift)
        {
            juce::Path p;
            p.startNewSubPath (x0, base - lift);
            p.quadraticTo (s.getCentreX(), 2.0f * peak - base - lift, x1, base - lift);
            return p;
        };
        const auto edge = bow (0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); // its shadow on the cabinet top
        g.strokePath (edge, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (1.5f, 3.0f));
        g.setColour (C (0xff0b0908)); // the cut edge of the leather, toward us
        g.strokePath (edge, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (C (0xff3b2c20)); // the top face
        g.strokePath (bow (2.2f), juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (C (0xff8a6d52).withAlpha (0.7f)); // where the light runs along its back edge
        g.strokePath (bow (4.4f), juce::PathStrokeType (1.0f));
        {
            // stitching: pale thread in short dashes along the top face
            const float dashes[] = { 3.0f, 2.6f };
            juce::Path stitched;
            juce::PathStrokeType (0.9f).createDashedStroke (stitched, bow (2.2f), dashes, 2);
            g.setColour (C (0xffd9c7a6).withAlpha (0.75f));
            g.fillPath (stitched);
        }
        for (auto x : { s.getX() - 6.0f, s.getRight() - 20.0f })
        {
            const R capR (x, base - 9.0f, 26.0f, 11.0f);
            juce::Path cp;
            cp.addRoundedRectangle (capR, 3.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 4, { 1, 2 }).drawForPath (g, cp);
            fillMetal (g, capR, C (0xffc2c6c9), 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (capR, 3.0f, 0.8f);
            screw (g, capR.getCentre(), 4.5f, 0.4f + x * 0.01f);
        }
    }

    // A steel tube handle on two posts, the kind bolted to a small metal amp.
    void barHandle (juce::Graphics& g, R s, float cabinetTop)
    {
        const float top = cabinetTop - 24.0f;
        for (auto x : { s.getX() + 10.0f, s.getRight() - 22.0f })
        {
            juce::ColourGradient post (C (0xff3c3f42), x, 0, C (0xff111213), x + 12.0f, 0, false);
            post.addColour (0.35, C (0xffa9adb1));
            g.setGradientFill (post);
            g.fillRect (x, top + 5.0f, 12.0f, cabinetTop - top - 3.0f);
            fillMetal (g, R (x - 5.0f, cabinetTop - 5.0f, 22.0f, 6.0f), C (0xff6d7175), 2.0f);
        }
        const R bar (s.getX(), top, s.getWidth(), 11.0f);
        juce::ColourGradient tube (C (0xff8f9498), 0, bar.getY(), C (0xff101112), 0, bar.getBottom(), false);
        tube.addColour (0.25, C (0xffe6e8ea));
        tube.addColour (0.55, C (0xff4a4d50));
        g.setGradientFill (tube);
        g.fillRoundedRectangle (bar, 5.5f);
        // a rubber grip in the middle
        const auto grip = bar.withSizeKeepingCentre (bar.getWidth() * 0.46f, 13.0f);
        g.setGradientFill (juce::ColourGradient (C (0xff2c2c2c), 0, grip.getY(), C (0xff070707), 0, grip.getBottom(), false));
        g.fillRoundedRectangle (grip, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        for (float x = grip.getX() + 8.0f; x < grip.getRight() - 6.0f; x += 7.0f)
            g.fillRect (x, grip.getY() + 2.0f, 1.2f, grip.getHeight() - 4.0f);
    }

    juce::Path wordPath (const juce::Font& f, const juce::String& text)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, text, 0.0f, 0.0f);
        juce::Path p;
        ga.createPath (p);
        return p;
    }

    // Sizes a shape to sit inside a box, against its left edge or in its middle.
    void seat (juce::Path& p, R box, bool centred)
    {
        const auto b = p.getBounds();
        if (b.isEmpty() || box.isEmpty()) return;
        const float k = std::min (box.getWidth() / b.getWidth(), box.getHeight() / b.getHeight());
        const float w = b.getWidth() * k, h = b.getHeight() * k;
        p.applyTransform (juce::AffineTransform::translation (-b.getX(), -b.getY()).scaled (k)
                              .translated (centred ? box.getCentreX() - w * 0.5f : box.getX(), box.getCentreY() - h * 0.5f));
    }

    // What stands up off an amp's face, handed to the 3D renderer as outlines instead of being drawn:
    // kind 0 plated metal, 1 moulded plastic, 2 enamel, 3 a screw head.
    struct Relief { juce::Path path; C colour; float depth; int kind; float lift; };
    std::vector<Relief>* reliefSink = nullptr;
    void relief (const juce::Path& p, C colour, float depth, int kind, float lift = 0.0f)
    {
        if (reliefSink != nullptr) reliefSink->push_back ({ p, colour, depth, kind, lift });
    }
    void reliefScrew (juce::Point<float> c, float size)
    {
        juce::Path p;
        p.addEllipse (R (size, size).withCentre (c));
        relief (p, C (0xffc9ccd0), 1.0f, 3);
    }
    juce::Path ring (const juce::Path& outer, const juce::Path& inner)
    {
        juce::Path p (outer);
        p.addPath (inner);
        p.setUsingNonZeroWinding (false);
        return p;
    }

    // A shape standing up off the surface: its wall on the shaded side, its face lit from above.
    void raised (juce::Graphics& g, const juce::Path& p, C top, C bottom, float depth, int kind = 0, float lift = 0.0f)
    {
        if (reliefSink != nullptr) { relief (p, top.interpolatedWith (bottom, 0.35f), depth, kind, lift); return; }
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), juce::jmax (2, juce::roundToInt (depth * 2.5f)),
                          { juce::jmax (1, juce::roundToInt (depth)), juce::jmax (1, juce::roundToInt (depth * 2.0f)) }).drawForPath (g, p);
        g.setColour (bottom.darker (0.75f));
        for (float k = depth; k > 0.0f; k -= 0.5f)
            g.fillPath (p, juce::AffineTransform::translation (k * 0.7f, k));
        const auto b = p.getBounds();
        g.setGradientFill (juce::ColourGradient (top, 0, b.getY(), bottom, 0, b.getBottom(), false));
        g.fillPath (p);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (p);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.strokePath (p, juce::PathStrokeType (1.0f), juce::AffineTransform::translation (0.5f, 0.7f));
    }

    // Each maker puts its name on the front its own way.
    enum class Logo { script, plate, rules, moulded, pennant, milled, printed, enamel };
    Logo logoFor (const AmpDesign& d)
    {
        const juce::String b (d.badge);
        if (b == "glass")        return Logo::script;
        if (b == "chime")        return Logo::rules;
        if (b == "SOVEREIGN")    return Logo::moulded;
        if (b == "redline")      return Logo::pennant;
        if (b == "MODERN")       return Logo::milled;
        if (b == "TIGHT")        return Logo::printed;
        if (b == "bass station") return Logo::enamel;
        return Logo::plate;
    }

    void ampBadge (juce::Graphics& g, const AmpDesign& d, R area)
    {
        const juce::String text = d.upper ? juce::String (d.badge).toUpperCase() : juce::String (d.badge);
        switch (logoFor (d))
        {
            case Logo::script:
            {
                // written by hand, cast in one piece and nickel plated
                auto word = wordPath (faceFont (Face::script, 100.0f), titled (text));
                seat (word, area, false);
                raised (g, word, C (0xfff6f7f7), C (0xff8b9096), std::max (1.0f, area.getHeight() * 0.022f));
                return;
            }
            case Logo::moulded:
            {
                // a brush-written name moulded in white plastic and pinned to the covering
                auto word = wordPath (faceFont (Face::brush, 100.0f), titled (text));
                seat (word, area, true);
                raised (g, word, C (0xfffbf9f1), C (0xffd6d0bf), std::max (1.2f, area.getHeight() * 0.03f), 1);
                return;
            }
            case Logo::rules:
            {
                // brass capitals set wide between two brass rules, straight onto the cloth
                auto f = faceFont (Face::instrumentSemi, 100.0f);
                f.setExtraKerningFactor (0.42f);
                auto word = wordPath (f, text.toUpperCase());
                const auto band = area.withSizeKeepingCentre (area.getWidth(), area.getHeight() * 0.44f);
                seat (word, band, false);
                const auto wb = word.getBounds();
                juce::Path all = word;
                const float gap = area.getHeight() * 0.2f, th = std::max (1.6f, area.getHeight() * 0.045f);
                all.addRectangle (wb.getX(), wb.getY() - gap - th, wb.getWidth(), th);
                all.addRectangle (wb.getX(), wb.getBottom() + gap, wb.getWidth(), th);
                raised (g, all, C (0xfff3dc9f), C (0xffa07c33), 1.0f);
                return;
            }
            case Logo::pennant:
            {
                // an enamel pennant in a chromed rim, the name written across it
                const float h = std::min (area.getHeight(), area.getWidth() * 0.27f);
                const auto b = R (area.getX(), area.getCentreY() - h * 0.5f, std::min (area.getWidth(), h * 3.9f), h);
                auto flag = [] (R r)
                {
                    juce::Path p;
                    const float nose = r.getHeight() * 0.5f;
                    p.startNewSubPath (r.getX(), r.getY());
                    p.lineTo (r.getRight() - nose, r.getY());
                    p.lineTo (r.getRight(), r.getCentreY());
                    p.lineTo (r.getRight() - nose, r.getBottom());
                    p.lineTo (r.getX(), r.getBottom());
                    p.lineTo (r.getX() + nose * 0.45f, r.getCentreY());
                    p.closeSubPath();
                    return p.createPathWithRoundedCorners (r.getHeight() * 0.07f);
                };
                const auto rim = flag (b);
                if (reliefSink != nullptr)
                {
                    const float e = std::max (2.0f, h * 0.07f);
                    const auto inner = flag (b.reduced (e * 1.9f, e));
                    relief (ring (rim, inner), C (0xffd9dcde), 3.0f, 0);
                    relief (inner, C (0xff8e1d15), 2.0f, 2);
                    auto name = wordPath (faceFont (Face::script, 100.0f), titled (text));
                    seat (name, R (b.getX() + h * 0.42f, b.getY() + h * 0.2f, b.getWidth() - h * 1.1f, h * 0.6f), true);
                    relief (name, C (0xfff4f4f1), 0.9f, 1, 2.0f);
                    return;
                }
                juce::DropShadow (juce::Colours::black.withAlpha (0.65f), 6, { 1, 3 }).drawForPath (g, rim);
                juce::ColourGradient chrome (C (0xfffafbfb), 0, b.getY(), C (0xff5d6267), 0, b.getBottom(), false);
                chrome.addColour (0.5, C (0xffb9bdc1));
                g.setGradientFill (chrome);
                g.fillPath (rim);
                const float edge = std::max (2.0f, h * 0.07f);
                const auto field = flag (b.reduced (edge * 1.9f, edge));
                g.setGradientFill (juce::ColourGradient (C (0xffb92a1e), 0, b.getY(), C (0xff6a120d), 0, b.getBottom(), false));
                g.fillPath (field);
                {
                    juce::Graphics::ScopedSaveState ss (g);
                    g.reduceClipRegion (field);
                    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), 0, b.getY(), juce::Colours::white.withAlpha (0.0f), 0, b.getCentreY(), false));
                    g.fillRect (b.withHeight (h * 0.5f));
                }
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.strokePath (field, juce::PathStrokeType (0.8f));
                auto word = wordPath (faceFont (Face::script, 100.0f), titled (text));
                seat (word, R (b.getX() + h * 0.42f, b.getY() + h * 0.2f, b.getWidth() - h * 1.1f, h * 0.6f), true);
                raised (g, word, C (0xffffffff), C (0xffd2d5d8), 0.8f);
                return;
            }
            case Logo::milled:
            {
                // a thick black plate, the name milled through the anodising to the bare metal
                auto f = faceFont (Face::shoulders, 100.0f);
                f.setExtraKerningFactor (0.14f);
                auto word = wordPath (f, text.toUpperCase());
                const float h = std::min (area.getHeight(), 58.0f);
                seat (word, R (area.getX() + h * 0.62f, area.getCentreY() - h * 0.24f, area.getWidth() - h * 1.24f, h * 0.48f), false);
                const auto wb = word.getBounds();
                const auto plate = R (area.getX(), area.getCentreY() - h * 0.5f, wb.getRight() + h * 0.62f - area.getX(), h);
                juce::Path pp;
                pp.addRoundedRectangle (plate, 3.0f);
                if (reliefSink != nullptr)
                {
                    relief (pp, C (0xff1b1c1e), 3.0f, 0);
                    for (auto x : { plate.getX() + h * 0.3f, plate.getRight() - h * 0.3f })
                        reliefScrew ({ x, plate.getCentreY() }, std::max (7.0f, h * 0.17f));
                    g.setColour (C (0xffd9dcde));
                    g.fillPath (word);
                    return;
                }
                juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 7, { 1, 3 }).drawForPath (g, pp);
                g.setGradientFill (juce::ColourGradient (C (0xff2a2c2f), 0, plate.getY(), C (0xff0c0d0e), 0, plate.getBottom(), false));
                g.fillPath (pp);
                g.setColour (juce::Colours::white.withAlpha (0.28f)); // the chamfer catches the light along the top
                g.drawLine (plate.getX() + 3.0f, plate.getY() + 0.8f, plate.getRight() - 3.0f, plate.getY() + 0.8f, 1.2f);
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.drawRoundedRectangle (plate, 3.0f, 0.9f);
                for (auto x : { plate.getX() + h * 0.3f, plate.getRight() - h * 0.3f })
                    screw (g, { x, plate.getCentreY() }, std::max (3.5f, h * 0.085f), x * 0.21f);
                g.setColour (juce::Colours::black.withAlpha (0.85f)); // the cut's upper wall, in shade
                g.fillPath (word, juce::AffineTransform::translation (0.0f, -1.0f));
                g.setGradientFill (juce::ColourGradient (C (0xffb4b9bd), 0, wb.getY(), C (0xfff1f2f3), 0, wb.getBottom(), false));
                g.fillPath (word);
                return;
            }
            case Logo::printed:
            {
                // screen printed on the steel in one colour
                auto f = faceFont (Face::slab, 100.0f);
                f.setExtraKerningFactor (0.06f);
                auto word = wordPath (f, text.toUpperCase());
                seat (word, area.withSizeKeepingCentre (area.getWidth(), std::min (area.getHeight() * 0.62f, 46.0f)), false);
                g.setColour (C (d.ink));
                g.fillPath (word);
                const auto wb = word.getBounds();
                g.fillRect (wb.getX(), wb.getBottom() + wb.getHeight() * 0.22f, wb.getWidth(), std::max (1.5f, wb.getHeight() * 0.08f));
                return;
            }
            case Logo::enamel:
            {
                // a blue enamel plaque in a chromed frame
                auto word = wordPath (faceFont (Face::slab, 100.0f), text.toUpperCase());
                const float h = std::min (area.getHeight(), 50.0f);
                seat (word, R (area.getX() + h * 0.5f, area.getCentreY() - h * 0.21f, area.getWidth() - h, h * 0.42f), false);
                const auto wb = word.getBounds();
                const auto plate = R (area.getX(), area.getCentreY() - h * 0.5f, wb.getRight() + h * 0.5f - area.getX(), h);
                juce::Path pp;
                pp.addRoundedRectangle (plate, h * 0.16f);
                if (reliefSink != nullptr)
                {
                    const auto inner = plate.reduced (std::max (2.0f, h * 0.07f));
                    juce::Path ip;
                    ip.addRoundedRectangle (inner, h * 0.11f);
                    relief (ring (pp, ip), C (0xffd9dcde), 3.0f, 0);
                    relief (ip, C (0xff1f4c8a), 2.0f, 2);
                    g.setColour (C (0xfff6f4ee));
                    g.fillPath (word);
                    return;
                }
                juce::DropShadow (juce::Colours::black.withAlpha (0.65f), 6, { 1, 3 }).drawForPath (g, pp);
                fillMetal (g, plate, C (0xffc9cdd1), h * 0.16f);
                const auto field = plate.reduced (std::max (2.0f, h * 0.07f));
                g.setGradientFill (juce::ColourGradient (C (0xff2f67b4), 0, field.getY(), C (0xff163763), 0, field.getBottom(), false));
                g.fillRoundedRectangle (field, h * 0.11f);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.2f), 0, field.getY(), juce::Colours::white.withAlpha (0.0f), 0, field.getCentreY(), false));
                g.fillRoundedRectangle (field.withHeight (field.getHeight() * 0.5f), h * 0.11f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.drawRoundedRectangle (field, h * 0.11f, 0.8f);
                g.fillPath (word, juce::AffineTransform::translation (0.6f, 1.0f));
                g.setColour (C (0xfff6f4ee));
                g.fillPath (word);
                return;
            }
            case Logo::plate:
                break;
        }
        // an engraved metal plate: an oval of brass on the tweed, a screwed-on bar elsewhere
        {
            const bool oval = d.tolex == Tolex::tweed;
            auto f = faceFont (Face::shoulders, area.getHeight() * (oval ? 0.5f : 0.6f));
            f.setExtraKerningFactor (0.06f);
            const float tw = juce::GlyphArrangement::getStringWidth (f, text);
            const auto plate = R (std::min (area.getWidth(), tw + (oval ? 70.0f : 44.0f)), area.getHeight()).withPosition (area.getPosition());
            const float rad = oval ? plate.getHeight() * 0.5f : 5.0f;
            const C metal = oval ? C (0xffc9a550) : d.panel == Panel::gold ? C (0xffe7e1d2) : d.panel == Panel::brushed && d.tolex == Tolex::cream ? C (0xffd4b66a) : C (0xffd9dcde);
            juce::Path pp;
            pp.addRoundedRectangle (plate, rad);
            if (reliefSink != nullptr)
            {
                relief (pp, metal, 2.2f, 0);
                for (auto x : { plate.getX() + 12.0f, plate.getRight() - 12.0f })
                    if (! oval) reliefScrew ({ x, plate.getCentreY() }, 6.0f);
                g.setColour (C (0xff15130f).withAlpha (0.7f));
                g.drawRoundedRectangle (plate.reduced (5.0f), std::max (2.0f, rad - 5.0f), 1.0f);
                letter (g, text, plate.reduced (22.0f, 6.0f), f, C (0xff15130f), juce::Justification::centred);
                return;
            }
            juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 1, 3 }).drawForPath (g, pp);
            fillMetal (g, plate, metal, rad);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawRoundedRectangle (plate.reduced (0.6f), rad, 1.0f);
            // an engraved border, then the name cut into the metal
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (plate.reduced (5.0f), std::max (2.0f, rad - 5.0f), 1.0f);
            for (auto x : { plate.getX() + 12.0f, plate.getRight() - 12.0f })
                if (! oval) screw (g, { x, plate.getCentreY() }, 4.0f, x * 0.3f);
            const auto ta = plate.reduced (oval ? 22.0f : 22.0f, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.5f));
            g.setFont (f);
            g.drawText (text, ta.translated (0.0f, 1.0f), juce::Justification::centred, false);
            letter (g, text, ta, f, C (0xff15130f), juce::Justification::centred);
            return;
        }
    }

    // Slots punched in a steel case, with the dark of the chassis behind them.
    void vents (juce::Graphics& g, R area)
    {
        const float slotW = 30.0f, slotH = 6.0f, gapX = 12.0f, gapY = 9.0f;
        const int cols = std::max (1, (int) ((area.getWidth() + gapX) / (slotW + gapX)));
        const int rows = std::max (1, (int) ((area.getHeight() + gapY) / (slotH + gapY)));
        const float x0 = area.getCentreX() - ((float) cols * (slotW + gapX) - gapX) * 0.5f;
        const float y0 = area.getCentreY() - ((float) rows * (slotH + gapY) - gapY) * 0.5f;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                const R slot (x0 + (float) c * (slotW + gapX) + (r % 2 == 1 ? (slotW + gapX) * 0.5f - (c == cols - 1 ? 1.0e4f : 0.0f) : 0.0f),
                              y0 + (float) r * (slotH + gapY), slotW, slotH);
                if (slot.getRight() > area.getRight() + 1.0f) continue;
                g.setColour (juce::Colours::white.withAlpha (0.10f));
                g.fillRoundedRectangle (slot.translated (0.0f, 1.0f), 3.0f);
                g.setGradientFill (juce::ColourGradient (C (0xff000000), 0, slot.getY(), C (0xff17181a), 0, slot.getBottom(), false));
                g.fillRoundedRectangle (slot, 3.0f);
            }
    }

    // The fixed part of the power control: what stays put whether the amp is on or off.
    void lampHousing (juce::Graphics& g, const AmpLayout& l, const AmpDesign& d)
    {
        const auto jc = l.jewel.getCentre();
        if (d.lamp == 1)
        {
            hexNut (g, jc, 13.0f);
            // the threaded bush the lever comes out of: a turned collar with its threads, and the dark mouth
            const float bush = 6.6f;
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillEllipse (R (bush * 2.0f + 1.6f, bush * 2.0f + 1.6f).withCentre (jc.translated (0.3f, 0.7f)));
            conic (g, jc, bush, 0.0f, [] (float a) { return metalAt (C (0xffb2b6ba), a); }, 96);
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            for (float k : { 0.92f, 0.8f })
                g.drawEllipse (R (bush * 2.0f * k, bush * 2.0f * k).withCentre (jc), 0.55f);
            const auto mouth = R (7.4f, 7.4f).withCentre (jc);
            g.setGradientFill (juce::ColourGradient (C (0xff000000), mouth.getX(), mouth.getY(), C (0xff2c2e30), mouth.getRight(), mouth.getBottom(), false));
            g.fillEllipse (mouth);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawEllipse (mouth.expanded (0.5f), 0.5f);
            const auto lamp = R (11.0f, 11.0f).withCentre ({ jc.x, jc.y - 30.0f });
            conic (g, lamp.getCentre(), 8.0f, 0.0f, [] (float a) { return metalAt (C (0xffb9bdc0), a); }, 48);
            g.setColour (C (0xff0a0a0a));
            g.fillEllipse (lamp.expanded (0.5f));
            return;
        }
        if (d.lamp == 2)
        {
            const auto bezel = R (30.0f, 42.0f).withCentre (jc);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (bezel.translated (0.8f, 1.6f), 4.0f);
            g.setGradientFill (juce::ColourGradient (C (0xff3a3b3d), bezel.getX(), bezel.getY(), C (0xff0c0c0d), bezel.getRight(), bezel.getBottom(), false));
            g.fillRoundedRectangle (bezel, 4.0f);
            g.setColour (C (0xff050505));
            g.fillRoundedRectangle (bezel.reduced (3.0f), 2.5f);
            return;
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (l.jewel.expanded (6.5f).translated (0.6f, 1.4f));
        conic (g, jc, l.jewel.getWidth() * 0.5f + 5.5f, 0.0f, [] (float a) { return metalAt (C (0xffb9bdc0), a); }, 64);
        g.setColour (C (0xff0a0a0a));
        g.fillEllipse (l.jewel.expanded (1.0f));
    }

    // Thin printed lines that bracket each group of controls, with the group's name in the gap.
    void groupSilk (juce::Graphics& g, const AmpLayout& l, const AmpDesign& d)
    {
        if (d.groups == 0 || l.slots.size() < 8 || l.form == 4) return;
        static const int patterns[4][4] = { { 8, 0, 0, 0 }, { 1, 3, 2, 2 }, { 4, 4, 0, 0 }, { 1, 3, 4, 0 } };
        static const char* const names[4][4] = { { "", "", "", "" }, { "drive", "equaliser", "voicing", "output" },
                                                 { "preamp", "power amp", "", "" }, { "drive", "tone", "power amp", "" } };
        const C ink = C (d.ink).withAlpha (0.8f);
        auto f = faceFont (Face::instrumentSemi, 9.2f);
        f.setExtraKerningFactor (0.12f);
        const float y = l.knobs.getY() - 5.0f;
        int k = 0;
        for (int gi = 0; gi < 4 && patterns[d.groups][gi] > 0; ++gi)
        {
            const int n = patterns[d.groups][gi];
            const float x0 = l.slots[(size_t) k].getX() - 2.0f, x1 = l.slots[(size_t) (k + n - 1)].getRight() + 2.0f;
            const juce::String name = juce::String (names[d.groups][gi]).toUpperCase();
            const float tw = juce::GlyphArrangement::getStringWidth (f, name) + 12.0f;
            const float mid = (x0 + x1) * 0.5f;
            g.setColour (ink);
            if (x1 - x0 > tw + 16.0f)
            {
                g.fillRect (x0, y, mid - tw * 0.5f - x0, 1.0f);
                g.fillRect (mid + tw * 0.5f, y, x1 - (mid + tw * 0.5f), 1.0f);
                g.fillRect (x0, y, 1.0f, 5.0f);
                g.fillRect (x1 - 1.0f, y, 1.0f, 5.0f);
            }
            g.setFont (f);
            g.drawText (name, R (tw + 30.0f, 12.0f).withCentre ({ mid, y + 0.5f }), juce::Justification::centred, false);
            k += n;
        }
    }

    // Everything screen printed on the panel that is not under a knob.
    void ampSilk (juce::Graphics& g, const AmpDesign& d, const AmpLayout& l)
    {
        const float labelCy = d.form == 4 ? l.jack.y + 36.0f : (l.cells.empty() ? l.jack.y + 36.0f : l.cells.front().getBottom() - ampLabelH * 0.5f);
        const auto f = faceFont (d.face == Face::shoulders ? Face::shoulders : Face::instrument, d.face == Face::shoulders ? 12.0f : 11.4f);
        auto label = [&] (const char* t, float cx)
        {
            const juce::String text = d.upper ? juce::String (t).toUpperCase() : juce::String (t);
            // on the same line as the names under the knobs
            const bool withKnobs = d.form != 4 && ! l.cells.empty();
            const auto box = withKnobs ? R (cx - 40.0f, l.cells.front().getBottom() - ampLabelH + 1.0f, 80.0f, ampLabelH)
                                       : R (80.0f, 16.0f).withCentre ({ cx, labelCy });
            letter (g, text, box, f, C (d.ink), withKnobs ? juce::Justification::centredTop : juce::Justification::centred);
        };
        label ("input", l.jack.x);
        label ("power", l.jewel.getCentreX());
        groupSilk (g, l, d);
    }
}

static juce::String ampArtName (const AmpDesign& d) { return "amp_" + art::slug (d.badge); }

bool ampPictured (const AmpDesign& d) { return art::has (ampArtName (d)); }

// The photograph shows a little more than the amp's own place: the top of the cabinet, and its shadow.
R ampArtArea (R area) { return area.expanded (36.0f, 0.0f).withTop (area.getY() - 16.0f).withBottom (area.getBottom() + 36.0f); }

juce::Image renderAmpPrint (const AmpDesign& d, const AmpLayout& l, R area, float scale, juce::var& raisedParts)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    ampSilk (g, d, l);
    std::vector<Relief> parts;
    reliefSink = &parts;
    if (d.form != 5) ampBadge (g, d, l.badge);
    reliefSink = nullptr;
    juce::Array<juce::var> list;
    for (const auto& r : parts)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("path", r.path.toString());
        o->setProperty ("colour", juce::String::toHexString ((juce::int64) (r.colour.getARGB() & 0xffffff)).paddedLeft ('0', 6));
        o->setProperty ("depth", r.depth);
        o->setProperty ("kind", r.kind);
        o->setProperty ("lift", r.lift);
        list.add (juce::var (o));
    }
    raisedParts = list;
    return img;
}

juce::Image renderAmp (const AmpDesign& d, const AmpLayout& l, R area, float scale, const juce::String& displayText)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    // photographed at the size the amp page shows it: any other size is drawn
    const bool rightSize = std::abs (l.area.getWidth() - ampCanvasW) < 0.5f && std::abs (l.area.getHeight() - ampCanvasH) < 0.5f;
    if (rightSize && ampPictured (d) && art::draw (g, ampArtName (d), ampArtArea (l.area)))
    {
        if (d.form == 5)
            letter (g, displayText.isNotEmpty() ? displayText : juce::String (d.badge), l.grille.reduced (14.0f, 0.0f),
                    faceFont (Face::doto, std::min (30.0f, l.grille.getHeight() * 0.36f)), C (d.jewel), juce::Justification::centred);
        return img;
    }
    const auto h = l.head;
    const bool cabinet = d.form != 3 && d.form != 5;
    const float corner = cabinet ? 10.0f : d.form == 3 ? 12.0f : 3.0f;

    const auto outer = d.form == 5 ? l.rack : h; // what stands on the floor
    {
        juce::Path p;
        p.addRoundedRectangle (outer.translated (0.0f, 8.0f), d.form == 5 ? 10.0f : corner);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 30, { 0, 12 }).drawForPath (g, p);
    }
    if (d.form == 5)
    {
        // the road case: covered ply with an aluminium lip round the opening, and a blank panel above and below the unit
        juce::Path box;
        box.addRoundedRectangle (outer, 10.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (box);
            AmpDesign skin = d;
            skin.tolex = Tolex::levant;
            skin.tolexColour = 0xff141516;
            tolexTexture (g, outer, skin);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.12f), 0, outer.getY(), juce::Colours::white.withAlpha (0.0f), 0, outer.getY() + 14.0f, false));
            g.fillRect (outer.withHeight (14.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), 0, outer.getBottom() - 16.0f, juce::Colours::black.withAlpha (0.45f), 0, outer.getBottom(), false));
            g.fillRect (outer.withTop (outer.getBottom() - 16.0f));
        }
        const auto bay = outer.reduced (22.0f);
        fillMetal (g, bay.expanded (5.0f), C (0xffa9adb1), 2.0f);
        g.setColour (C (0xff060606));
        g.fillRect (bay);
        for (int i = 0; i < 2; ++i)
        {
            const R blank = i == 0 ? bay.withBottom (h.getY() - 2.0f) : bay.withTop (h.getBottom() + 2.0f);
            g.setGradientFill (juce::ColourGradient (C (0xff2b2d30), 0, blank.getY(), C (0xff17181a), 0, blank.getBottom(), false));
            g.fillRect (blank);
            finishTexture (g, blank, Finish::anodised, C (0xff222427), 41 + i);
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRect (blank.withHeight (1.0f));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRect (blank.withTop (blank.getBottom() - 1.0f));
            if (i == 0)
            {
                // the top one is a vent: three rows of slots over the valves' heat
                const auto vent = blank.reduced (86.0f, 0.0f).withSizeKeepingCentre (blank.getWidth() - 172.0f, std::min (30.0f, blank.getHeight() - 22.0f));
                for (int row = 0; row < 3; ++row)
                    for (float x = vent.getX(); x + 34.0f <= vent.getRight(); x += 44.0f)
                    {
                        const R slot (x, vent.getY() + vent.getHeight() * ((float) row + 0.5f) / 3.0f - 2.0f, 34.0f, 4.0f);
                        g.setColour (C (0xff030303));
                        g.fillRoundedRectangle (slot, 2.0f);
                        g.setColour (juce::Colours::white.withAlpha (0.09f));
                        g.fillRoundedRectangle (slot.translated (0.0f, 4.0f).withHeight (1.0f), 0.5f);
                    }
            }
            for (int side = 0; side < 2; ++side)
            {
                const juce::Point<float> c (side == 0 ? blank.getX() + 23.0f : blank.getRight() - 23.0f, blank.getCentreY());
                g.setColour (C (0xff050505));
                g.fillRoundedRectangle (R (24.0f, 12.0f).withCentre (c), 6.0f);
                screw (g, c, 10.0f, 1.3f * (float) (i * 2 + side));
            }
        }
        for (int i = 0; i < 4; ++i)
            cabinetCorner (g, outer, i % 2 == 1, i >= 2, 34.0f);
    }
    if (d.form == 3) barHandle (g, l.handle, h.getY());
    else if (d.form != 5) strapHandle (g, l.handle, h.getY());

    juce::Path shell;
    shell.addRoundedRectangle (h, corner);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shell);
        if (cabinet)
            tolexTexture (g, h, d);
        else
        {
            // a folded steel case in black wrinkle paint
            g.setGradientFill (juce::ColourGradient (C (0xff2a2c2e), 0, h.getY(), C (0xff0d0e0f), 0, h.getBottom(), false));
            g.fillRect (h);
            finishTexture (g, h, Finish::crinkle, C (0xff1c1d1f), 31);
        }
        // the rounded edges catch the light
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.12f), 0, h.getY(), juce::Colours::white.withAlpha (0.0f), 0, h.getY() + 14.0f, false));
        g.fillRect (h.withHeight (14.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), 0, h.getBottom() - 16.0f, juce::Colours::black.withAlpha (0.45f), 0, h.getBottom(), false));
        g.fillRect (h.withTop (h.getBottom() - 16.0f));
    }
    // rubber feet, then whatever protects the corners
    for (auto x : { outer.getX() + 64.0f, outer.getRight() - 64.0f - 54.0f })
    {
        g.setGradientFill (juce::ColourGradient (C (0xff1c1c1c), 0, outer.getBottom(), C (0xff070707), 0, outer.getBottom() + 9.0f, false));
        g.fillRoundedRectangle (x, outer.getBottom() - 2.0f, 54.0f, 10.0f, 3.0f);
    }
    if (cabinet)
        for (int i = 0; i < 4; ++i)
            cabinetCorner (g, h, i % 2 == 1, i >= 2, 34.0f);
    if (d.form == 3)
        for (int i = 0; i < 4; ++i) // the case is held together by a bolt at each corner
            screw (g, { i % 2 == 0 ? h.getX() + 11.0f : h.getRight() - 11.0f, i < 2 ? h.getY() + 11.0f : h.getBottom() - 11.0f }, 6.5f, 0.7f * (float) i);
    panelFinish (g, l.panel, d);
    if (d.form != 5)
        for (auto x : { l.panel.getX() + 10.0f, l.panel.getRight() - 10.0f })
            screw (g, { x, l.panel.getY() + 12.0f }, 6.0f, 0.6f);
    else
        for (int side = 0; side < 2; ++side)
        {
            // rack ears: the ends of the same plate, each with two slots and bolts, set off by a pressed line
            const R ear (side == 0 ? h.getX() : h.getRight() - 46.0f, h.getY(), 46.0f, h.getHeight());
            const float edge = side == 0 ? ear.getRight() : ear.getX();
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRect (edge - 0.5f, ear.getY() + 3.0f, 1.0f, ear.getHeight() - 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.fillRect (edge + 0.6f, ear.getY() + 3.0f, 1.0f, ear.getHeight() - 6.0f);
            for (float fy : { 0.2f, 0.8f })
            {
                const juce::Point<float> c (ear.getCentreX(), ear.getY() + ear.getHeight() * fy);
                g.setColour (C (0xff050505));
                g.fillRoundedRectangle (R (24.0f, 12.0f).withCentre (c), 6.0f);
                screw (g, c, 10.0f, fy * 3.0f + (float) side);
            }
        }
    // what is printed and fitted on the panel beside the controls: the input socket and the power switch
    jackSocket (g, l.jack, l.knobSize < 62.0f ? 13.0f : 15.0f);
    lampHousing (g, l, d);
    ampSilk (g, d, l);

    // the face above or below the controls
    if (d.form == 3)
        vents (g, l.grille.withTrimmedLeft (l.badge.getWidth() + 40.0f));
    else if (d.form == 5)
        drawWell (g, l.grille, 8.0f);
    else if (d.weave != Weave::none)
    {
        juce::Path cloth;
        cloth.addRoundedRectangle (l.grille, 6.0f);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (cloth);
            weave (g, l.grille, d);
            if (d.form == 2)
            {
                // the speaker behind the cloth shows as a darker round
                const int n = l.grille.getWidth() > l.grille.getHeight() * 3.0f ? 2 : 1;
                const float r = std::min (l.grille.getHeight() * 0.44f, l.grille.getWidth() / (float) (2 * n) * 0.86f);
                for (int i = 0; i < n; ++i)
                {
                    const juce::Point<float> c (l.grille.getX() + l.grille.getWidth() * ((float) i + 0.5f) / (float) n, l.grille.getCentreY() + 8.0f);
                    // the hole in the baffle has an edge, and the cone, its ribs and the dust cap show faintly through
                    const auto hole = R (r * 2.0f, r * 2.0f).withCentre (c);
                    g.setColour (juce::Colours::black.withAlpha (0.36f));
                    g.fillEllipse (hole);
                    g.setColour (juce::Colours::black.withAlpha (0.3f));
                    g.drawEllipse (hole, 1.6f);
                    g.setColour (juce::Colours::white.withAlpha (0.07f));
                    g.drawEllipse (hole.reduced (r * 0.06f), r * 0.05f); // the frame's rim
                    juce::ColourGradient cone (juce::Colours::black.withAlpha (0.3f), c.x, c.y, juce::Colours::black.withAlpha (0.0f), c.x + r * 0.84f, c.y, true);
                    cone.addColour (0.3, juce::Colours::black.withAlpha (0.22f));
                    g.setGradientFill (cone);
                    g.fillEllipse (hole.reduced (r * 0.16f));
                    g.setColour (juce::Colours::black.withAlpha (0.1f));
                    for (float k = 0.42f; k < 0.8f; k += 0.12f)
                        g.drawEllipse (R (r * 2.0f * k, r * 2.0f * k).withCentre (c), 1.0f);
                    const auto cap = R (r * 0.56f, r * 0.56f).withCentre (c);
                    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.1f), cap.getX() + cap.getWidth() * 0.3f, cap.getY() + cap.getHeight() * 0.25f,
                                                             juce::Colours::black.withAlpha (0.2f), cap.getRight(), cap.getBottom(), true));
                    g.fillEllipse (cap);
                }
            }
            // the cabinet's edge shades the top of the cloth
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.4f), 0, l.grille.getY(), juce::Colours::black.withAlpha (0.0f), 0, l.grille.getY() + 18.0f, false));
            g.fillRect (l.grille.withHeight (18.0f));
        }
        if (d.piping != 0)
        {
            g.setColour (C (d.piping));
            g.strokePath (cloth, juce::PathStrokeType (2.2f));
        }
    }
    else if (d.piping != 0)
    {
        // no cloth: the covering runs across the front, framed by a cord of piping
        const auto frame = l.grille.reduced (16.0f, 14.0f); // inside the corner protectors, so its own corners show
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (frame.translated (0.6f, 1.2f), 9.0f, 2.6f);
        g.setGradientFill (juce::ColourGradient (C (d.piping).brighter (0.25f), frame.getX(), frame.getY(), C (d.piping).darker (0.35f), frame.getRight(), frame.getBottom(), false));
        g.drawRoundedRectangle (frame, 9.0f, 2.2f);
    }

    // one soft light over the whole thing
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (shell);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), h.getCentreX(), h.getY(),
                                                 juce::Colours::black.withAlpha (0.18f), h.getX(), h.getBottom(), true));
        g.fillRect (h);
    }
    if (d.form == 5)
    {
        const auto ink = C (d.jewel);
        letter (g, displayText.isNotEmpty() ? displayText : juce::String (d.badge), l.grille.reduced (14.0f, 0.0f),
                faceFont (Face::doto, std::min (30.0f, l.grille.getHeight() * 0.36f)), ink, juce::Justification::centred);
    }
    else
        ampBadge (g, d, l.badge);
    return img;
}

void drawAmpJewel (juce::Graphics& g, const AmpLayout& l, const AmpDesign& d, bool on)
{
    const C jewel (d.jewel);
    // a lit lamp is hottest in the middle: a red one goes to orange there, never to pink
    const bool red = jewel.getHue() < 0.06f || jewel.getHue() > 0.94f;
    const C hot = red ? jewel.interpolatedWith (C (0xffffb46a), 0.7f) : jewel.interpolatedWith (juce::Colours::white, 0.6f);
    auto r = l.jewel;
    // valves glowing behind steel mesh or vents
    if (on && l.form != 5 && l.form != 2 && (d.weave == Weave::mesh || l.form == 3))
    {
        juce::Graphics::ScopedSaveState s (g);
        const auto win = l.form == 3 ? l.grille.withTrimmedLeft (l.badge.getWidth() + 40.0f) : l.grille;
        g.reduceClipRegion (win.toNearestInt());
        const int n = 4;
        const float gr = win.getHeight() * 0.62f;
        for (int i = 0; i < n; ++i)
        {
            const juce::Point<float> c (win.getX() + win.getWidth() * (0.2f + 0.2f * (float) i), win.getBottom() - win.getHeight() * 0.28f);
            g.setGradientFill (juce::ColourGradient (C (0xffff9a3c).withAlpha (0.20f), c.x, c.y, C (0xffff7a1c).withAlpha (0.0f), c.x + gr, c.y, true));
            g.fillEllipse (R (gr * 2.0f, gr * 2.0f).withCentre (c));
        }
    }
    if (d.lamp == 1)
    {
        // a bat-handle toggle: up is on. Its little lamp sits above it. The lever is a turned
        // chrome rod that thickens to a ball; seen from above it leans toward the top or the bottom.
        const auto c = r.getCentre();
        const float dir = on ? -1.0f : 1.0f;
        const float len = 17.0f, baseW = 4.4f, neckW = 3.3f, ball = 4.5f;
        const juce::Point<float> tip (c.x + 0.4f, c.y + dir * len);
            // its shadow on the bush and the panel: soft round pools along the rod and under the ball,
        // each fading to nothing, so the shadow has no edge anywhere
        {
            const juce::Point<float> off (2.6f, 3.4f);
            for (int i = 0; i <= 6; ++i)
            {
                const float t = (float) i / 6.0f;
                const auto p = c + (tip - c) * t + off;
                const float rad = (i == 6 ? ball * 1.9f : 3.6f + 1.4f * t);
                juce::ColourGradient pool (juce::Colours::black.withAlpha (i == 6 ? 0.26f : 0.1f), p.x, p.y,
                                           juce::Colours::black.withAlpha (0.0f), p.x + rad, p.y, true);
                g.setGradientFill (pool);
                g.fillEllipse (R (rad * 2.0f, rad * 2.0f).withCentre (p));
            }
        }
        // the rod: round across, so a bright streak toward the light and a dark edge away from it
        {
            juce::Path shaft;
            const float ny = tip.y - dir * ball * 0.6f;
            shaft.startNewSubPath (c.x - baseW * 0.5f, c.y);
            shaft.lineTo (tip.x - neckW * 0.5f, ny);
            shaft.lineTo (tip.x + neckW * 0.5f, ny);
            shaft.lineTo (c.x + baseW * 0.5f, c.y);
            shaft.closeSubPath();
            juce::ColourGradient chrome (C (0xff4d5155), c.x - baseW * 0.5f, 0.0f, C (0xff3e4144), c.x + baseW * 0.5f, 0.0f, false);
            chrome.addColour (0.18, C (0xffd9dcde));
            chrome.addColour (0.3, C (0xffffffff));
            chrome.addColour (0.45, C (0xffc3c6c9));
            chrome.addColour (0.72, C (0xff7c8084));
            chrome.addColour (0.88, C (0xffa1a5a8)); // light thrown back from the panel
            g.setGradientFill (chrome);
            g.fillPath (shaft);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.strokePath (shaft, juce::PathStrokeType (0.5f));
            // where it comes out of the bush, a ring of shade
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (R (baseW + 1.4f, 2.2f).withCentre (c));
        }
        // the ball on the end: a polished sphere, its highlight toward the light
        {
            const auto bb = R (ball * 2.0f, ball * 2.0f * art::squash).withCentre (tip);
            juce::ColourGradient sphere (C (0xffffffff), bb.getX() + bb.getWidth() * 0.34f, bb.getY() + bb.getHeight() * 0.3f,
                                         C (0xff3f4245), bb.getRight() + ball * 0.2f, bb.getBottom() + ball * 0.2f, true);
            sphere.addColour (0.18, C (0xffe9ebed));
            sphere.addColour (0.5, C (0xffa7abaf));
            sphere.addColour (0.82, C (0xff5f6367));
            g.setGradientFill (sphere);
            g.fillEllipse (bb);
            // the panel's light caught low on the far side
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.drawEllipse (bb.reduced (0.7f).translated (0.4f, 0.5f), 0.6f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawEllipse (bb, 0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.95f));
            g.fillEllipse (R (1.7f, 1.4f).withCentre ({ bb.getX() + bb.getWidth() * 0.33f, bb.getY() + bb.getHeight() * 0.3f }));
        }
        r = R (9.0f, 9.0f).withCentre ({ c.x, c.y - 30.0f });
    }
    else if (d.lamp == 2)
    {
        // a rocker: the pressed half sinks, and the whole paddle lights when on
        const auto face = R (24.0f, 36.0f).withCentre (r.getCentre()).reduced (1.0f);
        const C lit = on ? jewel : jewel.darker (2.6f).withMultipliedSaturation (0.6f);
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (jewel.withAlpha (0.35f), face.getCentreX(), face.getCentreY(), jewel.withAlpha (0.0f), face.getCentreX() + 40.0f, face.getCentreY(), true));
            g.fillEllipse (face.expanded (30.0f));
        }
        juce::ColourGradient paddle (on ? lit.darker (0.35f) : lit.brighter (0.25f), 0, face.getY(), on ? lit.interpolatedWith (hot, 0.55f) : lit.darker (0.5f), 0, face.getBottom(), false);
        paddle.addColour (0.5, lit);
        g.setGradientFill (paddle);
        g.fillRoundedRectangle (face, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (face.getX(), face.getCentreY() - 0.5f, face.getWidth(), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.18f));
        g.fillRect (face.getX() + 2.0f, on ? face.getBottom() - 3.0f : face.getY() + 1.5f, face.getWidth() - 4.0f, 1.2f);
        // the I and O moulded into it
        g.setColour (juce::Colours::white.withAlpha (on ? 0.85f : 0.3f));
        g.fillRect (face.getCentreX() - 0.8f, face.getY() + 6.0f, 1.6f, 6.0f);
        g.drawEllipse (R (6.0f, 6.0f).withCentre ({ face.getCentreX(), face.getBottom() - 9.0f }), 1.2f);
        return;
    }
    if (on)
    {
        const float halo = r.getWidth() * 1.4f;
        g.setGradientFill (juce::ColourGradient (jewel.withAlpha (0.42f), r.getCentreX(), r.getCentreY(), jewel.withAlpha (0.0f), r.getCentreX() + halo, r.getCentreY(), true));
        g.fillEllipse (r.expanded (halo));
    }
    juce::ColourGradient glass (on ? hot : jewel.darker (2.0f), r.getCentreX() - r.getWidth() * 0.12f, r.getCentreY() - r.getWidth() * 0.12f,
                                on ? jewel.darker (0.35f) : jewel.darker (3.5f), r.getRight(), r.getBottom(), true);
    if (on) glass.addColour (0.45, jewel);
    g.setGradientFill (glass);
    g.fillEllipse (r);
    if (r.getWidth() > 16.0f)
    {
        // cut facets catch the light
        g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.15f));
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
            g.drawLine (juce::Line<float> (r.getCentre(), r.getCentre().getPointOnCircumference (r.getWidth() * 0.45f, a)), 0.6f);
        }
    }
    g.setColour (juce::Colours::white.withAlpha (on ? 0.8f : 0.3f));
    g.fillEllipse (R (r.getWidth() * 0.24f, r.getWidth() * 0.18f).withCentre (r.getCentre().translated (-r.getWidth() * 0.17f, -r.getWidth() * 0.2f)));
}

void drawPedalLed (juce::Graphics& g, const PedalLayout& l, const PedalDesign& d, float level)
{
    if (level <= 0.0f || level >= 1.0f) { drawPedalLed (g, l, d, level >= 1.0f); return; }
    drawPedalLed (g, l, d, false);
    g.beginTransparencyLayer (level);
    drawPedalLed (g, l, d, true);
    g.endTransparencyLayer();
}

void drawAmpJewel (juce::Graphics& g, const AmpLayout& l, const AmpDesign& d, float level)
{
    if (level <= 0.0f || level >= 1.0f) { drawAmpJewel (g, l, d, level >= 1.0f); return; }
    drawAmpJewel (g, l, d, false);
    g.beginTransparencyLayer (level);
    drawAmpJewel (g, l, d, true);
    g.endTransparencyLayer();
}

int ampLampMs (const AmpDesign& d, bool comingOn)
{
    if (d.lamp != 0) return 90; // a lever or a rocker: it is simply over
    return comingOn ? 240 : 420; // a filament behind coloured glass
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
            case 4: return { 2, 1, 12.0f, 0xff6d7176, 0xff121212, 1.75f };  // 2x12 American
            case 5: return { 1, 1, 8.0f,  0xff8c9196, 0xff2b2118, 1.05f };  // 1x8 tiny combo
            case 6: return { 2, 2, 10.0f, 0xff2a2c2f, 0xff1d2530, 1.0f };   // 4x10 bass
            case 7: return { 1, 1, 15.0f, 0xff2a2c2f, 0xff1d2530, 0.92f };  // 1x15 bass
            default: return { 1, 1, 12.0f, 0xff5d6166, 0xff161616, 1.6f };  // your IR: one speaker beside a display
        }
    }
    // What stands in front of the speakers, and whose box it is. Each cabinet belongs with one of
    // the amps and is dressed like it.
    struct CabLook
    {
        Weave weave;         // none = nothing over the speakers; mesh = a punched steel sheet
        juce::uint32 piping; // a cord round the cloth (0 = none)
        int amp;             // whose name is on it, -1 for no name
        float show;          // how much of the speakers reads through the cloth
        bool handle;         // a strap on top
    };
    CabLook cabLook (int m)
    {
        switch (m)
        {
            case 0:  return { Weave::black,      0,          5, 0.20f, true  };
            case 1:  return { Weave::diamond,    0xffcbb48a, 2, 0.12f, true  };
            case 2:  return { Weave::saltPepper, 0xffe9e2cf, 3, 0.16f, false };
            case 3:  return { Weave::grid,       0,          6, 0.24f, false };
            case 4:  return { Weave::silver,     0,          0, 0.15f, true  };
            case 5:  return { Weave::wheat,      0,          1, 0.12f, true  };
            case 6:
            case 7:  return { Weave::mesh,       0,          8, 1.0f,  false };
            default: return { Weave::none,       0,         -1, 1.0f,  false };
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
        ring (r, r * 0.955f, frame.darker (0.8f), frame.brighter (0.12f), true);
        ring (r * 0.955f, r * 0.93f, frame.darker (0.85f), frame.darker (0.2f), false);
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
    float h = (area.getHeight() - (cabLook (model).handle ? 14.0f : 0.0f)) * scale, w = h * spec.aspect;
    if (w > area.getWidth()) { w = area.getWidth(); h = w / spec.aspect; }
    l.box = R (w, h).withCentre (area.getCentre()).withBottomY (area.getBottom()); // every cab stands on the floor
    l.baffle = l.box.reduced (std::max (14.0f, w * 0.045f));
    const float cw = l.baffle.getWidth() / (float) spec.cols, ch = l.baffle.getHeight() / (float) spec.rows;
    const float d = std::min (cw, ch) * 0.86f * (spec.speakerInches == 10.0f ? 0.94f : 1.0f);
    for (int r = 0; r < spec.rows; ++r)
        for (int c = 0; c < spec.cols; ++c)
            l.speakers.push_back (R (d, d).withCentre ({ l.baffle.getX() + cw * ((float) c + 0.5f), l.baffle.getY() + ch * ((float) r + 0.5f) }));
    if (model == 7) // the big driver sits high, over a port slot
        l.speakers.front() = l.speakers.front().translated (0.0f, -l.baffle.getHeight() * 0.045f);
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
    const auto look = cabLook (model);
    const float bw = l.baffle.getWidth(), bh = l.baffle.getHeight();
    // the box's rim stands proud of the baffle and shades its top and left
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), l.baffle.getX(), 0, juce::Colours::black.withAlpha (0.0f), l.baffle.getX() + 14.0f, 0, false));
    g.fillRect (l.baffle.withWidth (14.0f));
    if (look.weave == Weave::none)
        for (int i = 0; i < 4; ++i)
            screw (g, { i % 2 == 0 ? l.baffle.getX() + 9.0f : l.baffle.getRight() - 9.0f, i < 2 ? l.baffle.getY() + 9.0f : l.baffle.getBottom() - 9.0f },
                   5.0f, 0.5f + 0.9f * (float) i);
    // speakers
    for (size_t i = 0; i < l.speakers.size(); ++i)
        drawSpeaker (g, l.speakers[i], C (spec.frame), model == 6, (juce::int64) model * 16 + (juce::int64) i);
    if (model == 6)
    {
        // a horn for the top end, in the middle of the four
        const auto horn = R (bw * 0.17f, bw * 0.085f).withCentre (l.baffle.getCentre());
        g.setColour (C (0xff050505));
        g.fillRoundedRectangle (horn.expanded (3.0f), 4.0f);
        g.setGradientFill (juce::ColourGradient (C (0xff3a3c3f), horn.getX(), horn.getY(), C (0xff101112), horn.getRight(), horn.getBottom(), false));
        g.fillRoundedRectangle (horn, 3.0f);
        g.setGradientFill (juce::ColourGradient (C (0xff000000), horn.getCentreX(), horn.getCentreY(), C (0x00000000), horn.getX(), horn.getCentreY(), true));
        g.fillRoundedRectangle (horn.reduced (2.0f), 3.0f);
    }
    if (model == 7)
    {
        // the port: a slot through the baffle, black all the way in
        const auto port = R (bw * 0.56f, bh * 0.07f).withCentre ({ l.baffle.getCentreX(), l.baffle.getBottom() - bh * 0.075f });
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (port.translated (0.0f, 1.2f), port.getHeight() * 0.5f);
        g.setGradientFill (juce::ColourGradient (C (0xff000000), 0, port.getY(), C (0xff141414), 0, port.getBottom(), false));
        g.fillRoundedRectangle (port, port.getHeight() * 0.5f);
    }
    // what covers the speakers
    if (look.weave == Weave::mesh)
    {
        // a punched steel sheet: the speakers stand in the dark behind it
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillRoundedRectangle (l.baffle, 4.0f);
        juce::Path sheet;
        sheet.setUsingNonZeroWinding (false);
        sheet.addRoundedRectangle (l.baffle, 4.0f);
        const float pitch = std::max (6.0f, bw / 64.0f), hole = pitch * 0.64f;
        int row = 0;
        for (float y = l.baffle.getY() + pitch * 0.85f; y < l.baffle.getBottom() - pitch * 0.6f; y += pitch * 0.866f, ++row)
            for (float x = l.baffle.getX() + pitch * 0.85f + (row % 2 == 1 ? pitch * 0.5f : 0.0f); x < l.baffle.getRight() - pitch * 0.6f; x += pitch)
                sheet.addEllipse (x - hole * 0.5f, y - hole * 0.5f, hole, hole);
        g.setColour (C (0xff7b8086).withAlpha (0.75f)); // the lower wall of every hole faces the light
        g.fillPath (sheet, juce::AffineTransform::translation (0.0f, -0.9f));
        g.setGradientFill (juce::ColourGradient (C (0xff33363a), 0, l.baffle.getY(), C (0xff131416), 0, l.baffle.getBottom(), false));
        g.fillPath (sheet);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), l.baffle.getCentreX(), l.baffle.getY() + bh * 0.25f,
                                                 juce::Colours::black.withAlpha (0.3f), l.baffle.getX(), l.baffle.getBottom(), true));
        g.fillRoundedRectangle (l.baffle, 4.0f);
        for (int i = 0; i < 6; ++i) // screwed down round its edge
            screw (g, { l.baffle.getX() + 8.0f + (bw - 16.0f) * (float) (i % 3) * 0.5f, i < 3 ? l.baffle.getY() + 8.0f : l.baffle.getBottom() - 8.0f }, 4.5f, 0.7f * (float) i);
    }
    else if (look.weave != Weave::none)
    {
        juce::Image cloth (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (bw * scale)), juce::jmax (1, juce::roundToInt (bh * scale)), true);
        {
            juce::Graphics cg (cloth);
            cg.addTransform (juce::AffineTransform::translation (-l.baffle.getX(), -l.baffle.getY()).scaled (scale));
            AmpDesign c {};
            c.weave = look.weave;
            weave (cg, l.baffle, c);
        }
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip;
        clip.addRoundedRectangle (l.baffle, 4.0f);
        g.reduceClipRegion (clip);
        g.setOpacity (1.0f - look.show);
        g.drawImage (cloth, l.baffle);
        g.setOpacity (1.0f);
        // the cloth is stretched over a frame: it dips a little where each speaker pulls the air
        for (const auto& sp : l.speakers)
        {
            const auto c = sp.getCentre();
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.16f), c.x, c.y, juce::Colours::black.withAlpha (0.0f), c.x + sp.getWidth() * 0.5f, c.y, true));
            g.fillEllipse (sp);
        }
        if (model == 2)
        {
            // the top of this one leans back: the cloth above the bend faces the light a little more
            const float bend = l.baffle.getY() + bh * 0.46f;
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (l.baffle.withBottom (bend));
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRect (l.baffle.getX(), bend, bw, 1.4f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (l.baffle.getX(), bend + 1.4f, bw, 1.0f);
        }
    }
    if (model == 2)
    {
        // the bend shows on the covering either side as well
        const float bend = l.baffle.getY() + bh * 0.46f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (b.getX(), bend, l.baffle.getX() - b.getX(), 1.4f);
        g.fillRect (l.baffle.getRight(), bend, b.getRight() - l.baffle.getRight(), 1.4f);
    }
    if (look.piping != 0)
    {
        const auto pipe = l.baffle.expanded (2.0f);
        g.setGradientFill (juce::ColourGradient (C (look.piping).brighter (0.2f), pipe.getX(), pipe.getY(), C (look.piping).darker (0.5f), pipe.getRight(), pipe.getBottom(), false));
        g.drawRoundedRectangle (pipe, 5.0f, 2.2f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (pipe.reduced (1.6f), 4.0f, 0.8f);
    }
    if (model == 4)
    {
        // tilt-back legs folded against the sides, seen edge on
        for (auto x : { b.getX() - 5.5f, b.getRight() + 1.0f })
        {
            const R leg (x, b.getY() + b.getHeight() * 0.2f, 4.5f, b.getHeight() * 0.62f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (leg.translated (1.0f, 2.0f), 2.2f);
            juce::ColourGradient tube (C (0xff6c7176), leg.getX(), 0, C (0xff2b2d30), leg.getRight(), 0, false);
            tube.addColour (0.35, C (0xfff3f4f5));
            g.setGradientFill (tube);
            g.fillRoundedRectangle (leg, 2.2f);
            screw (g, { leg.getCentreX(), leg.getY() + 5.0f }, 4.0f, x * 0.13f);
        }
    }
    if (look.handle)
        strapHandle (g, R (std::min (200.0f, b.getWidth() * 0.34f), 18.0f).withCentre ({ b.getCentreX(), b.getY() - 4.0f }), b.getY());
    // corners
    if (model != 5)
        for (int i = 0; i < 4; ++i)
            cabinetCorner (g, b, i % 2 == 1, i >= 2, std::min (32.0f, b.getWidth() * 0.075f));
    if (look.amp >= 0)
    {
        const float hh = juce::jlimit (16.0f, 58.0f, bh * 0.125f);
        ampBadge (g, ampDesign (look.amp), R (l.baffle.getX() + std::max (12.0f, bw * 0.045f), l.baffle.getY() + std::max (10.0f, bh * 0.05f), std::min (bw * 0.42f, hh * 6.0f), hh));
    }
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

MicPreLayout micPreLayout (R box)
{
    MicPreLayout l;
    l.box = box;
    const auto face = box.reduced (9.0f);
    l.socket = { face.getX() + 40.0f, face.getCentreY() + 12.0f };
    const auto strip = face.withTrimmedLeft (84.0f).withTrimmedRight (12.0f);
    const float gap = 20.0f, cw = (strip.getWidth() - gap) / 4.0f;
    l.knobSize = std::min (54.0f, cw - 10.0f);
    for (int i = 0; i < 4; ++i)
        l.cells.push_back (R (strip.getX() + cw * (float) i + (i >= 2 ? gap : 0.0f), face.getY() + 34.0f, cw, l.knobSize + 20.0f));
    return l;
}

juce::Image renderMicPre (const MicPreLayout& l, R area, float scale)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (area.getWidth() * scale)),
                     juce::jmax (1, juce::roundToInt (area.getHeight() * scale)), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::translation (-area.getX(), -area.getY()).scaled (scale));
    const auto b = l.box;
    {
        juce::Path p;
        p.addRoundedRectangle (b.translated (2.0f, 6.0f), 6.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.65f), 22, { 3, 9 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 5, { 1, 4 }).drawForPath (g, p);
    }
    // rubber feet
    for (float x : { b.getX() + 26.0f, b.getRight() - 54.0f })
    {
        g.setColour (C (0xff060606));
        g.fillRoundedRectangle (x, b.getBottom() - 2.0f, 28.0f, 8.0f, 2.5f);
    }
    // the case: folded steel in dark grey paint
    fillMetal (g, b, C (0xff2b2e33), 6.0f);
    {
        juce::Path shape;
        shape.addRoundedRectangle (b, 6.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        finishTexture (g, b, Finish::crinkle, C (0xff2b2e33), 91);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.4f), b.getX(), b.getY(), juce::Colours::black.withAlpha (0.5f), b.getRight(), b.getBottom(), false));
    g.drawRoundedRectangle (b.reduced (0.7f), 6.0f, 1.4f);
    // the face plate: black anodised aluminium let into the front
    const auto face = b.reduced (9.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (face.expanded (1.2f), 3.5f);
    g.setGradientFill (juce::ColourGradient (C (0xff222325), 0.0f, face.getY(), C (0xff0f1011), 0.0f, face.getBottom(), false));
    g.fillRoundedRectangle (face, 3.0f);
    finishTexture (g, face, Finish::anodised, C (0xff151617), 92);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.20f), face.getX(), face.getY(), juce::Colours::black.withAlpha (0.3f), face.getRight(), face.getBottom(), false));
    g.drawRoundedRectangle (face.reduced (0.5f), 3.0f, 1.0f);

    const C ink (0xffc9ccd0);
    // the socket the microphone plugs into: flange, insert, latch and three contacts
    {
        const auto c = l.socket;
        const float r = 19.0f;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (R (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (c.translated (0.8f, 1.6f)));
        conic (g, c, r, 0.0f, [] (float a) { return metalAt (C (0xffa9adb0), a); }, 96);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (R (r * 2.0f, r * 2.0f).withCentre (c), 0.8f);
        g.setGradientFill (juce::ColourGradient (C (0xff050505), c.x - r, c.y - r, C (0xff232425), c.x + r, c.y + r, false));
        g.fillEllipse (R (r * 1.46f, r * 1.46f).withCentre (c));
        fillMetal (g, R (9.0f, 5.0f).withCentre ({ c.x, c.y - r * 0.86f }), C (0xff9a9ea2), 1.2f);
        for (int i = 0; i < 3; ++i)
        {
            const auto pin = c.getPointOnCircumference (r * 0.36f, juce::MathConstants<float>::twoPi * (float) i / 3.0f + juce::MathConstants<float>::pi);
            g.setColour (C (0xff3a3c3e));
            g.fillEllipse (R (5.0f, 5.0f).withCentre (pin));
            g.setColour (C (0xff000000));
            g.fillEllipse (R (3.0f, 3.0f).withCentre (pin));
        }
        auto f = faceFont (Face::instrumentSemi, 9.2f);
        f.setExtraKerningFactor (0.12f);
        g.setFont (f);
        g.setColour (ink.withAlpha (0.8f));
        g.drawText ("MIC IN", R (80.0f, 12.0f).withCentre ({ c.x, c.y - r - 15.0f }), juce::Justification::centred, false);
    }
    // what each pair of controls is for
    {
        auto f = faceFont (Face::instrumentSemi, 9.2f);
        f.setExtraKerningFactor (0.12f);
        g.setFont (f);
        const float y = face.getY() + 17.0f;
        for (int pair = 0; pair < 2; ++pair)
        {
            const float x0 = l.cells[(size_t) (pair * 2)].getX() + 4.0f, x1 = l.cells[(size_t) (pair * 2 + 1)].getRight() - 4.0f;
            const juce::String name (pair == 0 ? "MICROPHONE" : "FILTER");
            const float tw = juce::GlyphArrangement::getStringWidth (f, name) + 12.0f, mid = (x0 + x1) * 0.5f;
            g.setColour (ink.withAlpha (0.75f));
            g.fillRect (x0, y, mid - tw * 0.5f - x0, 1.0f);
            g.fillRect (mid + tw * 0.5f, y, x1 - (mid + tw * 0.5f), 1.0f);
            g.fillRect (x0, y, 1.0f, 5.0f);
            g.fillRect (x1 - 1.0f, y, 1.0f, 5.0f);
            g.drawText (name, R (tw + 30.0f, 12.0f).withCentre ({ mid, y + 0.5f }), juce::Justification::centred, false);
        }
    }
    return img;
}

void drawCabMic (juce::Graphics& g, const CabLayout& l, float micPos, float distance, juce::Point<float> socket)
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
    head.addEllipse (R (hr * 1.9f, hr * 1.9f).withCentre (c + axis * (hr * 0.5f)));
    {
        juce::Path shade = body;
        shade.addPath (head);
        const float off = r * (0.03f + 0.16f * away);
        juce::DropShadow (juce::Colours::black.withAlpha (0.75f - 0.35f * away), (int) (r * (0.03f + 0.12f * away)), { (int) off, (int) off }).drawForPath (g, shade);
    }
    // The stand is in front of the cab, so its foot sits lower in the picture than the cab's own
    // bottom edge, and lower still as the mic is pulled back toward us.
    const auto clip = c + axis * (hr * 2.0f);
    const float footY = l.box.getBottom() + 4.0f + 7.0f * away;
    {
        // A tripod seen from the front, like everything else in the picture: two legs
        // spread to the sides, the third comes straight at us and shows as a short stub.
        const float w0 = std::max (5.0f, hr * 0.34f);
        const float spread = hr * 2.9f, hub = footY - hr * 1.5f;
        auto tube = [&g] (juce::Point<float> from, juce::Point<float> to, float thick)
        {
            juce::Path leg;
            leg.startNewSubPath (from);
            leg.lineTo (to);
            const juce::PathStrokeType st (thick, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.strokePath (leg, st, juce::AffineTransform::translation (1.5f, 2.5f));
            g.setColour (C (0xff0d0e0f));
            g.strokePath (leg, st);
            g.setColour (C (0xff777b80).withAlpha (0.8f));
            g.strokePath (leg, juce::PathStrokeType (std::max (1.0f, thick * 0.22f)), juce::AffineTransform::translation (-thick * 0.18f, -thick * 0.18f));
        };
        for (float side : { -1.0f, 1.0f })
        {
            const juce::Point<float> toe (clip.x + side * spread, footY);
            tube ({ clip.x, hub }, toe, w0 * 0.8f);
            g.setColour (C (0xff050505)); // rubber tip
            g.fillRoundedRectangle (R (w0 * 1.5f, w0 * 0.9f).withCentre ({ toe.x, toe.y + w0 * 0.1f }), w0 * 0.3f);
        }
        tube ({ clip.x, hub }, { clip.x, footY + 2.0f }, w0 * 0.8f);
        g.setColour (C (0xff050505));
        g.fillRoundedRectangle (R (w0 * 1.5f, w0 * 0.9f).withCentre ({ clip.x, footY + 2.5f }), w0 * 0.3f);
        // the rod, with the collar where its two sections clamp
        const float w = std::max (5.0f, hr * 0.34f);
        const R rod (clip.x - w * 0.5f, clip.y, w, hub + w - clip.y);
        juce::ColourGradient m (C (0xff111213), rod.getX(), 0, C (0xff0a0a0b), rod.getRight(), 0, false);
        m.addColour (0.35, C (0xff6a6e72));
        g.setGradientFill (m);
        g.fillRect (rod);
        const R collar (clip.x - w * 0.85f, clip.y + (footY - clip.y) * 0.45f, w * 1.7f, w * 2.2f);
        juce::ColourGradient cg (C (0xff17181a), collar.getX(), 0, C (0xff0a0a0b), collar.getRight(), 0, false);
        cg.addColour (0.35, C (0xff7b7f83));
        g.setGradientFill (cg);
        g.fillRoundedRectangle (collar, 1.2f);
        // the casting the three legs fold out of
        const R boss (clip.x - w * 1.1f, hub - w * 0.2f, w * 2.2f, w * 2.0f);
        juce::ColourGradient bg (C (0xff17181a), boss.getX(), 0, C (0xff0a0a0b), boss.getRight(), 0, false);
        bg.addColour (0.35, C (0xff5f6367));
        g.setGradientFill (bg);
        g.fillRoundedRectangle (boss, 2.0f);
    }
    // cable out of the tail, down to the floor, and along it to the channel strip
    {
        const float floorY = l.box.getBottom() + 5.0f;
        const juce::Point<float> land (tail.x + hr * 3.2f, floorY);
        juce::Path cable;
        cable.startNewSubPath (tail);
        // it leaves along the mic, hangs, and meets the floor at a shallow angle
        cable.cubicTo (tail + axis * (hr * 2.4f), { land.x - hr * 2.6f, floorY - hr * 0.2f }, land);
        const bool plugged = socket.x > land.x + 60.0f;
        if (plugged)
        {
            // slack on the floor, then a wide bend up into the socket
            const float run = socket.x - land.x;
            const float rise = floorY - socket.y;
            cable.cubicTo ({ land.x + run * 0.3f, floorY + 4.0f }, { land.x + run * 0.55f, floorY - 2.0f }, { socket.x - rise * 0.9f, floorY + 1.0f });
            cable.cubicTo ({ socket.x - rise * 0.25f, floorY + 2.0f }, { socket.x, floorY - rise * 0.35f }, { socket.x, socket.y + 14.0f });
        }
        const float cw = std::max (2.0f, hr * 0.2f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (cable, juce::PathStrokeType (cw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (1.5f, 2.5f));
        g.setColour (C (0xff070707));
        g.strokePath (cable, juce::PathStrokeType (cw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (cable, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (-0.7f, -0.5f));
        if (plugged)
        {
            // the plug, seen end on: a strain relief below, a knurled shell in the socket
            g.setColour (C (0xff0b0b0c));
            g.fillRoundedRectangle (R (cw + 3.0f, 14.0f).withCentre ({ socket.x, socket.y + 15.0f }), 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (R (30.0f, 30.0f).withCentre (socket.translated (0.8f, 1.6f)));
            conic (g, socket, 13.5f, 0.0f, [] (float a) { return metalAt (C (0xff2c2e31), a).withMultipliedBrightness (0.5f + 0.5f * std::abs (std::sin (a * 14.0f))); }, 168);
            g.setGradientFill (juce::ColourGradient (C (0xff3a3d40), socket.x - 5.0f, socket.y - 6.0f, C (0xff0c0d0e), socket.x + 8.0f, socket.y + 8.0f, true));
            g.fillEllipse (R (17.0f, 17.0f).withCentre (socket));
        }
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
    // The head of an instrument mic: a short ribbed cap, wider than the body, with its flat
    // front against the cone. We see its side and the dark rim of that front.
    {
        const float capLen = hr * 1.15f, capFar = hr * 0.98f, capNear = hr * 0.84f;
        const auto near = c + axis * capLen;
        juce::Path cap;
        cap.startNewSubPath (c + across * capFar);
        cap.lineTo (near + across * capNear);
        cap.lineTo (near - across * capNear);
        cap.lineTo (c - across * capFar);
        cap.closeSubPath();
        // the far end, turned a little away from us
        juce::Path front;
        front.addEllipse (R (capFar * 2.0f, capFar * 0.9f).withCentre ({ 0.0f, 0.0f }));
        front.applyTransform (juce::AffineTransform::rotation (std::atan2 (across.y, across.x)).translated (c.x, c.y));
        g.setColour (C (0xff111213));
        g.fillPath (front);
        const auto e0 = c + across * capFar, e1 = c - across * capFar;
        juce::ColourGradient metal (C (0xff2a2c2f), e0.x, e0.y, C (0xff0e0f10), e1.x, e1.y, false);
        metal.addColour (0.25, C (0xffb4b8bc));
        metal.addColour (0.5, C (0xff53575b));
        g.setGradientFill (metal);
        g.fillPath (cap);
        {
            // slots in the cap, running round it
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (cap);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            for (float t = 0.18f; t < 0.95f; t += 0.2f)
            {
                const auto p = c + axis * (capLen * t);
                g.drawLine ({ p + across * capFar, p - across * capFar }, std::max (1.0f, hr * 0.09f));
            }
        }
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (cap, juce::PathStrokeType (0.8f));
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
    getProperties().set ("control", p.getParameterID()); // what a film's pointer finds it by
}

void Footswitch::mouseDown (const juce::MouseEvent&) { pressed = true; repaint(); }

void Footswitch::mouseUp (const juce::MouseEvent& e)
{
    pressed = false;
    repaint();
    if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void drawFootswitch (juce::Graphics& g, R b, bool pressed, bool hover, int style, juce::Colour body)
{
    if (style == 1)
    {
        // a hinged plate in the box's own paint with a ribbed rubber mat: the whole thing is the switch
        const float u = b.getHeight() / 153.0f;
        const auto p = b.translated (0.0f, pressed ? 1.6f * u : 0.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (p.translated (1.0f * u, (pressed ? 1.0f : 3.0f) * u), 4.0f * u);
        g.setGradientFill (juce::ColourGradient (body.brighter (0.2f), p.getX(), p.getY(), body.darker (0.3f), p.getRight(), p.getBottom(), false));
        g.fillRoundedRectangle (p, 4.0f * u);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), p.getX(), p.getY(), juce::Colours::black.withAlpha (0.5f), p.getRight(), p.getBottom(), false));
        g.drawRoundedRectangle (p.reduced (0.7f * u), 4.0f * u, 1.4f * u);
        // the hinge pin along the top edge
        const auto pin = R (p.getX() + 8.0f * u, p.getY() + 4.0f * u, p.getWidth() - 16.0f * u, 4.5f * u);
        fillMetal (g, pin, C (0xffa9adb0), 2.2f * u);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (pin, 2.2f * u, 0.6f);
        // rubber mat
        const auto mat = R (p.getX() + 9.0f * u, p.getY() + 15.0f * u, p.getWidth() - 18.0f * u, p.getHeight() - 25.0f * u);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (mat.expanded (1.2f * u), 6.0f * u);
        g.setGradientFill (juce::ColourGradient (C (0xff1d1e1f), mat.getX(), mat.getY(), C (0xff0d0d0e), mat.getRight(), mat.getBottom(), false));
        g.fillRoundedRectangle (mat, 5.0f * u);
        const float pitch = 9.5f * u;
        const int ribs = std::max (3, (int) ((mat.getHeight() - 10.0f * u) / pitch));
        const float y0 = mat.getCentreY() - (float) ribs * pitch * 0.5f + pitch * 0.25f;
        for (int i = 0; i < ribs; ++i)
        {
            const auto rib = R (mat.getX() + 8.0f * u, y0 + (float) i * pitch, mat.getWidth() - 16.0f * u, pitch * 0.5f);
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (rib.translated (0.6f * u, 1.3f * u), rib.getHeight() * 0.5f);
            g.setGradientFill (juce::ColourGradient (C (0xff3a3b3d), 0.0f, rib.getY(), C (0xff1a1a1b), 0.0f, rib.getBottom(), false));
            g.fillRoundedRectangle (rib, rib.getHeight() * 0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.fillRect (rib.getX() + rib.getHeight() * 0.5f, rib.getY() + 0.5f * u, rib.getWidth() - rib.getHeight(), 0.9f * u);
        }
        if (pressed)
        {
            g.setColour (juce::Colours::black.withAlpha (0.14f));
            g.fillRoundedRectangle (p, 4.0f * u);
        }
        else if (hover)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (p, 4.0f * u);
        }
        return;
    }
    if (style == 2)
    {
        // a low soft-touch switch: a chrome ring round a satin button that barely moves
        b = b.reduced (2.0f);
        const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        conic (g, c, r, r * 0.78f, [] (float a) { return metalAt (C (0xffb6babd), a); }, 120);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (R (r * 2.0f, r * 2.0f).withCentre (c).reduced (0.4f), 0.9f);
        const float cr = r * 0.78f;
        const auto cap = R (cr * 2.0f, cr * 2.0f).withCentre (c.translated (0.0f, pressed ? 0.8f : 0.0f));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (cap.expanded (0.8f));
        g.setGradientFill (juce::ColourGradient (C (pressed ? 0xff303235 : 0xff4a4d51), c.x - cr * 0.4f, c.y - cr * 0.45f, C (0xff121314), c.x + cr * 0.8f, c.y + cr * 0.8f, true));
        g.fillEllipse (cap);
        // the button is dished: light catches the far lip, not the middle
        juce::Path lip;
        lip.addCentredArc (cap.getCentreX(), cap.getCentreY(), cr * 0.9f, cr * 0.9f, 0.0f, 1.2f, 2.9f, true);
        g.setColour (juce::Colours::white.withAlpha (pressed ? 0.12f : 0.26f));
        g.strokePath (lip, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (hover && ! pressed)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillEllipse (cap);
        }
        return;
    }

    b = b.reduced (2.0f);
    // a stomp switch is a small thing: thinner than the knobs above it
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f * 0.72f * (pressed ? 0.95f : 1.0f);
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
    const float cr = r * 0.62f;
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
    if (pictured)
    {
        // the switch itself is in the pedal's picture: a press pushes it into shade, the pointer over it lifts it a little
        const bool down = pressed || (motion::filmPress && motion::pointedAt (*this));
        if (! down && ! isMouseOver() && ! motion::pointedAt (*this)) return;
        const auto b = getLocalBounds().toFloat();
        const float fs = std::min (b.getWidth(), b.getHeight());
        g.setColour (down ? juce::Colours::black.withAlpha (0.32f) : juce::Colours::white.withAlpha (0.07f));
        if (style == 1)      g.fillRoundedRectangle (b.reduced (1.0f), b.getHeight() * 0.03f);
        else if (style == 2) g.fillEllipse (R (fs * 0.78f, fs * 0.78f * art::squash).withCentre (b.getCentre()));
        else                 g.fillEllipse (R (fs * 0.44f, fs * 0.44f * art::squash).withCentre (b.getCentre().translated (0.0f, -fs * 0.118f)));
        return;
    }
    drawFootswitch (g, getLocalBounds().toFloat(), pressed || (motion::filmPress && motion::pointedAt (*this)), isMouseOver() || motion::pointedAt (*this), style, body);
}

//==============================================================================
R treadleArtArea (R well, float boxHeight)
{
    const float u = boxHeight / 420.0f;
    return { well.getX() - 16.0f * u, well.getY() - 30.0f * u, well.getWidth() + 32.0f * u, well.getHeight() + 40.0f * u };
}

void drawTreadle (juce::Graphics& g, R well, float position, bool pressed, bool hover, const PedalDesign* pictured, float boxHeight)
{
    if (pictured != nullptr)
    {
        const int frames = 11;
        const auto name = "pedal_" + art::slug (pictured->name) + "_treadle";
        if (art::drawFrame (g, name, juce::roundToInt (juce::jlimit (0.0f, 1.0f, position) * (float) (frames - 1)), frames, frames, treadleArtArea (well, boxHeight)))
        {
            if (pressed || hover)
            {
                g.setColour (pressed ? juce::Colours::black.withAlpha (0.2f) : juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (well.reduced (2.0f), 4.0f);
            }
            return;
        }
    }
    const float u = well.getHeight() / 245.0f;
    const float t = juce::jlimit (-1.0f, 1.0f, position * 2.0f - 1.0f); // +1 toe down, -1 heel down
    const float heelLift = std::max (0.0f, t), toeLift = std::max (0.0f, -t);
    const auto area = well.reduced (5.0f * u, 6.0f * u).translated (0.0f, pressed ? 1.0f * u : 0.0f);
    const float cx = area.getCentreX();
    // the end that is up in the air is nearer the eye, so it looks a touch wider, and its end face shows
    const float endFace = 11.0f * u * heelLift;
    const float yT = area.getY() + 5.0f * u * toeLift, yB = area.getBottom() - endFace;
    const float wT = area.getWidth() * (0.93f + 0.05f * toeLift), wB = area.getWidth() * (0.97f + 0.03f * heelLift);
    auto plateShape = [&] (float inset, float insetEnds)
    {
        juce::Path p;
        p.startNewSubPath (cx - wT * 0.5f + inset, yT + insetEnds);
        p.lineTo (cx + wT * 0.5f - inset, yT + insetEnds);
        p.lineTo (cx + wB * 0.5f - inset, yB - insetEnds);
        p.lineTo (cx - wB * 0.5f + inset, yB - insetEnds);
        p.closeSubPath();
        return p.createPathWithRoundedCorners (7.0f * u);
    };
    const auto plate = plateShape (0.0f, 0.0f);
    // the raised end throws a longer shadow into the trough
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (well.expanded (5.0f * u).toNearestInt());
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), std::max (2, (int) (5.0f * u)), { (int) (1.0f * u), (int) (2.0f * u) }).drawForPath (g, plate);
        const float lift = std::max (heelLift, toeLift);
        if (lift > 0.02f)
        {
            juce::Path end;
            const float eh = area.getHeight() * 0.42f;
            end.addRoundedRectangle (R (area.getX(), heelLift > 0.0f ? yB - eh : yT, area.getWidth(), eh), 7.0f * u);
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f * lift), std::max (2, (int) (12.0f * u)), { (int) (5.0f * u * lift), (int) (9.0f * u * lift) }).drawForPath (g, end);
        }
    }
    if (endFace > 0.5f)
    {
        // the thickness of the casting at the heel
        const R ef (cx - wB * 0.5f + 2.0f * u, yB - 6.0f * u, wB - 4.0f * u, endFace + 6.0f * u);
        g.setGradientFill (juce::ColourGradient (C (0xff34363a), 0.0f, yB, C (0xff0b0b0c), 0.0f, ef.getBottom(), false));
        g.fillRoundedRectangle (ef, 5.0f * u);
    }
    // the casting: black wrinkle paint, brighter toward whichever end tips up to the light
    const C iron (0xff1a1b1d);
    g.setGradientFill (juce::ColourGradient (iron.brighter (0.25f + 0.55f * toeLift), 0.0f, yT, iron.brighter (0.25f + 0.55f * heelLift), 0.0f, yB, false));
    g.fillPath (plate);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), area.getX(), yT, juce::Colours::black.withAlpha (0.55f), area.getRight(), yB, false));
    g.strokePath (plate, juce::PathStrokeType (1.5f * u));

    // the rubber tread
    const auto mat = plateShape (9.0f * u, 11.0f * u);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (mat, juce::PathStrokeType (2.4f * u));
    g.setColour (C (0xff0e0e0f));
    g.fillPath (mat);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (mat);
        const float top = yT + 11.0f * u, bottom = yB - 11.0f * u;
        const int ribs = 13;
        const float pitch = (bottom - top) / (float) ribs;
        for (int i = 0; i < ribs; ++i)
        {
            const float y = top + pitch * ((float) i + 0.25f);
            const float f = ((float) i + 0.5f) / (float) ribs;
            const float tip = toeLift * (1.0f - f) + heelLift * f; // how far this rib is tipped up to the light
            const R rib (area.getX(), y, area.getWidth(), pitch * 0.55f);
            g.setColour (juce::Colours::black.withAlpha (0.8f));
            g.fillRect (rib.translated (0.0f, 1.4f * u));
            g.setGradientFill (juce::ColourGradient (C (0xff2f3032).brighter (0.5f * tip), 0.0f, rib.getY(), C (0xff161617), 0.0f, rib.getBottom(), false));
            g.fillRect (rib);
            g.setColour (juce::Colours::white.withAlpha (0.10f + 0.16f * tip));
            g.fillRect (rib.withHeight (1.0f * u));
        }
        // two grooves cut the ribs into blocks
        for (float fx : { 0.36f, 0.64f })
        {
            g.setColour (C (0xff070707));
            g.fillRect (area.getX() + area.getWidth() * fx - 1.6f * u, yT, 3.2f * u, yB - yT);
        }
    }
    if (hover && ! pressed)
    {
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillPath (plate);
    }
}

Treadle::Treadle (juce::RangedAudioParameter& position, juce::RangedAudioParameter& onOff)
    : param (position),
      pos (position, [this] (float v) { value = param.convertTo0to1 (v); refresh(); }),
      power (onOff, [this] (float v) { on = v >= 0.5f; refresh(); if (onChange) onChange (on); })
{
    pos.sendInitialUpdate();
    power.sendInitialUpdate();
    setTooltip ("Drag up and down to rock the pedal. Click to switch it on or off.");
}

void Treadle::paint (juce::Graphics& g)
{
    drawTreadle (g, getLocalBounds().toFloat(), rocking ? value : 0.5f, pressed && ! dragging, isMouseOver(), design, boxHeight);
}

void Treadle::refresh()
{
    repaint();
    if (design != nullptr)
        if (auto* parent = getParentComponent()) // the photograph reaches past the component
            parent->repaint (treadleArtArea (getBounds().toFloat(), boxHeight).getSmallestIntegerContainer());
}

void Treadle::mouseDown (const juce::MouseEvent&)
{
    pressed = true;
    startValue = value;
    repaint();
}

void Treadle::mouseDrag (const juce::MouseEvent& e)
{
    if (! rocking) return;
    if (! dragging && e.getDistanceFromDragStart() < 4) return;
    if (! dragging) { dragging = true; pos.beginGesture(); }
    // pushing the toe (up the screen) opens the filter
    const float n = juce::jlimit (0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() / ((float) getHeight() * 0.8f));
    pos.setValueAsPartOfGesture (param.convertFrom0to1 (n));
}

void Treadle::mouseUp (const juce::MouseEvent& e)
{
    const bool wasDragging = dragging;
    if (dragging) pos.endGesture();
    dragging = pressed = false;
    repaint();
    if (! wasDragging && e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
        power.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}


//==============================================================================
namespace
{
    constexpr float thumbStart = juce::MathConstants<float>::pi * 1.25f, thumbEnd = juce::MathConstants<float>::pi * 2.75f;
    float thumbAngle (int i) { return thumbStart + (thumbEnd - thumbStart) * (0.38f + 0.08f * (float) ((i * 37) % 5)); }
}

GearThumb pedalThumb (const PedalDesign& d, int n, bool faders, float scale, const juce::StringArray* labels)
{
    GearThumb t;
    const R nominal (0.0f, 0.0f, 300.0f, 250.0f);
    if (d.form == 5) n = 2; // the third control is the treadle itself
    t.pedal = pedalLayout (nominal, d, n, faders);
    t.box = t.pedal.box;
    t.area = nominal.withTrimmedTop (-12.0f).withTrimmedBottom (-30.0f);
    t.image = renderPedal (d, t.pedal, t.area, scale, labels != nullptr); // print in a small thumbnail would only be specks
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
            const float y = slot.getCentreY(); // flat, as the pedal comes out of its box
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
            const int li = i + (d.form == 5 ? 1 : 0); // a treadle is the first control and carries no label
            if (labels != nullptr && li < labels->size())
            {
                const bool wide = d.face == Face::shoulders;
                g.setFont (faceFont (wide ? Face::shoulders : Face::instrument, wide ? 12.6f : 11.4f));
                g.setColour (C (d.ink));
                const auto name = (*labels)[li];
                g.drawText (d.upper ? name.toUpperCase() : name.toLowerCase(), R (k.getCentreX() - 50.0f, k.getBottom() + 1.0f, 100.0f, 18.0f), juce::Justification::centredTop);
            }
        }
    }
    if (t.pedal.form == 5)
        drawTreadle (g, t.pedal.treadle, 0.62f, false, false, pedalPictured (d) ? &d : nullptr, t.pedal.box.getHeight());
    else if (! pedalPictured (d))
        drawFootswitch (g, t.pedal.footswitch, false, false, footswitchStyle (t.pedal), C (d.body));
    return t;
}

void drawThumb (juce::Graphics& g, const GearThumb& t, juce::Rectangle<float> target)
{
    if (! t.image.isValid() || t.box.isEmpty()) return;
    const float fit = std::min (target.getWidth() / t.box.getWidth(), target.getHeight() / t.box.getHeight());
    juce::Graphics::ScopedSaveState keep (g);
    g.addTransform (juce::AffineTransform::translation (-t.box.getCentreX(), -t.box.getBottom())
                        .scaled (fit)
                        .translated (target.getCentreX(), target.getBottom()));
    g.drawImage (t.image, t.area);
}

float thumbScaleFor (const GearThumb& probe, juce::Rectangle<float> target)
{
    if (probe.box.isEmpty()) return 0.5f;
    return juce::jlimit (0.1f, 2.0f, 2.0f * std::min (target.getWidth() / probe.box.getWidth(), target.getHeight() / probe.box.getHeight()));
}

GearThumb ampThumb (const AmpDesign& d, const juce::String& displayText, float scale, const juce::StringArray* labels)
{
    GearThumb t;
    const R place (0.0f, 0.0f, ampCanvasW, ampCanvasH);
    t.area = ampArtArea (place);
    t.amp = ampLayout (place, d);
    t.box = d.form == 5 ? t.amp.rack : t.amp.handle.isEmpty() ? t.amp.head : t.amp.head.getUnion (t.amp.handle.withY (t.amp.handle.getY() - (d.form == 3 ? 18.0f : 0.0f)));
    t.image = renderAmp (d, t.amp, t.area, scale, displayText);
    juce::Graphics g (t.image);
    g.addTransform (juce::AffineTransform::translation (-t.area.getX(), -t.area.getY()).scaled (scale));
    for (size_t i = 0; i < t.amp.cells.size(); ++i)
    {
        const auto cell = t.amp.cells[i];
        drawGearKnob (g, cell.withHeight (cell.getWidth()), thumbAngle ((int) i), thumbStart, thumbEnd, d.knob, C (d.knobCap), C (d.ink), true, false,
                      labels != nullptr && cell.getWidth() >= 64.0f);
        if (labels != nullptr && (int) i < labels->size())
        {
            const bool wide = d.face == Face::shoulders;
            g.setFont (faceFont (wide ? Face::shoulders : Face::instrument, wide ? 12.6f : 11.4f));
            g.setColour (C (d.ink));
            const juce::String name = d.labels[i] != nullptr ? juce::String (d.labels[i]) : (*labels)[(int) i];
            g.drawText (d.upper ? name.toUpperCase() : name.toLowerCase(), R (cell.getCentreX() - 50.0f, cell.getBottom() - ampLabelH + 1.0f, 100.0f, 18.0f), juce::Justification::centredTop);
        }
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
