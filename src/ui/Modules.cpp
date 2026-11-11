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
            return i == 0 ? "Set the position with the Pedal knob, or automate it while recording."
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
                px.setPixelColour (x, y, o.interpolatedWith (c, 0.6f).withMultipliedBrightness (0.42f).withAlpha (o.getAlpha()));
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
        const auto col = selected ? colours::text : (hover ? colours::text.withAlpha (0.8f) : (on ? colours::textDim : colours::textFaint));
        drawCaps (g, info.title, lab, 10.5f, col, juce::Justification::centred, 0.14f);
        if (selected)
        {
            const float tw = capsWidth (info.title, 10.5f, 0.14f);
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
        if (k != lastKey) repaint();
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
    // one patch cable running through the whole rig, from the guitar in to the speakers out
    const float y = cableY;
    const float x0 = 22.0f, x1 = (float) getWidth() - 22.0f;
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRect (x0, y - 2.0f, x1 - x0, 4.0f);
    g.setColour (juce::Colour (0xff2a2928));
    g.fillRect (x0, y - 1.5f, x1 - x0, 1.0f);
    for (auto [x, label] : { std::pair<float, const char*> { x0, "In" }, { x1, "Out" } })
    {
        // a quarter-inch jack plate
        const auto plate = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ x, y });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb9b7b2), plate.getX(), plate.getY(), juce::Colour (0xff4a4946), plate.getRight(), plate.getBottom(), false));
        g.fillEllipse (plate);
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillEllipse (plate.reduced (5.0f));
        drawCaps (g, label, juce::Rectangle<float> (x - 20.0f, y + 12.0f, 40.0f, 14.0f), 9.5f, colours::textFaint, juce::Justification::centred, 0.14f);
    }
}

