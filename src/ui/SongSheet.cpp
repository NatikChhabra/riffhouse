// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "SongSheet.h"

#include <cmath>

namespace rh::ui
{
using namespace rh::sheet;

namespace
{
const juce::Colour railBg (0xff0b0b0b), hairline (0xff222120), staveInk (0xff3d3a36), gridInk (0xff4a4642);

juce::String withoutBrackets (const juce::String& t)
{
    return t.removeCharacters ("()[]*,");
}

int effectiveSpeed (const Sheet& s) { return s.speed == 0 ? 3 : s.speed; }

// A song of our own for pictures of the app: the words and the riff were written for Riffhouse.
const char* const exampleSong =
    "{title: Kitchen Light}\n"
    "[Intro]\n"
    "Am   F   C   G\n"
    "\n"
    "[Verse 1]\n"
    "Am                    F\n"
    "Kettle on the stove again\n"
    "C                     G\n"
    "nobody sleeps before ten\n"
    "Am                    F\n"
    "radio low on the windowsill\n"
    "C                       G\n"
    "playing the one we know still\n"
    "\n"
    "[Chorus]\n"
    "F             G               Am\n"
    "leave the kitchen light on for me\n"
    "F               G            C   E7\n"
    "I'll be home by the second verse\n"
    "\n"
    "[Riff]\n"
    "e|-----------------|-----------------|\n"
    "B|-----1-----1-----|-----3-----3-----|\n"
    "G|---2-----2-----2-|---0-----0-----0-|\n"
    "D|-2---------------|-----------------|\n"
    "A|-----------------|-----------------|\n"
    "E|-----------------|-3---------------|\n"
    "\n"
    "[Verse 2]\n"
    "Am                    F\n"
    "Coats on the back of the door\n"
    "C                       G\n"
    "shoes where they were before\n";
} // namespace

//==============================================================================
// The sheet itself: words, chords over them, headings and tab, laid out for one width.
class SongSheetView::Body : public juce::Component
{
public:
    explicit Body (SongSheetView& o) : owner (o) { setOpaque (true); }

    float fontSize() const { return 17.0f + 1.6f * (float) owner.current.size; }

