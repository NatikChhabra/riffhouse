// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "DeviceNames.h"

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
} // namespace rh
