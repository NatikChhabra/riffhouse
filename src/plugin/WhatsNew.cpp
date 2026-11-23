// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "WhatsNew.h"

namespace rh
{
juce::String releaseOf (const juce::String& label) { return label.upToFirstOccurrenceOf ("-", false, false); }

WhatsNewEntry findWhatsNew (const juce::String& text, const juce::String& label)
{
    juce::Array<WhatsNewEntry> all;
    bool fixed = false;
    for (auto line : juce::StringArray::fromLines (text))
    {
        line = line.trim();
        if (line.startsWith ("## "))
        {
            all.add ({ line.substring (3).trim().upToFirstOccurrenceOf (" ", false, false), {}, {} });
            fixed = false;
        }
        else if (line.startsWith ("### "))
            fixed = line.substring (4).trim().equalsIgnoreCase ("Fixed");
        else if (line.startsWith ("- ") && ! all.isEmpty())
            (fixed ? all.getReference (all.size() - 1).fixed : all.getReference (all.size() - 1).added).add (line.substring (2).trim());
    }
    if (all.isEmpty()) return {};

    const auto core = releaseOf (label);
    const auto three = juce::StringArray::fromTokens (core, ".", {});
    const auto release = three.size() > 3 ? three[0] + "." + three[1] + "." + three[2] : core;
    for (const auto& e : all) if (e.version == core) return e;
    for (const auto& e : all) if (e.version == release) return e;
    return all[0];
}
} // namespace rh