    void build (int width, int extraBelow)
    {
        runs.clear(); rules.clear(); tabs.clear();
        const float fs = fontSize();
        const auto lyricF = font (fs), chordF = font (fs * 0.94f, Weight::semibold);
        const float cw = juce::GlyphArrangement::getStringWidth (lyricF, "n");
        const auto& lines = owner.current.lines;
        const int shift = owner.current.shift;
        const bool flats = flatsAfter (owner.current, shift);
        float y = 6.0f, right = 0.0f;

        auto widthOf = [] (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); };
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const auto& l = lines[i];
            switch (l.kind)
            {
                case LineKind::blank: y += fs * 0.9f; break;
                case LineKind::section:
                    if (i > 0) y += fs * 0.5f;
                    rules.push_back ({ y, l.text });
                    y += fs * 1.7f;
                    break;
                case LineKind::chords:
                {
                    const auto text = transposeLine (l.text, shift, flats);
                    const Line* words = (i + 1 < lines.size() && lines[i + 1].kind == LineKind::lyric) ? &lines[i + 1] : nullptr;
                    float last = -1000.0f;
                    int col = 0;
                    while (col < text.length())
                    {
                        if (text[col] == ' ') { ++col; continue; }
                        const int start = col;
                        while (col < text.length() && text[col] != ' ') ++col;
                        const auto token = text.substring (start, col);
                        float x = (float) start * cw;
                        if (words != nullptr)
                        {
                            const auto& w = words->text;
                            x = start <= w.length() ? widthOf (lyricF, w.substring (0, start))
                                                    : widthOf (lyricF, w) + (float) (start - w.length()) * cw;
                        }
                        x = std::max (x, last + cw * 0.8f);
                        Chord c;
                        const bool isChord = parseChord (withoutBrackets (token), c);
                        runs.push_back ({ token, { x, y + fs }, isChord ? 1 : 3 });
                        last = x + widthOf (chordF, token);
                        right = std::max (right, last);
                    }
                    y += fs * 1.3f;
                    if (words != nullptr)
                    {
                        runs.push_back ({ words->text, { 0.0f, y + fs }, 0 });
                        right = std::max (right, widthOf (lyricF, words->text));
                        y += fs * 1.6f;
                        ++i;
                    }
                    break;
                }
                case LineKind::lyric:
                    runs.push_back ({ l.text, { 0.0f, y + fs }, 0 });
                    right = std::max (right, widthOf (lyricF, l.text));
                    y += fs * 1.6f;
                    break;
                case LineKind::tab:
                {
                    Tab t;
                    t.y = y;
                    size_t j = i;
                    int longest = 0;
                    for (; j < lines.size() && lines[j].kind == LineKind::tab; ++j)
                    {
                        const auto& s = lines[j].text.trim();
                        const int bar = s.indexOfChar ('|');
                        const int colon = s.indexOfChar (':');
                        const int cut = (bar >= 0 && bar <= 3) ? bar : (colon >= 0 && colon <= 3 ? colon : -1);
                        juce::String name = cut > 0 ? s.substring (0, cut).trim() : juce::String();
                        juce::String strings = cut >= 0 ? s.substring (cut) : s;
                        t.rows.push_back ({ name, strings });
                        longest = std::max (longest, strings.length());
                    }
                    t.label = fs * 1.4f;
                    t.cell = juce::jlimit (5.0f, fs * 0.62f, ((float) width - t.label - 8.0f) / (float) std::max (1, longest));
                    t.row = fs * 0.95f;
                    y += t.row * (float) t.rows.size() + fs * 0.6f;
                    right = std::max (right, t.label + t.cell * (float) longest);
                    tabs.push_back (std::move (t));
                    i = j - 1;
                    break;
                }
            }
        }
        setSize (std::max (width, (int) std::ceil (right) + 16), (int) std::ceil (y) + extraBelow);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bg);
        const float fs = fontSize();
        const auto lyricF = font (fs), chordF = font (fs * 0.94f, Weight::semibold);
        const auto clip = g.getClipBounds().toFloat();

        for (const auto& r : rules)
        {
            if (r.y > clip.getBottom() || r.y + fs * 2.0f < clip.getY()) continue;
            const float h = fs * 0.64f;
            const auto area = juce::Rectangle<float> (0.0f, r.y, (float) getWidth(), fs * 1.2f);
            drawCaps (g, r.name, area, h, colours::textDim, juce::Justification::centredLeft, 0.14f);
            const float tw = capsWidth (r.name, h, 0.14f);
            g.setColour (hairline);
            g.fillRect (tw + 14.0f, area.getCentreY(), std::min (420.0f, (float) getWidth() - tw - 14.0f), 1.0f);
        }

        for (const auto& r : runs)
        {
            if (r.at.y - fs > clip.getBottom() || r.at.y + fs * 0.5f < clip.getY()) continue;
            switch (r.style)
            {
                case 0: g.setFont (lyricF); g.setColour (colours::text.withAlpha (0.92f)); break;
                case 1: g.setFont (chordF); g.setColour (colours::lamp); break;
                default: g.setFont (chordF); g.setColour (colours::textFaint); break;
            }
            g.drawSingleLineText (r.text, juce::roundToInt (r.at.x), juce::roundToInt (r.at.y));
        }

        // tab on drawn strings: the numbers sit on the lines like they do on a printed page
        const auto tabF = font (fs * 0.8f, Weight::numeric);
        for (const auto& t : tabs)
        {
            const float height = t.row * (float) t.rows.size();
            if (t.y > clip.getBottom() || t.y + height < clip.getY()) continue;
            const float first = t.y + t.row * 0.5f, lastY = t.y + t.row * ((float) t.rows.size() - 0.5f);
            for (size_t r = 0; r < t.rows.size(); ++r)
            {
                const float yy = t.y + t.row * ((float) r + 0.5f);
                const auto& [name, strings] = t.rows[r];
                if (name.isNotEmpty())
                    drawCaps (g, name, { 0.0f, yy - t.row * 0.5f, t.label - 8.0f, t.row }, fs * 0.56f, colours::textFaint,
                              juce::Justification::centredRight, 0.0f);
                g.setColour (staveInk);
                g.fillRect (t.label, yy - 0.5f, t.cell * (float) strings.length(), 1.0f);
            }
            for (size_t r = 0; r < t.rows.size(); ++r)
            {
                const float yy = t.y + t.row * ((float) r + 0.5f);
                const auto& strings = t.rows[r].second;
                for (int c = 0; c < strings.length(); ++c)
                {
                    const auto ch = strings[c];
                    const float x = t.label + t.cell * (float) c;
                    if (ch == '|' || ch == ':')
                    {
                        if (r == 0)
                        {
                            g.setColour (staveInk);
                            g.fillRect (x + t.cell * 0.5f - 0.5f, first, 1.0f, lastY - first);
                        }
                        continue;
                    }
                    if (ch == '-' || ch == ' ' || ch == 0x2014 || ch == 0x2013) continue;
                    g.setColour (colours::bg);
                    g.fillRect (x - 1.0f, yy - t.row * 0.42f, t.cell + 2.0f, t.row * 0.84f);
                    const bool digit = ch >= '0' && ch <= '9';
                    g.setFont (digit ? tabF : font (fs * 0.66f, Weight::medium));
                    g.setColour (digit ? colours::text : colours::textDim);
                    g.drawText (juce::String::charToString (ch), juce::Rectangle<float> (x - 2.0f, yy - t.row * 0.5f, t.cell + 4.0f, t.row),
                                juce::Justification::centred, false);
                }
            }
        }
    }

private:
    struct Run { juce::String text; juce::Point<float> at; int style; };
    struct Rule { float y; juce::String name; };
    struct Tab { float y = 0, label = 0, cell = 8, row = 16; std::vector<std::pair<juce::String, juce::String>> rows; };
    SongSheetView& owner;
    std::vector<Run> runs;
    std::vector<Rule> rules;
    std::vector<Tab> tabs;
};

