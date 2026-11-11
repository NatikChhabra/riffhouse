// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Modules.h"

namespace rh::ui
{
const std::vector<ModuleInfo>& modules()
{
    static const std::vector<ModuleInfo> m {
        { "gate", "Gate", juce::Colour (0xff94a3b8), P::gateOn, -1, { P::gateThresh, P::gateRelease },
          "Silences hiss and hum between notes. Setup sets the threshold for your guitar automatically." },
        { "comp", "Comp", juce::Colour (0xff2dd4bf), P::compOn, -1, { P::compSustain, P::compAttack, P::compLevel, P::compBlend },
          "Evens out your picking and adds sustain. Great for clean funk and country." },
        { "wah", "Wah", juce::Colour (0xffa78bfa), P::wahOn, (int) P::wahMode, { P::wahPos, P::wahSens, P::wahQ },
          "The vocal 'wah' sweep. Auto mode follows how hard you pick." },
        { "pitch", "Pitch", juce::Colour (0xfff472b6), P::pitchOn, (int) P::pitchMode, { P::pitchSemis, P::pitchMix },
          "Adds a note an octave below, above, or any interval. Your dry note is never delayed." },
        { "drive", "Drive", juce::Colour (0xffa3e635), P::drvOn, (int) P::drvType, { P::drvGain, P::drvTone, P::drvLevel },
          "Overdrive, distortion and fuzz pedals in front of the amp." },
        { "amp", "Amp", colours::accent, P::ampOn, (int) P::ampModel, {}, "" },
        { "cab", "Cab", juce::Colour (0xffd6a574), P::cabOn, (int) P::cabModel, { P::cabMic, P::cabDistance, P::cabLowCut, P::cabHighCut },
          "The speaker cabinet and microphone. This is what makes an amp sound like a recorded amp." },
        { "eq", "EQ", juce::Colour (0xff22d3ee), P::eqOn, -1, {},
          "Seven-band graphic EQ after the cab. Cut or boost to fit your tone in a mix." },
        { "mod", "Mod", juce::Colour (0xffc084fc), P::modOn, (int) P::modType, { P::modRate, P::modDepth, P::modMix },
          "Chorus, flanger, phaser, tremolo, vibrato and vibe, all in stereo." },
        { "delay", "Delay", juce::Colour (0xff60a5fa), P::dlyOn, (int) P::dlyType, { P::dlyTime, P::dlyFeedback, P::dlyTone, P::dlyMix },
          "Echoes. When you switch it off, the last repeats fade out naturally instead of cutting." },
        { "reverb", "Reverb", juce::Colour (0xff818cf8), P::revOn, (int) P::revType, { P::revDecay, P::revPreDelay, P::revTone, P::revMix },
          "Puts your guitar in a space, from a small room to a huge ambient cloud." },
        { "lofi", "Lo-Fi", juce::Colour (0xfffb7185), P::lofiOn, -1, { P::lofiBits, P::lofiRate, P::lofiWobble, P::lofiNoise, P::lofiFilter, P::lofiMix },
          "Old tape wobble, vinyl hiss, AM radio and bit crushing. Bedroom pop and lo-fi beats." },
    };
    return m;
}

juce::String describeChoice (P param, int i)
{
    switch (param)
    {
        case P::ampModel:
        {
            static const char* d[] = {
                "Loud, sparkly clean with tons of headroom.",
                "Warm little combo that breaks up when you dig in.",
                "Jangly, chimey British breakup.",
                "The classic British rock crunch. Turn the gain up for hard rock.",
                "Tighter and hotter British rock tone.",
                "Smooth, saturated lead tone that sings.",
                "Heavy, thick and modern.",
                "Fast, tight and aggressive.",
                "Big, round bass amp with a wide-range tone control.",
                "Almost clean preamp for direct, polished recordings.",
                "Loads a .nam capture of any real amp or pedal. Thousands are free online.",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::cabModel:
        {
            static const char* d[] = {
                "Open-back 1x12 combo. Airy and lively, great for clean and blues.",
                "2x12 with chimey alnico-style speakers. Sparkly and bright.",
                "Closed 4x12 with vintage speakers. Warm mids, the classic rock sound.",
                "Closed 4x12 with modern speakers. Tight lows and aggressive highs for metal.",
                "2x12 American combo. Scooped and glassy.",
                "Tiny 1x8 practice combo. Boxy and raw, perfect for lo-fi.",
                "4x10 bass cab with a tweeter. Punchy and clear.",
                "1x15 bass cab. Deep, round and old-school.",
                "Your own impulse response file (.wav).",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::drvType:
        {
            static const char* d[] = {
                "Pushes the amp harder without changing its voice.",
                "Bright, cutting boost for classic rock leads.",
                "Mid-hump overdrive. Tightens high-gain amps and makes solos sing.",
                "Open, amp-like breakup that cleans up when you turn your guitar down.",
                "Gritty distortion, from punk rhythm to grunge.",
                "Huge, woolly fuzz with endless sustain.",
                "High-gain distortion with scooped mids, all on its own.",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::modType:
        {
            static const char* d[] = {
                "Thick, shimmering doubling for cleans and 80s leads.",
                "The jet-plane sweep.",
                "Swirling, vocal sweep. Classic on funk and 70s rock.",
                "Volume pulses, like a vintage combo.",
                "Pitch wobble, warbly and dreamy.",
                "Throbbing, liquid sweep from the late 60s.",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::dlyType:
        {
            static const char* d[] = {
                "Clean, exact repeats.",
                "Warm, darker repeats that fade softly.",
                "Wobbly, saturated echoes with character.",
                "Repeats bounce between left and right.",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::revType:
        {
            static const char* d[] = {
                "A small room. Adds air without washing out your playing.",
                "Big, smooth concert hall.",
                "Bright, dense studio plate. Great on leads.",
                "Drippy spring tank, straight out of surf rock.",
                "Ambient wash with a glowing octave-up halo.",
                "Huge, slow-building ambient space.",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::pitchMode:
        {
            static const char* d[] = {
                "Adds a note one octave below. Instant heavy riffs.",
                "Adds a bright octave above.",
                "Adds a harmony at any interval, set with Semitones.",
            };
            return juce::isPositiveAndBelow (i, (int) std::size (d)) ? d[i] : "";
        }
        case P::wahMode:
            return i == 0 ? "Drag the pedal to set the sweep, or automate it while recording."
                          : "Follows how hard you pick. Funk heaven.";
        default: break;
    }
    return {};
}

juce::String ampTags (int model)
{
    static const char* t[] = {
        "Funk, Pop, Worship, Jazz, Indie",
        "Blues, Roots, Country, Classic rock",
        "Indie, Britpop, Jangle, Blues",
        "Classic rock, Hard rock, Blues rock",
        "Hard rock, Punk, 80s metal",
        "Rock leads, Modern rock, Fusion",
        "Metal, Metalcore, Djent",
        "Thrash, Death metal, Modern metal",
        "Bass, Any genre",
        "Pop, Direct recording, Ambient",
        "Any real amp you can find a capture for",
    };
    return juce::isPositiveAndBelow (model, (int) std::size (t)) ? utf8 (t[model]) : juce::String();
}

static juce::Colour ampColour (int model)
{
    if (look() == Look::signal) return colours::accent; // the amp carries the function orange
    if (look() == Look::pedalboard) return colours::accent;
    static const juce::uint32 c[] = { 0xff7dd3fc, 0xfffbbf24, 0xfffcd34d, 0xffff9f43, 0xfffb923c, 0xfff97316,
                                      0xffe0a83a, 0xffd97706, 0xff60a5fa, 0xffa3e635, 0xffc084fc };
    return juce::Colour (c[juce::jlimit (0, (int) std::size (c) - 1, model)]);
}

juce::Colour moduleColour (int index)
{
    if (index == mAmp && look() != Look::studio) return ampColour (0);
    switch (look())
    {
        case Look::signal:     return stringColour (index % 6); // one colour per string, low E first
        case Look::pedalboard: return colours::accent;          // amber LEDs on every box
        case Look::studio:     break;
    }
    return modules()[(size_t) juce::jlimit (0, numModules - 1, index)].colour;
}

//==============================================================================
// One piece of gear in the chain strip: a small picture of the real thing.
class ChainStrip::Tile : public juce::Component, public juce::SettableTooltipClient
{
public:
    Tile (RiffhouseProcessor& p, int idx)
        : proc (p), index (idx), info (modules()[(size_t) idx]),
          power (p, info.onParam, moduleColour (idx))
    {
        // the switch on the little picture turns it on and off; the component only holds the state
        power.style = PowerSwitch::Style::hidden;
        power.onChange = [this] (bool) { repaint(); };
        setTitle (info.title);
    }

    int currentType() const
    {
        return info.typeParam >= 0 ? juce::roundToInt (proc.value ((P) info.typeParam)) : 0;
    }

    juce::String subtitle() const
    {
        if (index == mAmp)
        {
            const int m = currentType();
            if (m == choices::neuralCapture && proc.captureName().isNotEmpty()) return proc.captureName();
            return spec (P::ampModel).choices[(size_t) juce::jlimit (0, choices::numAmpModels - 1, m)];
        }
        if (index == mCab && currentType() == choices::cabUserIr && proc.userIrName().isNotEmpty())
            return proc.userIrName();
        if (info.typeParam >= 0)
        {
            const auto& s = spec ((P) info.typeParam);
            return s.choices[(size_t) juce::jlimit (0, (int) s.choices.size() - 1, currentType())];
        }
        return spec (info.onParam).name;
    }

    juce::String thumbKey() const
    {
        juce::String k (currentType());
        if (index == mAmp && currentType() == choices::neuralCapture) k << proc.captureName();
        return k;
    }

    void updateThumb()
    {
        const auto key = thumbKey();
        if (key == lastKey && thumb.image.isValid()) return;
        lastKey = key;
        if (index == mAmp)
        {
            const int m = currentType();
            const juce::String display = m == choices::neuralCapture ? (proc.captureName().isNotEmpty() ? proc.captureName() : juce::String ("no capture"))
                                                                     : juce::String (spec (P::ampModel).choices[(size_t) m]).toLowerCase();
            thumb = ampThumb (ampDesign (m), display, 0.36f);
        }
        else if (index == mCab)
            thumb = cabThumb (currentType(), 0.45f);
        else
        {
            const bool faders = index == mEq;
            thumb = pedalThumb (*pedalDesignFor (index, currentType()), faders ? 8 : (int) info.knobs.size(), faders, 0.6f);
        }
        // switched off, the gear sits in the shadow: darker and a little greyer, still solid
        dim = thumb.image.createCopy();
        dim.desaturate();
        juce::Image::BitmapData px (dim, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < px.height; ++y)
            for (int x = 0; x < px.width; ++x)
            {
                const auto c = px.getPixelColour (x, y);
                const auto o = thumb.image.getPixelAt (x, y);
                px.setPixelColour (x, y, o.interpolatedWith (c, 0.4f).withMultipliedBrightness (0.74f).withAlpha (o.getAlpha()));
            }
    }

    juce::Rectangle<float> thumbArea() const
    {
        return getLocalBounds().toFloat().withTrimmedTop (2.0f).withTrimmedBottom (24.0f).reduced (3.0f, 0.0f);
    }

    juce::AffineTransform thumbTransform() const
    {
        const auto target = thumbArea();
        const float s = std::min (target.getWidth() / thumb.box.getWidth(), target.getHeight() / thumb.box.getHeight());
        // every piece stands on the same line, like gear on a floor
        return juce::AffineTransform::translation (-thumb.box.getCentreX(), -thumb.box.getBottom())
            .scaled (s)
            .translated (target.getCentreX(), target.getBottom());
    }

    // Where the patch cables meet this piece of gear, in the strip's own coordinates:
    // the left and right ends of its box at socket height.
    std::pair<juce::Point<float>, juce::Point<float>> sockets()
    {
        updateThumb();
        const auto b = thumb.box.transformedBy (thumbTransform()).translated ((float) getX(), (float) getY());
        const float y = b.getY() + b.getHeight() * (index == mAmp || index == mCab ? 0.62f : (thumb.pedal.form == 4 ? 0.3f : 0.22f));
        return { { b.getX(), y }, { b.getRight(), y } };
    }

    juce::Rectangle<float> switchArea() const
    {
        if (index == mCab) return {};
        const auto r = index == mAmp ? thumb.amp.jewel.expanded (16.0f) : thumb.pedal.footswitch.expanded (6.0f);
        return r.transformedBy (thumbTransform());
    }

    void paint (juce::Graphics& g) override
    {
        updateThumb();
        const bool on = power.isOn();
        const bool hover = isMouseOver (true);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.addTransform (thumbTransform());
            g.drawImage (on ? thumb.image : dim, thumb.area);
            if (index == mAmp) drawAmpJewel (g, thumb.amp, ampDesign (currentType()), on);
            else if (index != mCab) drawPedalLed (g, thumb.pedal, *pedalDesignFor (index, currentType()), on);
        }

        auto lab = getLocalBounds().toFloat().removeFromBottom (20.0f);
        // the label only says which page is open; on and off is the gear's own light
        const auto col = selected ? colours::text : (hover ? colours::text.withAlpha (0.85f) : colours::textDim);
        drawCaps (g, info.title, lab, 11.5f, col, juce::Justification::centred, 0.12f);
        if (selected)
        {
            const float tw = capsWidth (info.title, 11.5f, 0.12f);
            g.setColour (colours::accent);
            g.fillRect (juce::Rectangle<float> (tw, 1.5f).withCentre ({ lab.getCentreX(), lab.getBottom() - 1.0f }));
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        if (switchArea().contains (e.position)) { power.toggle(); return; }
        if (onClick) onClick();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool overSwitch = switchArea().contains (e.position);
        setTooltip (overSwitch ? juce::String ("Switch ") + info.title + (power.isOn() ? " off" : " on")
                               : subtitle());
    }

    void refresh()
    {
        const auto k = thumbKey();
        if (k != lastKey)
        {
            repaint();
            if (auto* strip = getParentComponent()) strip->repaint(); // the cables end on this box
        }
    }

    RiffhouseProcessor& proc;
    int index;
    const ModuleInfo& info;
    PowerSwitch power;
    bool selected = false;
    std::function<void()> onClick;
    juce::String lastKey { "none" };
    GearThumb thumb;
    juce::Image dim;
};

ChainStrip::ChainStrip (RiffhouseProcessor& p) : proc (p)
{
    for (int i = 0; i < numModules; ++i)
    {
        auto* t = tiles.add (new Tile (proc, i));
        t->onClick = [this, i] { setSelected (i); if (onSelect) onSelect (i); };
        addAndMakeVisible (t);
    }
    setSelected (mAmp);
}

ChainStrip::~ChainStrip() = default;

void ChainStrip::setSelected (int i)
{
    selected = i;
    for (int k = 0; k < tiles.size(); ++k)
    {
        tiles[k]->selected = (k == i);
        tiles[k]->repaint();
    }
}

void ChainStrip::refresh()
{
    for (auto* t : tiles) t->refresh();
}

void ChainStrip::resized()
{
    auto r = getLocalBounds().reduced (44, 0);
    const int gap = 4;
    auto units = [] (int i) { return i == mAmp ? 2.2f : (i == mCab ? 1.2f : (i == mEq ? 1.15f : 1.0f)); };
    float total = 0.0f;
    for (int i = 0; i < numModules; ++i) total += units (i);
    const float unit = (float) (r.getWidth() - gap * (numModules - 1)) / total;
    float x = (float) r.getX();
    for (int i = 0; i < numModules; ++i)
    {
        const float w = unit * units (i);
        tiles[i]->setBounds (juce::Rectangle<float> (x, (float) r.getY(), w, (float) r.getHeight()).toNearestInt());
        x += w + (float) gap;
    }
    cableY = 2.0f + ((float) getHeight() - 26.0f) * 0.32f;
}

void ChainStrip::paint (juce::Graphics& g)
{
    // patch cables from the guitar in, through every piece of gear, to the speakers out:
    // short leads that sag between one socket and the next
    const float y = cableY;
    const float x0 = 22.0f, x1 = (float) getWidth() - 22.0f;
    auto lead = [&g] (juce::Point<float> from, juce::Point<float> to)
    {
        const float span = std::abs (to.x - from.x);
        const float sag = juce::jlimit (5.0f, 13.0f, span * 0.42f);
        juce::Path p;
        p.startNewSubPath (from);
        p.cubicTo ({ from.x + span * 0.3f, from.y + sag }, { to.x - span * 0.3f, to.y + sag }, to);
        const juce::PathStrokeType stroke (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.strokePath (p, stroke, juce::AffineTransform::translation (0.8f, 2.2f));
        g.setColour (juce::Colour (0xff050505));
        g.strokePath (p, stroke);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (p, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (-0.3f, -0.9f));
        // the plugs' metal sleeves, where each lead goes into a box
        for (auto end : { from, to })
        {
            g.setColour (juce::Colour (0xff9da1a5));
            g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 5.0f).withCentre (end), 1.0f);
        }
    };
    juce::Point<float> from (x0 + 7.0f, y);
    for (auto* t : tiles)
    {
        const auto [left, right] = t->sockets();
        lead (from, left);
        from = right;
    }
    lead (from, { x1 - 7.0f, y });
    for (auto [x, label] : { std::pair<float, const char*> { x0, "In" }, { x1, "Out" } })
    {
        // a quarter-inch jack plate
        const auto plate = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ x, y });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9b7b2), plate.getX(), plate.getY(), juce::Colour (0xff4a4946), plate.getRight(), plate.getBottom(), false));
        g.fillEllipse (plate);
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillEllipse (plate.reduced (5.0f));
        drawCaps (g, label, juce::Rectangle<float> (x - 20.0f, y + 12.0f, 40.0f, 14.0f), 10.5f, colours::textDim, juce::Justification::centred, 0.14f);
    }
}

//==============================================================================
namespace
{
    // Every page has the same top: the module's name and on light on the left, the
    // type picker in the middle, one line of description under it.
    constexpr int headerH = 36, descTop = 40, descH = 20, gearTop = 70;
    // kept clear under the gear, so a message about the audio connection never covers a switch
    constexpr int gearFloor = 40;
}

//==============================================================================
MicMap::MicMap (RiffhouseProcessor& p) : proc (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag the mic. Toward the middle of the speaker is brighter, toward the edge warmer. Further back lets the room in.");
}

MicMap::Plan MicMap::plan() const
{
    Plan p;
    p.area = getLocalBounds().toFloat().withTrimmedTop (24.0f);
    p.coneR = std::min (150.0f, p.area.getWidth() * 0.4f);
    p.cx = p.area.getCentreX() - 20.0f;
    p.baffleY = p.area.getY() + 74.0f;
    p.travel = p.coneR * 0.72f;                 // as far as the mic goes toward the edge
    p.near = p.baffleY + 7.0f;                  // the mic's nose at distance 0
    p.far = p.area.getBottom() - 66.0f;         // and at distance 10
    return p;
}

void MicMap::paint (juce::Graphics& g)
{
    const auto p = plan();
    const juce::Colour ink (0xffc9ccd0);
    drawCaps (g, "The mic, seen from above", getLocalBounds().toFloat().removeFromTop (16.0f), 11.5f, colours::textDim, juce::Justification::centredLeft, 0.14f);

    const float pos = juce::jlimit (0.0f, 10.0f, proc.value (P::cabMic)) / 10.0f;
    const float dist = juce::jlimit (0.0f, 10.0f, proc.value (P::cabDistance)) / 10.0f;
    const float apexY = p.baffleY - 46.0f, capR = p.coneR * 0.25f;

    // the cabinet's front board, cut through, with the hole the speaker sits in
    for (float side : { -1.0f, 1.0f })
    {
        const float x0 = p.cx + side * (p.coneR + 4.0f), x1 = side < 0.0f ? p.area.getX() : p.area.getRight();
        const juce::Rectangle<float> board (std::min (x0, x1), p.baffleY - 4.0f, std::abs (x1 - x0), 8.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4640), 0.0f, board.getY(), juce::Colour (0xff23211e), 0.0f, board.getBottom(), false));
        g.fillRect (board);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (float x = board.getX() + 5.0f; x < board.getRight(); x += 7.0f) // the cut grain of plywood
            g.drawLine (x, board.getY() + 1.0f, x - 3.0f, board.getBottom() - 1.0f, 0.8f);
    }
    // the speaker in section: paper cone falling back to the dust cap, the magnet behind it
    {
        juce::Path cone;
        cone.startNewSubPath (p.cx - p.coneR, p.baffleY);
        cone.lineTo (p.cx - capR, apexY);
        cone.quadraticTo (p.cx, apexY + capR * 0.95f, p.cx + capR, apexY);
        cone.lineTo (p.cx + p.coneR, p.baffleY);
        juce::Path fill = cone;
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b352d).withAlpha (0.9f), 0.0f, apexY, juce::Colour (0xff1c1a17).withAlpha (0.25f), 0.0f, p.baffleY, false));
        g.fillPath (fill);
        g.setColour (ink.withAlpha (0.8f));
        g.strokePath (cone, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // the pressed steel basket that holds it all, and the magnet on its back
        const juce::Rectangle<float> magnet (p.cx - capR * 0.95f, apexY - 20.0f, capR * 1.9f, 15.0f);
        juce::Path basket;
        basket.startNewSubPath (p.cx - p.coneR - 3.0f, p.baffleY - 5.0f);
        basket.lineTo (magnet.getX() + 3.0f, magnet.getBottom());
        basket.startNewSubPath (p.cx + p.coneR + 3.0f, p.baffleY - 5.0f);
        basket.lineTo (magnet.getRight() - 3.0f, magnet.getBottom());
        g.setColour (ink.withAlpha (0.28f));
        g.strokePath (basket, juce::PathStrokeType (1.2f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5b5e62), 0.0f, magnet.getY(), juce::Colour (0xff26282a), 0.0f, magnet.getBottom(), false));
        g.fillRoundedRectangle (magnet, 2.0f);
        g.setColour (juce::Colour (0xff3a3c3f));
        g.fillRect (juce::Rectangle<float> (capR * 1.2f, 6.0f).withCentre ({ p.cx, magnet.getBottom() + 2.0f }));
    }

    // the mic
    const float mx = p.cx + p.travel * pos;
    const float nose = p.near + (p.far - p.near) * dist;
    {
        // what it points at: a faint line from its nose to the paper
        const float t = std::abs (mx - p.cx) <= capR ? 1.0f : 1.0f - (std::abs (mx - p.cx) - capR) / (p.coneR - capR);
        const float hitY = p.baffleY - (p.baffleY - apexY) * juce::jlimit (0.0f, 1.0f, t);
        juce::Path aim;
        aim.startNewSubPath (mx, nose - 3.0f);
        aim.lineTo (mx, hitY + 3.0f);
        juce::Path dotted;
        const float dashes[] = { 2.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dotted, aim, dashes, 2);
        g.setColour (colours::lamp.withAlpha (0.85f));
        g.fillPath (dotted);
    }
    {
        const float headW = 19.0f, headL = 15.0f, bodyW = 13.0f, bodyL = 46.0f;
        // cable from the tail, off the bottom of the picture
        juce::Path cable;
        cable.startNewSubPath (mx, nose + headL + bodyL);
        cable.cubicTo (mx, nose + headL + bodyL + 26.0f, mx + 26.0f, p.area.getBottom() - 8.0f, mx + 60.0f, p.area.getBottom() + 4.0f);
        g.setColour (juce::Colour (0xff070707));
        g.strokePath (cable, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.strokePath (cable, juce::PathStrokeType (0.8f), juce::AffineTransform::translation (-0.8f, -0.4f));

        const juce::Rectangle<float> body (mx - bodyW * 0.5f, nose + headL - 1.0f, bodyW, bodyL);
        juce::ColourGradient tube (juce::Colour (0xff17181a), body.getX(), 0.0f, juce::Colour (0xff0b0c0d), body.getRight(), 0.0f, false);
        tube.addColour (0.3, juce::Colour (0xff8b9095));
        tube.addColour (0.55, juce::Colour (0xff3a3d40));
        g.setGradientFill (tube);
        juce::Path bodyPath;
        bodyPath.startNewSubPath (body.getX(), body.getY());
        bodyPath.lineTo (body.getRight(), body.getY());
        bodyPath.lineTo (body.getRight() - 1.8f, body.getBottom());
        bodyPath.lineTo (body.getX() + 1.8f, body.getBottom());
        bodyPath.closeSubPath();
        g.fillPath (bodyPath);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRect (body.getX(), body.getY() + bodyL * 0.42f, bodyW, 2.0f);

        const juce::Rectangle<float> head (mx - headW * 0.5f, nose, headW, headL);
        juce::ColourGradient cap (juce::Colour (0xff2a2c2f), head.getX(), 0.0f, juce::Colour (0xff0e0f10), head.getRight(), 0.0f, false);
        cap.addColour (0.3, juce::Colour (0xffb4b8bc));
        cap.addColour (0.55, juce::Colour (0xff53575b));
        g.setGradientFill (cap);
        g.fillRoundedRectangle (head, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        for (float y = head.getY() + 3.5f; y < head.getBottom() - 1.0f; y += 3.5f)
            g.fillRect (head.getX(), y, headW, 1.0f);
        if (dragging || isMouseOver())
        {
            g.setColour (colours::lamp.withAlpha (dragging ? 0.9f : 0.5f));
            g.drawRoundedRectangle (head.getUnion (body).expanded (5.0f), 5.0f, 1.2f);
        }
    }

    // what moving it does, written where you would move it to
    const float capY = p.area.getBottom() - 34.0f;
    auto note = [&] (const juce::String& where, const juce::String& what, float x, float y, juce::Justification j)
    {
        const juce::Rectangle<float> r (j.testFlags (juce::Justification::right) ? x - 120.0f : (j.testFlags (juce::Justification::left) ? x : x - 60.0f), y, 120.0f, 14.0f);
        drawCaps (g, where, r, 11.0f, colours::textDim, j, 0.12f);
        g.setFont (font (12.5f));
        g.setColour (colours::textFaint.brighter (0.35f));
        g.drawText (what, r.translated (0.0f, 15.0f), j, false);
    };
    note ("Middle", "bright, cutting", p.cx, capY, juce::Justification::centred);
    note ("Edge", "warm, soft", p.cx + p.travel + 56.0f, capY, juce::Justification::centred);
    note ("Close", "full, direct", p.area.getX(), p.near + 4.0f, juce::Justification::centredLeft);
    note ("Far", "airy, roomy", p.area.getX(), p.far + 4.0f, juce::Justification::centredLeft);
}

void MicMap::moveTo (juce::Point<float> at)
{
    const auto p = plan();
    // the hand holds the mic by its body, a little behind the nose
    const float pos = juce::jlimit (0.0f, 1.0f, (at.x - p.cx) / p.travel);
    const float dist = juce::jlimit (0.0f, 1.0f, (at.y - 24.0f - p.near) / (p.far - p.near));
    proc.param (P::cabMic)->setValueNotifyingHost (proc.param (P::cabMic)->convertTo0to1 (pos * 10.0f));
    proc.param (P::cabDistance)->setValueNotifyingHost (proc.param (P::cabDistance)->convertTo0to1 (dist * 10.0f));
    repaint();
}

void MicMap::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    proc.param (P::cabMic)->beginChangeGesture();
    proc.param (P::cabDistance)->beginChangeGesture();
    moveTo (e.position);
}

void MicMap::mouseDrag (const juce::MouseEvent& e) { moveTo (e.position); }

void MicMap::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    proc.param (P::cabMic)->endChangeGesture();
    proc.param (P::cabDistance)->endChangeGesture();
    repaint();
}

ModulePage::ModulePage (RiffhouseProcessor& p, int idx)
    : proc (p), info (modules()[(size_t) idx]), index (idx), power (p, info.onParam, moduleColour (idx))
{
    power.style = PowerSwitch::Style::led;
    power.setTooltip ("Switch on or off");
    addAndMakeVisible (power);
    power.onChange = [this] (bool) { repaint(); };
    cabMode = index == mCab;
    if (! cabMode)
    {
        // the pedal's own footswitch turns it on and off too
        foot = std::make_unique<Footswitch> (*proc.param (info.onParam));
        foot->onChange = [this] (bool) { repaint(); };
        addAndMakeVisible (*foot);
    }
    if (index == mWah)
    {
        // the wah has no stomp switch: its treadle is the control and the switch
        foot->setVisible (false);
        treadle = std::make_unique<Treadle> (*proc.param (P::wahPos), *proc.param (info.onParam));
        addAndMakeVisible (*treadle);
    }
    if (info.typeParam >= 0)
    {
        types = std::make_unique<ChoiceSelector> (proc, (P) info.typeParam);
        types->onChange = [this] (int)
        {
            resized();
            updateTypeText();
            updateEnablement();
            repaint();
        };
        // Asking for a file only when the player picked "your IR" here. A preset or
        // automation that lands on it must never throw a file window at them.
        types->onPick = [this] (int sel)
        {
            if (index == mCab && sel == choices::cabUserIr && proc.userIrName().isEmpty() && loadIr != nullptr)
                loadIr->triggerClick();
        };
        addAndMakeVisible (*types);
    }
    for (auto id : info.knobs)
        addAndMakeVisible (knobs.add (new Knob (proc, id, moduleColour (index), id == P::cabMic ? juce::String ("Position") : juce::String())));

    if (index == mEq)
    {
        const P bands[] = { P::eq100, P::eq200, P::eq400, P::eq800, P::eq1k6, P::eq3k2, P::eq6k4, P::eqLevel };
        for (auto b : bands)
            addAndMakeVisible (faders.add (new Fader (proc, b, moduleColour (index), b == P::eqLevel ? "Level" : juce::String())));
    }
    if (index == mDelay)
    {
        sync = std::make_unique<ChoiceSelector> (proc, P::dlySync);
        sync->textHeight = 13.0f;
        sync->onChange = [this] (int) { updateEnablement(); repaint(); };
        addAndMakeVisible (*sync);
    }
    if (index == mCab)
    {
        micMap = std::make_unique<MicMap> (proc);
        addAndMakeVisible (*micMap);
        loadIr = std::make_unique<IconButton> (icons::folder(), "Load IR");
        loadIr->drawBackground = false;
        loadIr->iconSize = 16.0f;
        loadIr->setTooltip ("Use any cabinet impulse response (.wav)");
        loadIr->onClick = [this]
        {
            // owned by the page, so the file window closes with it
            chooser = std::make_unique<juce::FileChooser> ("Choose a cabinet IR", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile())
                                          extraText = proc.loadUserIr (f) ? "Loaded " + f.getFileName()
                                                                          : "Couldn't read " + f.getFileName();
                                      updateTypeText();
                                      resized();
                                      repaint();
                                  });
        };
        addAndMakeVisible (*loadIr);
    }
    updateTypeText();
    updateEnablement();
}

