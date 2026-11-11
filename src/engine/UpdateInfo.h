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
// a pre-release ("0.2.0-rc1") is older than the release with the same numbers, and
// test builds of one version go in order: rc1, rc2, ... rc10.
int compareVersions (const juce::String& a, const juce::String& b);

// A test build ("0.1.0-rc2") or one built from source between releases ("0.1.0-dev.1a2b3c4").
bool isPreRelease (const juce::String& version);

// Reads GitHub's JSON for a release and picks the asset whose name ends in `installerSuffix`.
// Drafts always come back empty, and so do pre-releases unless they are asked for.
ReleaseInfo parseRelease (const juce::String& json, const juce::String& installerSuffix, bool allowPreRelease = false);

// From GitHub's list of releases: the newest one, test releases included. Someone running a
// test build is offered the next test build, and the full release when it arrives.
ReleaseInfo newestRelease (const juce::String& listJson, const juce::String& installerSuffix);

// What the answer from the "latest release" address means. GitHub answers 404 while nothing has
// been published yet (test releases do not count): that is "you have the latest", not a fault.
// Anything else that is not a 200 with a body means the check could not be made.
enum class CheckAnswer { unreachable, nothingPublished, release };
CheckAnswer readCheckAnswer (int httpStatus, const juce::String& body);

// Only this project's own release downloads, over HTTPS.
bool isTrustedDownloadUrl (const juce::String& url);

juce::String sha256Hex (const juce::File&);
} // namespace rh