//==============================================================================
namespace
{
    // Every page has the same top: the module's name and on light on the left, the
    // type picker in the middle, one line of description under it.
    constexpr int headerH = 36, descTop = 40, descH = 20, gearTop = 70;
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
    if (info.typeParam >= 0)
    {
        types = std::make_unique<ChoiceSelector> (proc, (P) info.typeParam);
        types->onChange = [this] (int sel)
        {
            resized();
            if (index == mCab && sel == choices::cabUserIr && proc.userIrName().isEmpty() && loadIr != nullptr)
                loadIr->triggerClick();
            updateTypeText();
            updateEnablement();
            repaint();
        };
        addAndMakeVisible (*types);
    }
    for (auto id : info.knobs)
        addAndMakeVisible (knobs.add (new Knob (proc, id, moduleColour (index))));

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
        loadIr = std::make_unique<IconButton> (icons::folder(), "Load IR");
        loadIr->drawBackground = false;
        loadIr->iconSize = 16.0f;
        loadIr->setTooltip ("Use any cabinet impulse response (.wav)");
        loadIr->onClick = [this]
        {
            auto chooser = std::make_shared<juce::FileChooser> ("Choose a cabinet IR", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this, chooser] (const juce::FileChooser& fc)
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
        knobs[0]->setAlpha (lastSync == choices::syncOff ? 1.0f : 0.4f);
    }
    if (index == mPitch && knobs.size() > 0)
    {
        const bool shift = juce::roundToInt (proc.value (P::pitchMode)) == choices::pitchShift;
        knobs[0]->setEnabled (shift);
        knobs[0]->setAlpha (shift ? 1.0f : 0.4f);
    }
    if (index == mWah && knobs.size() > 1)
    {
        const bool manual = juce::roundToInt (proc.value (P::wahMode)) == choices::wahManual;
        knobs[0]->setEnabled (manual);
        knobs[0]->setAlpha (manual ? 1.0f : 0.4f);
        knobs[1]->setEnabled (! manual);
        knobs[1]->setAlpha (manual ? 0.4f : 1.0f);
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
        if (m != lastMic || d != lastDist) { lastMic = m; lastDist = d; repaint (cabArea.toNearestInt().expanded (40)); }
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

    const auto gear = getLocalBounds().withTrimmedTop (gearTop);
    if (cabMode)
    {
        auto g = gear;
        auto knobRow = g.removeFromBottom (104);
        cabArea = g.toFloat().reduced (0.0f, 4.0f);
        const int model = juce::roundToInt (proc.value (P::cabModel));
        cab = cabLayout (cabArea, model);
        pedalImage = renderCab (model, cab, cabArea, 2.0f, proc.userIrName());
        const int n = knobs.size();
        const int cell = 120, size = 62;
        auto row = knobRow.withSizeKeepingCentre (cell * n, size + 24).translated (0, 6);
        for (int i = 0; i < n; ++i)
        {
            knobs[i]->setGear (KnobStyle::modern, colours::text, colours::textDim, Face::instrumentSemi, true);
            knobs[i]->setLabelHeight (24);
            knobs[i]->setBounds (row.removeFromLeft (cell).withSizeKeepingCentre (size, size + 24));
        }
        return;
    }

    const auto& d = design();
    const bool fad = ! faders.isEmpty();
    pedalArea = gear.toFloat().withTrimmedBottom (-6.0f);
    pedal = pedalLayout (pedalArea, d, fad ? faders.size() : knobs.size(), fad);
    pedalImage = renderPedal (d, pedal, pedalArea, 2.0f);
    foot->setBounds (pedal.footswitch.toNearestInt());
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
    const int n = knobs.size();
    const int labelH = 20;
    for (int i = 0; i < n; ++i)
    {
        const auto cell = pedalKnobCell (pedal, i, n);
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
        drawCaps (g, index == mWah ? "Mode" : "Sync", syncLabel.toFloat(), 11.0f, colours::textFaint, juce::Justification::centredRight);
    g.setColour (colours::textDim);
    g.setFont (font (13.5f));
    g.drawText (typeText, descArea, juce::Justification::centred, true);

    if (cabMode)
    {
        if (pedalImage.isValid())
        {
            g.setOpacity (on ? 1.0f : 0.5f);
            g.drawImage (pedalImage, cabArea);
            g.setOpacity (1.0f);
        }
        drawCabMic (g, cab, proc.value (P::cabMic), proc.value (P::cabDistance));
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
        m.addItem (1000, "Load a capture file (.nam)...");
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
                              status = "Loading " + f.getFileName() + "...";
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
    titleArea = head.removeFromLeft (260);
    power.setBounds (titleArea.getX() + (int) capsWidth ("Amp", 12.0f) + 14, titleArea.getY(), 56, headerH);
    tagArea = head.removeFromRight (260);
    loadNam.setBounds (tagArea.withTrimmedLeft (110));
    selector.setBounds (head.withSizeKeepingCentre (std::min (head.getWidth(), 560), headerH));
    descArea = juce::Rectangle<int> (0, descTop, getWidth(), descH);

    // the amp itself fills the rest of the page
    const auto gear = getLocalBounds().withTrimmedTop (gearTop).toFloat();
    ampArea = gear.withSizeKeepingCentre (std::min (gear.getWidth(), 1080.0f), std::min (gear.getHeight() - 8.0f, 420.0f));
    layout = ampLayout (ampArea);
    const auto& d = ampDesign (current);
    const juce::String display = current == choices::neuralCapture ? (proc.captureName().isNotEmpty() ? proc.captureName() : juce::String ("no capture"))
                                                                    : juce::String (spec (P::ampModel).choices[(size_t) current]).toLowerCase();
    ampImage = renderAmp (d, layout, ampArea, 2.0f, display);
    jewel.setBounds (layout.jewel.expanded (8.0f).toNearestInt());

    const int n = knobs.size();
    const auto kr = layout.knobs;
    const float cell = kr.getWidth() / (float) n;
    const int labelH = 22;
    const int size = (int) std::min (cell - 6.0f, kr.getHeight() - (float) labelH);
    for (int i = 0; i < n; ++i)
    {
        auto* k = knobs[i];
        k->setGear (d.knob, juce::Colour (d.knobCap), juce::Colour (d.ink), d.face == Face::shoulders ? Face::shoulders : Face::instrument, d.upper);
        k->setNumbers (true);
        k->setLabelHeight (labelH);
        k->setBounds (juce::Rectangle<float> (kr.getX() + cell * (float) i, kr.getY(), cell, kr.getHeight())
                          .withSizeKeepingCentre ((float) size, (float) (size + labelH)).toNearestInt());
    }
}

void AmpPage::paint (juce::Graphics& g)
{
    const bool on = power.isOn();
    drawCaps (g, "Amp", titleArea.toFloat(), 12.0f, colours::textDim);
    if (current != choices::neuralCapture)
        drawCaps (g, ampTags (current), tagArea.toFloat(), 10.5f, colours::textFaint, juce::Justification::centredRight, 0.1f);
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
