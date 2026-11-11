// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Theme.h"
#include "Gear.h"

#if __has_include("AssetData.h")
 #include "AssetData.h"
 #define RIFFHOUSE_HAS_ASSETS 1
#endif

namespace rh::ui
{
namespace
{
    juce::Typeface::Ptr load ([[maybe_unused]] const char* data, [[maybe_unused]] int size)
    {
       #if RIFFHOUSE_HAS_ASSETS
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
       #else
        return nullptr;
       #endif
    }

    struct Faces
    {
        juce::Typeface::Ptr inter[4], instrument[3], doto, shoulders, script, brush, slab;
        Faces()
        {
           #if RIFFHOUSE_HAS_ASSETS
            using namespace RiffhouseAssets;
            inter[0] = load (InterRegular_ttf, InterRegular_ttfSize);
            inter[1] = load (InterMedium_ttf, InterMedium_ttfSize);
            inter[2] = load (InterSemiBold_ttf, InterSemiBold_ttfSize);
            inter[3] = load (InterDisplayBold_ttf, InterDisplayBold_ttfSize);
            instrument[0] = load (InstrumentSansRegular_ttf, InstrumentSansRegular_ttfSize);
            instrument[1] = load (InstrumentSansMedium_ttf, InstrumentSansMedium_ttfSize);
            instrument[2] = load (InstrumentSansSemiBold_ttf, InstrumentSansSemiBold_ttfSize);
            doto = load (DotoBlack_ttf, DotoBlack_ttfSize);
            shoulders = load (BigShouldersExtraBold_ttf, BigShouldersExtraBold_ttfSize);
            script = load (YellowtailRegular_ttf, YellowtailRegular_ttfSize);
            brush = load (KaushanScriptRegular_ttf, KaushanScriptRegular_ttfSize);
            slab = load (AlfaSlabOneRegular_ttf, AlfaSlabOneRegular_ttfSize);
           #endif
        }
    };

    Faces& faces()
    {
        static Faces f;
        return f;
    }

