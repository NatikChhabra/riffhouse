// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Widgets.h"

namespace rh::ui
{
void drawCard (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float radius)
{
    if (look() == Look::pedalboard)
    {
        drawEnclosure (g, r, radius, fill == colours::panel ? colours::panel : fill, true);
        return;
    }
    if (fill == colours::panel)
    {
        drawPanel (g, r, radius);
        return;
    }
    // floating surfaces (popovers): one step lighter, a short shadow so they sit above the page
    juce::Path p;
    p.addRoundedRectangle (r, radius);
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 14, { 0, 6 }).drawForPath (g, p);
    g.setColour (fill);
    g.fillPath (p);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawHorizontalLine ((int) r.getY(), r.getX() + radius, r.getRight() - radius);
}

//==============================================================================
Knob::Knob (RiffhouseProcessor& proc, P id, juce::Colour colour, juce::String labelOverride)
{
    const auto& s = spec (id);
    label = labelOverride.isNotEmpty() ? labelOverride : juce::String (s.name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.getProperties().set ("bipolar", s.min < 0.0f && std::abs (s.min + s.max) < 1e-3f);
    slider.setPopupMenuEnabled (false);
    slider.onHover = [this] { repaint(); };
    slider.onValueChange = [this] { repaint(); };
    slider.onDragStart = [this] { repaint(); };
    slider.onDragEnd = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state(), s.key, slider);
    slider.setDoubleClickReturnValue (true, s.def);
    setTitle (label);
}

void Knob::setColour (juce::Colour c)
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    slider.repaint();
}

void Knob::setGear (KnobStyle style, juce::Colour cap, juce::Colour ink, Face face, bool upper)
{
    gear = true;
    gearInk = ink;
    gearFace = face;
    gearUpper = upper;
    slider.getProperties().set ("gear", (int) style + 1);
    slider.getProperties().set ("cap", (juce::int64) cap.getARGB());
    slider.getProperties().set ("ink", (juce::int64) ink.getARGB());
    repaint();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    slider.setBounds (r.removeFromTop (std::max (10, r.getHeight() - labelH)));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (labelH);
    const bool active = slider.isMouseOverOrDragging();
    const float h = std::max (12.0f, std::min (15.0f, (float) labelH * 0.44f));
    if (gear)
    {
        // lettering printed on the panel; the value shows while you turn it
        const juce::String txt = active ? slider.getTextFromValue (slider.getValue()) : (gearUpper ? label.toUpperCase() : label.toLowerCase());
        g.setFont (active ? faceFont (Face::instrumentSemi, h) : faceFont (gearFace, gearFace == Face::shoulders ? h * 1.05f : h * 0.95f));
        g.setColour (gearInk.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
        g.drawFittedText (txt, r.withTrimmedTop (1), juce::Justification::centredTop, 1);
        return;
    }
    g.setFont (font (h, active ? Weight::semibold : Weight::medium));
    g.setColour (active ? colours::text : (isEnabled() ? colours::textDim : colours::textFaint));
    const juce::String txt = active ? slider.getTextFromValue (slider.getValue()) : label;
    g.drawFittedText (txt, r.withTrimmedTop (2), juce::Justification::centredTop, 1);
}

//==============================================================================
Fader::Fader (RiffhouseProcessor& proc, P id, juce::Colour colour, juce::String labelOverride)
{
    const auto& s = spec (id);
    label = labelOverride.isNotEmpty() ? labelOverride : juce::String (s.name);
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::trackColourId, colour);
    slider.getProperties().set ("bipolar", s.min < 0.0f && std::abs (s.min + s.max) < 1e-3f);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state(), s.key, slider);
    slider.setDoubleClickReturnValue (true, s.def);
}

void Fader::setGear (juce::Colour ink)
{
    gear = true;
    gearInk = ink;
    slider.getProperties().set ("gear", 1);
    slider.getProperties().set ("ink", (juce::int64) ink.getARGB());
    repaint();
}

void Fader::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (22).withTrimmedBottom (24));
}