void ModulePage::updateTypeText()
{
    if (info.typeParam < 0)
    {
        typeText = info.blurb;
        return;
    }
    lastType = juce::roundToInt (proc.value ((P) info.typeParam));
    typeText = describeChoice ((P) info.typeParam, lastType);
    if (index == mCab && lastType == choices::cabUserIr)
        typeText = proc.userIrName().isNotEmpty() ? "Using your IR: " + proc.userIrName() : "Load a .wav impulse response to use your own cab.";
    if (extraText.isNotEmpty() && cabMode) typeText << "  " << extraText;
}

void ModulePage::updateEnablement()
{
    if (index == mDelay && knobs.size() > 0)
    {
        lastSync = juce::roundToInt (proc.value (P::dlySync));
        knobs[0]->setEnabled (lastSync == choices::syncOff);
    }
    if (index == mPitch && knobs.size() > 0)
    {
        const bool shift = juce::roundToInt (proc.value (P::pitchMode)) == choices::pitchShift;
        knobs[0]->setEnabled (shift);
    }
    if (index == mWah && knobs.size() > 1)
    {
        const bool manual = juce::roundToInt (proc.value (P::wahMode)) == choices::wahManual;
        if (treadle != nullptr) treadle->setRocking (manual);
        knobs[1]->setEnabled (! manual);
    }
}