//==============================================================================
SongSheetView::SongSheetView()
{
    setWantsKeyboardFocus (true);
    body = std::make_unique<Body> (*this);
    scroller.setViewedComponent (body.get(), false);
    scroller.setScrollBarsShown (true, true);
    scroller.setScrollBarThickness (8);
    scroller.setWantsKeyboardFocus (false);
    addChildComponent (scroller);

    close.setTooltip ("Close (Esc)");
    close.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (close);

    keyDown.setTooltip ("Down a semitone");
    keyUp.setTooltip ("Up a semitone");
    keyDown.onClick = [this] { current.shift = current.shift <= -11 ? 0 : current.shift - 1; changed (true); };
    keyUp.onClick = [this] { current.shift = current.shift >= 11 ? 0 : current.shift + 1; changed (true); };
    smaller.setTooltip ("Smaller writing");
    bigger.setTooltip ("Bigger writing, to read from further away");
    smaller.onClick = [this] { current.size = std::max (-4, current.size - 1); changed (true); };
    bigger.onClick = [this] { current.size = std::min (6, current.size + 1); changed (true); };
    scrollButton.setTooltip ("Scroll by itself while you play (Space)");
    scrollButton.onClick = [this] { startScroll (! scrolling); };
    slower.setTooltip ("Scroll slower");
    faster.setTooltip ("Scroll faster");
    slower.onClick = [this] { current.speed = std::max (1, effectiveSpeed (current) - 1); changed (true); };
    faster.onClick = [this] { current.speed = std::min (9, effectiveSpeed (current) + 1); changed (true); };
    for (auto* b : { &keyDown, &keyUp, &smaller, &bigger, &scrollButton, &slower, &faster })
    {
        b->iconSize = 14.0f;
        addChildComponent (*b);
    }
    scrollButton.iconSize = 12.0f;

    edit.setTooltip ("Change the words, chords or title");
    edit.onClick = [this] { setEditing (true, false); };
    copy.setTooltip ("Copy the song in the key you see, to send to someone");
    copy.onClick = [this]
    {
        if (! hasSheet) return;
        juce::SystemClipboard::copyTextToClipboard (toText (current, current.shift, false)
                                                    + "\nSet out in Riffhouse, the free guitar amp: https://natikchhabra.github.io/riffhouse/\n");
        const auto key = keyText();
        message (key.isNotEmpty() ? "Copied in " + key.upToFirstOccurrenceOf (",", false, false).fromFirstOccurrenceOf ("Key of ", false, false)
                                        + ". Paste it in any chat or notes app."
                                  : juce::String ("Copied. Paste it in any chat or notes app."));
    };
    paste.setTooltip ("Copy a song's chords or tab from a website first (Ctrl+V)");
    paste.onClick = [this]
    {
        const auto text = juce::SystemClipboard::getTextFromClipboard();
        if (! pasteSong (text))
            message (text.trim().isEmpty() ? "Nothing is copied yet. Select the chords on the song's page, press Ctrl+C, then come back."
                                           : "What you copied has no chords or tab in it. Select them on the song's page and copy again.");
    };
    type.onClick = [this] { setEditing (true, true); };
    done.onClick = [this] { finishEditing(); };
    cancel.onClick = [this] { setEditing (false, false); };
    paste.setColour (juce::TextButton::buttonColourId, colours::accent);
    paste.setColour (juce::TextButton::textColourOffId, colours::bg);
    done.setColour (juce::TextButton::buttonColourId, colours::accent);
    done.setColour (juce::TextButton::textColourOffId, colours::bg);
    for (auto* b : { &edit, &copy, &paste, &type, &done, &cancel })
        addChildComponent (*b);

    editor.setMultiLine (true, false);
    editor.setReturnKeyStartsNewLine (true);
    editor.setScrollbarsShown (true);
    editor.setFont (juce::Font (juce::FontOptions().withName (juce::Font::getDefaultMonospacedFontName()).withHeight (15.0f)));
    editor.setIndents (14, 12);
    editor.setTextToShowWhenEmpty ("Song title\n\n[Verse]\nAm            F\nWords go under the chords, chords over the words", colours::textFaint);
    editor.onEscapeKey = [this] { setEditing (false, false); };
    addChildComponent (editor);
}

SongSheetView::~SongSheetView()
{
    scroller.setViewedComponent (nullptr, false);
}

juce::File SongSheetView::folder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Riffhouse").getChildFile ("Songs");
}

bool SongSheetView::isSongFile (const juce::File& f)
{
    return juce::StringArray { ".txt", ".cho", ".chopro", ".pro", ".crd", ".chordpro" }.contains (f.getFileExtension().toLowerCase());
}

bool SongSheetView::looksLikeSong (const juce::String& text)
{
    if (text.length() > 200000) return false;
    int count = 0;
    for (const auto& l : parse (text).lines)
        if (l.kind == LineKind::chords || l.kind == LineKind::tab) ++count;
    return count >= 2;
}

void SongSheetView::refreshList()
{
    if (example) return;
    songs.clear();
    for (const auto& f : folder().findChildFiles (juce::File::findFiles, false, "*.txt;*.cho;*.chopro;*.pro;*.crd;*.chordpro"))
        songs.push_back ({ f, f.getFileNameWithoutExtension() });
    std::sort (songs.begin(), songs.end(), [] (const Entry& a, const Entry& b) { return a.name.compareNatural (b.name) < 0; });
    layout();
    repaint();
}