void Fader::paint (juce::Graphics& g)
{
    if (gear)
    {
        const bool active = slider.isMouseOverOrDragging();
        g.setColour (gearInk.withAlpha (active ? 1.0f : 0.0f));
        g.setFont (faceFont (Face::instrumentSemi, 11.5f));
        g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromTop (20), juce::Justification::centred);
        g.setColour (gearInk);
        g.setFont (faceFont (Face::shoulders, 13.0f));
        g.drawText (label.toUpperCase(), getLocalBounds().removeFromBottom (20), juce::Justification::centred);
        return;
    }
    g.setFont (font (12.0f, Weight::medium));
    g.setColour (colours::textDim);
    g.drawText (slider.getTextFromValue (slider.getValue()), getLocalBounds().removeFromTop (20), juce::Justification::centred);
    g.setColour (colours::text);
    g.setFont (font (13.0f, Weight::semibold));
    g.drawText (label, getLocalBounds().removeFromBottom (20), juce::Justification::centred);
}

//==============================================================================
PowerSwitch::PowerSwitch (RiffhouseProcessor& proc, P id, juce::Colour c)
    : attachment (*proc.param (id), [this] (float v) { on = v >= 0.5f; repaint(); if (onChange) onChange (on); }),
      colour (c)
{
    attachment.sendInitialUpdate();

    setTitle (spec (id).name);
    setTooltip ("Turn on or off");
}

void PowerSwitch::paint (juce::Graphics& g)
{
    if (style == Style::hidden) return;
    if (style == Style::led)
    {
        auto r = getLocalBounds().toFloat();
        const auto dot = r.removeFromLeft (10.0f).withSizeKeepingCentre (8.0f, 8.0f);
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colours::record.withAlpha (0.5f), dot.getCentreX(), dot.getCentreY(),
                                                     colours::record.withAlpha (0.0f), dot.getCentreX() + 9.0f, dot.getCentreY(), true));
            g.fillEllipse (dot.expanded (9.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff3d6), dot.getX() + 2.0f, dot.getY() + 2.0f,
                                                     colours::record, dot.getRight(), dot.getBottom(), true));
        }
        else
            g.setColour (juce::Colour (0xff2a2826));
        g.fillEllipse (dot);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (dot.expanded (1.0f), 1.0f);
        r.removeFromLeft (8.0f);
        drawCaps (g, on ? "On" : "Off", r, 11.0f, on ? colours::text : (isMouseOver() ? colours::textDim : colours::textFaint));
        return;
    }
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float d = std::min (r.getWidth(), r.getHeight());
    const auto c = r.withSizeKeepingCentre (d, d);
    if (on)
    {
        g.setColour (colour.withAlpha (0.22f));
        g.fillEllipse (c);
        g.setColour (colour);
        g.drawEllipse (c.reduced (0.75f), 1.5f);
    }
    else
    {
        g.setColour (isMouseOver() ? colours::raisedHi.brighter (0.1f) : colours::raisedHi);
        g.fillEllipse (c);
    }
    strokeIcon (g, icons::power(), c.reduced (d * 0.2f), on ? colour.brighter (0.3f) : colours::textFaint, std::max (1.5f, d * 0.07f));
}

void PowerSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

//==============================================================================
ChipRow::ChipRow (RiffhouseProcessor& proc, P id, juce::Colour c) : colour (c)
{
    for (auto* n : spec (id).choices) items.add (n);
    attachment = std::make_unique<juce::ParameterAttachment> (*proc.param (id), [this] (float v)
    {
        selected = juce::roundToInt (v);
        repaint();
        if (onChange) onChange (selected);
    });
    pick = [this] (int i) { attachment->setValueAsCompleteGesture ((float) i); };
    attachment->sendInitialUpdate();

}

ChipRow::ChipRow (juce::StringArray it, juce::Colour c, std::function<void (int)> onPick)
    : items (std::move (it)), colour (c), pick (std::move (onPick))
{

}

int ChipRow::preferredWidth() const
{
    float w = 0;
    for (auto& s : items) w += juce::GlyphArrangement::getStringWidth (font (14.0f, Weight::medium), s) + 30.0f + 8.0f;
    return (int) w;
}

