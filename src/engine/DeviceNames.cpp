// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "DeviceNames.h"

#include <cstring>

namespace rh
{
juce::String hardwareOf (const juce::String& name)
{
    // the last pair of brackets, which may hold brackets of its own: "Microphone (Realtek(R) Audio)"
    const int close = name.lastIndexOfChar (')');
    if (close < 0) return {};
    int depth = 0;
    for (int i = close; i >= 0; --i)
    {
        if (name[i] == ')') ++depth;
        else if (name[i] == '(' && --depth == 0)
            return i > 0 ? name.substring (i + 1, close).trim() : juce::String();
    }
    return {};
}

juce::String outputBeside (const juce::String& input, const juce::StringArray& outputs)
{
    const auto wanted = hardwareOf (input);
    if (wanted.isEmpty()) return {};
    juce::String first;
    for (auto& o : outputs)
    {
        if (! hardwareOf (o).equalsIgnoreCase (wanted)) continue;
        const auto n = o.toLowerCase();
        if (n.startsWith ("speakers") || n.startsWith ("headphones") || n.contains ("1/2") || n.contains ("1 + 2") || n.contains ("1-2")) return o;
        if (first.isEmpty()) first = o;
    }
    return first;
}

juce::String outputPairName (const juce::String& left, const juce::String& right)
{
    const auto a = left.trim(), b = right.trim();
    if (b.isEmpty()) return a;
    // the same name with a side after it: "Headphones L" / "Headphones R", "Main Left" / "Main Right"
    for (auto [l, r] : { std::pair<const char*, const char*> { " L", " R" }, { " Left", " Right" }, { " left", " right" } })
        if (a.endsWith (l) && b.endsWith (r) && a.dropLastCharacters ((int) std::strlen (l)) == b.dropLastCharacters ((int) std::strlen (r)))
            return a.dropLastCharacters ((int) std::strlen (l)).trim();
    // the same name with a number after it: "Out 1" / "Out 2" becomes "Out 1 + 2"
    const auto stem = a.trimCharactersAtEnd ("0123456789").trim();
    if (stem.isNotEmpty() && stem == b.trimCharactersAtEnd ("0123456789").trim() && stem != a && stem != b)
        return stem + " " + a.substring (stem.length()).trim() + " + " + b.substring (stem.length()).trim();
    return a + " + " + b;
}

bool sameHardware (const juce::String& asioDriver, const juce::String& windowsName)
{
    // words that say what kind of thing it is, not whose it is
    static const juce::StringArray common { "asio", "driver", "usb", "audio", "device", "interface", "the", "and", "for", "series", "mk", "gen", "pro" };
    const auto where = (hardwareOf (windowsName).isNotEmpty() ? hardwareOf (windowsName) : windowsName).toLowerCase();
    juce::StringArray whereWords;
    whereWords.addTokens (where.replaceCharacters ("()-_,.", "      "), " ", "");
    whereWords.removeEmptyStrings();
    juce::StringArray words;
    words.addTokens (asioDriver.toLowerCase().replaceCharacters ("()-_,.", "      "), " ", "");
    for (const auto& w : words)
        if (w.length() >= 3 && ! common.contains (w) && ! w.containsOnly ("0123456789") && whereWords.contains (w)) return true;
    return false;
}
} // namespace rh
