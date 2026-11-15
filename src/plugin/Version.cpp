// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Version.h"

#ifndef RIFFHOUSE_VERSION_LABEL
 #define RIFFHOUSE_VERSION_LABEL "0.0.0-dev"
#endif

namespace rh
{
juce::String versionLabel() { return RIFFHOUSE_VERSION_LABEL; }

juce::String spokenVersion (const juce::String& label)
{
    // A fourth number is a follow-up to that release: "0.1.5.1" is said "0.1.5 Hotfix 1" when it
    // only fixes things, "0.1.5 Update 1" when it also brings something new. Set in the release commit.
    // 0.1.5.4 brought real amp captures and the auto gate, so it is "0.1.5 Update 1", not Hotfix 4
    const auto core = label.upToFirstOccurrenceOf ("-", false, false), rest = label.substring (core.length());
    if (core == "0.1.5.4") return "0.1.5 Update 1" + rest;
    const auto parts = juce::StringArray::fromTokens (core, ".", {});
    if (parts.size() == 4 && parts[3].getIntValue() > 0)
        return parts[0] + "." + parts[1] + "." + parts[2] + " Hotfix " + parts[3] + rest;
    return label;
}

juce::String versionForPeople()
{
    const auto v = spokenVersion (versionLabel());
    if (v.contains ("-dev")) return v + " (built from source)";
    if (v.containsChar ('-')) return v + " (test build)";
    return v;
}

// kept to one sentence that fits a toast; change it in the release commit of each version
juce::String whatsNew()
{
    return "Real amp captures for the metal amps, and a noise gate that sets itself.";
}
} // namespace rh
