// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "PresetBrowser.h"

namespace rh::ui
{
class PresetBrowser::Categories : public juce::Component
{
public:
    explicit Categories (PresetBrowser& o) : owner (o) { }

    void update()
    {
        static const char* order[] = { "All", "Favourites", "Inspired By", "Essentials", "Clean", "Crunch", "Rock", "Blues",
                                        "Metal", "Lead", "Funk", "Ambient", "Lo-Fi", "Bass" };
        items.clear();
        for (auto* o : order) items.add (o);
        for (auto& c : owner.proc.presets().categories())
            items.addIfNotAlreadyThere (c);
        counts.clear();
        for (auto& c : items)
            counts.push_back ((int) owner.proc.presets().search ({}, c).size());
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < items.size(); ++i)
        {
            const auto r = rowBounds (i);
            const bool sel = items[i] == owner.category;
            if (sel || i == hover)
            {
                g.setColour (sel ? colours::accent.withAlpha (0.14f) : colours::raised);
                g.fillRoundedRectangle (r.toFloat(), 8.0f);
            }
            g.setFont (font (14.5f, sel ? Weight::semibold : Weight::medium));
            g.setColour (sel ? colours::accentHi : colours::text.withAlpha (0.85f));
            g.drawText (items[i], r.reduced (12, 0), juce::Justification::centredLeft);
            g.setFont (font (12.5f));
            g.setColour (colours::textFaint);
            g.drawText (juce::String (counts[(size_t) i]), r.reduced (12, 0), juce::Justification::centredRight);
            if (i == 2)
            {
                g.setColour (colours::line);
                g.fillRect (r.getX() + 12, r.getBottom() + 4, r.getWidth() - 24, 1);
            }
        }
    }

    juce::Rectangle<int> rowBounds (int i) const
    {
        const int extra = i >= 3 ? 9 : 0;
        return { 0, i * 34 + extra, getWidth(), 32 };
    }

    int rowAt (juce::Point<int> p) const
    {
        for (int i = 0; i < items.size(); ++i)
            if (rowBounds (i).contains (p)) return i;
        return -1;
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = rowAt (e.getPosition()); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int i = rowAt (e.getPosition());
        if (i >= 0)
        {
            owner.category = items[i];
            owner.refreshList();
            repaint();
        }
    }

    PresetBrowser& owner;
    juce::StringArray items;
    std::vector<int> counts;
    int hover = -1;
};

PresetBrowser::PresetBrowser (RiffhouseProcessor& p) : proc (p)
{
    categories = std::make_unique<Categories> (*this);
    addAndMakeVisible (*categories);

    search.setTextToShowWhenEmpty ("Search tones, artists, genres  (try \"metal\", \"Hendrix\", \"lofi\")", colours::textFaint);
    search.setFont (font (16.0f));
    search.setIndents (40, 10);
    search.addListener (this);
    search.setColour (juce::TextEditor::backgroundColourId, colours::raised);
    search.setTitle ("Search tones");
    addAndMakeVisible (search);

    list.setRowHeight (58);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
    list.getViewport()->setScrollBarThickness (10);
    addAndMakeVisible (list);

    close.onClick = [this] { if (onClose) onClose(); };
    close.setTooltip ("Close (Esc)");
    addAndMakeVisible (close);

    paste.onClick = [this] { pasteShareCode(); };
    copy.onClick = [this] { copyShareCode(); };
    folder.onClick = [] {
        auto f = RiffhouseProcessor::userFolder().getChildFile ("Presets");
        f.createDirectory();
        f.startAsProcess();
    };
    paste.setTooltip ("Load a tone someone shared with you as text");
    copy.setTooltip ("Copy your current tone as a short code you can paste in a chat");
    folder.setTooltip ("Your saved tones and installed packs live here");
    for (auto* b : { &paste, &copy, &folder }) addAndMakeVisible (*b);

    setWantsKeyboardFocus (true);
}

PresetBrowser::~PresetBrowser() = default;

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        categories->update();
        refreshList();
        search.grabKeyboardFocus();
    }
}

void PresetBrowser::refreshList()
{
    results = proc.presets().search (search.getText(), category);
    if (category == "Inspired By")
    {
        const auto& all = proc.presets().all();
        std::stable_sort (results.begin(), results.end(), [&] (int a, int b)
                          { return all[(size_t) a].inspiredBy.compareNatural (all[(size_t) b].inspiredBy) < 0; });
    }
    list.updateContent();
    ignoreSelection = true;
    const auto it = std::find (results.begin(), results.end(), proc.currentPresetIndex());
    if (it != results.end())
    {
        const int row = (int) (it - results.begin());
        list.selectRow (row, true, true);
        list.scrollToEnsureRowIsOnscreen (row);
    }
    else
        list.deselectAllRows();
    ignoreSelection = false;
    list.repaint();
    repaint();
}

void PresetBrowser::resized()
{
    auto r = getLocalBounds().reduced (24, 20);
    auto top = r.removeFromTop (46);
    close.setBounds (top.removeFromRight (46));
    top.removeFromRight (12);
    search.setBounds (top);
    r.removeFromTop (16);
    auto bottom = r.removeFromBottom (40);
    folder.setBounds (bottom.removeFromRight (170));
    bottom.removeFromRight (10);
    copy.setBounds (bottom.removeFromRight (160));
    bottom.removeFromRight (10);
    paste.setBounds (bottom.removeFromRight (160));
    r.removeFromBottom (12);
    categories->setBounds (r.removeFromLeft (210));
    r.removeFromLeft (16);
    list.setBounds (r);
}

