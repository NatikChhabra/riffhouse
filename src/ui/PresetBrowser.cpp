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

    search.setTextToShowWhenEmpty ("Search tones, bands, songs, genres", colours::textFaint);
    search.setFont (font (16.0f));
    search.setIndents (40, 0);
    search.setJustification (juce::Justification::centredLeft); // on the field's middle line, like the magnifier
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

void PresetBrowser::show (const juce::String& group, const juce::String& words)
{
    category = group;
    search.setText (words, false);
    categories->update();
    categories->repaint();
    refreshList();
}

void PresetBrowser::refreshList()
{
    results = proc.presets().search (search.getText(), category);
    if (category == "Inspired By")
    {
        const auto& all = proc.presets().all();
        // by band, then by title: everything made after one band stands together
        std::stable_sort (results.begin(), results.end(), [&] (int a, int b)
                          {
                              const auto& x = all[(size_t) a];
                              const auto& y = all[(size_t) b];
                              const int byArtist = x.artist().compareNatural (y.artist());
                              return byArtist != 0 ? byArtist < 0 : x.title().compareNatural (y.title()) < 0;
                          });
    }
    else if (search.getText().trim().isNotEmpty())
    {
        // words typed: the tones made after a song come first, since a song is what people type
        const auto& all = proc.presets().all();
        const auto rest = std::stable_partition (results.begin(), results.end(), [&] (int i) { return all[(size_t) i].song.isNotEmpty() && all[(size_t) i].part() != all[(size_t) i].title(); });
        std::stable_sort (results.begin(), rest, [&] (int a, int b)
                          {
                              const auto& x = all[(size_t) a];
                              const auto& y = all[(size_t) b];
                              const int byArtist = x.artist().compareNatural (y.artist());
                              return byArtist != 0 ? byArtist < 0 : x.title().compareNatural (y.title()) < 0;
                          });
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
    {
        // another group or other words: a new list starts at its top, not where the last one was left
        list.deselectAllRows();
        list.getViewport()->setViewPosition (0, 0);
    }
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
                                                   : "Nothing matches. Try fewer words, a band, a song or a genre.",
                          lr.withHeight (120), juce::Justification::centred, 2, 1.0f);
    }
}

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) results.size())) return;
    const auto& p = proc.presets().all()[(size_t) results[(size_t) row]];
    auto r = juce::Rectangle<int> (w, h).reduced (2, 3);
    // the tone you are on is a lit band exactly as wide as the rules between the others
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.10f));
        g.fillRect (18.0f, 0.0f, (float) w - 36.0f, (float) h - 1.0f);
    }
    // one quiet rule between tones: the names carry the list, not the boxes
    g.setColour (selected ? colours::accent.withAlpha (0.5f) : juce::Colours::white.withAlpha (0.055f));
    g.fillRect (18.0f, (float) h - 1.0f, (float) w - 36.0f, 1.0f);

    auto content = r.reduced (16, 8);
    // star
    auto starArea = content.removeFromRight (30).toFloat().withSizeKeepingCentre (22, 22);
    if (p.favourite) fillIcon (g, icons::star (true), starArea, colours::accent);
    else strokeIcon (g, icons::star (false), starArea, colours::textFaint, 1.5f);
    content.removeFromRight (10);

    // A tone made after someone says who in front of its title: the band, or the player when there is no band.
    const auto artist = p.artist();

    // the tag on the right: the category, or the genre for a tone made after someone (every one of those is "Inspired By")
    const auto tag = artist.isNotEmpty() && ! p.genres.isEmpty() ? p.genres[0] : p.category;
    const int tw = (int) capsWidth (tag, 11.0f, 0.12f) + 8;
    drawCaps (g, tag, content.removeFromRight (tw).toFloat(), 11.0f, colours::textDim, juce::Justification::centredRight, 0.12f);
    content.removeFromRight (14);

    auto top = content.removeFromTop (22);
    g.setColour (selected ? colours::accentHi : colours::text);
    if (artist.isNotEmpty())
    {
        // the artist carries the weight, the title follows in the plain cut: one glance says whose sound it is
        const auto heavy = font (16.0f, Weight::semibold);
        const int aw = std::min (top.getWidth(), (int) std::ceil (juce::GlyphArrangement::getStringWidth (heavy, artist)));
        g.setFont (heavy);
        g.drawText (artist, top.removeFromLeft (aw), juce::Justification::centredLeft, true);
        top.removeFromLeft (10);
        g.setFont (font (16.0f));
        g.setColour ((selected ? colours::accentHi : colours::text).withAlpha (0.86f));
        // a tone named after a song carries the song in quotation marks, the way song titles are written
        const auto part = p.part();
        const auto title = part != p.title() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9c")) + p.song + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x9d"))
                                                   + (part.isNotEmpty() ? " " + part : juce::String())
                                             : p.title();
        g.drawText (title, top, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setFont (font (16.0f, Weight::semibold));
        g.drawText (p.name, top, juce::Justification::centredLeft, true);
    }
    juce::String sub = p.credit() + p.description;
    if (sub.isEmpty() && ! p.genres.isEmpty()) sub = p.genres.joinIntoString (", ");
    if (! p.factory && p.author.isNotEmpty()) sub << (sub.endsWithChar ('.') ? " By " : ". By ") << p.author;
    g.setFont (font (13.5f));
    g.setColour (colours::textDim);
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
    juce::SystemClipboard::copyTextToClipboard (presetToShareMessage (p));
    if (onMessage) onMessage ("Copied \"" + p.name + "\". Paste it in any chat; a friend copies it back and presses Ctrl+V in Riffhouse.");
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