void SongSheetView::visibilityChanged()
{
    if (! isVisible()) { startScroll (false); return; }
    refreshList();
    if (! hasSheet && ! editing && ! songs.empty())
    {
        // back where you were: the song touched last
        auto latest = songs.front().file;
        for (const auto& s : songs)
            if (s.file.getLastModificationTime() > latest.getLastModificationTime()) latest = s.file;
        open (latest);
    }
}

bool SongSheetView::pasteSong (const juce::String& text)
{
    auto s = parse (text);
    int count = 0;
    juce::String firstWords;
    for (const auto& l : s.lines)
    {
        if (l.kind == LineKind::chords || l.kind == LineKind::tab) ++count;
        if (l.kind == LineKind::lyric && firstWords.isEmpty()) firstWords = l.text.trim();
    }
    if (count == 0) return false;
    if (s.title.isEmpty())
        s.title = firstWords.isNotEmpty() ? (firstWords.length() > 40 ? firstWords.substring (0, 40).upToLastOccurrenceOf (" ", false, false) : firstWords)
                                          : juce::String ("Untitled song");
    auto file = folder().getChildFile (juce::File::createLegalFileName (s.title) + ".txt");
    if (! example)
    {
        folder().createDirectory();
        file = file.getNonexistentSibling (true);
        if (! file.replaceWithText (toText (s, 0, true)))
        {
            message ("Couldn't keep the song in " + folder().getFullPathName() + ". It is open, but won't be there next time.");
            file = juce::File();
        }
    }
    openSheet (s, file);
    refreshList();
    if (file != juce::File()) message ("Kept \"" + s.title + "\" with your songs.");
    return true;
}

bool SongSheetView::importFile (const juce::File& f)
{
    if (f.getParentDirectory() == folder()) { open (f); refreshList(); return true; }
    auto text = f.loadFileAsString();
    auto s = parse (text);
    if (s.title.isEmpty()) text = "{title: " + f.getFileNameWithoutExtension() + "}\n" + text;
    if (pasteSong (text)) return true;
    message (f.getFileName() + " has no chords or tab in it.");
    return false;
}

void SongSheetView::open (const juce::File& f)
{
    auto s = parse (f.loadFileAsString());
    if (s.title.isEmpty()) s.title = f.getFileNameWithoutExtension();
    openSheet (std::move (s), f);
}

void SongSheetView::openSheet (Sheet s, const juce::File& f)
{
    startScroll (false);
    current = std::move (s);
    currentFile = f;
    hasSheet = true;
    editing = false;
    editor.setVisible (false);
    layout();
    scroller.setViewPosition (0, 0);
    scrollY = 0.0;
    repaint();
}

void SongSheetView::save()
{
    if (example || ! hasSheet || currentFile == juce::File()) return;
    currentFile.replaceWithText (toText (current, 0, true));
}

void SongSheetView::changed (bool keep)
{
    if (keep) save();
    const auto at = scroller.getViewPosition();
    layout();
    scroller.setViewPosition (at);
    repaint();
}

void SongSheetView::setEditing (bool on, bool fresh)
{
    startScroll (false);
    editing = on;
    editingFresh = fresh;
    if (on)
    {
        editor.setText (fresh || ! hasSheet ? juce::String() : toText (current, 0, false).trimEnd(), false);
        editor.setVisible (true);
    }
    else editor.setVisible (false);
    layout();
    repaint();
    if (on) editor.grabKeyboardFocus();
    else if (isShowing()) grabKeyboardFocus();
}

void SongSheetView::finishEditing()
{
    const auto text = editor.getText();
    if (text.trim().isEmpty()) { setEditing (false, false); return; }
    auto s = parse (text);
    if (editingFresh || ! hasSheet)
    {
        if (! pasteSong (text))
        {
            // words without chords are still a song to keep
            if (s.title.isEmpty()) s.title = "Untitled song";
            auto file = folder().getChildFile (juce::File::createLegalFileName (s.title) + ".txt");
            folder().createDirectory();
            file = file.getNonexistentSibling (true);
            file.replaceWithText (toText (s, 0, true));
            openSheet (s, file);
            refreshList();
        }
        if (isShowing()) grabKeyboardFocus();
        return;
    }
    s.shift = current.shift; s.size = current.size; s.speed = current.speed;
    if (s.title.isEmpty()) s.title = current.title;
    auto file = currentFile;
    if (! example && file != juce::File() && s.title != current.title)
    {
        // a new title is a new name in the list
        auto renamed = folder().getChildFile (juce::File::createLegalFileName (s.title) + ".txt").getNonexistentSibling (true);
        if (file.moveFileTo (renamed)) file = renamed;
    }
    openSheet (s, file);
    save();
    refreshList();
    if (isShowing()) grabKeyboardFocus();
}