    Look currentLook = Look::studio;
} // namespace

Look look() noexcept { return currentLook; }

juce::String lookName (Look l)
{
    switch (l)
    {
        case Look::studio:     return "Studio";
        case Look::signal:     return "Signal";
        case Look::pedalboard: return "Pedalboard";
    }
    return {};
}

juce::Colour stringColour (int s)
{
    static const juce::uint32 c[] = { 0xffff6fd8, 0xffffd23f, 0xff3fa9ff, 0xffff9f1c, 0xff3ee08f, 0xffb18cff };
    return juce::Colour (c[((s % 6) + 6) % 6]);
}

void setLook (Look l)
{
    using namespace colours;
    currentLook = l;
    switch (l)
    {
        case Look::studio:
            // neutral warm charcoal, like a dark studio: the gear brings the colour
            bg = juce::Colour (0xff121212); bgTop = juce::Colour (0xff171717); panel = juce::Colour (0xff1a1a1a);
            raised = juce::Colour (0xff232323); raisedHi = juce::Colour (0xff2d2d2d); line = juce::Colour (0xff2e2e2e);
            well = juce::Colour (0xff0a0a0a); text = juce::Colour (0xffece9e3); textDim = juce::Colour (0xffa5a199);
            textFaint = juce::Colour (0xff7f7b74); accent = juce::Colour (0xffe6dfd2); accentHi = juce::Colour (0xffffffff);
            accentInk = juce::Colour (0xff121212); good = juce::Colour (0xff8fd694); warn = juce::Colour (0xffe8c25a); hot = juce::Colour (0xffffb020);
            record = juce::Colour (0xffe5483a);
            break;
        case Look::signal:
            bg = juce::Colour (0xff141518); bgTop = juce::Colour (0xff17181c); panel = juce::Colour (0xff1c1e22);
            raised = juce::Colour (0xff24272c); raisedHi = juce::Colour (0xff2d3137); line = juce::Colour (0xff2a2d33);
            well = juce::Colour (0xff050607); text = juce::Colour (0xffe9e7e2); textDim = juce::Colour (0xff8d9096);
            textFaint = juce::Colour (0xff5d6066); accent = juce::Colour (0xffff9f1c); accentHi = juce::Colour (0xffffc266);
            accentInk = juce::Colour (0xff141518); good = stringColour (4); warn = stringColour (1); hot = stringColour (1);
            record = juce::Colour (0xffe5483a);
            break;
        case Look::pedalboard:
            bg = juce::Colour (0xff1b1d1b); bgTop = juce::Colour (0xff1f211f); panel = juce::Colour (0xff2c2f2d);
            raised = juce::Colour (0xff353836); raisedHi = juce::Colour (0xff3e423f); line = juce::Colour (0xff3a3d3b);
            well = juce::Colour (0xff0b0d0c); text = juce::Colour (0xffeeeae0); textDim = juce::Colour (0xffaaa597);
            textFaint = juce::Colour (0xff76736a); accent = juce::Colour (0xfff4a524); accentHi = juce::Colour (0xffffc457);
            accentInk = juce::Colour (0xff1b1d1b); good = juce::Colour (0xff7bd88f); warn = juce::Colour (0xfff4d03f);
            hot = juce::Colour (0xfff4d03f); record = juce::Colour (0xffe5483a);
            loud = juce::Colour (0xff0f6b66); silk = juce::Colour (0xffece4cf);
            break;
    }
}

juce::String titleCase (const juce::String& t)
{
    return currentLook == Look::pedalboard ? t.toLowerCase() : t;
}

juce::Font font (float height, Weight w)
{
    auto& f = faces();
    juce::Typeface::Ptr tf;
    const bool instrument = f.instrument[0] != nullptr; // Instrument Sans everywhere: warmer than the usual app face
    switch (w)
    {
        case Weight::regular:  tf = instrument ? f.instrument[0] : f.inter[0]; break;
        case Weight::medium:   tf = instrument ? f.instrument[1] : f.inter[1]; break;
        case Weight::semibold: tf = instrument ? f.instrument[2] : f.inter[2]; break;
        case Weight::display:
            tf = currentLook == Look::pedalboard ? f.shoulders : (instrument ? f.instrument[2] : f.inter[3]);
            if (currentLook == Look::pedalboard) height *= 1.12f; // a condensed face reads smaller
            break;
        case Weight::numeric:
            tf = currentLook == Look::studio ? f.inter[3] : f.doto;
            if (currentLook != Look::studio) height *= 1.4f; // Doto's dots sit small in its em box
            break;
    }
    if (tf == nullptr)
        return juce::Font (juce::FontOptions (height, w == Weight::regular ? juce::Font::plain : juce::Font::bold));
    auto font = juce::Font (juce::FontOptions (tf).withHeight (height));
    if (w == Weight::display && currentLook == Look::signal) font.setExtraKerningFactor (-0.02f);
    return font;
}

juce::Font capsFont (float height, float tracking)
{
    return font (height, Weight::semibold).withExtraKerningFactor (tracking);
}

float capsWidth (const juce::String& t, float height, float tracking)
{
    return juce::GlyphArrangement::getStringWidth (capsFont (height, tracking), t.toUpperCase());
}

void drawCaps (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float height, juce::Colour c,
               juce::Justification j, float tracking)
{
    g.setFont (capsFont (height, tracking));
    g.setColour (c);
    g.drawText (t.toUpperCase(), r, j, true);
}

juce::Image renderStage (int w, int h)
{
    // a dark room with one soft light on the gear, like a product shot. Worked out per
    // pixel with a little dither, so the falloff is smooth instead of showing rings.
    juce::Image img (juce::Image::RGB, std::max (1, w), std::max (1, h), false);
    juce::Image::BitmapData px (img, juce::Image::BitmapData::writeOnly);
    const float cx = (float) w * 0.5f, cy = (float) h * 0.42f, rad = (float) w * 0.62f;
    juce::Random rnd (17);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const float dx = ((float) x - cx) / rad, dy = ((float) y - cy) / (rad * 0.8f);
            const float t = std::exp (-2.2f * (dx * dx + dy * dy)); // 1 at the light, 0 far away
            const float n = rnd.nextFloat() - 0.5f;
            const float v = 14.0f + 21.0f * t + n;
            const auto c = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (v));
            const auto cr = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (v + 1.2f * t + n * 0.3f)); // the light is a touch warm
            px.setPixelColour (x, y, juce::Colour (cr, c, (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (v - 0.8f * t))));
        }
    return img;
}

juce::Font faceFont (Face face, float height)
{
    auto& f = faces();
    juce::Typeface::Ptr tf;
    switch (face)
    {
        case Face::inter:          tf = f.inter[1]; break;
        case Face::interBold:      tf = f.inter[3]; break;
        case Face::instrument:     tf = f.instrument[1]; break;
        case Face::instrumentSemi: tf = f.instrument[2]; break;
        case Face::doto:           tf = f.doto; height *= 1.4f; break;
        case Face::shoulders:      tf = f.shoulders; height *= 1.12f; break;
        case Face::script:         tf = f.script; break;
        case Face::brush:          tf = f.brush; break;
        case Face::slab:           tf = f.slab; break;
    }
    if (tf == nullptr) return juce::Font (juce::FontOptions (height, juce::Font::bold));
    return juce::Font (juce::FontOptions (tf).withHeight (height));
}