void ModulePage::refresh()
{
    if (info.typeParam >= 0 && juce::roundToInt (proc.value ((P) info.typeParam)) != lastType)
    {
        resized();
        updateTypeText();
        updateEnablement();
        repaint();
    }
    if (cabMode)
    {
        const float m = proc.value (P::cabMic), d = proc.value (P::cabDistance);
        if (m != lastMic || d != lastDist) { lastMic = m; lastDist = d; repaint(); if (micMap != nullptr) micMap->repaint(); }
    }
    if (sync != nullptr && juce::roundToInt (proc.value (P::dlySync)) != lastSync)
    {
        updateEnablement();
        repaint();
    }
}

const PedalDesign& ModulePage::design() const
{
    const int type = info.typeParam >= 0 ? juce::roundToInt (proc.value ((P) info.typeParam)) : 0;
    return *pedalDesignFor (index, type);
}

void ModulePage::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (headerH);
    titleArea = head.removeFromLeft (260);
    const int tw = (int) capsWidth (spec (info.onParam).name, 12.0f) + 14;
    power.setBounds (titleArea.getX() + tw, titleArea.getY(), 56, headerH);
    auto right = head.removeFromRight (260);
    if (types != nullptr && index == mWah)
    {
        // the wah's choice is how it moves, not which pedal it is: a small switch on the right
        types->textHeight = 13.0f;
        types->setBounds (right.removeFromRight (150));
        syncLabel = right.removeFromRight (60);
    }
    else if (types != nullptr)
        types->setBounds (head.withSizeKeepingCentre (std::min (head.getWidth(), 520), headerH));
    if (sync != nullptr)
    {
        auto s = right.removeFromRight (150);
        sync->setBounds (s);
        syncLabel = right.removeFromRight (60);
    }
    if (loadIr != nullptr)
        loadIr->setBounds (right.removeFromRight (120));
    descArea = juce::Rectangle<int> (0, descTop, getWidth(), descH);

    const auto gear = getLocalBounds().withTrimmedTop (gearTop).withTrimmedBottom (gearFloor);
    if (cabMode)
    {
        // the cab stands on the floor with its mic in front; the mic's cable runs to the channel
        // strip beside it, and the four controls live on that box
        const auto room = gear.toFloat();
        const float floorY = room.getBottom() - 26.0f;
        // The cab has the left of the page and the mic's controls the right, each in a fixed
        // place, so nothing jumps when you step through the cabinets.
        const float preW = 392.0f, preH = 150.0f, gap = 60.0f;
        const int model = juce::roundToInt (proc.value (P::cabModel));
        const float zoneW = room.getWidth() - preW - gap;
        cab = cabLayout ({ room.getX(), room.getY(), zoneW, floorY - room.getY() }, model);
        const float cabW = cab.box.getWidth();
        const float x0 = cab.box.getX();
        cabArea = { x0 - 50.0f, room.getY() - 6.0f, cabW + 100.0f, floorY - room.getY() + 46.0f };
        pedalImage = renderCab (model, cab, cabArea, 2.0f, proc.userIrName());
        pre = micPreLayout ({ room.getRight() - preW, floorY - preH - 6.0f, preW, preH });
        preArea = pre.box.expanded (44.0f);
        preImage = renderMicPre (pre, preArea, 2.0f);
        if (micMap != nullptr)
            micMap->setBounds (juce::Rectangle<float> (pre.box.getX(), room.getY() + 4.0f, preW, pre.box.getY() - room.getY() - 26.0f).toNearestInt());
        for (int i = 0; i < knobs.size() && i < (int) pre.cells.size(); ++i)
        {
            knobs[i]->setGear (KnobStyle::dome, juce::Colour (0xff16191b), juce::Colour (0xffc9ccd0), Face::instrument, true);
            knobs[i]->capsLabel = true;
            knobs[i]->setLabelHeight (20);
            knobs[i]->setBounds (pre.cells[(size_t) i].withSizeKeepingCentre (pre.cells[(size_t) i].getWidth() + 8.0f, pre.knobSize + 20.0f).toNearestInt());
        }
        return;
    }

    const auto& d = design();
    const bool fad = ! faders.isEmpty();
    pedalArea = gear.toFloat().withTrimmedBottom (10.0f);
    pedal = pedalLayout (pedalArea, d, fad ? faders.size() : (d.form == 5 ? knobs.size() - 1 : knobs.size()), fad);
    pedalImage = renderPedal (d, pedal, pedalArea, 2.0f);
    foot->setStyle (footswitchStyle (pedal), juce::Colour (d.body));
    foot->setBounds (pedal.footswitch.toNearestInt());
    if (treadle != nullptr) treadle->setBounds (pedal.treadle.toNearestInt());
    if (fad)
    {
        const int n = faders.size();
        const float cw = pedal.knobs.getWidth() / ((float) n + 0.4f);
        for (int i = 0; i < n; ++i)
        {
            const float x = pedal.knobs.getX() + cw * ((float) i + (i == n - 1 ? 0.4f : 0.0f));
            faders[i]->setGear (juce::Colour (d.ink));
            faders[i]->setBounds (juce::Rectangle<float> (x, pedal.knobs.getY(), cw, pedal.knobs.getHeight()).toNearestInt());
        }
        return;
    }
    // on the treadle pedal the first control is the treadle itself
    const int first = pedal.form == 5 ? 1 : 0;
    if (first > 0) knobs[0]->setVisible (false);
    const int n = knobs.size() - first;
    const int labelH = 20;
    for (int k = 0; k < n; ++k)
    {
        const int i = k + first;
        const auto cell = pedalKnobCell (pedal, k, n);
        const int size = (int) std::min (cell.getWidth() - 6.0f, std::min (cell.getHeight() - (float) labelH, 96.0f));
        knobs[i]->setGear (d.knob, juce::Colour (d.knobCap), juce::Colour (d.ink), d.face == Face::shoulders ? Face::shoulders : Face::instrument, d.upper);
        knobs[i]->setLabelHeight (labelH);
        knobs[i]->setBounds (cell.withSizeKeepingCentre ((float) size, (float) (size + labelH)).toNearestInt());
    }
}