juce::String SongSheetView::keyText() const
{
    if (current.chords.isEmpty()) return {};
    Chord first;
    if (! parseChord (current.chords[0], first)) return {};
    const bool minor = (first.quality.startsWith ("m") && ! first.quality.startsWith ("maj")) || first.quality.startsWith ("-");
    const auto was = noteName (first.root, current.flats) + (minor ? "m" : "");
    const auto now = noteName (first.root + current.shift, flatsAfter (current, current.shift)) + (minor ? "m" : "");
    if (current.shift == 0) return "Key of " + now;
    return "Key of " + now + ", moved " + (current.shift > 0 ? "up " : "down ") + juce::String (std::abs (current.shift))
         + (std::abs (current.shift) == 1 ? " semitone" : " semitones") + " from " + was;
}

float SongSheetView::pixelsPerSecond() const
{
    return 6.0f * std::pow (1.3f, (float) (effectiveSpeed (current) - 1)) * (body->fontSize() / 17.0f);
}

void SongSheetView::startScroll (bool on)
{
    if (on && (! hasSheet || editing)) on = false;
    scrolling = on;
    scrollButton.setIcon (on ? icons::pause() : icons::play(), true);
    scrollButton.setToggleState (on, juce::dontSendNotification);
    if (on)
    {
        scrollY = scroller.getViewPositionY();
        lastTick = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }
    else stopTimer();
}

void SongSheetView::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = std::min (0.1, (now - lastTick) / 1000.0);
    lastTick = now;
    // a hand on the wheel wins: carry on from wherever it put the page
    if (std::abs (scroller.getViewPositionY() - (int) scrollY) > 2) scrollY = scroller.getViewPositionY();
    scrollY += pixelsPerSecond() * dt;
    const double end = std::max (0, body->getHeight() - scroller.getViewHeight());
    if (scrollY >= end) { scrollY = end; startScroll (false); }
    scroller.setViewPosition (scroller.getViewPositionX(), (int) scrollY);
}

bool SongSheetView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('v', juce::ModifierKeys::commandModifier, 0))
    {
        const auto text = juce::SystemClipboard::getTextFromClipboard();
        if (text.contains ("riffhouse:")) return false; // a tone code: the rig loads it
        if (! pasteSong (text))
            message ("What you copied has no chords or tab in it. Select them on the song's page and copy again.");
        return true;
    }
    if (! hasSheet || editing) return false;
    if (k == juce::KeyPress::spaceKey) { startScroll (! scrolling); return true; }
    const int step = juce::roundToInt (body->fontSize() * 3.0f);
    auto at = scroller.getViewPosition();
    if (k == juce::KeyPress::downKey) at.y += step;
    else if (k == juce::KeyPress::upKey) at.y -= step;
    else if (k == juce::KeyPress::pageDownKey) at.y += scroller.getViewHeight() - step;
    else if (k == juce::KeyPress::pageUpKey) at.y -= scroller.getViewHeight() - step;
    else if (k == juce::KeyPress::homeKey) at.y = 0;
    else return false;
    scroller.setViewPosition (at);
    return true;
}

//==============================================================================
void SongSheetView::showExample (int shift, bool editingIt, float scrolled)
{
    example = true;
    songs = { { {}, "Kitchen Light" }, { {}, "Morning Train" }, { {}, "Slow Blues in A" }, { {}, "Small Hours" } };
    auto s = parse (exampleSong);
    s.shift = shift;
    openSheet (s, {});
    if (editingIt) setEditing (true, false);
    scroller.setViewPosition (0, juce::roundToInt (scrolled * (float) std::max (0, body->getHeight() - scroller.getViewHeight())));
}

void SongSheetView::resized() { layout(); }