//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    switch (currentLook)
    {
        case Look::studio:
        {
            g.setColour (colours::panel);
            g.fillRoundedRectangle (r, radius);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0, r.getY(),
                                                     juce::Colours::white.withAlpha (0.0f), 0, r.getY() + 24.0f, false));
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            break;
        }
        case Look::signal:
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.4f), 8, { 0, 2 }).drawForPath (g, p);
            g.setColour (colours::panel);
            g.fillPath (p);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) r.getY(), r.getX() + radius, r.getRight() - radius);
            break;
        }
        case Look::pedalboard:
            drawEnclosure (g, r, radius, colours::panel, true);
            break;
    }
}

void drawWell (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    if (currentLook == Look::studio)
    {
        g.setColour (colours::raised);
        g.fillRoundedRectangle (r, radius);
        return;
    }
    g.setColour (colours::well);
    g.fillRoundedRectangle (r, radius);
    // inset: a shadow along the top edge, a faint lip along the bottom
    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.7f), 0, r.getY(),
                                             juce::Colours::black.withAlpha (0.0f), 0, r.getY() + 7.0f, false));
    g.fillRect (r.withHeight (7.0f));
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());
}

void drawEnclosure (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour body, bool screws)
{
    // the box's foot: a darker band just under it, then a soft shadow on the mat
    {
        juce::Path foot;
        foot.addRoundedRectangle (r.translated (0.0f, 4.0f), radius);
        juce::DropShadow (juce::Colours::black.withAlpha (0.45f), 18, { 0, 10 }).drawForPath (g, foot);
        g.setColour (body.darker (0.65f));
        g.fillPath (foot);
    }
    g.setGradientFill (juce::ColourGradient (body.brighter (0.07f), 0, r.getY(), body.darker (0.08f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + radius, r.getRight() - radius);
    if (screws)
    {
        for (auto p : { r.getTopLeft() + juce::Point<float> (10, 10), r.getTopRight() + juce::Point<float> (-10, 10),
                        r.getBottomLeft() + juce::Point<float> (10, -10), r.getBottomRight() + juce::Point<float> (-10, -10) })
        {
            const auto sr = juce::Rectangle<float> (7.0f, 7.0f).withCentre (p);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9e9b), sr.getX(), sr.getY(), juce::Colour (0xff3b3e3c), sr.getRight(), sr.getBottom(), false));
            g.fillEllipse (sr);
            g.setColour (juce::Colour (0xff2a2c2b));
            g.drawLine (sr.getX() + 1.5f, sr.getCentreY(), sr.getRight() - 1.5f, sr.getCentreY(), 1.0f);
        }
    }
}

//==============================================================================
LookAndFeel::LookAndFeel() { applyColours(); }

