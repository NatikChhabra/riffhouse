// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "UpdateInfo.h"

#include <juce_cryptography/juce_cryptography.h>

namespace rh
{
namespace
{
    bool isHex64 (const juce::String& s)
    {
        return s.length() == 64 && s.containsOnly ("0123456789abcdefABCDEF");
    }

    // a 64-digit hex number on the line of the release notes that names the file
    juce::String hashFromNotes (const juce::String& notes, const juce::String& fileName)
    {
        for (auto& line : juce::StringArray::fromLines (notes))
        {
            if (! line.containsIgnoreCase (fileName)) continue;
            for (auto& word : juce::StringArray::fromTokens (line, " \t`*:=()|", {}))
                if (isHex64 (word)) return word.toLowerCase();
        }
        return {};
    }
} // namespace

int compareVersions (const juce::String& a, const juce::String& b)
{
    auto split = [] (juce::String v, juce::StringArray& numbers, bool& pre)
    {
        v = v.trim();
        if (v.startsWithIgnoreCase ("v")) v = v.substring (1);
        pre = v.containsAnyOf ("-+") && v.indexOfChar ('-') >= 0;
        v = v.upToFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false);
        numbers = juce::StringArray::fromTokens (v, ".", {});
    };
    juce::StringArray na, nb;
    bool pa = false, pb = false;
    split (a, na, pa);
    split (b, nb, pb);
    for (int i = 0; i < std::max (na.size(), nb.size()); ++i)
    {
        const auto x = i < na.size() ? na[i].getLargeIntValue() : 0;
        const auto y = i < nb.size() ? nb[i].getLargeIntValue() : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    if (pa != pb) return pa ? -1 : 1;
    return 0;
}

CheckAnswer readCheckAnswer (int httpStatus, const juce::String& body)
{
    if (httpStatus == 404) return CheckAnswer::nothingPublished;
    if (httpStatus == 200 && body.isNotEmpty()) return CheckAnswer::release;
    return CheckAnswer::unreachable;
}

bool isTrustedDownloadUrl (const juce::String& url)
{
    return url.startsWithIgnoreCase ("https://github.com/NatikChhabra/riffhouse/releases/download/")
        && ! url.containsAnyOf (" \r\n\"'<>\\");
}

ReleaseInfo parseRelease (const juce::String& json, const juce::String& installerSuffix)
{
    ReleaseInfo info;
    const auto root = juce::JSON::parse (json);
    auto* obj = root.getDynamicObject();
    if (obj == nullptr) return info;
    if ((bool) obj->getProperty ("draft") || (bool) obj->getProperty ("prerelease")) return info;
    const auto tag = obj->getProperty ("tag_name").toString().trim();
    if (tag.isEmpty() || ! tag.containsAnyOf ("0123456789")) return info;
    info.version = tag.startsWithIgnoreCase ("v") ? tag.substring (1) : tag;
    info.pageUrl = obj->getProperty ("html_url").toString();
    info.notes = obj->getProperty ("body").toString();

    if (auto* assets = obj->getProperty ("assets").getArray())
        for (auto& a : *assets)
        {
            const auto name = a["name"].toString();
            if (installerSuffix.isEmpty() || ! name.endsWithIgnoreCase (installerSuffix)) continue;
            const auto url = a["browser_download_url"].toString();
            if (! isTrustedDownloadUrl (url)) continue;
            info.assetName = juce::File::createLegalFileName (name);
            info.assetUrl = url;
            info.size = (juce::int64) a["size"];
            const auto digest = a["digest"].toString();
            if (digest.startsWithIgnoreCase ("sha256:") && isHex64 (digest.substring (7)))
                info.sha256 = digest.substring (7).toLowerCase();
            else
                info.sha256 = hashFromNotes (info.notes, name);
            break;
        }
    return info;
}

juce::String sha256Hex (const juce::File& f)
{
    if (! f.existsAsFile()) return {};
    juce::FileInputStream in (f);
    if (! in.openedOk()) return {};
    return juce::SHA256 (in).toHexString().toLowerCase();
}
} // namespace rh
