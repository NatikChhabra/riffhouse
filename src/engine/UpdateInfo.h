// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include <juce_core/juce_core.h>

namespace rh
{
// What the latest published release offers this computer.
struct ReleaseInfo
{
    juce::String version, pageUrl, notes;
    juce::String assetName, assetUrl, sha256; // the installer for this platform, and the hash it must have
    juce::int64 size = 0;
    bool hasInstaller() const noexcept { return assetUrl.isNotEmpty() && sha256.length() == 64 && size > 0; }
};

// < 0 when a is older than b, 0 when equal, > 0 when newer. "v0.2.0" equals "0.2";
// a pre-release ("0.2.0-beta") is older than the release with the same numbers.
int compareVersions (const juce::String& a, const juce::String& b);

// Reads GitHub's JSON for a release and picks the asset whose name ends in `installerSuffix`.
// Drafts and pre-releases come back empty.
ReleaseInfo parseRelease (const juce::String& json, const juce::String& installerSuffix);

// Only this project's own release downloads, over HTTPS.
bool isTrustedDownloadUrl (const juce::String& url);

juce::String sha256Hex (const juce::File&);
} // namespace rh