// the magnifier sits inside the search field, so it is drawn after the field
void PresetBrowser::paintOverChildren (juce::Graphics& g)
{
    const auto s = getLocalBounds().reduced (24, 20).removeFromTop (46).withTrimmedRight (58);
    strokeIcon (g, icons::search(), juce::Rectangle<float> ((float) s.getX() + 13.0f, (float) s.getCentreY() - 9.0f, 18.0f, 18.0f), colours::textDim);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    auto r = getLocalBounds().reduced (24, 20);
    r.removeFromTop (46);

    auto bottom = getLocalBounds().reduced (24, 20).removeFromBottom (40);
    g.setFont (font (13.5f));
    g.setColour (colours::textDim);
    g.drawText (juce::String ((int) results.size()) + (results.size() == 1 ? " tone" : " tones")
                    + ". Click to try it while you play, double-click to close.",
                bottom, juce::Justification::centredLeft);
    if (results.empty())
    {
        auto lr = list.getBounds();
        g.setFont (font (16.0f));
        g.setColour (colours::textDim);
        g.drawFittedText (category == "Favourites" ? "No favourites yet. Click the star next to a tone to keep it here."
                                                   : "Nothing matches. Try fewer words, an artist, or a genre.",
                          lr.withHeight (120), juce::Justification::centred, 2, 1.0f);
    }
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) results.size())) return;
    const auto& p = proc.presets().all()[(size_t) results[(size_t) row]];
    auto r = juce::Rectangle<int> (w, h).reduced (2, 3);
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.13f));
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (colours::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }
    else
    {
        // one quiet rule between tones: the names carry the list, not the boxes
        g.setColour (juce::Colours::white.withAlpha (0.055f));
        g.fillRect (18.0f, (float) h - 1.0f, (float) w - 36.0f, 1.0f);
    }

    auto content = r.reduced (16, 8);
    // star
    auto starArea = content.removeFromRight (30).toFloat().withSizeKeepingCentre (22, 22);
    if (p.favourite) fillIcon (g, icons::star (true), starArea, colours::accent);
    else strokeIcon (g, icons::star (false), starArea, colours::textFaint, 1.5f);
    content.removeFromRight (10);

    // category tag
    const int tw = (int) capsWidth (p.category, 11.0f, 0.12f) + 8;
    drawCaps (g, p.category, content.removeFromRight (tw).toFloat(), 11.0f, colours::textDim, juce::Justification::centredRight, 0.12f);
    content.removeFromRight (14);

    g.setFont (font (16.0f, Weight::semibold));
    g.setColour (colours::text);
    g.drawText (p.name, content.removeFromTop (22), juce::Justification::centredLeft, true);
    juce::String sub;
    if (p.inspiredBy.isNotEmpty()) sub << "Inspired by " << p.inspiredBy;
    if (p.description.isNotEmpty()) sub << (sub.isEmpty() ? juce::String() : juce::String (". ")) << p.description;
    if (sub.isEmpty() && ! p.genres.isEmpty()) sub = p.genres.joinIntoString (", ");
    if (! p.factory && p.author.isNotEmpty()) sub << ". By " << p.author;
    g.setFont (font (13.5f));
    g.setColour (p.inspiredBy.isNotEmpty() ? colours::accentHi.withAlpha (0.85f) : colours::textDim);
    g.drawText (sub, content, juce::Justification::centredLeft, true);
}

void PresetBrowser::load (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) results.size())) return;
    proc.loadPreset (results[(size_t) row]);
    if (onLoaded) onLoaded();
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent& e)
{
    // star column
    if (e.x > list.getVisibleRowWidth() - 52)
    {
        proc.toggleFavourite (results[(size_t) row]);
        categories->update();
        if (category == "Favourites") refreshList();
        list.repaintRow (row);
        return;
    }
    load (row);
}

void PresetBrowser::selectedRowsChanged (int row)
{
    // arrow keys audition tones too
    if (! ignoreSelection && row >= 0 && results[(size_t) row] != proc.currentPresetIndex()
        && juce::ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown() == false)
        load (row);
}

void PresetBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    load (row);
    if (onClose) onClose();
}

void PresetBrowser::returnKeyPressed (int row)
{
    load (row);
    if (onClose) onClose();
}

void PresetBrowser::textEditorReturnKeyPressed (juce::TextEditor&)
{
    if (! results.empty())
    {
        const int sel = std::max (0, list.getSelectedRow());
        load (sel);
        if (onClose) onClose();
    }
}

bool PresetBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey)
    {
        const int d = k == juce::KeyPress::downKey ? 1 : -1;
        const int row = juce::jlimit (0, (int) results.size() - 1, list.getSelectedRow() + d);
        list.selectRow (row);
        return true;
    }
    return false;
}

void PresetBrowser::copyShareCode()
{
    const auto p = proc.captureCurrent (proc.currentPresetName(), "Shared");
    juce::SystemClipboard::copyTextToClipboard (presetToShareCode (p));
    if (onMessage) onMessage ("Tone code copied. Paste it anywhere to share this tone.");
}

void PresetBrowser::pasteShareCode()
{
    Preset p;
    juce::String err;
    const auto text = juce::SystemClipboard::getTextFromClipboard().trim();
    if (! presetFromShareCode (text, p, err) && ! presetFromJson (text, p, err))
    {
        if (onMessage) onMessage ("The clipboard doesn't hold a Riffhouse tone code.");
        return;
    }
    proc.applySharedPreset (p);
    if (onLoaded) onLoaded();
    if (onMessage) onMessage ("Loaded \"" + p.name + "\". Save it to keep it.");
}
} // namespace rh::ui