void ModulePage::paint (juce::Graphics& g)
{
    const bool on = power.isOn();
    drawCaps (g, spec (info.onParam).name, titleArea.toFloat(), 12.0f, colours::textDim);
    if (types == nullptr || index == mWah)
        drawCaps (g, spec (info.onParam).name, getLocalBounds().removeFromTop (headerH).toFloat(), 22.0f, colours::text,
                  juce::Justification::centred, 0.06f);
    if (sync != nullptr || index == mWah)
        drawCaps (g, index == mWah ? "Mode" : "Sync", syncLabel.toFloat(), 11.5f, colours::textDim, juce::Justification::centredRight);
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawText (typeText, descArea, juce::Justification::centred, true);

    if (cabMode)
    {
        if (pedalImage.isValid())
        {
            g.setOpacity (on ? 1.0f : 0.55f);
            g.drawImage (pedalImage, cabArea);
            g.drawImage (preImage, preArea);
            g.setOpacity (1.0f);
        }
        drawCabMic (g, cab, proc.value (P::cabMic), proc.value (P::cabDistance), pre.socket);
        return;
    }
    if (pedalImage.isValid())
        g.drawImage (pedalImage, pedalArea);
    drawPedalLed (g, pedal, design(), on);
}

//==============================================================================
AmpPage::AmpPage (RiffhouseProcessor& p)
    : proc (p), power (p, P::ampOn, colours::accent), jewel (p, P::ampOn, colours::accent), selector (p, P::ampModel)
{
    power.style = PowerSwitch::Style::led;
    power.setTooltip ("Switch the amp on or off");
    jewel.style = PowerSwitch::Style::hidden;
    jewel.setTooltip ("Power");
    for (auto* s : { &power, &jewel })
    {
        addAndMakeVisible (*s);
        s->onChange = [this] (bool) { repaint(); };
    }

    selector.displayName = [this] (int m) { return nameFor (m); };
    selector.stepper = [this] (int cur, int d)
    {
        const int n = choices::numAmpModels;
        int to = (cur + d + n) % n;
        if (to == choices::neuralCapture && proc.captureName().isEmpty())
            to = (to + d + n) % n; // skip the capture slot until one is loaded
        return to;
    };
    selector.buildMenu = [this] (juce::PopupMenu& m)
    {
        const auto& names = spec (P::ampModel).choices;
        auto add = [&] (int i) { m.addItem (i + 1, names[(size_t) i], true, i == current); };
        m.addSectionHeader ("Clean");
        add (choices::glassClean); add (choices::tweedEdge); add (choices::studioDirect);
        m.addSectionHeader ("Crunch and rock");
        add (choices::chimeTop); add (choices::britPlexi); add (choices::brit800);
        m.addSectionHeader ("High gain");
        add (choices::hotRodLead); add (choices::modernHigh); add (choices::tightMetal);
        m.addSectionHeader ("Bass");
        add (choices::bassStation);
        m.addSectionHeader ("Neural capture");
        if (proc.captureName().isNotEmpty())
            m.addItem (choices::neuralCapture + 1, proc.captureName(), true, current == choices::neuralCapture);
        m.addItem (1000, utf8 ("Load a capture file (.nam)\xe2\x80\xa6"));
    };
    selector.onMenuItem = [this] (int id) { if (id == 1000) loadCapture(); };
    selector.onChange = [this] (int m) { current = m; refresh(); resized(); repaint(); };
    addAndMakeVisible (selector);

    const P ks[] = { P::ampGain, P::ampBass, P::ampMid, P::ampTreble, P::ampPresence, P::ampDepth, P::ampMaster, P::ampLevel };
    for (auto id : ks)
        addAndMakeVisible (knobs.add (new Knob (proc, id, colours::accent, id == P::ampLevel ? "Volume" : juce::String())));

    loadNam.drawBackground = false;
    loadNam.iconSize = 16.0f;
    loadNam.onClick = [this] { loadCapture(); };
    addChildComponent (loadNam);
    current = selector.getSelected();
    refresh();
}