std::vector<juce::Rectangle<float>> ChipRow::layoutChips() const
{
    // flows onto more lines if it doesn't fit
    std::vector<juce::Rectangle<float>> out;
    float x = 0, y = 0;
    const float gap = 8.0f;
    for (auto& s : items)
    {
        const float w = juce::GlyphArrangement::getStringWidth (font (14.0f, Weight::medium), s) + 30.0f;
        if (x > 0 && x + w > (float) getWidth()) { x = 0; y += chipHeight + gap; }
        out.push_back ({ x, y, w, chipHeight });
        x += w + gap;
    }
    return out;
}

void ChipRow::paint (juce::Graphics& g)
{
    const auto rects = layoutChips();
    g.setFont (font (14.0f, Weight::medium));
    for (int i = 0; i < items.size(); ++i)
    {
        const auto r = rects[(size_t) i];
        const bool sel = i == selected;
        if (sel)
        {
            g.setColour (colour.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.2f);
        }
        else
        {
            g.setColour (i == hover ? colours::raisedHi : colours::raised);
            g.fillRoundedRectangle (r, 7.0f);
        }
        g.setColour (sel ? colours::text : (i == hover ? colours::text : colours::textDim));
        g.drawText (items[i], r, juce::Justification::centred);
    }
}

void ChipRow::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    const auto rects = layoutChips();
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].contains (e.position)) h = (int) i;
    if (h != hover) { hover = h; repaint(); }
}

void ChipRow::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    const auto rects = layoutChips();
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].contains (e.position))
        {
            if (attachment == nullptr)
            {
                selected = (int) i;
                repaint();
            }
            if (pick) pick ((int) i);
            if (attachment == nullptr && onChange) onChange ((int) i);
        }
}

//==============================================================================
IconButton::IconButton (juce::Path p, juce::String text, bool filled)
    : juce::Button (text), icon (std::move (p)), filled_ (filled)
{

}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    if (drawBackground)
    {
        auto bgc = on ? activeColour.withAlpha (0.16f) : (over ? colours::raisedHi : colours::raised);
        if (down) bgc = bgc.darker (0.1f);
        g.setColour (bgc);
        g.fillRoundedRectangle (r.reduced (0.5f), std::min (6.0f, r.getHeight() * 0.5f));
    }
    else if (on)
    {
        // a lit state is a short line under the icon, like a selected tab
        g.setColour (activeColour);
        g.fillRect (r.withSizeKeepingCentre (18.0f, 2.0f).withY (r.getBottom() - 4.0f));
    }
    const auto c = ! isEnabled() ? colours::textFaint : (on ? activeColour : (over ? colours::text : iconColour));
    const auto caption = getButtonText();
    if (caption.isEmpty())
    {
        const auto ir = r.withSizeKeepingCentre (iconSize, iconSize);
        if (filled_) fillIcon (g, icon, ir, c); else strokeIcon (g, icon, ir, c);
        return;
    }
    g.setFont (font (14.0f, Weight::medium));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), caption);
    const float total = iconSize + 8.0f + tw;
    auto content = r.withSizeKeepingCentre (total, r.getHeight());
    const auto ir = content.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);
    if (filled_) fillIcon (g, icon, ir, c); else strokeIcon (g, icon, ir, c);
    content.removeFromLeft (8.0f);
    g.setColour (on ? activeColour : (isEnabled() ? colours::text : colours::textFaint));
    g.drawText (caption, content, juce::Justification::centredLeft);
}

//==============================================================================
void Meter::setLevel (float lin)
{
    const float db = juce::Decibels::gainToDecibels (lin, -100.0f);
    // fast attack, smooth fall
    const float next = db > levelDb ? db : std::max (db, levelDb - 1.6f);
    if (db >= holdDb) { holdDb = db; holdTicks = 40; }
    else if (--holdTicks <= 0) holdDb = std::max (-100.0f, holdDb - 1.2f);
    if (std::abs (next - levelDb) > 0.05f || holdTicks > 0)
    {
        levelDb = next;
        repaint();
    }
}

