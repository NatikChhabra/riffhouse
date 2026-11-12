// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "../engine/ChordSheet.h"
#include "Widgets.h"

namespace rh::ui
{
// Chords and tabs to play from. Copy a song's chords or tab from any website and press Ctrl+V:
// the sheet is set out to read at arm's length, chords in the amp's lamp colour over their
// words, tab drawn on six strings, the chord shapes along the top. It changes key, grows,
// scrolls by itself while both hands are on the guitar, and copies out in the key you see.
// Songs are plain text files in Documents/Riffhouse/Songs, so they can be shared and edited anywhere.
class SongSheetView : public juce::Component, private juce::Timer
{
public:
    SongSheetView();
    ~SongSheetView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Text that reads as a song: two or more lines of chords or tab.
    static bool looksLikeSong (const juce::String&);
    // Keeps the text as a new song and opens it. False when there is nothing to set out.
    bool pasteSong (const juce::String& text);
    bool importFile (const juce::File&);
    static bool isSongFile (const juce::File&);
    static juce::File folder();

    // Pictures of the app: a song of our own, never saved.
    void showExample (int shift, bool editing, float scrolled = 0.0f);

    std::function<void()> onClose;
    std::function<void (juce::String)> onMessage;

private:
    class Body;
    struct Entry { juce::File file; juce::String name; };

    void refreshList();
    void open (const juce::File&);
    void openSheet (sheet::Sheet, const juce::File&);
    void save();
    void setEditing (bool on, bool fresh);
    void finishEditing();
    void layout();
    void changed (bool keep);
    void startScroll (bool on);
    void timerCallback() override;
    int railItemAt (juce::Point<int>) const;
    void paintStrip (juce::Graphics&);
    void paintEmpty (juce::Graphics&);
    juce::String keyText() const;
    float pixelsPerSecond() const;
    void message (const juce::String& m) { if (onMessage) onMessage (m); }

    std::vector<Entry> songs;
    juce::File currentFile;
    sheet::Sheet current;
    bool hasSheet = false, editing = false, editingFresh = false, scrolling = false, example = false;
    double scrollY = 0.0, lastTick = 0.0;
    int hover = -1, railScroll = 0;

    juce::Rectangle<int> rail, main, titleArea, leadArea, toolbar, strip, sheetArea, emptyArea;
    juce::Rectangle<int> keyValue, speedValue, keyLabel, sizeLabel, scrollLabel;
    std::vector<std::pair<int, juce::Rectangle<int>>> railItems;

    IconButton close { icons::close() };
    IconButton keyDown { icons::minus() }, keyUp { icons::plus() }, smaller { icons::minus() }, bigger { icons::plus() };
    IconButton scrollButton { icons::play(), {}, true }, slower { icons::minus() }, faster { icons::plus() };
    juce::TextButton edit { "Edit" }, copy { "Copy" }, paste { "Paste a song" }, type { "Type one in" };
    juce::TextButton done { "Done" }, cancel { "Cancel" };
    std::unique_ptr<Body> body;
    juce::Viewport scroller;
    juce::TextEditor editor;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace rh::ui
