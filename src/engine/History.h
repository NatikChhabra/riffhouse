// Riffhouse - free, open-source guitar rig
// Copyright (C) 2026 Riffhouse contributors. Licensed under AGPL-3.0-or-later.
#pragma once

#include "Params.h"

#include <juce_core/juce_core.h>
#include <vector>

namespace rh
{
// Everything that makes the tone what it is right now: the value of every control, the values
// the tone had when it was loaded (so "edited" still means something after an undo), and which
// tone it is.
struct ToneState
{
    ParamValues values {}, snapshot {};
    juce::String libraryName, external;
};

// Input-side settings belong to your setup, not to the tone: tones never touch them and
// neither does undo.
bool belongsToSetup (P id);
bool sameTone (const ToneState&, const ToneState&);

// Undo and redo. The rig is small enough to keep whole, so a step is a complete ToneState.
// Nothing has to tell the history that something changed: it is shown the tone a few times a
// second and makes a step of a change once it has stopped moving and no control is held, so
// one long drag of a knob is one step.
class ToneHistory
{
public:
    void tick (const ToneState& now, bool handsOn);
    bool canUndo (const ToneState& now) const;
    bool canRedo() const noexcept { return ! future.empty(); }
    // Both give the state to put back and return false when there is nothing to do.
    // Call settle() once it has been put back.
    bool undo (const ToneState& now, ToneState& to);
    bool redo (const ToneState& now, ToneState& to);
    void settle (const ToneState& now);
    int depth() const noexcept { return (int) past.size(); }

    static constexpr size_t maxSteps = 200;

private:
    std::vector<ToneState> past, future;
    ToneState settled, lastSeen;
    bool started = false;
    int stable = 0;
};
} // namespace rh
