// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#include "History.h"

#include <cmath>

namespace rh
{
bool belongsToSetup (P id)
{
    return id == P::inGain || id == P::inputFix || id == P::inputFixAmt || id == P::humFilter || id == P::gateThresh || id == P::autoLevel;
}

bool sameTone (const ToneState& a, const ToneState& b)
{
    if (a.libraryName != b.libraryName || a.external != b.external) return false;
    for (const auto& s : paramSpecs())
        if (! belongsToSetup (s.id)
            && std::abs (a.values[(size_t) s.id] - b.values[(size_t) s.id]) > 1.0e-4f * std::max (1.0f, std::abs (s.max - s.min)))
            return false;
    return true;
}

void ToneHistory::tick (const ToneState& now, bool handsOn)
{
    if (! started)
    {
        settled = lastSeen = now;
        started = true;
        return;
    }
    if (sameTone (now, settled)) { lastSeen = now; stable = 0; return; }
    if (handsOn || ! sameTone (now, lastSeen)) { lastSeen = now; stable = 0; return; } // still on the move
    if (++stable < 2) return;
    past.push_back (settled);
    if (past.size() > maxSteps) past.erase (past.begin());
    future.clear();
    settled = now;
    stable = 0;
}

bool ToneHistory::canUndo (const ToneState& now) const
{
    return ! past.empty() || (started && ! sameTone (now, settled));
}

bool ToneHistory::undo (const ToneState& now, ToneState& to)
{
    if (started && ! sameTone (now, settled)) // a change that has not settled yet is a step too
    {
        past.push_back (settled);
        future.clear();
        settled = now;
    }
    if (past.empty()) return false;
    future.push_back (settled);
    to = past.back();
    past.pop_back();
    return true;
}

bool ToneHistory::redo (const ToneState& now, ToneState& to)
{
    if (future.empty()) return false;
    if (started && ! sameTone (now, settled)) // something new was done since: there is no forward any more
    {
        future.clear();
        return false;
    }
    past.push_back (settled);
    to = future.back();
    future.pop_back();
    return true;
}

void ToneHistory::settle (const ToneState& now)
{
    settled = lastSeen = now;
    started = true;
    stable = 0;
}
} // namespace rh
