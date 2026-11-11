// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "Version.h"

#ifndef RIFFHOUSE_VERSION_LABEL
 #define RIFFHOUSE_VERSION_LABEL "0.0.0-dev"
#endif

namespace rh
{
juce::String versionLabel() { return RIFFHOUSE_VERSION_LABEL; }

juce::String versionForPeople()
{
    const auto v = versionLabel();
    if (v.contains ("-dev")) return v + " (built from source)";
    if (v.containsChar ('-')) return v + " (test build)";
    return v;
}
} // namespace rh
