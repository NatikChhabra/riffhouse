// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace rh::sheet
{
// Chords and tabs copied from any website or typed by hand, read back as a song sheet:
// which lines are chords, which are tab, which are lyrics, the chords in another key,
// and where to put your fingers for each chord.

struct Chord
{
    int root = 0;          // 0 = C ... 11 = B
    juce::String quality;  // as written after the root: "", "m", "7", "maj7", "sus4", "m7b5" ...
    int bass = -1;         // the note after a slash, or -1
};

// "Am", "F#m7", "Bb/D", "Csus4", "E5". False for anything that isn't a chord name.
bool parseChord (const juce::String& token, Chord& out);
juce::String chordName (const Chord&, bool flats);
juce::String noteName (int note, bool flats);

enum class LineKind { blank, section, chords, tab, lyric };

struct Line
{
    LineKind kind = LineKind::blank;
    juce::String text;
};

struct Sheet
{
    juce::String title;            // a ChordPro {title}, or a first line that stands alone
    std::vector<Line> lines;
    juce::StringArray chords;      // each chord once, in the order it first appears
    bool flats = false;            // the sheet spells its chords with flats
    int shift = 0, size = 0, speed = 0; // the reader's own settings, kept in the text
};

// Cleans what a website copy brings along ([ch]..[/ch], [tab]..[/tab], tabs, ChordPro's
// [Am]inline chords and {directives}) and sorts every line.
Sheet parse (const juce::String& text);

// The text a sheet is kept and shared as, its chords moved by `semitones`; with the reader's
// settings on a last line when it is kept on disk.
juce::String toText (const Sheet&, int semitones, bool withSettings);

// The same line in another key. Chords keep their columns over the lyrics.
juce::String transposeLine (const juce::String& chordLine, int semitones, bool flats);
// Sharps or flats for a sheet moved by this much: what the new key is usually written with.
bool flatsAfter (const Sheet&, int semitones);

// Where the fingers go. frets[0] is the low E string; -1 = not played, 0 = open.
struct Shape
{
    std::array<int, 6> frets { -1, -1, -1, -1, -1, -1 };
    int barre = 0;   // fret held by one finger across the strings, or 0
};
bool shapeFor (const juce::String& chordName, Shape& out);
} // namespace rh::sheet