void SongSheetView::layout()
{
    auto r = getLocalBounds();
    rail = r.removeFromLeft (juce::jlimit (210, 270, getWidth() / 5));
    main = r;
    close.setBounds (getWidth() - 64, 18, 44, 44);

    auto c = main.withTrimmedLeft (56).withTrimmedRight (72).withTrimmedTop (36);
    titleArea = c.removeFromTop (40);
    leadArea = c.removeFromTop (24);
    c.removeFromTop (16);
    toolbar = c.removeFromTop (32);
    c.removeFromTop (20);

    const bool reading = hasSheet && ! editing;
    for (juce::Component* b : { (juce::Component*) &keyDown, (juce::Component*) &keyUp, (juce::Component*) &smaller, (juce::Component*) &bigger,
                                (juce::Component*) &scrollButton, (juce::Component*) &slower, (juce::Component*) &faster,
                                (juce::Component*) &edit, (juce::Component*) &copy })
        b->setVisible (reading);
    done.setVisible (editing);
    cancel.setVisible (editing);
    scroller.setVisible (reading);

    if (reading)
    {
        auto t = toolbar;
        keyLabel = t.removeFromLeft (38);
        keyDown.setBounds (t.removeFromLeft (32));
        keyValue = t.removeFromLeft (44);
        keyUp.setBounds (t.removeFromLeft (32));
        t.removeFromLeft (32);
        sizeLabel = t.removeFromLeft (42);
        smaller.setBounds (t.removeFromLeft (32));
        t.removeFromLeft (6);
        bigger.setBounds (t.removeFromLeft (32));
        t.removeFromLeft (32);
        scrollLabel = t.removeFromLeft (62);
        scrollButton.setBounds (t.removeFromLeft (32));
        t.removeFromLeft (12);
        slower.setBounds (t.removeFromLeft (32));
        speedValue = t.removeFromLeft (30);
        faster.setBounds (t.removeFromLeft (32));
        auto right = toolbar;
        copy.setBounds (right.removeFromRight (84));
        right.removeFromRight (8);
        edit.setBounds (right.removeFromRight (84));
    }
    if (editing)
    {
        auto right = toolbar;
        done.setBounds (right.removeFromRight (96));
        right.removeFromRight (8);
        cancel.setBounds (right.removeFromRight (96));
    }

    strip = (reading && ! current.chords.isEmpty()) ? c.removeFromTop (88) : juce::Rectangle<int>();
    if (! strip.isEmpty()) c.removeFromTop (18);
    sheetArea = c.withRight (main.getRight() - 24).withTrimmedBottom (12);
    emptyArea = main.withTrimmedLeft (56).withTrimmedRight (72).withTrimmedTop (36);

    // the two ways in: big in the middle while there is nothing to read, then at the foot of the list
    if (! hasSheet && ! editing)
    {
        const int y = emptyArea.getY() + 268;
        paste.setBounds (emptyArea.getX(), y, 200, 44);
        type.setBounds (emptyArea.getX() + 212, y, 160, 44);
    }
    else
    {
        auto foot = rail.reduced (16, 0).withTrimmedBottom (20);
        type.setBounds (foot.removeFromBottom (38));
        foot.removeFromBottom (8);
        paste.setBounds (foot.removeFromBottom (38));
    }
    paste.setVisible (true);
    type.setVisible (true);

    railItems.clear();
    {
        int y = rail.getY() + 118 - railScroll;
        for (size_t i = 0; i < songs.size(); ++i, y += 40)
            railItems.push_back ({ (int) i, juce::Rectangle<int> (rail.getX() + 16, y, rail.getWidth() - 32, 40) });
    }

    if (editing) editor.setBounds (c.withRight (main.getRight() - 72).withTrimmedBottom (16));
    if (reading)
    {
        scroller.setBounds (sheetArea);
        body->build (std::max (200, sheetArea.getWidth() - 12), sheetArea.getHeight() / 2);
    }
}

int SongSheetView::railItemAt (juce::Point<int> p) const
{
    const int listBottom = (hasSheet || editing) ? paste.getY() - 12 : rail.getBottom();
    if (p.y < rail.getY() + 110 || p.y > listBottom) return -1;
    for (const auto& [i, area] : railItems)
        if (area.contains (p)) return i;
    return -1;
}

void SongSheetView::mouseMove (const juce::MouseEvent& e)
{
    const int h = railItemAt (e.getPosition());
    if (h != hover) { hover = h; repaint (rail); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void SongSheetView::mouseExit (const juce::MouseEvent&)
{
    if (hover >= 0) { hover = -1; repaint (rail); }
}

void SongSheetView::mouseUp (const juce::MouseEvent& e)
{
    const int i = railItemAt (e.getPosition());
    if (i < 0 || ! e.mouseWasClicked() || example) return;
    const auto entry = songs[(size_t) i];
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, "Show in folder");
        m.addItem (2, "Delete \"" + entry.name + "\"");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                         [safe = juce::Component::SafePointer<SongSheetView> (this), entry] (int choice)
                         {
                             if (safe == nullptr) return;
                             if (choice == 1) entry.file.revealToUser();
                             if (choice == 2)
                             {
                                 const bool wasOpen = entry.file == safe->currentFile;
                                 if (! entry.file.moveToTrash()) { safe->message ("Couldn't delete " + entry.file.getFileName() + "."); return; }
                                 if (wasOpen) { safe->hasSheet = false; safe->currentFile = juce::File(); safe->startScroll (false); }
                                 safe->refreshList();
                                 safe->message ("\"" + entry.name + "\" is in the Recycle Bin, if you want it back.");
                             }
                         });
        return;
    }
    if (editing) setEditing (false, false);
    open (entry.file);
}

void SongSheetView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! rail.contains (e.getPosition())) return;
    const int listHeight = (int) songs.size() * 40;
    const int room = ((hasSheet || editing) ? paste.getY() - 12 : rail.getBottom()) - (rail.getY() + 118);
    railScroll = juce::jlimit (0, std::max (0, listHeight - room), railScroll - juce::roundToInt (w.deltaY * 160.0f));
    layout();
    repaint (rail);
}