void Meter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float radius = 0.0f;
    g.setColour (juce::Colour (0xff262524));
    g.fillRoundedRectangle (r, radius);
    auto norm = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
    const float v = norm (levelDb);
    const auto barColour = levelDb > -1.0f ? colours::hot : (levelDb > -9.0f ? colours::warn : colours::good);
    g.setColour (barColour);
    if (vertical)
        g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() * (1.0f - v)), radius);
    else
        g.fillRoundedRectangle (r.withWidth (r.getWidth() * v), radius);
    // cut into segments, like an LED ladder
    g.setColour (colours::bg);
    if (! vertical)
        for (float x = r.getX() + 3.0f; x < r.getRight(); x += 4.0f)
            g.fillRect (x, r.getY(), 1.0f, r.getHeight());
    if (holdDb > -59.0f)
    {
        const float hv = norm (holdDb);
        g.setColour (colours::text.withAlpha (0.8f));
        if (vertical) g.fillRect (r.getX(), r.getBottom() - r.getHeight() * hv, r.getWidth(), 1.0f);
        else g.fillRect (r.getX() + r.getWidth() * hv - 1.0f, r.getY(), 1.0f, r.getHeight());
    }
}

//==============================================================================
ChoiceSelector::ChoiceSelector (RiffhouseProcessor& proc, P id)
    : attachment (*proc.param (id), [this] (float v)
      {
          selected = juce::roundToInt (v);
          resized();
          repaint();
          if (onChange) onChange (selected);
      })
{
    for (auto* n : spec (id).choices) items.add (n);
    for (auto* b : { &prev, &next })
    {
        b->drawBackground = false;
        b->iconSize = 16.0f;
        b->iconColour = colours::textFaint;
        addAndMakeVisible (*b);
    }
    prev.setTooltip ("Previous");
    next.setTooltip ("Next");
    auto step = [this] (int d)
    {
        const int n = items.size();
        const int to = stepper ? stepper (selected, d) : (selected + d + n) % n;
        select (to);
    };
    prev.onClick = [step] { step (-1); };
    next.onClick = [step] { step (1); };
    attachment.sendInitialUpdate();
    setTooltip ("Click for the full list");
}

juce::String ChoiceSelector::nameFor (int i) const
{
    if (displayName) return displayName (i);
    return items[i];
}

void ChoiceSelector::resized()
{
    const float tw = capsWidth (nameFor (selected), textHeight, 0.06f) + 30.0f;
    const int w = juce::jmin (getWidth() - 64, (int) tw);
    nameArea = juce::Rectangle<int> (w, getHeight()).withCentre (getLocalBounds().getCentre());
    prev.setBounds (nameArea.getX() - 32, 0, 28, getHeight());
    next.setBounds (nameArea.getRight() + 4, 0, 28, getHeight());
}

void ChoiceSelector::paint (juce::Graphics& g)
{
    const bool hover = nameArea.contains (getMouseXYRelative()) && isMouseOver();
    auto r = nameArea.toFloat();
    const auto chev = r.removeFromRight (18.0f).withSizeKeepingCentre (12.0f, 12.0f);
    drawCaps (g, nameFor (selected), r, textHeight, hover ? colours::accentHi : colours::text, juce::Justification::centred, 0.06f);
    strokeIcon (g, icons::chevronDown(), chev, hover ? colours::text : colours::textFaint, 1.6f);
}

void ChoiceSelector::showMenu()
{
    juce::PopupMenu m;
    if (buildMenu) buildMenu (m);
    else
        for (int i = 0; i < items.size(); ++i)
            m.addItem (i + 1, nameFor (i), true, i == selected);
    juce::Component::SafePointer<ChoiceSelector> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (std::max (240, nameArea.getWidth())),
                     [safe] (int r)
                     {
                         if (safe == nullptr || r <= 0) return;
                         if (r <= safe->items.size()) safe->select (r - 1);
                         else if (safe->onMenuItem) safe->onMenuItem (r);
                     });
}

void ChoiceSelector::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && nameArea.contains (e.getPosition()))
        showMenu();
}
} // namespace rh::ui
