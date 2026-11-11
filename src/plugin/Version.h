// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

namespace rh
{
// The version the player is running, as they would say it: "0.1.0" for a release,
// "0.1.0-rc2" for a test build, "0.1.0-dev.1a2b3c4" for one built from source in between.
juce::String versionLabel();

// "0.1.0", or "0.1.0-rc2 (test build)": for the places that say what this copy is.
juce::String versionForPeople();
} // namespace rh