//==============================================================================
void SongSheetView::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (railBg);
    g.fillRect (rail);
    g.setColour (hairline);
    g.fillRect (rail.getRight() - 1, 0, 1, getHeight());

    auto head = rail.reduced (28, 0).withTrimmedTop (36);
    g.setColour (colours::text);
    g.setFont (font (24.0f, Weight::display));
    g.drawText ("Songs", head.removeFromTop (32), juce::Justification::centredLeft);
    g.setColour (colours::textFaint);
    g.setFont (font (12.5f));
    g.drawText (songs.empty() ? juce::String ("None kept yet") : juce::String ((int) songs.size()) + (songs.size() == 1 ? " song" : " songs"),
                head.removeFromTop (22), juce::Justification::centredLeft);

    {
        const int listBottom = (hasSheet || editing) ? paste.getY() - 12 : rail.getBottom();
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (rail.getX(), rail.getY() + 110, rail.getWidth() - 1, listBottom - (rail.getY() + 110));
        const auto openName = currentFile != juce::File() ? currentFile.getFileNameWithoutExtension() : current.title;
        for (const auto& [i, area] : railItems)
        {
            const auto& entry = songs[(size_t) i];
            const bool on = hasSheet && entry.name == openName, over = i == hover;
            auto a = area.toFloat();
            if (on || over)
            {
                g.setColour (juce::Colours::white.withAlpha (on ? 0.06f : 0.03f));
                g.fillRoundedRectangle (a, 6.0f);
            }
            const auto lamp = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ a.getX() + 16.0f, a.getCentreY() });
            if (on)
            {
                g.setColour (colours::lamp.withAlpha (0.25f));
                g.fillEllipse (lamp.expanded (3.0f));
                g.setColour (colours::lamp);
            }
            else g.setColour (juce::Colour (0xff2a2826));
            g.fillEllipse (lamp);
            g.setColour (on ? colours::text : over ? colours::text.withAlpha (0.85f) : colours::textDim);
            g.setFont (font (15.0f, on ? Weight::semibold : Weight::medium));
            g.drawFittedText (entry.name, area.withTrimmedLeft (34).withTrimmedRight (6), juce::Justification::centredLeft, 1, 0.9f);
        }
    }

    if (editing)
    {
        g.setColour (colours::text);
        g.setFont (font (30.0f, Weight::display));
        g.drawText (editingFresh || ! hasSheet ? juce::String ("A new song") : "Changing " + current.title, titleArea, juce::Justification::centredLeft);
        g.setColour (colours::textDim);
        g.setFont (font (15.0f));
        g.drawFittedText ("Title on the first line. Chords on their own line over the words. [Verse] or [Chorus] on a line makes a heading.",
                          leadArea.withRight (done.getX() > 0 ? cancel.getX() - 16 : leadArea.getRight()), juce::Justification::centredLeft, 1, 0.85f);
        return;
    }
    if (! hasSheet) { paintEmpty (g); return; }

    g.setColour (colours::text);
    g.setFont (font (30.0f, Weight::display));
    g.drawFittedText (current.title.isNotEmpty() ? current.title : juce::String ("Untitled song"), titleArea, juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (colours::textDim);
    g.setFont (font (15.0f));
    auto lead = keyText();
    if (lead.isEmpty()) lead = "Tab";
    g.drawText (lead, leadArea, juce::Justification::centredLeft);

    drawCaps (g, "Key", keyLabel.toFloat(), 11.0f, colours::textFaint);
    drawCaps (g, "Size", sizeLabel.toFloat(), 11.0f, colours::textFaint);
    drawCaps (g, "Scroll", scrollLabel.toFloat(), 11.0f, colours::textFaint);
    g.setFont (font (16.0f, Weight::numeric));
    g.setColour (current.shift == 0 ? colours::textDim : colours::lamp);
    g.drawText (current.shift > 0 ? "+" + juce::String (current.shift) : juce::String (current.shift), keyValue, juce::Justification::centred);
    g.setColour (colours::textDim);
    g.drawText (juce::String (effectiveSpeed (current)), speedValue, juce::Justification::centred);

    paintStrip (g);
}

