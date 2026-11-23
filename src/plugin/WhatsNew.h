// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

namespace rh
{
// The "What's new" text, written by hand in resources/whatsnew.txt: "## <version>" starts a
// version, "### New" and "### Fixed" its sections, "- " a line. Lines starting with # and a space
// are comments.
struct WhatsNewEntry
{
    juce::String version;
    juce::StringArray added, fixed;
};

// The entry for this version label ("0.1.6", "0.1.6.1", "0.1.6-rc2"). A build from source with no
// entry of its own gets the newest one.
WhatsNewEntry findWhatsNew (const juce::String& text, const juce::String& versionLabel);

// "0.1.6" for "0.1.6-rc2": the release the notes are filed under.
juce::String releaseOf (const juce::String& versionLabel);
} // namespace rh
