// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "DeviceNames.h"

#include <cstring>
#include <cstdlib>

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

juce::String whyShared (const juce::String& type, const juce::String& input, const juce::String& output)
{
    if (type != "Windows Audio" && type != "DirectSound") return {};
    const auto inHw = hardwareOf (input), outHw = hardwareOf (output);
    const auto inName = inHw.isNotEmpty() ? inHw : input;
    if (inHw.isNotEmpty() && outHw.isNotEmpty() && ! inHw.equalsIgnoreCase (outHw))
        return "Your guitar comes in on " + inHw + " and the sound goes out on " + outHw + ". Two separate devices can't keep "
               "in step when Riffhouse has them to itself, so Windows' shared mode joins them, which adds delay. For the shortest "
               "delay, plug your headphones into " + inHw + " and pick it under Sound out.";
    if (type == "DirectSound")
        return "Only DirectSound, the oldest and slowest way, would start on " + inName + ". Close other audio apps and run "
               "setup again, or pick a driver yourself in Input, output and delay.";
    return "The faster modes would not play clean on " + inName + " (another app may be holding it, or its driver refuses "
           "them), so Windows' shared mode is in use. Close other audio apps and run setup again, or pick a driver yourself "
           "in Input, output and delay.";
}

juce::Array<int> buffersToTry (const juce::Array<int>& available)
{
    // Spread over the whole range, smallest first: the six smallest sizes can all be too small
    // for a USB device that runs clean at 256 or 512, and a mode is only judged by what is tried.
    juce::Array<int> all;
    for (int b : available)
        if (b >= 32 && b <= 1024) all.addIfNotAlreadyThere (b);
    all.sort();
    if (all.size() <= 6) return all;
    juce::Array<int> picked { all.getFirst() };
    for (int target : { 64, 128, 256, 512, 1024 })
    {
        int nearest = all.getFirst();
        for (int b : all)
            if (std::abs (b - target) < std::abs (nearest - target)) nearest = b;
        picked.addIfNotAlreadyThere (nearest);
    }
    picked.sort();
    return picked;
}

} // namespace rh
