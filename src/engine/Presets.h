// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Params.h"

#include <juce_core/juce_core.h>

namespace rh
{
// A preset is a small JSON document. Parameters it leaves out keep their
// defaults, so presets stay short and readable.
//
// {
//   "name": "Sparkle Clean",
//   "category": "Clean",
//   "genres": ["Funk", "Neo-Soul"],
//   "inspiredBy": "",            // a player or a band
//   "band": "", "song": "",      // optional: the player's band, and the one song the tone is made after
//   "description": "Glassy and bright.",
//   "params": { "amp_model": "Glass Clean", "amp_gain": 3.5, "rev_on": true }
// }
struct Preset
{
    juce::String name, category, inspiredBy, band, song, description, author, file;
    juce::StringArray genres, tags;
    ParamValues values {};
    bool factory = false;
    bool favourite = false;

    juce::String searchText() const;
    // Whose sound it is after, as a player looks for it: the band when there is one, else the player.
    juce::String artist() const { return band.isNotEmpty() ? band : inspiredBy; }
    // The name without "Inspired by <someone>: " in front, for places that show the artist beside it.
    juce::String title() const;
    // What is left of the title after the song it starts with ("Interlude", or nothing). The whole title when it
    // does not start with the tone's song.
    juce::String part() const;
    // "Inspired by James Hetfield. ", "Inspired by David Gilmour on "Comfortably Numb". " or "":
    // starts the line under the title, where the artist already stands in front of the title
    juce::String credit() const;
};

// Parses a preset. Choice parameters accept either an index or the choice's name.
bool presetFromJson (const juce::String& json, Preset& out, juce::String& error);
juce::String presetToJson (const Preset& p);

// Compact single-line form ("riffhouse:<base64>") for pasting into chats.
juce::String presetToShareCode (const Preset& p);
// Finds the code anywhere in pasted text (a whole chat message, line breaks inside the code).
bool presetFromShareCode (const juce::String& text, Preset& out, juce::String& error);
// What "Copy tone code" puts on the clipboard: a line a friend understands, the free download, the code.
juce::String presetToShareMessage (const Preset& p);

class PresetLibrary
{
public:
    void clear() { presets.clear(); }
    void addFactory (const char* data, int size);
    void scanFolder (const juce::File& folder, bool factory);
    void add (const Preset& p) { presets.push_back (p); }
    const std::vector<Preset>& all() const { return presets; }
    std::vector<int> search (const juce::String& query, const juce::String& category) const;
    juce::StringArray categories() const;
    int indexOf (const juce::String& name) const;

private:
    std::vector<Preset> presets;
};
} // namespace rh