juce::String AmpPage::nameFor (int m) const
{
    if (m == choices::neuralCapture && proc.captureName().isNotEmpty()) return proc.captureName();
    return spec (P::ampModel).choices[(size_t) juce::jlimit (0, choices::numAmpModels - 1, m)];
}

void AmpPage::loadCapture()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a NAM capture", juce::File(), "*.nam");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (! f.existsAsFile()) return;
                              status = "Loading " + f.getFileName() + utf8 ("\xe2\x80\xa6");
                              repaint();
                              juce::Component::SafePointer<AmpPage> safe (this);
                              proc.loadCaptureAsync (f, [safe, f] (juce::String err)
                              {
                                  if (safe == nullptr) return;
                                  safe->status = err.isEmpty() ? juce::String() : "Couldn't load " + f.getFileName() + ": " + err;
                                  safe->refresh();
                                  safe->resized();
                                  safe->repaint();
                              });
                          });
}

void AmpPage::refresh()
{
    const bool neural = current == choices::neuralCapture;
    loadNam.setVisible (neural);
    for (int i = 0; i < knobs.size(); ++i)
    {
        const bool usable = ! neural || (i != 4 && i != 5 && i != 6); // presence, depth, master are part of the capture
        knobs[i]->setEnabled (usable);
        knobs[i]->setAlpha (usable ? 1.0f : 0.35f);
    }
}

