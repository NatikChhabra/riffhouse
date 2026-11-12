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

juce::String whyShared (const juce::String& type, const juce::String& input, const juce::String& output,
                        const juce::StringArray& outputs)
{
    if (type != "Windows Audio" && type != "DirectSound") return {};
    const auto inHw = hardwareOf (input), outHw = hardwareOf (output);
    const auto inName = inHw.isNotEmpty() ? inHw : input;
    if (inHw.isNotEmpty() && outHw.isNotEmpty() && ! inHw.equalsIgnoreCase (outHw))
        return "Your guitar comes in on " + inHw + " and the sound goes out on " + outHw + ". Two separate devices can't keep "
               "in step when Riffhouse has them to itself, so Windows' shared mode joins them, which adds delay. "
               + (! outputs.isEmpty() && ! [&] { for (const auto& o : outputs) if (hardwareOf (o).equalsIgnoreCase (inHw)) return true; return false; }()
                      ? juce::String (inHw + " has no headphone socket, so the shortest delay needs one device for both: an audio "
                                      "interface with a guitar input and a headphone socket.")
                      : juce::String ("For the shortest delay, plug your headphones into " + inHw + " and pick it under Sound out."));
    if (type == "DirectSound")
        return "Only DirectSound, the oldest and slowest way, would start on " + inName + ". Close other audio apps and run "
               "setup again, or pick a driver yourself in Settings, Driver, input and output.";
    return "The faster modes would not play clean on " + inName + " (another app may be holding it, or its driver refuses "
           "them), so Windows' shared mode is in use. Close other audio apps and run setup again, or pick a driver yourself "
           "in Settings, Driver, input and output.";
}

juce::Array<double> asioRatesToTry (double asked)
{
    juce::Array<double> rates;
    if (asked > 0) rates.add (asked);
    for (double r : { 0.0, 48000.0, 44100.0, 96000.0, 88200.0 })
        rates.addIfNotAlreadyThere (r);
    return rates;
}

juce::String whyAsioFailed (const juce::String& driver, const juce::String& error, bool silent)
{
    const auto e = error.toLowerCase();
    // the driver itself would not load or start: almost always another program has it
    const bool held = e.contains ("initiali") || e.contains ("can't start") || e.contains ("not present")
                      || e.contains ("no driver") || e.contains ("busy") || e.contains ("in use");
    juce::String s;
    if (silent)
        s = driver + " opened but no sound came through it at any sample rate. In its own settings window, check that the "
                     "clock source (or sync) is set to Internal, then pick it again.";
    else if (held)
        s = driver + " would not start. Most often another program is using it: close any voice call (Discord, Zoom, Meet), music program, game "
                     "or browser tab that uses your interface, then pick it again.";
    else
        s = driver + " would not start at any sample rate or buffer size. Most often another program is using the device, "
                     "such as a voice call (Discord, Zoom, Meet), a music program or a browser tab: close it, or unplug the "
                     "device and plug it back in, then pick it again.";
    // a driver that wraps Windows audio (ASIO4ALL, FlexASIO) needs the device all to itself
    if (! silent && (driver.containsIgnoreCase ("asio4all") || driver.containsIgnoreCase ("flexasio")))
        s << " " << driver << " needs the device all to itself, so it cannot run while any other app uses that mic or those headphones.";
    return s;
}

juce::String asioBufferAdvice (int bufferSize, double sampleRate, int sizesOffered, const juce::String& button)
{
    if (sizesOffered > 1 || bufferSize <= 0 || sampleRate <= 0) return {};
    const double ms = 1000.0 * bufferSize / sampleRate;
    if (ms < 6.0) return {};
    return "Your interface's own settings hold the buffer at " + juce::String (bufferSize) + " samples ("
           + juce::String (ms, 1) + " ms each way), and only they can change it. Press \"" + button
           + "\", set the buffer size to 128 samples (or 64), and close that window. Riffhouse picks it up by itself.";
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


BufferSearch::BufferSearch (const juce::Array<int>& all, int running) : bestSize (running)
{
    for (int b : all)
        if (b < running) sizes.add (b);
    sizes.sort();
    if (sizes.isEmpty()) return;
    // nearest to 128 samples: about 3 ms at 44.1 kHz, where most sound cards run clean
    at = 0;
    for (int i = 1; i < sizes.size(); ++i)
        if (std::abs (sizes[i] - 128) < std::abs (sizes[at] - 128)) at = i;
    trying = sizes[at];
}

void BufferSearch::result (bool clean)
{
    if (trying == 0) return;
    if (clean)
    {
        foundClean = true;
        bestSize = std::min (bestSize, trying);
        // smaller while it stays clean; after a crackle below, this is the floor
        trying = (! crackled && at > 0) ? sizes[--at] : 0;
        return;
    }
    // it crackled: below this one never runs clean. Above it, unless a clean one is already known.
    crackled = true;
    if (foundClean || at + 1 >= sizes.size()) { trying = 0; return; }
    trying = sizes[++at];
}
juce::String delayFeel (double ms)
{
    // the figure counts every queue on the way, so these are generous to it
    if (ms <= 20.0) return "instant";
    if (ms <= 35.0) return "fine";
    return "noticeable";
}

juce::String delayFigure (double ms)
{
    // juce::String (x, 0) prints every digit, so the whole number is rounded here
    return ms < 10.0 ? juce::String (ms, 1) : juce::String (juce::roundToInt (ms));
}
} // namespace rh