void SongSheetView::paintStrip (juce::Graphics& g)
{
    if (strip.isEmpty()) return;
    // the shapes of the chords in this song, in the key you see, in the order they come
    const int cellW = 70;
    const int fits = std::max (1, strip.getWidth() / cellW);
    const int total = current.chords.size();
    const int shown = total > fits ? fits - 1 : total;
    const bool flats = flatsAfter (current, current.shift);
    for (int i = 0; i < shown; ++i)
    {
        Chord c;
        if (! parseChord (current.chords[i], c)) continue;
        c.root = (c.root + current.shift + 24) % 12;
        if (c.bass >= 0) c.bass = (c.bass + current.shift + 24) % 12;
        const auto name = chordName (c, flats);
        const float x0 = (float) (strip.getX() + i * cellW), y0 = (float) strip.getY();
        g.setColour (colours::lamp);
        g.setFont (font (14.5f, Weight::semibold));
        g.drawFittedText (name, juce::Rectangle<int> ((int) x0, (int) y0, 56, 18), juce::Justification::centred, 1, 0.8f);

        Shape shape;
        if (! shapeFor (name, shape)) continue;
        const float gx = x0 + 8.0f, gy = y0 + 32.0f, sw = 8.0f, fh = 12.5f;
        int lo = 99, hi = 0;
        for (int f : shape.frets) if (f > 0) { lo = std::min (lo, f); hi = std::max (hi, f); }
        const int start = hi <= 4 ? 1 : lo;
        g.setColour (gridInk);
        for (int s = 0; s < 6; ++s) g.fillRect (gx + (float) s * sw - 0.5f, gy, 1.0f, fh * 4.0f);
        for (int f = 1; f <= 4; ++f) g.fillRect (gx, gy + (float) f * fh - 0.5f, sw * 5.0f, 1.0f);
        if (start == 1)
        {
            g.setColour (colours::textDim);
            g.fillRect (gx - 0.5f, gy - 2.5f, sw * 5.0f + 1.0f, 3.0f);
        }
        else
        {
            g.setColour (colours::textFaint);
            g.setFont (font (10.5f, Weight::medium));
            g.drawText (juce::String (start) + "fr", juce::Rectangle<float> (gx + sw * 5.0f + 3.0f, gy, 22.0f, fh), juce::Justification::centredLeft);
            g.setColour (gridInk);
            g.fillRect (gx, gy - 0.5f, sw * 5.0f, 1.0f);
        }
        if (shape.barre > 0)
        {
            int first = -1, last = -1;
            for (int s = 0; s < 6; ++s) if (shape.frets[(size_t) s] == shape.barre) { if (first < 0) first = s; last = s; }
            const float cy = gy + ((float) (shape.barre - start) + 0.5f) * fh;
            g.setColour (colours::text);
            g.fillRoundedRectangle (gx + (float) first * sw - 3.5f, cy - 3.5f, (float) (last - first) * sw + 7.0f, 7.0f, 3.5f);
        }
        for (int s = 0; s < 6; ++s)
        {
            const int f = shape.frets[(size_t) s];
            const float x = gx + (float) s * sw;
            if (f < 0)
            {
                g.setColour (colours::textFaint);
                g.drawLine (x - 2.5f, gy - 10.5f, x + 2.5f, gy - 5.5f, 1.2f);
                g.drawLine (x + 2.5f, gy - 10.5f, x - 2.5f, gy - 5.5f, 1.2f);
            }
            else if (f == 0)
            {
                g.setColour (colours::textDim);
                g.drawEllipse (x - 2.6f, gy - 10.6f, 5.2f, 5.2f, 1.1f);
            }
            else if (f != shape.barre)
            {
                g.setColour (colours::text);
                g.fillEllipse (x - 3.6f, gy + ((float) (f - start) + 0.5f) * fh - 3.6f, 7.2f, 7.2f);
            }
        }
    }
    if (shown < total)
    {
        g.setColour (colours::textFaint);
        g.setFont (font (13.0f, Weight::medium));
        g.drawText ("+" + juce::String (total - shown) + " more", juce::Rectangle<int> (strip.getX() + shown * cellW, strip.getY() + 30, cellW, 20),
                    juce::Justification::centredLeft);
    }
}

void SongSheetView::paintEmpty (juce::Graphics& g)
{
    auto a = emptyArea;
    g.setColour (colours::text);
    g.setFont (font (30.0f, Weight::display));
    g.drawText ("Chords and tabs", a.removeFromTop (40), juce::Justification::centredLeft);
    a.removeFromTop (8);
    g.setColour (colours::textDim);
    g.setFont (font (16.0f));
    g.drawFittedText ("Copy the chords or tab of any song from a website, then press Ctrl+V here. Riffhouse sets it out to "
                      "read while you play: chords over their words, tab on six strings, the shapes of the chords along the top. "
                      "It changes key, and scrolls by itself so both hands stay on the guitar.",
                      a.removeFromTop (72).withWidth (std::min (a.getWidth(), 640)), juce::Justification::topLeft, 3, 1.0f);
    a.removeFromTop (34);

    // the two keys, drawn like keys
    auto key = [&g] (juce::Rectangle<float> r, const juce::String& legend)
    {
        g.setColour (juce::Colour (0xff070707));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 9.0f);
        g.setColour (juce::Colour (0xff1f1e1c));
        g.fillRoundedRectangle (r, 9.0f);
        const auto top = r.reduced (6.0f).withTrimmedBottom (5.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2a27), top.getX(), top.getY(), juce::Colour (0xff232220), top.getX(), top.getBottom(), false));
        g.fillRoundedRectangle (top, 6.0f);
        g.setColour (colours::text.withAlpha (0.9f));
        g.setFont (font (17.0f, Weight::medium));
        g.drawText (legend, top.reduced (10.0f, 6.0f), juce::Justification::topLeft);
    };
    const float y = (float) a.getY();
    key ({ (float) a.getX(), y, 96.0f, 72.0f }, "Ctrl");
    g.setColour (colours::textFaint);
    g.setFont (font (22.0f, Weight::medium));
    g.drawText ("+", juce::Rectangle<float> ((float) a.getX() + 96.0f, y, 40.0f, 72.0f), juce::Justification::centred);
    key ({ (float) a.getX() + 136.0f, y, 72.0f, 72.0f }, "V");

    g.setColour (colours::textFaint);
    g.setFont (font (13.0f));
    g.drawText (juce::String ("Songs are kept as text files in Documents") + utf8 (" \xe2\x80\xba ") + "Riffhouse" + utf8 (" \xe2\x80\xba ") + "Songs. "
                + "You can drop one on the app too.",
                juce::Rectangle<int> (a.getX(), paste.getBottom() + 18, std::min (a.getWidth(), 640), 20), juce::Justification::centredLeft);
}
} // namespace rh::ui