void LookAndFeel::applyColours()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::DocumentWindow::textColourId, textDim);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::TextEditor::backgroundColourId, raised);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ComboBox::backgroundColourId, raised);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textFaint);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, raisedHi);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, raisedHi);
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, raised);
    setColour (juce::TooltipWindow::backgroundColourId, raisedHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, line);
    setColour (juce::GroupComponent::outlineColourId, line);
    setColour (juce::GroupComponent::textColourId, textDim);
    setColour (juce::HyperlinkButton::textColourId, accent);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, accent.withAlpha (0.2f));
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, text);
    setColour (juce::FileBrowserComponent::currentPathBoxBackgroundColourId, raised);
    setColour (juce::FileBrowserComponent::filenameBoxBackgroundColourId, raised);
    setColour (juce::ListBox::textColourId, text);

    getCurrentColourScheme().setUIColour (ColourScheme::widgetBackground, raised);
    getCurrentColourScheme().setUIColour (ColourScheme::windowBackground, panel);
    getCurrentColourScheme().setUIColour (ColourScheme::outline, line);
    getCurrentColourScheme().setUIColour (ColourScheme::defaultText, text);
    getCurrentColourScheme().setUIColour (ColourScheme::defaultFill, accent);
    getCurrentColourScheme().setUIColour (ColourScheme::highlightedFill, accent.withAlpha (0.25f));
    getCurrentColourScheme().setUIColour (ColourScheme::highlightedText, text);
    getCurrentColourScheme().setUIColour (ColourScheme::menuBackground, raised);
    getCurrentColourScheme().setUIColour (ColourScheme::menuText, text);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        if (auto tf = font (f.getHeight(), f.isBold() ? Weight::semibold : Weight::regular).getTypefacePtr())
            return tf;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    using namespace colours;
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    if (const int gear = s.getProperties()["gear"]; gear > 0)
    {
        const float sz = std::min (area.getWidth(), area.getHeight());
        drawGearKnob (g, area.withSizeKeepingCentre (sz, sz), start + pos * (end - start), start, end, (KnobStyle) (gear - 1),
                      juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["cap"]),
                      juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["ink"]),
                      s.isEnabled(), s.isMouseOverOrDragging(), (bool) s.getProperties()["numbers"]);
        return;
    }
    const float size = std::min (area.getWidth(), area.getHeight());
    const auto c = area.withSizeKeepingCentre (size, size).getCentre();
    const float rOuter = size * 0.5f;
    const float arcW = std::max (3.0f, size * 0.055f);
    const float rArc = rOuter - arcW * 0.5f;
    const float rBody = rOuter - arcW - std::max (4.0f, size * 0.07f);
    const auto fill = s.findColour (juce::Slider::rotarySliderFillColourId);
    const bool enabled = s.isEnabled();
    const float angle = start + pos * (end - start);

    // track
    juce::Path track;
    track.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, start, end, true);
    g.setColour (raisedHi);
    g.strokePath (track, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc (from the centre for bipolar controls)
    const bool bipolar = s.getProperties()["bipolar"];
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, rArc, rArc, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (enabled ? fill : textFaint);
        g.strokePath (val, juce::PathStrokeType (arcW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body: soft shadow + subtle vertical gradient, like a machined cap
    const auto body = juce::Rectangle<float> (rBody * 2.0f, rBody * 2.0f).withCentre (c);
    {
        juce::Path shadowPath;
        shadowPath.addEllipse (body.translated (0.0f, size * 0.03f));
        juce::DropShadow (juce::Colours::black.withAlpha (0.55f), (int) (size * 0.10f), { 0, (int) (size * 0.04f) })
            .drawForPath (g, shadowPath);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3f4a), c.x, body.getY(),
                                             juce::Colour (0xff1b1e24), c.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawEllipse (body.reduced (0.5f), 1.0f);
    const auto inner = body.reduced (rBody * 0.16f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2e37), c.x, inner.getY(),
                                             juce::Colour (0xff22252d), c.x, inner.getBottom(), false));
    g.fillEllipse (inner);

    // pointer
    const float pLen = rBody * 0.62f;
    const float pW = std::max (2.5f, size * 0.04f);
    juce::Path ptr;
    ptr.addRoundedRectangle (-pW * 0.5f, -rBody + rBody * 0.18f, pW, pLen, pW * 0.5f);
    g.setColour (enabled ? text : textFaint);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));

    if (s.isMouseOverOrDragging() && enabled)
    {
        g.setColour (fill.withAlpha (0.10f));
        g.fillEllipse (body.expanded (arcW * 0.6f));
    }
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace colours;
    const auto fill = s.findColour (juce::Slider::trackColourId);
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if ((int) s.getProperties()["gear"] > 0 && vertical)
    {
        // a slide pot: a cut slot, printed ticks, a ribbed cap with a white line
        const juce::Colour ink ((juce::uint32) (juce::int64) s.getProperties()["ink"]);
        const auto slot = r.withSizeKeepingCentre (5.0f, r.getHeight() + 6.0f);
        g.setColour (juce::Colour (0xff050505));
        g.fillRoundedRectangle (slot, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine ((int) slot.getBottom(), slot.getX(), slot.getRight());
        for (int i = 0; i <= 8; ++i)
        {
            const float ty = r.getY() + r.getHeight() * (float) i / 8.0f;
            const float tw = i == 4 ? 9.0f : 5.0f;
            g.setColour (ink.withAlpha (i % 4 == 0 ? 0.95f : 0.6f));
            g.fillRect (r.getCentreX() - 9.0f - tw, ty - 0.5f, tw, 1.0f);
            g.fillRect (r.getCentreX() + 9.0f, ty - 0.5f, tw, 1.0f);
        }
        const auto cap = juce::Rectangle<float> (18.0f, 30.0f).withCentre ({ r.getCentreX(), pos });
        juce::Path cp;
        cp.addRoundedRectangle (cap, 3.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, { 0, 3 }).drawForPath (g, cp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3e41), cap.getX(), 0, juce::Colour (0xff111213), cap.getRight(), 0, false));
        g.fillPath (cp);
        for (float yy = cap.getY() + 4.0f; yy < cap.getBottom() - 3.0f; yy += 3.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.fillRect (cap.getX() + 2.0f, yy, cap.getWidth() - 4.0f, 1.0f);
        }
        g.setColour (juce::Colour (0xfff1ead8));
        g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 4.0f, 2.0f));
        return;
    }
    const float t = 4.0f;
    const bool bipolar = s.getProperties()["bipolar"];

    if (vertical)
    {
        const auto track = r.withSizeKeepingCentre (t, r.getHeight());
        g.setColour (raisedHi);
        g.fillRoundedRectangle (track, t * 0.5f);
        const float mid = bipolar ? r.getCentreY() : r.getBottom();
        const float top = std::min (mid, pos), bot = std::max (mid, pos);
        g.setColour (s.isEnabled() ? fill : textFaint);
        g.fillRoundedRectangle (track.withY (top).withHeight (std::max (0.0f, bot - top)), t * 0.5f);
        if (bipolar)
        {
            g.setColour (textFaint);
            g.fillRect (juce::Rectangle<float> (12.0f, 1.0f).withCentre ({ r.getCentreX(), r.getCentreY() }));
        }
        const auto thumb = juce::Rectangle<float> (22.0f, 12.0f).withCentre ({ r.getCentreX(), pos });
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (thumb.translated (0, 1.5f), 4.0f);
        g.setColour (s.isMouseOverOrDragging() ? juce::Colours::white : text);
        g.fillRoundedRectangle (thumb, 4.0f);
    }
    else
    {
        const auto track = r.withSizeKeepingCentre (r.getWidth(), t);
        g.setColour (raisedHi);
        g.fillRoundedRectangle (track, t * 0.5f);
        const float mid = bipolar ? r.getCentreX() : r.getX();
        const float l = std::min (mid, pos), rr = std::max (mid, pos);
        g.setColour (s.isEnabled() ? fill : textFaint);
        g.fillRoundedRectangle (track.withX (l).withWidth (std::max (0.0f, rr - l)), t * 0.5f);
        const auto thumb = juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ pos, r.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillEllipse (thumb.translated (0, 1.5f));
        g.setColour (s.isMouseOverOrDragging() ? juce::Colours::white : text);
        g.fillEllipse (thumb);
    }
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = std::min (8.0f, r.getHeight() * 0.5f);
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : base;
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.5f);
    if (down) c = c.darker (0.15f);
    else if (over) c = c.brighter (0.12f);
    g.setColour (c);
    g.fillRoundedRectangle (r, radius);
    if (! b.getToggleState())
    {
        g.setColour (juce::Colours::white.withAlpha (over ? 0.10f : 0.05f));
        g.drawRoundedRectangle (r, radius, 1.0f);
    }
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (10, 0), juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    // a pill switch followed by the label
    using namespace colours;
    const auto r = b.getLocalBounds().toFloat();
    const auto sw = juce::Rectangle<float> (34.0f, 20.0f).withY (r.getCentreY() - 10.0f).withX (r.getX() + 1.0f);
    const bool on = b.getToggleState();
    const auto onColour = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? onColour : (over ? raisedHi.brighter (0.1f) : raisedHi));
    g.fillRoundedRectangle (sw, 10.0f);
    const auto knob = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ on ? sw.getRight() - 10.0f : sw.getX() + 10.0f, sw.getCentreY() });
    g.setColour (on ? bg : textDim);
    g.fillEllipse (knob);
    g.setColour (b.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.setFont (font (14.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (44), juce::Justification::centredLeft, 2);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) w, (float) h), 8.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    const bool focused = e.hasKeyboardFocus (true) && ! e.isReadOnly();
    g.setColour (e.findColour (focused ? juce::TextEditor::focusedOutlineColourId : juce::TextEditor::outlineColourId));
    g.drawRoundedRectangle (juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f), 8.0f, focused ? 1.5f : 1.0f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
    strokeIcon (g, icons::chevronDown(), juce::Rectangle<float> ((float) w - 26.0f, (float) h * 0.5f - 8.0f, 16.0f, 16.0f),
                box.findColour (juce::ComboBox::arrowColourId));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::raised);
    g.setColour (colours::line);
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                     bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& txt,
                                     const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (colours::line);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (4, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (colours::accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
    }
    if (isTicked)
        strokeIcon (g, icons::check(), r.removeFromLeft (26).toFloat().withSizeKeepingCentre (16, 16), colours::accent, 2.0f);
    else
        r.removeFromLeft (26);
    g.setColour (textColour != nullptr ? *textColour : (isActive ? colours::text : colours::textFaint));
    g.setFont (getPopupMenuFont());
    g.drawFittedText (txt, r.withTrimmedRight (hasSubMenu ? 24 : 8), juce::Justification::centredLeft, 1);
    if (shortcut.isNotEmpty())
    {
        g.setColour (colours::textFaint);
        g.setFont (font (13.0f));
        g.drawText (shortcut, r.withTrimmedRight (10), juce::Justification::centredRight);
    }
    if (hasSubMenu)
        strokeIcon (g, icons::chevronRight(), r.removeFromRight (20).toFloat().withSizeKeepingCentre (14, 14), colours::textDim);
    juce::ignoreUnused (icon);
}