void AmpPage::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (headerH);
    titleArea = head.removeFromLeft (330);
    power.setBounds (titleArea.getX() + (int) capsWidth ("Amp", 12.0f) + 14, titleArea.getY(), 56, headerH);
    tagArea = head.removeFromRight (330);
    loadNam.setBounds (tagArea.withTrimmedLeft (110));
    selector.setBounds (head.withSizeKeepingCentre (std::min (head.getWidth(), 480), headerH));
    descArea = juce::Rectangle<int> (0, descTop, getWidth(), descH);

    // the amp itself fills the rest of the page
    const auto gear = getLocalBounds().withTrimmedTop (gearTop).withTrimmedBottom (gearFloor).toFloat();
    ampArea = gear.withSizeKeepingCentre (std::min (gear.getWidth(), 1080.0f), std::min (gear.getHeight(), 420.0f));
    const auto& d = ampDesign (current);
    layout = ampLayout (ampArea, d);
    const juce::String display = current == choices::neuralCapture ? (proc.captureName().isNotEmpty() ? proc.captureName() : juce::String ("no capture"))
                                                                    : juce::String (spec (P::ampModel).choices[(size_t) current]).toLowerCase();
    ampImage = renderAmp (d, layout, ampArea, 2.0f, display);
    // the whole switch is the target: the lamp, and the lever or rocker under it
    jewel.setBounds ((d.lamp == 1 ? layout.jewel.expanded (10.0f, 22.0f).translated (0.0f, -10.0f) : layout.jewel.expanded (d.lamp == 2 ? 6.0f : 8.0f, d.lamp == 2 ? 12.0f : 8.0f)).toNearestInt());

    const int labelH = 22;
    for (int i = 0; i < knobs.size() && i < (int) layout.cells.size(); ++i)
    {
        auto* k = knobs[i];
        k->setGear (d.knob, juce::Colour (d.knobCap), juce::Colour (d.ink), d.face == Face::shoulders ? Face::shoulders : Face::instrument, d.upper);
        k->setNumbers (layout.knobSize >= 64.0f);
        k->setLabelHeight (labelH);
        k->setBounds (layout.cells[(size_t) i].toNearestInt());
    }
}

void AmpPage::paint (juce::Graphics& g)
{
    const bool on = power.isOn();
    drawCaps (g, "Amp", titleArea.toFloat(), 12.0f, colours::textDim);
    if (current != choices::neuralCapture)
        drawCaps (g, ampTags (current), tagArea.toFloat(), 11.5f, colours::textDim, juce::Justification::centredRight, 0.08f);
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawText (status.isNotEmpty() ? status : describeChoice (P::ampModel, current), descArea, juce::Justification::centred, true);

    if (ampImage.isValid())
    {
        g.setOpacity (on ? 1.0f : 0.55f);
        g.drawImage (ampImage, ampArea);
        g.setOpacity (1.0f);
    }
    drawAmpJewel (g, layout, ampDesign (current), on);
}
} // namespace rh::ui
