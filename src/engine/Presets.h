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
//   "inspiredBy": "",
//   "description": "Glassy and bright.",
//   "params": { "amp_model": "Glass Clean", "amp_gain": 3.5, "rev_on": true }
// }
struct Preset
{
    juce::String name, category, inspiredBy, description, author, file;
    juce::StringArray genres, tags;
    ParamValues values {};
    bool factory = false;
    bool favourite = false;

    juce::String searchText() const;
};

// Parses a preset. Choice parameters accept either an index or the choice's name.
bool presetFromJson (const juce::String& json, Preset& out, juce::String& error);
juce::String presetToJson (const Preset& p);

// Compact single-line form ("riffhouse:<base64>") for pasting into chats.
juce::String presetToShareCode (const Preset& p);
bool presetFromShareCode (const juce::String& code, Preset& out, juce::String& error);

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