void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setColour (colours::textFaint);
    g.setFont (font (12.0f, Weight::semibold));
    g.drawText (name.toUpperCase(), area.withTrimmedLeft (30).withTrimmedTop (4), juce::Justification::centredLeft);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> ((float) x + 2, (float) thumbStart, (float) w - 4, (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, (float) y + 2, (float) thumbSize, (float) h - 4);
    g.setColour (colours::raisedHi.brighter (over || down ? 0.25f : 0.0f));
    g.fillRoundedRectangle (thumb, 3.0f);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& t, int w, int h)
{
    g.setColour (colours::raisedHi);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) w, (float) h), 6.0f);
    g.setColour (colours::text);
    g.setFont (font (13.0f));
    g.drawFittedText (t, juce::Rectangle<int> (w, h).reduced (10, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& t, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const int w = std::min (320, (int) juce::GlyphArrangement::getStringWidth (font (13.0f), t) + 24);
    const int lines = 1 + (int) juce::GlyphArrangement::getStringWidth (font (13.0f), t) / 300;
    const int h = 10 + 18 * lines;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}

void LookAndFeel::drawDocumentWindowTitleBar (juce::DocumentWindow& win, juce::Graphics& g, int w, int h, int titleSpaceX,
                                              int titleSpaceW, const juce::Image*, bool)
{
    g.fillAll (colours::bg);
    g.setColour (colours::textDim);
    g.setFont (font (13.0f, Weight::medium));
    g.drawText (win.getName(), titleSpaceX, 0, titleSpaceW, h, juce::Justification::centred);
    juce::ignoreUnused (w);
}

//==============================================================================
namespace icons
{
    juce::Path chevronLeft()  { juce::Path p; p.startNewSubPath (15, 5); p.lineTo (8, 12); p.lineTo (15, 19); return p; }
    juce::Path chevronRight() { juce::Path p; p.startNewSubPath (9, 5); p.lineTo (16, 12); p.lineTo (9, 19); return p; }
    juce::Path chevronDown()  { juce::Path p; p.startNewSubPath (6, 9); p.lineTo (12, 15); p.lineTo (18, 9); return p; }
    juce::Path check()        { juce::Path p; p.startNewSubPath (5, 12.5f); p.lineTo (10, 17.5f); p.lineTo (19, 7); return p; }

    juce::Path undo()
    {
        // an arrow that comes back round on itself
        juce::Path p;
        p.startNewSubPath (9, 14); p.lineTo (4, 9); p.lineTo (9, 4);
        p.startNewSubPath (4, 9); p.lineTo (14.5f, 9);
        p.addCentredArc (14.5f, 14.5f, 5.5f, 5.5f, 0.0f, 0.0f, juce::MathConstants<float>::pi, false);
        p.lineTo (11, 20);
        return p;
    }
    juce::Path redo()
    {
        auto p = undo();
        p.applyTransform (juce::AffineTransform::scale (-1.0f, 1.0f).translated (24.0f, 0.0f));
        return p;
    }

    juce::Path loop()
    {
        // two arrows chasing each other round
        juce::Path p;
        p.startNewSubPath (17, 2); p.lineTo (21, 6); p.lineTo (17, 10);
        p.startNewSubPath (3, 11); p.lineTo (3, 10); p.quadraticTo (3, 6, 7, 6); p.lineTo (21, 6);
        p.startNewSubPath (7, 22); p.lineTo (3, 18); p.lineTo (7, 14);
        p.startNewSubPath (21, 13); p.lineTo (21, 14); p.quadraticTo (21, 18, 17, 18); p.lineTo (3, 18);
        return p;
    }

    juce::Path star (bool)
    {
        juce::Path p;
        p.addStar ({ 12.0f, 12.6f }, 5, 4.2f, 9.6f, 0.0f);
        return p;
    }

    juce::Path save()
    {
        juce::Path p;
        p.addRoundedRectangle (4.0f, 4.0f, 16.0f, 16.0f, 2.5f);
        p.startNewSubPath (8, 4); p.lineTo (8, 9); p.lineTo (15, 9); p.lineTo (15, 4);
        p.addRoundedRectangle (8.0f, 13.0f, 8.0f, 7.0f, 1.0f);
        return p;
    }

    juce::Path tuningFork()
    {
        juce::Path p;
        p.startNewSubPath (8, 3); p.lineTo (8, 10);
        p.cubicTo (8, 13.5f, 16, 13.5f, 16, 10);
        p.lineTo (16, 3);
        p.startNewSubPath (12, 12.6f); p.lineTo (12, 21);
        return p;
    }

    juce::Path metronome()
    {
        juce::Path p;
        p.startNewSubPath (9, 3.5f); p.lineTo (15, 3.5f); p.lineTo (19, 20.5f); p.lineTo (5, 20.5f); p.closeSubPath();
        p.startNewSubPath (12, 16); p.lineTo (17, 7);
        p.startNewSubPath (7, 16.5f); p.lineTo (17, 16.5f);
        return p;
    }

    juce::Path gear()
    {
        juce::Path p;
        const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 2);
            const float r = (i % 2 == 0) ? 9.5f : 7.4f;
            const float a1 = a - 0.17f, a2 = a + 0.17f;
            const juce::Point<float> p1 (12 + r * std::sin (a1), 12 - r * std::cos (a1));
            const juce::Point<float> p2 (12 + r * std::sin (a2), 12 - r * std::cos (a2));
            if (i == 0) p.startNewSubPath (p1); else p.lineTo (p1);
            p.lineTo (p2);
        }
        p.closeSubPath();
        p.addEllipse (9.0f, 9.0f, 6.0f, 6.0f);
        return p;
    }

    juce::Path search()
    {
        juce::Path p;
        p.addEllipse (4.0f, 4.0f, 12.0f, 12.0f);
        p.startNewSubPath (14.5f, 14.5f); p.lineTo (20, 20);
        return p;
    }

    juce::Path close()
    {
        juce::Path p;
        p.startNewSubPath (6, 6); p.lineTo (18, 18);
        p.startNewSubPath (18, 6); p.lineTo (6, 18);
        return p;
    }

    juce::Path folder()
    {
        juce::Path p;
        p.startNewSubPath (3, 7); p.lineTo (3, 18.5f); p.lineTo (21, 18.5f); p.lineTo (21, 8.5f); p.lineTo (12, 8.5f);
        p.lineTo (10, 5.5f); p.lineTo (3, 5.5f); p.closeSubPath();
        return p;
    }

    juce::Path play()  { juce::Path p; p.addTriangle (7, 4.5f, 7, 19.5f, 19.5f, 12); return p; }
    juce::Path pause() { juce::Path p; p.addRoundedRectangle (6.0f, 5.0f, 4.0f, 14.0f, 1.0f); p.addRoundedRectangle (14.0f, 5.0f, 4.0f, 14.0f, 1.0f); return p; }
    juce::Path stop()  { juce::Path p; p.addRoundedRectangle (6.0f, 6.0f, 12.0f, 12.0f, 2.0f); return p; }

    juce::Path headphones()
    {
        juce::Path p;
        p.startNewSubPath (4, 16); p.lineTo (4, 12);
        p.addCentredArc (12, 12, 8, 8, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi);
        p.lineTo (20, 16);
        p.addRoundedRectangle (3.0f, 14.0f, 4.0f, 7.0f, 1.5f);
        p.addRoundedRectangle (17.0f, 14.0f, 4.0f, 7.0f, 1.5f);
        return p;
    }

    juce::Path power()
    {
        juce::Path p;
        p.addCentredArc (12, 13, 7.5f, 7.5f, 0, 0.75f, juce::MathConstants<float>::twoPi - 0.75f, true);
        p.startNewSubPath (12, 3.5f); p.lineTo (12, 11);
        return p;
    }

    juce::Path guitar()
    {
        juce::Path p;
        p.addEllipse (3.5f, 11.5f, 9.0f, 9.0f);
        p.startNewSubPath (11, 13); p.lineTo (19.5f, 4.5f);
        p.startNewSubPath (17, 3); p.lineTo (21, 7);
        return p;
    }

    juce::Path share()
    {
        juce::Path p;
        p.addEllipse (15.0f, 3.5f, 5.0f, 5.0f);
        p.addEllipse (4.0f, 9.5f, 5.0f, 5.0f);
        p.addEllipse (15.0f, 15.5f, 5.0f, 5.0f);
        p.startNewSubPath (8.5f, 10.8f); p.lineTo (15.4f, 7.2f);
        p.startNewSubPath (8.5f, 13.2f); p.lineTo (15.4f, 16.8f);
        return p;
    }

    juce::Path music()
    {
        juce::Path p;
        p.startNewSubPath (9, 17.5f); p.lineTo (9, 5.5f); p.lineTo (19, 3.5f); p.lineTo (19, 15.5f);
        p.addEllipse (4.5f, 15.0f, 5.0f, 5.0f);
        p.addEllipse (14.5f, 13.0f, 5.0f, 5.0f);
        return p;
    }
} // namespace icons

