// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "ChordSheet.h"
#include <cstring>

namespace rh::sheet
{
namespace
{
const char* const sharpNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* const flatNames[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

int wrap12 (int n) { return ((n % 12) + 12) % 12; }

// Reads a note name at the start of s: the letter and any sharp or flat. Returns its length.
int readNote (const juce::String& s, int& note)
{
    if (s.isEmpty()) return 0;
    static const int letters[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const auto c = s[0];
    if (c < 'A' || c > 'G') return 0;
    note = letters[c - 'A'];
    int used = 1;
    if (s.length() > 1)
    {
        const auto a = s[1];
        if (a == '#' || a == 0x266f) { ++note; used = 2; }
        else if (a == 'b' || a == 0x266d) { --note; used = 2; }
    }
    note = wrap12 (note);
    return used;
}

// Everything that may follow a chord's root, longest first so "maj7" wins over "m".
const char* const pieces[] = { "maj13", "maj11", "maj9", "maj7", "add13", "add11", "add9", "add4", "add2",
                               "dim7", "sus2", "sus4", "maj", "min", "dim", "aug", "sus", "M13", "M9", "M7",
                               "69", "11", "13", "b13", "#11", "b5", "#5", "b9", "#9", "m", "M", "5", "6", "7",
                               "9", "2", "4", "+", "-", "(", ")", "/9" };

bool validQuality (const juce::String& q)
{
    int at = 0;
    juce::StringArray seen;
    while (at < q.length())
    {
        bool matched = false;
        for (const char* piece : pieces)
        {
            const juce::String pc (piece);
            if (q.substring (at, at + pc.length()) == pc)
            {
                // a word made of chord pieces ("mmm") is not a chord: each piece once
                if (pc != "(" && pc != ")" && seen.contains (pc)) return false;
                seen.add (pc);
                at += pc.length();
                matched = true;
                break;
            }
        }
        if (! matched)
        {
            const auto c = q[at];
            if (c == 0x00b0 || c == 0x00f8 || c == 0x0394 || c == 0x00d8) { ++at; continue; } // degree, half-diminished, triangle
            return false;
        }
    }
    return true;
}

bool isIgnorable (const juce::String& t)
{
    if (t.isEmpty()) return true;
    if (t.containsOnly ("|:-/*%.~>_=")) return true;
    const auto l = t.toLowerCase().removeCharacters ("()[]");
    if (l == "n.c." || l == "nc" || l == "n.c" || l == "riff" || l == "x") return true;
    // repeat marks: x2, 2x, (x4)
    if ((l.startsWith ("x") && l.substring (1).containsOnly ("0123456789") && l.length() > 1)
        || (l.endsWith ("x") && l.dropLastCharacters (1).containsOnly ("0123456789") && l.length() > 1))
        return true;
    return false;
}

juce::String chordCore (const juce::String& token)
{
    auto t = token;
    while (t.startsWithChar ('(') || t.startsWithChar ('[')) t = t.substring (1);
    while (t.endsWithChar (')') || t.endsWithChar (',') || t.endsWithChar (']') || t.endsWithChar ('*')) t = t.dropLastCharacters (1);
    // "(Am)" keeps the closing bracket off, "Cadd9)" too; an unbalanced "(" inside stays
    return t;
}

const juce::StringArray& sectionWords()
{
    static const juce::StringArray w { "intro", "verse", "chorus", "pre-chorus", "prechorus", "pre chorus", "bridge",
                                       "outro", "solo", "interlude", "instrumental", "hook", "refrain", "ending",
                                       "coda", "riff", "break", "breakdown", "post-chorus", "tag", "fill", "main riff" };
    return w;
}

// "Verse 1", "Chorus:", "(Chorus x2)", "Guitar solo" as a heading of its own
bool looksLikeSection (const juce::String& line, juce::String& name)
{
    auto t = line.trim();
    if (t.startsWithChar ('(') && t.endsWithChar (')')) t = t.substring (1, t.length() - 1).trim();
    if (t.endsWithChar (':')) t = t.dropLastCharacters (1).trim();
    if (t.isEmpty() || t.length() > 32) return false;
    const auto l = t.toLowerCase();
    for (const auto& w : sectionWords())
    {
        if (l == w || l.startsWith (w + " ") || l.startsWith ("guitar " + w) || l.startsWith ("final " + w))
        {
            // anything after the word: a number or a repeat mark, never a lyric
            const auto rest = l.fromFirstOccurrenceOf (w, false, false).trim();
            if (rest.isEmpty() || rest.containsOnly ("0123456789x()# "))
            {
                name = t;
                return true;
            }
        }
    }
    return false;
}

bool chordTokens (const juce::String& line, int& chords)
{
    chords = 0;
    auto tokens = juce::StringArray::fromTokens (line, " ", "");
    tokens.removeEmptyStrings();
    if (tokens.isEmpty()) return false;
    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto& t = tokens[i];
        Chord c;
        if (parseChord (chordCore (t), c)) { ++chords; continue; }
        if (isIgnorable (t)) continue;
        // a label in front: "Intro:" "Chorus:"
        juce::String name;
        if (i == 0 && t.endsWithChar (':') && looksLikeSection (t, name)) continue;
        if (i == 0 && t.endsWithChar (':') && t.length() <= 12) continue;
        return false;
    }
    return chords > 0;
}

bool isTabLine (const juce::String& raw)
{
    const auto t = raw.trim();
    if (t.length() < 6) return false;
    auto body = t;
    // an optional string name in front: "e|", "B |", "G:", "D#|"
    int note = 0;
    const int n = readNote (t.toUpperCase(), note);
    if (n > 0)
    {
        auto after = t.substring (n).trimStart();
        if (after.startsWithChar ('|') || after.startsWithChar (':')) body = after;
    }
    else if (t[0] == 'e' || t[0] == 'b' || t[0] == 'g' || t[0] == 'd' || t[0] == 'a')
    {
        auto after = t.substring (1).trimStart();
        if (after.startsWithChar ('|') || after.startsWithChar (':')) body = after;
    }
    if (body == t && ! (t.startsWithChar ('|') || t.startsWithChar ('-'))) return false;
    int dashes = 0, other = 0;
    for (auto p = body.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;
        if (c == '-' || c == 0x2014 || c == 0x2013) ++dashes;
        else if (! juce::String ("0123456789|:hpbrsxX/\\~().^vtPS*=<>[] ").containsChar (c)) ++other;
    }
    return dashes >= 4 && other <= 2 && dashes * 3 >= body.length();
}

juce::String cleanWebText (juce::String s)
{
    s = s.replace ("\r\n", "\n").replace ("\r", "\n");
    s = s.replaceCharacter ((juce::juce_wchar) 0x00a0, ' ').replaceCharacter ((juce::juce_wchar) 0x2009, ' ')
         .replaceCharacter ((juce::juce_wchar) 0x202f, ' ');
    s = s.replace ("[ch]", "").replace ("[/ch]", "").replace ("[tab]", "").replace ("[/tab]", "");
    // tabs to spaces, every 8 columns, so chords stay over their words
    juce::String out;
    int col = 0;
    for (auto p = s.getCharPointer(); ! p.isEmpty(); ++p)
    {
        const auto c = *p;
        if (c == '\n') { out << '\n'; col = 0; }
        else if (c == '\t') { const int to = (col / 8 + 1) * 8; while (col < to) { out << ' '; ++col; } }
        else { out << juce::String::charToString (c); ++col; }
    }
    return out;
}

// "[Am]Walk a[G]long" -> chords over the words. Returns false when the line has no inline chord.
bool splitInline (const juce::String& line, juce::String& chords, juce::String& words)
{
    if (! line.containsChar ('[')) return false;
    juce::String c, w;
    bool any = false;
    int i = 0;
    while (i < line.length())
    {
        if (line[i] == '[')
        {
            const int close = line.indexOfChar (i, ']');
            Chord parsed;
            if (close > i && parseChord (line.substring (i + 1, close), parsed))
            {
                const auto name = line.substring (i + 1, close);
                // a chord needs room: when the last one is still running, push the words along
                if (c.length() > w.length()) w = w.paddedRight (' ', c.length());
                if (c.length() < w.length()) c = c.paddedRight (' ', w.length());
                c << name << ' ';
                any = true;
                i = close + 1;
                continue;
            }
        }
        w << juce::String::charToString (line[i]);
        ++i;
    }
    if (! any) return false;
    chords = c.trimEnd();
    words = w.trimEnd();
    return true;
}

bool parseDirective (const juce::String& line, Sheet& sheet, std::vector<Line>& lines)
{
    const auto t = line.trim();
    if (! (t.startsWithChar ('{') && t.endsWithChar ('}'))) return false;
    const auto inner = t.substring (1, t.length() - 1);
    const auto key = inner.upToFirstOccurrenceOf (":", false, false).trim().toLowerCase();
    const auto value = inner.fromFirstOccurrenceOf (":", false, false).trim();
    if (key == "title" || key == "t") sheet.title = value;
    else if (key == "comment" || key == "c" || key == "ci" || key == "cb" || key == "comment_italic")
        lines.push_back ({ LineKind::section, value });
    else if (key == "start_of_chorus" || key == "soc") lines.push_back ({ LineKind::section, value.isNotEmpty() ? value : juce::String ("Chorus") });
    else if (key == "start_of_verse" || key == "sov") lines.push_back ({ LineKind::section, value.isNotEmpty() ? value : juce::String ("Verse") });
    else if (key == "start_of_bridge" || key == "sob") lines.push_back ({ LineKind::section, value.isNotEmpty() ? value : juce::String ("Bridge") });
    else if (key == "x_riffhouse")
    {
        for (const auto& kv : juce::StringArray::fromTokens (value, " ,", ""))
        {
            const auto k = kv.upToFirstOccurrenceOf ("=", false, false).trim();
            const int v = kv.fromFirstOccurrenceOf ("=", false, false).trim().getIntValue();
            if (k == "shift") sheet.shift = juce::jlimit (-11, 11, v);
            else if (k == "size") sheet.size = juce::jlimit (-4, 6, v);
            else if (k == "speed") sheet.speed = juce::jlimit (0, 9, v);
        }
    }
    return true; // other directives (subtitle, key, capo, end_of_...) are left out of the sheet
}

juce::String tidyTitle (juce::String t)
{
    t = t.trim();
    for (const char* w : { " Chords by ", " Chords By ", " Tab by ", " Tabs by ", " Guitar Pro by " })
        t = t.replace (w, " by ");
    for (const char* w : { " Chords", " Tab", " Tabs" })
        if (t.endsWith (w)) t = t.dropLastCharacters ((int) strlen (w));
    return t;
}
} // namespace

juce::String noteName (int note, bool flats)
{
    return (flats ? flatNames : sharpNames)[wrap12 (note)];
}

bool parseChord (const juce::String& token, Chord& out)
{
    const auto t = token.trim();
    int root = 0;
    const int used = readNote (t, root);
    if (used == 0) return false;
    auto rest = t.substring (used);
    int bass = -1;
    const int slash = rest.lastIndexOfChar ('/');
    if (slash >= 0 && rest.substring (slash) != "/9")
    {
        const auto b = rest.substring (slash + 1);
        int note = 0;
        if (readNote (b, note) != b.length() || b.isEmpty()) return false;
        bass = note;
        rest = rest.substring (0, slash);
    }
    if (! validQuality (rest)) return false;
    out.root = root;
    out.quality = rest;
    out.bass = bass;
    return true;
}

juce::String chordName (const Chord& c, bool flats)
{
    auto s = noteName (c.root, flats) + c.quality;
    if (c.bass >= 0) s << "/" << noteName (c.bass, flats);
    return s;
}

Sheet parse (const juce::String& text)
{
    Sheet sheet;
    std::vector<Line> lines;
    for (const auto& raw : juce::StringArray::fromLines (cleanWebText (text)))
    {
        const auto line = raw.trimEnd();
        if (line.trim().isEmpty()) { lines.push_back ({ LineKind::blank, {} }); continue; }
        if (parseDirective (line, sheet, lines)) continue;
        if (line.trim().containsOnly ("-=_~* ")) { lines.push_back ({ LineKind::blank, {} }); continue; } // a website's rule line
        if (isTabLine (line)) { lines.push_back ({ LineKind::tab, line }); continue; }
        int chords = 0;
        juce::String name;
        const auto t = line.trim();
        if (t.startsWithChar ('[') && t.endsWithChar (']') && t.indexOfChar (1, '[') < 0)
        {
            Chord c;
            const auto inner = t.substring (1, t.length() - 1).trim();
            if (! parseChord (inner, c)) { lines.push_back ({ LineKind::section, inner }); continue; }
        }
        juce::String chordLine, words;
        if (splitInline (line, chordLine, words))
        {
            if (words.trim().isEmpty()) lines.push_back ({ LineKind::chords, chordLine });
            else { lines.push_back ({ LineKind::chords, chordLine }); lines.push_back ({ LineKind::lyric, words }); }
            continue;
        }
        if (chordTokens (line, chords)) { lines.push_back ({ LineKind::chords, line }); continue; }
        if (looksLikeSection (line, name)) { lines.push_back ({ LineKind::section, name }); continue; }
        lines.push_back ({ LineKind::lyric, line });
    }

    // trim the ends, and never more than one empty line in a row
    std::vector<Line> tidy;
    for (const auto& l : lines)
    {
        if (l.kind == LineKind::blank && (tidy.empty() || tidy.back().kind == LineKind::blank)) continue;
        tidy.push_back (l);
    }
    while (! tidy.empty() && tidy.back().kind == LineKind::blank) tidy.pop_back();

    // a first line standing on its own above the song is its title
    if (sheet.title.isEmpty() && tidy.size() > 3 && tidy[0].kind == LineKind::lyric && tidy[0].text.trim().length() <= 70
        && tidy[1].kind != LineKind::lyric)
    {
        sheet.title = tidy[0].text;
        tidy.erase (tidy.begin());
        while (! tidy.empty() && tidy.front().kind == LineKind::blank) tidy.erase (tidy.begin());
    }
    sheet.title = tidyTitle (sheet.title);
    sheet.lines = std::move (tidy);

    int flats = 0, sharps = 0;
    for (const auto& l : sheet.lines)
    {
        if (l.kind != LineKind::chords) continue;
        for (const auto& tok : juce::StringArray::fromTokens (l.text, " ", ""))
        {
            Chord c;
            const auto core = chordCore (tok);
            if (core.isEmpty() || ! parseChord (core, c)) continue;
            if (core.length() > 1 && core[1] == 'b') ++flats;
            if (core.length() > 1 && core[1] == '#') ++sharps;
            sheet.chords.addIfNotAlreadyThere (core);
        }
    }
    sheet.flats = flats > sharps;
    return sheet;
}

juce::String transposeLine (const juce::String& line, int semitones, bool flats)
{
    if (wrap12 (semitones) == 0) return line;
    juce::String out;
    int i = 0;
    while (i < line.length())
    {
        if (line[i] == ' ') { ++i; continue; }
        const int start = i;
        while (i < line.length() && line[i] != ' ') ++i;
        const auto token = line.substring (start, i);
        auto core = chordCore (token);
        Chord c;
        juce::String moved = token;
        if (core.isNotEmpty() && parseChord (core, c))
        {
            c.root = wrap12 (c.root + semitones);
            if (c.bass >= 0) c.bass = wrap12 (c.bass + semitones);
            const int at = token.indexOf (core);
            moved = token.substring (0, at) + chordName (c, flats) + token.substring (at + core.length());
        }
        // keep each chord over its word: take spaces back after a longer name, pad after a shorter one
        while (out.length() > start && out.endsWith ("  ")) out = out.dropLastCharacters (1);
        if (out.isNotEmpty() && ! out.endsWithChar (' ')) out << ' ';
        if (out.length() < start) out = out.paddedRight (' ', start);
        out << moved;
    }
    return out;
}

bool flatsAfter (const Sheet& sheet, int semitones)
{
    if (wrap12 (semitones) == 0) return sheet.flats;
    if (sheet.chords.isEmpty()) return false;
    Chord first;
    if (! parseChord (sheet.chords[0], first)) return false;
    const bool minor = (first.quality.startsWith ("m") && ! first.quality.startsWith ("maj")) || first.quality.startsWith ("-");
    const int key = wrap12 (first.root + semitones + (minor ? 3 : 0)); // a minor key is written like its relative major
    return key == 5 || key == 10 || key == 3 || key == 8 || key == 1;
}

juce::String toText (const Sheet& sheet, int semitones, bool withSettings)
{
    const bool flats = flatsAfter (sheet, semitones);
    juce::StringArray out;
    // kept on disk, the title is marked so even a two-line song reads back with it
    if (sheet.title.isNotEmpty()) { out.add (withSettings ? "{title: " + sheet.title + "}" : sheet.title); out.add ({}); }
    for (const auto& l : sheet.lines)
    {
        switch (l.kind)
        {
            case LineKind::blank:   out.add ({}); break;
            case LineKind::section: out.add ("[" + l.text + "]"); break;
            case LineKind::chords:  out.add (transposeLine (l.text, semitones, flats)); break;
            case LineKind::tab:
            case LineKind::lyric:   out.add (l.text); break;
        }
    }
    if (withSettings && (sheet.shift != 0 || sheet.size != 0 || sheet.speed != 0))
    {
        out.add ({});
        out.add ("{x_riffhouse: shift=" + juce::String (sheet.shift) + " size=" + juce::String (sheet.size)
                 + " speed=" + juce::String (sheet.speed) + "}");
    }
    return out.joinIntoString ("\n") + "\n";
}

//==============================================================================
namespace
{
enum Family { fMaj, fMin, f7, fM7, fMaj7, fSus2, fSus4, f5, fDim, fDim7, fAug, fM7b5, fNone };

Family familyOf (juce::String q)
{
    q = q.removeCharacters ("()");
    if (q.startsWith ("m7b5") || q.containsChar (0x00f8) || q.containsChar (0x00d8)) return fM7b5;
    if (q.contains ("dim7") || q == juce::String::charToString (0x00b0) + "7") return fDim7;
    if (q.contains ("dim") || q.containsChar (0x00b0)) return fDim;
    if (q.contains ("aug") || q.containsChar ('+') || q == "#5") return fAug;
    const bool minor = (q.startsWith ("m") && ! q.startsWith ("maj")) || q.startsWith ("min") || q.startsWith ("-");
    if (minor)
    {
        const auto rest = q.startsWith ("min") ? q.substring (3) : q.substring (1);
        if (rest.startsWith ("7") || rest.startsWith ("9") || rest.startsWith ("11") || rest.startsWith ("13")) return fM7;
        return fMin;
    }
    if (q.contains ("sus2")) return fSus2;
    if (q.contains ("sus")) return fSus4;
    if ((q.startsWith ("maj") || q.startsWith ("M")) && q != "maj" && q != "M") return fMaj7;
    if (q == "5") return f5;
    if (q.startsWith ("7") || q.startsWith ("9") || q.startsWith ("11") || q.startsWith ("13")) return f7;
    return fMaj; // "", maj, M, 6, 69, add9, 2: the plain chord carries it
}

constexpr int X = 99; // a string left out
struct Movable { Family f; std::array<int, 6> off; };
const Movable eShapes[] = {
    { fMaj,  { 0, 2, 2, 1, 0, 0 } }, { fMin,  { 0, 2, 2, 0, 0, 0 } }, { f7,    { 0, 2, 0, 1, 0, 0 } },
    { fM7,   { 0, 2, 0, 0, 0, 0 } }, { fMaj7, { 0, X, 1, 1, 0, X } }, { fSus4, { 0, 2, 2, 2, 0, 0 } },
    { f5,    { 0, 2, 2, X, X, X } } };
const Movable aShapes[] = {
    { fMaj,  { X, 0, 2, 2, 2, 0 } }, { fMin,  { X, 0, 2, 2, 1, 0 } }, { f7,    { X, 0, 2, 0, 2, 0 } },
    { fM7,   { X, 0, 2, 0, 1, 0 } }, { fMaj7, { X, 0, 2, 1, 2, 0 } }, { fSus2, { X, 0, 2, 2, 0, 0 } },
    { fSus4, { X, 0, 2, 2, 3, 0 } }, { f5,    { X, 0, 2, 2, X, X } }, { fDim,  { X, 0, 1, 2, 1, X } },
    { fDim7, { X, 0, 1, 2, 1, 2 } }, { fAug,  { X, 0, 3, 2, 2, 1 } }, { fM7b5, { X, 0, 1, 0, 1, X } } };

// The open chords everyone learns first, as they are played, not as a barre shape moved down.
struct Open { int root; Family f; const char* frets; };
const Open openChords[] = {
    { 0, fMaj, "x32010" }, { 0, f7, "x32310" }, { 0, fMaj7, "x32000" }, { 0, fSus4, "x33011" }, { 0, fSus2, "x30013" },
    { 2, fMaj, "xx0232" }, { 2, fMin, "xx0231" }, { 2, f7, "xx0212" }, { 2, fM7, "xx0211" }, { 2, fMaj7, "xx0222" },
    { 2, fSus2, "xx0230" }, { 2, fSus4, "xx0233" }, { 2, f5, "xx023x" },
    { 4, fMaj7, "021100" }, { 4, fM7, "022030" },
    { 5, fMaj7, "xx3210" },
    { 7, fMaj, "320003" }, { 7, f7, "320001" }, { 7, fMaj7, "320002" }, { 7, fSus4, "3x0013" },
    { 11, f7, "x21202" } };
} // namespace

bool shapeFor (const juce::String& name, Shape& out)
{
    Chord c;
    if (! parseChord (name, c)) return false;
    const auto f = familyOf (c.quality);
    out = {};
    if (c.root == 0 && c.quality == "add9") { out.frets = { -1, 3, 2, 0, 3, 3 }; return true; }
    for (const auto& o : openChords)
        if (o.root == c.root && o.f == f)
        {
            for (int s = 0; s < 6; ++s) out.frets[(size_t) s] = o.frets[s] == 'x' ? -1 : o.frets[s] - '0';
            return true;
        }
    // the barre shape nearest the nut: root on the low E string or on the A string
    const Movable* best = nullptr;
    int bestFret = 99;
    for (const auto& m : eShapes)
        if (m.f == f) { const int fr = wrap12 (c.root - 4); if (fr < bestFret) { best = &m; bestFret = fr; } }
    for (const auto& m : aShapes)
        if (m.f == f) { const int fr = wrap12 (c.root - 9); if (fr < bestFret) { best = &m; bestFret = fr; } }
    if (best == nullptr) return false;
    int firstZero = -1, lastZero = -1;
    bool gap = false;
    for (int s = 0; s < 6; ++s)
    {
        const int o = best->off[(size_t) s];
        out.frets[(size_t) s] = o == X ? -1 : bestFret + o;
        if (o == 0) { if (firstZero < 0) firstZero = s; lastZero = s; }
        else if (o == X && firstZero >= 0) gap = true;
    }
    // one finger flat across: the root's string to the top string, nothing left out between
    out.barre = (bestFret > 0 && firstZero >= 0 && lastZero - firstZero >= 3 && ! gap) ? bestFret : 0;
    return true;
}
} // namespace rh::sheet