static juce::AffineTransform iconTransform (juce::Rectangle<float> area)
{
    const float s = std::min (area.getWidth(), area.getHeight()) / 24.0f;
    return juce::AffineTransform::scale (s).translated (area.getCentreX() - 12.0f * s, area.getCentreY() - 12.0f * s);
}

void fillIcon (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> area, juce::Colour c)
{
    g.setColour (c);
    g.fillPath (p, iconTransform (area));
}

void strokeIcon (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> area, juce::Colour c, float thickness)
{
    g.setColour (c);
    juce::Path t (p);
    t.applyTransform (iconTransform (area));
    g.strokePath (t, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
void drawBrandMark (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour ink, bool simple)
{
    // A guitar pick with a roof over the letter H cut out of it, and the amp's pilot light under
    // the roof. Drawn on a 100 unit square; the same numbers are in resources/brand/mark.svg.
    const float side = std::min (area.getWidth(), area.getHeight());
    const auto sq = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
    const float u = side / 100.0f;
    auto pt = [&] (float x, float y) { return juce::Point<float> (sq.getX() + x * u, sq.getY() + y * u); };
    juce::Path mark;
    mark.setUsingNonZeroWinding (false); // what is drawn inside the pick is a hole in it
    mark.startNewSubPath (pt (50.0f, 5.0f));
    mark.cubicTo (pt (74.0f, 5.0f), pt (95.0f, 14.0f), pt (95.0f, 36.0f));
    mark.cubicTo (pt (95.0f, 58.0f), pt (60.0f, 96.0f), pt (50.0f, 96.0f));
    mark.cubicTo (pt (40.0f, 96.0f), pt (5.0f, 58.0f), pt (5.0f, 36.0f));
    mark.cubicTo (pt (5.0f, 14.0f), pt (26.0f, 5.0f), pt (50.0f, 5.0f));
    mark.closeSubPath();
    mark.startNewSubPath (pt (50.0f, 21.0f));
    for (auto p : { pt (79.0f, 45.0f), pt (67.5f, 45.0f), pt (50.0f, 30.5f), pt (32.5f, 45.0f), pt (21.0f, 45.0f) }) mark.lineTo (p);
    mark.closeSubPath();
    if (! simple) // too fine to hold below about 40 pixels
    {
        mark.startNewSubPath (pt (34.5f, 51.0f));
        for (auto p : { pt (41.5f, 51.0f), pt (41.5f, 61.0f), pt (58.5f, 61.0f), pt (58.5f, 51.0f), pt (65.5f, 51.0f), pt (65.5f, 78.0f),
                        pt (58.5f, 78.0f), pt (58.5f, 68.0f), pt (41.5f, 68.0f), pt (41.5f, 78.0f), pt (34.5f, 78.0f) }) mark.lineTo (p);
        mark.closeSubPath();
    }
    g.setColour (ink);
    g.fillPath (mark);
    const auto c = pt (50.0f, 42.2f);
    const float r = (simple ? 5.0f : 4.4f) * u;
    g.setColour (juce::Colour (0xfff08a1c)); // the lamp, deep enough to read on the pick's cream
    g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
}

juce::Image renderAppIcon (int size, bool simple)
{
    juce::Image icon (juce::Image::ARGB, size, size, true);
    juce::Graphics g (icon);
    const float s = (float) size;
    const auto tile = juce::Rectangle<float> (s, s).reduced (s * (simple ? 0.0f : 0.03f));
    const float radius = tile.getWidth() * 0.225f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff211f1d), 0.0f, tile.getY(), juce::Colour (0xff0f0e0d), 0.0f, tile.getBottom(), false));
    g.fillRoundedRectangle (tile, radius);
    if (! simple)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), 0.0f, tile.getY(), juce::Colours::white.withAlpha (0.02f), 0.0f, tile.getBottom(), false));
        g.drawRoundedRectangle (tile.reduced (s * 0.003f), radius, s * 0.006f);
    }
    const float m = tile.getWidth() * (simple ? 0.88f : 0.80f);
    drawBrandMark (g, juce::Rectangle<float> (m, m).withCentre (tile.getCentre().translated (0.0f, m * 0.005f)), juce::Colour (0xffece9e3), simple);
    return icon;
}
} // namespace rh::ui
